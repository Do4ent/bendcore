/**
 * Экземпляры и позиции (5.8, 5.9): у детали один главный экземпляр — его можно править и делать
 * адаптивным; остальные — копии только для чтения. «Сделать главным» переносит роль,
 * «Отвязать» превращает копию в независимую деталь. Позиции — одинаковые трубы с количеством.
 */
import React, { useCallback, useEffect, useMemo, useState } from 'react';
import { CoreClient } from './core/coreClient';
import { t } from './core/i18n';
import type { InstanceInfo, InstancesState } from './core/types';
import './panels.css';

export interface InstancesPanelProps {
  doc?: string;
  client?: CoreClient;
  onSelect?: (instanceId: string) => void;
  /** Для показа без ядра (предпросмотр, тесты). */
  initialState?: InstancesState;
}

export const InstancesPanel: React.FC<InstancesPanelProps> = ({ doc = 'default', client, onSelect, initialState }) => {
  const core = useMemo(() => client ?? new CoreClient(), [client]);
  const [state, setState] = useState<InstancesState | null>(initialState ?? null);
  const [error, setError] = useState<string | null>(null);

  const apply = useCallback(async (p: Promise<InstancesState>) => {
    try {
      setState(await p);
      setError(null);
    } catch (e) {
      setError((e as Error).message);
    }
  }, []);

  useEffect(() => {
    if (!initialState) void apply(core.instancesState(doc));
  }, [core, doc, apply, initialState]);

  const byPart = new Map<string, InstanceInfo[]>();
  for (const i of state?.instances ?? []) {
    const list = byPart.get(i.part) ?? [];
    list.push(i);
    byPart.set(i.part, list);
  }
  const newPartId = (part: string) => {
    let k = 2;
    const parts = new Set((state?.instances ?? []).map((i) => i.part));
    while (parts.has(`${part}-${k}`)) k++;
    return `${part}-${k}`;
  };

  return (
    <section className="bc-panel" aria-label={t('inst.title')}>
      <div className="bc-header">
        <h2>{t('inst.title')}</h2>
      </div>
      <div className="bc-body">
        {error && <div className="bc-banner error">{error}</div>}
        <table className="bc-table">
          <thead>
            <tr>
              <th>{t('check.tube')}</th>
              <th>Деталь</th>
              <th />
              <th>Обозначение</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {[...byPart.entries()].map(([part, list]) =>
              list.map((i) => (
                <tr key={i.id} className="clickable" onClick={() => onSelect?.(i.id)}>
                  <td>
                    {i.primary ? '★ ' : '🔒 '}
                    {i.id}
                  </td>
                  <td>{part}</td>
                  <td>
                    <span className={`bc-badge${i.primary ? ' primary' : ''}`}>
                      {i.primary ? t('inst.primary') : t('inst.copy')}
                    </span>
                  </td>
                  <td>{i.designation}</td>
                  <td onClick={(e) => e.stopPropagation()}>
                    <div style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
                    {i.primary ? (
                      <label className="bc-field" style={{ flexDirection: 'row', alignItems: 'center', gap: 4 }}>
                        <input
                          style={{ minWidth: 0 }}
                          type="checkbox"
                          checked={i.adaptive}
                          onChange={(e) => void apply(core.setAdaptive(i.id, e.target.checked, doc))}
                        />
                        {i.adaptive ? t('inst.adaptive') : t('inst.frozen')}
                      </label>
                    ) : (
                      <button className="bc-btn small" onClick={() => void apply(core.setPrimary(i.id, doc))}>
                        {t('inst.makePrimary')}
                      </button>
                    )}
                    {list.length > 1 && (
                      <button className="bc-btn small" onClick={() => void apply(core.detach(i.id, newPartId(part), doc))}>
                        {t('inst.detach')}
                      </button>
                    )}
                    </div>
                  </td>
                </tr>
              )),
            )}
          </tbody>
        </table>

        <h3 style={{ fontSize: 13, margin: '16px 0 6px' }}>Позиции</h3>
        <table className="bc-table">
          <thead>
            <tr>
              <th className="num">{t('inst.position')}</th>
              <th>Обозначение</th>
              <th className="num">{t('inst.qty')}</th>
              <th>Трубы</th>
              <th />
            </tr>
          </thead>
          <tbody>
            {(state?.positions ?? []).map((p) => (
              <tr key={p.number}>
                <td className="num">{p.number}</td>
                <td>
                  {p.designation}
                  {p.designations_differ && (
                    <span title={t('inst.differ')} style={{ color: 'var(--bc-warn)', marginLeft: 6 }} aria-label={t('inst.differ')}>
                      ▲
                    </span>
                  )}
                </td>
                <td className="num">{p.quantity}</td>
                <td>{p.instances.join(', ')}</td>
                <td>
                  {p.designations_differ && (
                    <button
                      className="bc-btn small"
                      onClick={() => void apply(core.unifyDesignation(p.instances, p.designation, doc))}
                    >
                      {t('inst.unify')}
                    </button>
                  )}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  );
};

export default InstancesPanel;
