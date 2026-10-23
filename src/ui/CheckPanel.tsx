/**
 * Панель «Проверка» (4.2): все трубы сборки за один запуск, единая таблица нарушений
 * (тип, место, сообщение, факт / норма), фильтр по классам 4.5, переход к месту нарушения.
 * Результат устаревает, если входные данные труб изменились после запуска.
 */
import React, { useMemo, useState } from 'react';
import { CoreClient } from './core/coreClient';
import { countBy, factNorm, filterViolations, iconFor, place } from './core/checkModel';
import { t } from './core/i18n';
import { Severity, type CheckResult, type CheckTubeInput, type Violation } from './core/types';
import './panels.css';

export interface CheckPanelProps {
  tubes: CheckTubeInput[];
  /** Клик по строке: переход к участку в 3D и к строке таблицы гибов. */
  onNavigate?: (tubeId: string, element: number, v: Violation) => void;
  client?: CoreClient;
  /** Норма зазора по умолчанию, мм (6.13). */
  clearance?: number;
}

const num = (x: number) => x.toLocaleString('ru-RU', { minimumFractionDigits: 1, maximumFractionDigits: 1 });

const CLASSES: Array<{ sev: Severity; ru: string }> = [
  { sev: Severity.Geometric, ru: 'Геом. ошибки' },
  { sev: Severity.Technological, ru: 'Техн. ошибки' },
  { sev: Severity.Warning, ru: 'Предупреждения' },
  { sev: Severity.Mark, ru: 'Пометки' },
  { sev: Severity.Info, ru: 'Информация' },
];

/** Ключи входа по трубам: изменился ключ — результат устарел. */
export function inputKeys(tubes: CheckTubeInput[]): Record<string, string> {
  const k: Record<string, string> = {};
  for (const tb of tubes) k[tb.id] = JSON.stringify(tb);
  return k;
}

function sameKeys(a: Record<string, string>, b: Record<string, string>): boolean {
  const ka = Object.keys(a);
  if (ka.length !== Object.keys(b).length) return false;
  return ka.every((k) => a[k] === b[k]);
}

export const CheckPanel: React.FC<CheckPanelProps> = ({ tubes, onNavigate, client, clearance = 5 }) => {
  const core = useMemo(() => client ?? new CoreClient(), [client]);
  const [result, setResult] = useState<CheckResult | null>(null);
  const [runKeys, setRunKeys] = useState<Record<string, string>>({});
  const [running, setRunning] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [gap, setGap] = useState(clearance);
  const [classes, setClasses] = useState<Set<Severity>>(new Set(CLASSES.map((c) => c.sev)));
  const [query, setQuery] = useState('');
  const [selected, setSelected] = useState<number | null>(null);

  const current = useMemo(() => inputKeys(tubes), [tubes]);
  const stale = result !== null && !sameKeys(current, runKeys);
  const state = running
    ? 'running'
    : !result
      ? 'not_checked'
      : stale
        ? 'stale'
        : result.violations.length
          ? 'violations'
          : 'passed';
  const stateText: Record<string, string> = {
    running: t('check.running'),
    not_checked: t('check.notChecked'),
    stale: t('check.stale'),
    violations: t('check.hasViolations'),
    passed: t('check.passed'),
  };

  const run = async () => {
    setRunning(true);
    setError(null);
    const keys = inputKeys(tubes);
    try {
      const r = await core.checkRun(tubes, { clearance: gap });
      setResult(r);
      setRunKeys(keys);
      setSelected(null);
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setRunning(false);
    }
  };

  const all = result?.violations ?? [];
  const counts = countBy(all);
  const shown = filterViolations(all, classes, query);

  const toggle = (sev: Severity) => {
    const next = new Set(classes);
    if (next.has(sev)) next.delete(sev);
    else next.add(sev);
    setClasses(next);
  };

  return (
    <section className="bc-panel" aria-label={t('check.title')} style={{ height: '100%' }}>
      <div className="bc-header">
        <h2>{t('check.title')}</h2>
        <span className={`bc-status ${state}`} role="status">
          {stateText[state]}
        </span>
        <label className="bc-field" style={{ flexDirection: 'row', alignItems: 'center', gap: 6 }}>
          {t('check.clearance')}
          <input
            type="number"
            min={0}
            step={0.5}
            value={gap}
            style={{ width: 60 }}
            onChange={(e) => setGap(Math.max(0, Number(e.target.value) || 0))}
          />
        </label>
        <button className="bc-btn primary" onClick={run} disabled={running || tubes.length === 0}>
          {t('check.run')}
        </button>
      </div>
      <div className="bc-body">
        {error && <div className="bc-banner error">{error}</div>}
        {result && (
          <div className="bc-row" style={{ alignItems: 'center' }}>
            <div className="bc-chips" role="group">
              {CLASSES.map((c) => (
                <button
                  key={c.sev}
                  className="bc-chip"
                  aria-pressed={classes.has(c.sev)}
                  onClick={() => toggle(c.sev)}
                >
                  <span className="bc-icon" style={{ color: iconFor({ severity: c.sev, rule: '' }).color }}>
                    {iconFor({ severity: c.sev, rule: '' }).glyph}
                  </span>
                  {c.ru}: {counts[c.sev]}
                </button>
              ))}
            </div>
            <input
              type="search"
              placeholder={t('check.tube')}
              value={query}
              onChange={(e) => setQuery(e.target.value)}
              style={{ marginLeft: 'auto', padding: '4px 6px', border: '1px solid var(--bc-line)', borderRadius: 4 }}
            />
          </div>
        )}
        {result && shown.length === 0 && <div className="bc-banner info">{t('check.none')}</div>}
        {shown.length > 0 && (
          <table className="bc-table">
            <thead>
              <tr>
                <th />
                <th>{t('check.type')}</th>
                <th>{t('check.place')}</th>
                <th>{t('check.message')}</th>
                <th className="num">{t('check.value')}</th>
              </tr>
            </thead>
            <tbody>
              {shown.map((v, i) => {
                const ic = iconFor(v);
                return (
                  <tr
                    key={`${v.tube}-${v.rule}-${v.element}-${v.other}-${i}`}
                    className={`clickable${selected === i ? ' selected' : ''}`}
                    onClick={() => {
                      setSelected(i);
                      onNavigate?.(v.tube, v.element, v);
                    }}
                  >
                    <td>
                      <span className="bc-icon" style={{ color: ic.color }} aria-hidden="true">
                        {ic.glyph}
                      </span>
                    </td>
                    <td>{v.class}</td>
                    <td>{place(v, t('check.element'))}</td>
                    <td>{v.message}</td>
                    <td className="num">{factNorm(v, num)}</td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        )}
      </div>
    </section>
  );
};

export default CheckPanel;
