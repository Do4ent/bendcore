/**
 * SelectionPanel: element mode (faces / edges / vertices / bodies or components), geometry type
 * filters and the box selection switch of a viewport (docs/ui/KERNEL_VIEWPORT.md, "Фильтры выбора и
 * выбор рамкой"). Shared by the part, assembly and STEP-viewer pages; the page keeps the state.
 */

import React from 'react';
import { caption, useI18n } from '../app/i18n';
import { EDGE_FILTER_TYPES, FACE_FILTER_TYPES, toggleEdgeType, toggleFaceType } from './selection';
import type { ElementMode, SelectionFilter } from './selection';

export interface SelectionPanelProps {
  filter: SelectionFilter;
  onFilter(filter: SelectionFilter): void;
  boxSelect: boolean;
  onBoxSelect(on: boolean): void;
  /** Element modes the page offers (the fourth one is "components" in assemblies). */
  modes: readonly ElementMode[];
  /** The element kind is dictated by an active operation slot: the mode buttons are disabled. */
  forcedMode?: ElementMode | null;
  /** Number of selected elements and a kernel-computed total (area of faces, length of edges). */
  count: number;
  total?: { kind: 'area' | 'length'; value: number } | null;
  onClear(): void;
  /** Label of the body mode ('component' in assemblies). */
  bodyLabel?: 'body' | 'component';
}

const fmt = (v: number) => Number(v.toFixed(3)).toString();

export const SelectionPanel: React.FC<SelectionPanelProps> = ({
  filter,
  onFilter,
  boxSelect,
  onBoxSelect,
  modes,
  forcedMode = null,
  count,
  total = null,
  onClear,
  bodyLabel = 'body',
}) => {
  const { t } = useI18n();
  const mode = forcedMode ?? filter.mode;
  const label = (m: ElementMode) => t(m === 'body' && bodyLabel === 'component' ? 'sel.mode.component' : (`sel.mode.${m}` as 'sel.mode.face'));
  return (
    <section data-testid="selection-panel" data-mode={mode}>
      <h2>{t('sel.title')}</h2>
      <div role="group" className="pm-field">
        {modes.map((m) => (
          <button
            key={m}
            type="button"
            data-testid={`sel-mode-${m}`}
            aria-pressed={mode === m}
            disabled={forcedMode !== null && forcedMode !== m}
            onClick={() => onFilter({ ...filter, mode: m })}
          >
            {label(m)}
          </button>
        ))}
      </div>
      {forcedMode && (
        <p className="pm-hint" data-testid="sel-forced">
          {t('sel.slotKind', { kind: t(`sel.kind.${forcedMode}` as 'sel.kind.face') })}
        </p>
      )}
      {(mode === 'face' || mode === 'edge') && (
        <div role="group" className="pm-field" aria-label={t('sel.filters')} title={t('sel.filters')}>
          {mode === 'face'
            ? FACE_FILTER_TYPES.map((type) => (
                <button
                  key={type}
                  type="button"
                  data-testid={`sel-filter-${type}`}
                  aria-pressed={filter.faceTypes.includes(type)}
                  onClick={() => onFilter(toggleFaceType(filter, type))}
                >
                  {caption(t, 'sel.type.', type)}
                </button>
              ))
            : EDGE_FILTER_TYPES.map((type) => (
                <button
                  key={type}
                  type="button"
                  data-testid={`sel-filter-${type}`}
                  aria-pressed={filter.edgeTypes.includes(type)}
                  onClick={() => onFilter(toggleEdgeType(filter, type))}
                >
                  {caption(t, 'sel.type.', type)}
                </button>
              ))}
        </div>
      )}
      <button type="button" data-testid="sel-box" aria-pressed={boxSelect} title={t('sel.box.hint')} onClick={() => onBoxSelect(!boxSelect)}>
        {t('sel.box')}
      </button>
      {boxSelect && <p className="pm-hint">{t('sel.box.hint')}</p>}
      <p data-testid="sel-count" data-count={count} data-total={total ? total.value : ''}>
        {t('sel.count', { count })}
        {total ? `, ${t(total.kind === 'area' ? 'sel.area' : 'sel.length', { value: fmt(total.value) })}` : ''}
      </p>
      {count > 0 && (
        <button type="button" data-testid="sel-clear" onClick={onClear}>
          {t('sel.clear')}
        </button>
      )}
    </section>
  );
};

export default SelectionPanel;
