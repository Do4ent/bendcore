/**
 * Assembly tree (docs/ui/ASSEMBLY_UI.md): components with their degrees of freedom, fixed and
 * suppressed states; mates with their solve status; component patterns. Every button calls the
 * assembly document through the callbacks of AssemblyApp.
 */

import React from 'react';
import type { AssemblyState, MateInfo, OccurrenceInfo } from '../../wasm/bendcore_assembly_api.mjs';
import { flattenOccurrences } from './assemblyMath';
import { caption, translate, useI18n } from '../app/i18n';
import type { Translate } from '../app/i18n';

export interface AssemblyTreeProps {
  state: AssemblyState;
  selectedComponent: number | null;
  onSelectComponent: (id: number | null) => void;
  onSetFixed: (id: number, fixed: boolean) => void;
  onSetSuppressed: (id: number, suppressed: boolean) => void;
  onRemoveComponent: (id: number) => void;
  onSelectMate: (mate: MateInfo) => void;
  onSuppressMate: (id: number, suppressed: boolean) => void;
  onRemoveMate: (id: number) => void;
  onRemovePattern: (id: number) => void;
}

function occurrenceName(state: AssemblyState, path: number[]): string {
  const names: string[] = [];
  let list: OccurrenceInfo[] = state.occurrences;
  for (const id of path) {
    const o = list.find((x) => x.id === id);
    if (!o) return path.join('/');
    names.push(o.name);
    list = o.children ?? [];
  }
  return names.join(' / ');
}

export function refLabel(state: AssemblyState, ref: MateInfo['a'], t: Translate = (k, p) => translate('ru', k, p)): string {
  const where = occurrenceName(state, ref.path);
  if (ref.index === undefined && !ref.name) return where;
  const kind = ref.kind === 'edge' ? t('assembly.ref.edge') : ref.kind === 'vertex' ? t('assembly.ref.vertex') : t('assembly.ref.face');
  return `${where}: ${kind} ${ref.index ?? ref.name}`;
}

const AssemblyTree: React.FC<AssemblyTreeProps> = ({
  state,
  selectedComponent,
  onSelectComponent,
  onSetFixed,
  onSetSuppressed,
  onRemoveComponent,
  onSelectMate,
  onSuppressMate,
  onRemoveMate,
  onRemovePattern,
}) => {
  const { t } = useI18n();
  const rows = flattenOccurrences(state.occurrences);
  return (
    <div className="as-tree" data-testid="assembly-tree">
      <h2>{t('assembly.tree.components')}</h2>
      {rows.length === 0 && <p className="pm-hint">{t('assembly.tree.empty')}</p>}
      <ul>
        {rows.map(({ occurrence: o, path, depth }) => {
          const top = depth === 0;
          const cls = [
            'as-row',
            o.suppressed ? 'as-suppressed' : '',
            top && selectedComponent === o.id ? 'as-selected' : '',
          ].join(' ');
          return (
            <li
              key={path.join('/')}
              className={cls}
              style={{ paddingLeft: 4 + depth * 14 }}
              data-testid={`component-${path.join('-')}`}
              onClick={() => top && onSelectComponent(o.id)}
            >
              <span className="as-name" title={o.name}>
                {o.kind === 'assembly' ? '▸ ' : ''}
                {o.name}
              </span>
              {top && (
                <span
                  className={`as-dof ${o.fixed ? 'as-dof-fixed' : o.dof === 0 ? 'as-dof-zero' : ''}`}
                  data-testid={`dof-${o.id}`}
                  title={o.dofShared ? t('assembly.tree.dofShared') : t('assembly.tree.dofOwn')}
                >
                  {o.fixed ? t('assembly.tree.fixed') : `${t('assembly.tree.dof', { n: o.dof ?? '' })}${o.dofShared ? '*' : ''}`}
                </span>
              )}
              {top && o.pattern === null && (
                <span className="as-actions" onClick={(e) => e.stopPropagation()}>
                  <button type="button" title={o.fixed ? t('menu.unfix') : t('menu.fix')} onClick={() => onSetFixed(o.id, !o.fixed)}>
                    {o.fixed ? t('menu.unfix') : t('menu.fix')}
                  </button>
                  <button
                    type="button"
                    title={o.suppressed ? t('assembly.tree.unsuppress') : t('assembly.tree.suppress')}
                    onClick={() => onSetSuppressed(o.id, !o.suppressed)}
                  >
                    {o.suppressed ? t('assembly.tree.unsuppressShort') : t('assembly.tree.suppress')}
                  </button>
                  <button type="button" title={t('assembly.tree.removeComponent')} onClick={() => onRemoveComponent(o.id)}>
                    ✕
                  </button>
                </span>
              )}
              {top && o.pattern !== null && <span className="as-muted">{t('assembly.tree.inPattern', { id: o.pattern })}</span>}
            </li>
          );
        })}
      </ul>
      <h2>{t('assembly.tree.mates')}</h2>
      {state.mates.length === 0 && <p className="pm-hint">{t('assembly.tree.matesEmpty')}</p>}
      <ul>
        {state.mates.map((m) => (
          <li
            key={m.id}
            className={`as-row as-mate as-mate-${m.status}`}
            data-testid={`mate-${m.id}`}
            title={`${refLabel(state, m.a, t)} — ${refLabel(state, m.b, t)}${m.broken ? `\n${m.broken}` : ''}`}
            onClick={() => onSelectMate(m)}
          >
            <span className="as-name">
              {m.name || caption(t, 'assembly.mateType.', m.type)}
              {m.type === 'distance' && ` ${t('assembly.mm', { value: m.value })}`}
              {m.type === 'angle' && ` ${m.value}°`}
            </span>
            <span className="as-status">{caption(t, 'assembly.mateStatus.', m.status)}</span>
            <span className="as-actions" onClick={(e) => e.stopPropagation()}>
              <button type="button" onClick={() => onSuppressMate(m.id, !m.suppressed)}>
                {m.suppressed ? t('assembly.tree.unsuppressShort') : t('assembly.tree.suppress')}
              </button>
              <button type="button" title={t('assembly.tree.removeMate')} onClick={() => onRemoveMate(m.id)}>
                ✕
              </button>
            </span>
          </li>
        ))}
      </ul>
      {state.patterns.length > 0 && (
        <>
          <h2>{t('assembly.tree.patterns')}</h2>
          <ul>
            {state.patterns.map((p) => (
              <li key={p.id} className="as-row" data-testid={`pattern-${p.id}`}>
                <span className="as-name">
                  {caption(t, 'assembly.tree.pattern.', p.kind === 'Linear' || p.kind === 'Circular' ? p.kind : 'Feature')}{' '}
                  {occurrenceName(state, [p.seed])} × {p.instances.length + 1}
                </span>
                <span className="as-actions">
                  <button type="button" title={t('assembly.tree.removePattern')} onClick={() => onRemovePattern(p.id)}>
                    ✕
                  </button>
                </span>
              </li>
            ))}
          </ul>
        </>
      )}
    </div>
  );
};

export default AssemblyTree;
