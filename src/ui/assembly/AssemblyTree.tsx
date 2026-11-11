/**
 * Assembly tree (docs/ui/ASSEMBLY_UI.md): components with their degrees of freedom, fixed and
 * suppressed states; mates with their solve status; component patterns. Every button calls the
 * assembly document through the callbacks of AssemblyApp.
 */

import React from 'react';
import type { AssemblyState, MateInfo, OccurrenceInfo } from '../../wasm/bendcore_assembly_api.mjs';
import { flattenOccurrences } from './assemblyMath';
import { MATE_LABELS, MATE_STATUS_LABELS } from './labels';

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

export function refLabel(state: AssemblyState, ref: MateInfo['a']): string {
  const where = occurrenceName(state, ref.path);
  if (ref.index === undefined && !ref.name) return where;
  const kind = ref.kind === 'edge' ? 'ребро' : ref.kind === 'vertex' ? 'вершина' : 'грань';
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
  const rows = flattenOccurrences(state.occurrences);
  return (
    <div className="as-tree" data-testid="assembly-tree">
      <h2>Компоненты</h2>
      {rows.length === 0 && <p className="pm-hint">Вставьте деталь, подсборку или крепёж.</p>}
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
                  title={o.dofShared ? 'степени свободы подсистемы из нескольких компонентов' : 'степени свободы компонента'}
                >
                  {o.fixed ? 'фикс.' : `${o.dof} СС${o.dofShared ? '*' : ''}`}
                </span>
              )}
              {top && o.pattern === null && (
                <span className="as-actions" onClick={(e) => e.stopPropagation()}>
                  <button type="button" title={o.fixed ? 'Освободить' : 'Зафиксировать'} onClick={() => onSetFixed(o.id, !o.fixed)}>
                    {o.fixed ? 'Освободить' : 'Фиксировать'}
                  </button>
                  <button
                    type="button"
                    title={o.suppressed ? 'Включить' : 'Подавить'}
                    onClick={() => onSetSuppressed(o.id, !o.suppressed)}
                  >
                    {o.suppressed ? 'Вкл.' : 'Подавить'}
                  </button>
                  <button type="button" title="Удалить компонент" onClick={() => onRemoveComponent(o.id)}>
                    ✕
                  </button>
                </span>
              )}
              {top && o.pattern !== null && <span className="as-muted">массив {o.pattern}</span>}
            </li>
          );
        })}
      </ul>
      <h2>Сопряжения</h2>
      {state.mates.length === 0 && <p className="pm-hint">Выберите два элемента во вьюпорте.</p>}
      <ul>
        {state.mates.map((m) => (
          <li
            key={m.id}
            className={`as-row as-mate as-mate-${m.status}`}
            data-testid={`mate-${m.id}`}
            title={`${refLabel(state, m.a)} — ${refLabel(state, m.b)}${m.broken ? `\n${m.broken}` : ''}`}
            onClick={() => onSelectMate(m)}
          >
            <span className="as-name">
              {m.name || MATE_LABELS[m.type]}
              {m.type === 'distance' && ` ${m.value} мм`}
              {m.type === 'angle' && ` ${m.value}°`}
            </span>
            <span className="as-status">{MATE_STATUS_LABELS[m.status]}</span>
            <span className="as-actions" onClick={(e) => e.stopPropagation()}>
              <button type="button" onClick={() => onSuppressMate(m.id, !m.suppressed)}>
                {m.suppressed ? 'Вкл.' : 'Подавить'}
              </button>
              <button type="button" title="Удалить сопряжение" onClick={() => onRemoveMate(m.id)}>
                ✕
              </button>
            </span>
          </li>
        ))}
      </ul>
      {state.patterns.length > 0 && (
        <>
          <h2>Массивы</h2>
          <ul>
            {state.patterns.map((p) => (
              <li key={p.id} className="as-row" data-testid={`pattern-${p.id}`}>
                <span className="as-name">
                  {p.kind === 'Linear' ? 'Линейный' : p.kind === 'Circular' ? 'Круговой' : 'По отверстиям'} массив{' '}
                  {occurrenceName(state, [p.seed])} × {p.instances.length + 1}
                </span>
                <span className="as-actions">
                  <button type="button" title="Удалить массив" onClick={() => onRemovePattern(p.id)}>
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
