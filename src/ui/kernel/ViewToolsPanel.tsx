/**
 * Panels of the viewport tools (docs/ui/KERNEL_VIEWPORT.md, "Измерения и сечение"): measurements
 * between picked faces and edges and the section plane with a slider. Shared by the part, assembly
 * and STEP-viewer pages; the pages call the kernel and keep the state, the panels only show it.
 */

import React from 'react';
import { caption, useI18n } from '../app/i18n';
import { MEASURE_TYPES, SECTION_AXES, formatMeasure, picksNeeded, singleKind } from './viewTools';
import type { MeasurePick, MeasureResult, MeasureType, SectionAxis, SectionState } from './viewTools';

export interface MeasurePanelProps {
  active: boolean;
  type: MeasureType;
  picks: readonly MeasurePick[];
  result: MeasureResult | null;
  error: string | null;
  /** Element kind a click picks (faces or edges), for the types that accept both. */
  pickKind: 'face' | 'edge';
  onActive(active: boolean): void;
  onType(type: MeasureType): void;
  onPickKind(kind: 'face' | 'edge'): void;
  onClear(): void;
}

export const MeasurePanel: React.FC<MeasurePanelProps> = ({ active, type, picks, result, error, pickKind, onActive, onType, onPickKind, onClear }) => {
  const { t } = useI18n();
  const needed = picksNeeded(type);
  const fixedKind = singleKind(type);
  return (
    <section data-testid="measure-panel">
      <h2>{t('tools.measure.title')}</h2>
      <button type="button" data-testid="measure-toggle" aria-pressed={active} onClick={() => onActive(!active)}>
        {active ? t('tools.measure.off') : t('tools.measure.on')}
      </button>
      {active && (
        <>
          <label className="pm-field">
            <span>{t('tools.measure.type')}</span>
            <select data-testid="measure-type" value={type} onChange={(e) => onType(e.target.value as MeasureType)}>
              {MEASURE_TYPES.map((m) => (
                <option key={m} value={m}>
                  {caption(t, 'tools.type.', m)}
                </option>
              ))}
            </select>
          </label>
          {!fixedKind && (
            <div role="group" className="pm-field">
              <button type="button" data-testid="measure-kind-face" aria-pressed={pickKind === 'face'} onClick={() => onPickKind('face')}>
                {t('tools.measure.pickFaces')}
              </button>
              <button type="button" data-testid="measure-kind-edge" aria-pressed={pickKind === 'edge'} onClick={() => onPickKind('edge')}>
                {t('tools.measure.pickEdges')}
              </button>
            </div>
          )}
          <p className="pm-hint" data-testid="measure-hint">
            {needed === 2 ? t('tools.measure.hintTwo') : t('tools.measure.hintOne')}
          </p>
          <p data-testid="measure-picked" data-count={picks.length}>
            {t('tools.measure.picked', { count: picks.length, needed })}
          </p>
          <button type="button" data-testid="measure-clear" disabled={picks.length === 0} onClick={onClear}>
            {t('tools.measure.clear')}
          </button>
          {error && (
            <p role="alert" data-testid="measure-error">
              {t('tools.measure.failed', { reason: error })}
            </p>
          )}
          {result && (
            <dl data-testid="measure-result" data-type={result.type} data-value={result.value}>
              <dt>{caption(t, 'tools.type.', result.type)}</dt>
              <dd data-testid="measure-value">{formatMeasure(result)}</dd>
              {result.diameter !== undefined && (
                <>
                  <dt>{t('tools.measure.diameter')}</dt>
                  <dd data-testid="measure-diameter">{formatMeasure({ value: result.diameter, unit: 'mm' })}</dd>
                </>
              )}
            </dl>
          )}
        </>
      )}
    </section>
  );
};

export interface SectionPanelProps {
  state: SectionState;
  /** Plane position on the axis, mm; null while there is no model. */
  position: number | null;
  /** Section area from the kernel, mm^2 (null: not computed or the plane misses the solid). */
  area: number | null;
  error: string | null;
  disabled?: boolean;
  onChange(next: SectionState): void;
}

export const SectionPanel: React.FC<SectionPanelProps> = ({ state, position, area, error, disabled = false, onChange }) => {
  const { t } = useI18n();
  return (
    <section data-testid="section-panel">
      <h2>{t('tools.section.title')}</h2>
      <button
        type="button"
        data-testid="section-toggle"
        aria-pressed={state.enabled}
        disabled={disabled}
        onClick={() => onChange({ ...state, enabled: !state.enabled })}
      >
        {state.enabled ? t('tools.section.off') : t('tools.section.on')}
      </button>
      {state.enabled && (
        <>
          <label className="pm-field">
            <span>{t('tools.section.axis')}</span>
            <select data-testid="section-axis" value={state.axis} onChange={(e) => onChange({ ...state, axis: e.target.value as SectionAxis })}>
              {SECTION_AXES.map((a) => (
                <option key={a} value={a}>
                  {caption(t, 'tools.axis.', a)}
                </option>
              ))}
            </select>
          </label>
          <label className="pm-field">
            <span>{t('tools.section.position')}</span>
            <input
              type="range"
              min={0}
              max={1000}
              step={1}
              value={Math.round(state.fraction * 1000)}
              data-testid="section-slider"
              aria-label={t('tools.section.slider')}
              onChange={(e) => onChange({ ...state, fraction: Number(e.target.value) / 1000 })}
            />
            <output data-testid="section-position" data-value={position ?? ''}>
              {position === null ? '—' : Number(position.toFixed(3)).toString()}
            </output>
          </label>
          <label className="pm-field">
            <input type="checkbox" data-testid="section-flip" checked={state.flip} onChange={(e) => onChange({ ...state, flip: e.target.checked })} />
            <span>{t('tools.section.flip')}</span>
          </label>
          <p data-testid="section-area" data-value={area ?? ''}>
            {t('tools.section.area')}: {area === null ? t('tools.section.noArea') : Number(area.toFixed(3)).toString()}
          </p>
          {error && (
            <p role="alert" data-testid="section-error">
              {t('tools.section.failed', { reason: error })}
            </p>
          )}
        </>
      )}
    </section>
  );
};
