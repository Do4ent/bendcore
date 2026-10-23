/**
 * Диалог «Карта гибки → CSV XYZ / LRA» (3.6) для измерительных систем.
 * Значения — по детали после подрезки (с глубиной вставки E), без компенсации пружинения.
 * Вывод заблокирован при технологической или геометрической ошибке (4.5); для спирали ядро
 * отвечает «LRA не описывает спираль».
 */
import React, { useEffect, useMemo, useState } from 'react';
import { CoreClient } from './core/coreClient';
import { t } from './core/i18n';
import type { Feed, LraRow, PathItem, Vec3, XyzRow } from './core/types';
import './panels.css';

export interface BendDataTube {
  name: string;
  path: PathItem[];
  E_A?: number;
  E_B?: number;
  /** Свойство «Подача с конца» (3.5). */
  feed?: Feed;
  /** «Оба порядка»: можно выбрать вариант подачи (3.6). */
  bothOrders?: boolean;
  /** Есть технологическая или геометрическая ошибка — вывод заблокирован. */
  blocksOutput?: boolean;
}

export interface BendDataDialogProps {
  open: boolean;
  tube: BendDataTube;
  onClose: () => void;
  client?: CoreClient;
  /** Сохранение файла; по умолчанию — скачивание через браузер. */
  onSave?: (fileName: string, content: string) => void;
}

type Format = 'lra' | 'xyz';

function download(fileName: string, content: string): void {
  // BOM, чтобы Excel открыл кириллицу в UTF-8.
  const blob = new Blob(['﻿' + content], { type: 'text/csv;charset=utf-8' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = fileName;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

export const BendDataDialog: React.FC<BendDataDialogProps> = ({ open, tube, onClose, client, onSave }) => {
  const core = useMemo(() => client ?? new CoreClient(), [client]);
  const [format, setFormat] = useState<Format>('lra');
  const [feed, setFeed] = useState<Feed>(tube.feed ?? 'A');
  const [delimiter, setDelimiter] = useState(';');
  const [precision, setPrecision] = useState(2);
  const [origin, setOrigin] = useState<Vec3>([0, 0, 0]);
  // Строки хранятся вместе с форматом: при переключении формата старые строки не рисуются чужой таблицей.
  const [data, setData] = useState<{ format: Format; rows: Array<LraRow | XyzRow> }>({ format: 'lra', rows: [] });
  const [csv, setCsv] = useState('');
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    if (!open) return;
    let alive = true;
    const args = {
      path: tube.path,
      feed: tube.bothOrders ? feed : tube.feed ?? 'A',
      E_A: tube.E_A ?? 0,
      E_B: tube.E_B ?? 0,
      precision,
      delimiter,
      frame: format === 'xyz' ? { origin } : undefined,
    };
    (format === 'lra' ? core.lra(args) : core.xyz(args))
      .then((r) => {
        if (!alive) return;
        setData({ format, rows: r.rows as Array<LraRow | XyzRow> });
        setCsv(r.csv);
        setError(null);
      })
      .catch((e: Error) => {
        if (!alive) return;
        setData({ format, rows: [] });
        setCsv('');
        setError(e.message);
      });
    return () => {
      alive = false;
    };
  }, [open, core, tube, format, feed, delimiter, precision, origin]);

  if (!open) return null;
  const effFeed = tube.bothOrders ? feed : tube.feed ?? 'A';
  const blocked = !!tube.blocksOutput;
  const fileName = `${tube.name}_${format.toUpperCase()}_${effFeed === 'A' ? 'A' : 'B'}.csv`;
  const f = (x: number) => x.toFixed(precision);
  const rows = data.format === format ? data.rows : [];

  return (
    <div className="bc-modal-backdrop" role="dialog" aria-modal="true" aria-label={t('bend.title')}>
      <div className="bc-panel bc-modal">
        <div className="bc-header">
          <h2>
            {t('bend.title')} — {tube.name}
          </h2>
          <button className="bc-btn small" onClick={onClose} aria-label={t('common.close')}>
            ✕
          </button>
        </div>
        <div className="bc-body">
          {blocked && <div className="bc-banner error">{t('bend.blocked')}</div>}
          <p className="bc-note">{t('bend.note')}</p>
          <div className="bc-row">
            <div className="bc-field">
              {t('bend.format')}
              <div className="bc-seg" role="group">
                <button aria-pressed={format === 'lra'} onClick={() => setFormat('lra')}>
                  {t('bend.lra')}
                </button>
                <button aria-pressed={format === 'xyz'} onClick={() => setFormat('xyz')}>
                  {t('bend.xyz')}
                </button>
              </div>
            </div>
            <div className="bc-field">
              {t('bend.feed')}
              <div className="bc-seg" role="group">
                {(['A', 'B'] as Feed[]).map((x) => (
                  <button
                    key={x}
                    aria-pressed={effFeed === x}
                    disabled={!tube.bothOrders && x !== effFeed}
                    onClick={() => setFeed(x)}
                  >
                    {x === 'A' ? t('bend.endA') : t('bend.endB')}
                  </button>
                ))}
              </div>
            </div>
            <label className="bc-field">
              {t('bend.delimiter')}
              <select value={delimiter} onChange={(e) => setDelimiter(e.target.value)}>
                <option value=";">;</option>
                <option value=",">,</option>
                <option value={'\t'}>Tab</option>
              </select>
            </label>
            <label className="bc-field">
              {t('bend.precision')}
              <select value={precision} onChange={(e) => setPrecision(Number(e.target.value))}>
                {[0, 1, 2, 3, 4].map((p) => (
                  <option key={p} value={p}>
                    {p}
                  </option>
                ))}
              </select>
            </label>
          </div>
          {format === 'xyz' && (
            <div className="bc-row">
              <span className="bc-field">{t('bend.frame')}: {t('bend.framePart')} +</span>
              {(['X', 'Y', 'Z'] as const).map((ax, i) => (
                <label key={ax} className="bc-field">
                  {ax}₀
                  <input
                    type="number"
                    value={origin[i]}
                    onChange={(e) => {
                      const o = [...origin] as Vec3;
                      o[i] = Number(e.target.value) || 0;
                      setOrigin(o);
                    }}
                  />
                </label>
              ))}
            </div>
          )}
          {error && <div className="bc-banner error">{error}</div>}
          {!error && (
            <table className="bc-table" aria-label={t('bend.preview')}>
              <thead>
                {format === 'lra' ? (
                  <tr>
                    <th>№</th>
                    <th className="num">L, мм</th>
                    <th className="num">R — поворот, °</th>
                    <th className="num">A — угол, °</th>
                    <th className="num">Радиус, мм</th>
                  </tr>
                ) : (
                  <tr>
                    <th>№</th>
                    <th className="num">X</th>
                    <th className="num">Y</th>
                    <th className="num">Z</th>
                    <th className="num">R</th>
                  </tr>
                )}
              </thead>
              <tbody>
                {format === 'lra'
                  ? (rows as LraRow[]).map((r, i) => (
                      <tr key={i}>
                        <td>{r.is_end ? t('bend.end') : r.bend_no}</td>
                        <td className="num">{f(r.L)}</td>
                        <td className="num">{r.is_end ? '' : f(r.rotation)}</td>
                        <td className="num">{r.is_end ? '' : f(r.angle)}</td>
                        <td className="num">{r.is_end ? '' : f(r.radius)}</td>
                      </tr>
                    ))
                  : (rows as XyzRow[]).map((r, i) => (
                      <tr key={i}>
                        <td>{r.label}</td>
                        <td className="num">{f(r.p[0])}</td>
                        <td className="num">{f(r.p[1])}</td>
                        <td className="num">{f(r.p[2])}</td>
                        <td className="num">{r.radius === null ? '' : f(r.radius)}</td>
                      </tr>
                    ))}
              </tbody>
            </table>
          )}
        </div>
        <div className="bc-footer">
          <span className="bc-note" style={{ marginRight: 'auto', marginBottom: 0 }}>
            {fileName}
          </span>
          <button className="bc-btn" onClick={onClose}>
            {t('common.cancel')}
          </button>
          <button
            className="bc-btn primary"
            disabled={blocked || !!error || !csv}
            onClick={() => (onSave ?? download)(fileName, csv)}
          >
            {t('bend.save')}
          </button>
        </div>
      </div>
    </div>
  );
};

export default BendDataDialog;
