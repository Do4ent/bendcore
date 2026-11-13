/**
 * Operation panel (docs/ui/PART_MODELING_UI.md, "Панель операции"): fields of the feature type
 * from the kernel catalog — expression parameters, options, links to earlier nodes, reference
 * slots filled by picking in the viewport — plus the sketch profile (new sketch) or the sketch
 * dimensions (existing sketch). "Предпросмотр" applies the form inside the open transaction and
 * rebuilds; "ОК" applies and commits; "Отмена" aborts the transaction.
 */

import React from 'react';
import type { FeatureTypeSpec, FeatureView } from '../../wasm/bendcore_modeling_api.mjs';
import { FeatureForm, eligibleLinks, missingInputs } from './featureForm';
import { caption, useI18n } from '../app/i18n';
import type { MessageKey } from '../app/i18n';
import { SketchTemplateForm, sketchDimensions } from './sketchDimensions';

export interface FeaturePanelProps {
  mode: 'add' | 'edit';
  spec: FeatureTypeSpec;
  form: FeatureForm;
  /** Current node (after a preview, or the edited node); null before the first preview. */
  view: FeatureView | null;
  features: FeatureView[];
  /** Index where the node is (edit) or will be inserted (add). */
  position: number;
  activeSlot: string | null;
  onChange: (form: FeatureForm) => void;
  onActivateSlot: (slot: string | null) => void;
  onPreview: () => void;
  onOk: () => void;
  onCancel: () => void;
}

const fmt = (v: number) => (Number.isFinite(v) ? Number(v.toFixed(4)).toString() : String(v));

const PROFILE_KINDS: SketchTemplateForm['kind'][] = ['rectangle', 'circle', 'polygon'];

const TEMPLATE_FIELDS: Record<'rectangle' | 'circle', (keyof SketchTemplateForm & ('x' | 'y' | 'width' | 'height' | 'cx' | 'cy' | 'radius'))[]> = {
  rectangle: ['x', 'y', 'width', 'height'],
  circle: ['cx', 'cy', 'radius'],
};

const KIND_KEYS: Record<string, MessageKey> = {
  edge: 'part.panel.kind.edge',
  face: 'part.panel.kind.face',
  vertex: 'part.panel.kind.vertex',
};

const FeaturePanel: React.FC<FeaturePanelProps> = ({
  mode,
  spec,
  form,
  view,
  features,
  position,
  activeSlot,
  onChange,
  onActivateSlot,
  onPreview,
  onOk,
  onCancel,
}) => {
  const { t } = useI18n();
  const slot = (s: string) => caption(t, 'part.slot.', s);
  const missing = missingInputs(spec, form, slot, t);
  const setTemplate = (patch: Partial<SketchTemplateForm>) =>
    form.template && onChange({ ...form, template: { ...form.template, ...patch } });
  const dims = view?.sketch && form.dimensions ? sketchDimensions(view.sketch, t) : [];

  return (
    <section className="pm-panel" data-testid="feature-panel" data-type={spec.type}>
      <h2>
        {mode === 'add' ? t('part.panel.new') : t('part.panel.edit')}: {caption(t, 'part.feature.', spec.type)}
        {view ? ` (${view.id})` : ''}
      </h2>
      <label className="pm-field">
        <span>{t('part.panel.name')}</span>
        <input
          value={form.label}
          placeholder={view?.id ?? t('part.panel.defaultName')}
          data-testid="panel-label"
          onChange={(e) => onChange({ ...form, label: e.target.value })}
        />
      </label>

      {form.template && (
        <fieldset className="pm-group">
          <legend>{t('part.panel.profile')}</legend>
          <label className="pm-field">
            <span>{t('part.panel.shape')}</span>
            <select
              value={form.template.kind}
              data-testid="sketch-kind"
              onChange={(e) => setTemplate({ kind: e.target.value as SketchTemplateForm['kind'] })}
            >
              {PROFILE_KINDS.map((k) => (
                <option key={k} value={k}>
                  {t(`part.shape.${k}`)}
                </option>
              ))}
            </select>
          </label>
          {form.template.kind === 'polygon' ? (
            <label className="pm-field pm-field-wide">
              <span>{t('part.panel.vertices')}</span>
              <textarea
                rows={5}
                value={form.template.polygon}
                data-testid="sketch-polygon"
                onChange={(e) => setTemplate({ polygon: e.target.value })}
              />
            </label>
          ) : (
            TEMPLATE_FIELDS[form.template.kind].map((f) => (
              <label className="pm-field" key={f}>
                <span>{t(`part.shape.${f}`)}</span>
                <input
                  value={form.template![f]}
                  data-testid={`sketch-${f}`}
                  onChange={(e) => setTemplate({ [f]: e.target.value })}
                />
              </label>
            ))
          )}
        </fieldset>
      )}

      {dims.length > 0 && form.dimensions && (
        <fieldset className="pm-group">
          <legend>{t('part.panel.sketchDimensions')}</legend>
          {dims.map((d) => (
            <label className="pm-field" key={d.key}>
              <span>{d.label}</span>
              <input
                value={form.dimensions![d.key]}
                data-testid={`dim-${d.key}`}
                onChange={(e) => onChange({ ...form, dimensions: { ...form.dimensions!, [d.key]: e.target.value } })}
              />
            </label>
          ))}
        </fieldset>
      )}

      {spec.parameters.length > 0 && (
        <fieldset className="pm-group">
          <legend>{t('part.panel.parameters')}</legend>
          {spec.parameters.map((p) => (
            <label className="pm-field" key={p.name}>
              <span>{caption(t, 'part.param.', p.name)}</span>
              <input
                value={form.parameters[p.name] ?? ''}
                data-testid={`param-${p.name}`}
                onChange={(e) => onChange({ ...form, parameters: { ...form.parameters, [p.name]: e.target.value } })}
              />
              {view && view.values[p.name] !== undefined && (
                <em className="pm-value" data-testid={`value-${p.name}`}>
                  = {fmt(view.values[p.name])}
                </em>
              )}
            </label>
          ))}
        </fieldset>
      )}

      {spec.options.length > 0 && (
        <fieldset className="pm-group">
          <legend>{t('part.panel.options')}</legend>
          {spec.options.map((o) => (
            <label className="pm-field" key={o.name}>
              <span>{caption(t, 'part.option.', o.name)}</span>
              <select
                value={form.options[o.name] ?? o.allowed[0]}
                data-testid={`option-${o.name}`}
                onChange={(e) => onChange({ ...form, options: { ...form.options, [o.name]: e.target.value } })}
              >
                {o.allowed.map((v) => (
                  <option key={v} value={v}>
                    {caption(t, 'part.value.', v)}
                  </option>
                ))}
              </select>
            </label>
          ))}
        </fieldset>
      )}

      {spec.links.length > 0 && (
        <fieldset className="pm-group">
          <legend>{t('part.panel.links')}</legend>
          {spec.links.map((l) => (
            <label className="pm-field" key={l.name}>
              <span>{slot(l.name)}</span>
              <select
                value={form.links[l.name] ?? ''}
                data-testid={`link-${l.name}`}
                onChange={(e) => onChange({ ...form, links: { ...form.links, [l.name]: e.target.value } })}
              >
                <option value="">{l.optional ? t('part.panel.noLink') : t('part.panel.chooseLink')}</option>
                {eligibleLinks(l, features, position).map((f) => (
                  <option key={f.id} value={f.id}>
                    {f.label} ({caption(t, 'part.feature.', f.type)})
                  </option>
                ))}
              </select>
            </label>
          ))}
        </fieldset>
      )}

      {spec.references.length > 0 && (
        <fieldset className="pm-group">
          <legend>{t('part.panel.references')}</legend>
          {spec.references.map((r) => {
            const picks = form.references[r.name] ?? [];
            const active = activeSlot === r.name;
            return (
              <div className={`pm-slot${active ? ' pm-slot-active' : ''}`} key={r.name} data-testid={`slot-${r.name}`}>
                <div className="pm-slot-head">
                  <span>
                    {slot(r.name)} ({KIND_KEYS[r.kind] ? t(KIND_KEYS[r.kind]) : r.kind}
                    {r.multiple ? t('part.panel.multiple') : ''}
                    {r.optional ? t('part.panel.optional') : ''})
                  </span>
                  <button
                    type="button"
                    data-testid={`slot-pick-${r.name}`}
                    disabled={r.kind === 'vertex'}
                    onClick={() => onActivateSlot(active ? null : r.name)}
                  >
                    {active ? t('part.panel.pickDone') : t('part.panel.pick')}
                  </button>
                </div>
                <ul className="pm-picks" data-testid={`picks-${r.name}`}>
                  {picks.map((p, i) => (
                    <li key={`${p.ref.name}-${i}`}>
                      <code>{p.ref.name}</code>
                      <button
                        type="button"
                        aria-label={t('part.panel.unpick')}
                        onClick={() =>
                          onChange({
                            ...form,
                            references: { ...form.references, [r.name]: picks.filter((_, k) => k !== i) },
                          })
                        }
                      >
                        ×
                      </button>
                    </li>
                  ))}
                </ul>
                {active && <p className="pm-hint">{r.kind === 'edge' ? t('part.panel.pickEdgeHint') : t('part.panel.pickFaceHint')}</p>}
              </div>
            );
          })}
        </fieldset>
      )}

      {view && (
        <div className={`pm-preview-state pm-state-${view.state}`} data-testid="panel-state" data-state={view.state}>
          {t('part.panel.state', { state: caption(t, 'part.state.', view.state) })}
          {view.messages.length > 0 && <div>{view.messages.join('; ')}</div>}
          {view.referenceReports
            .filter((r) => r.method !== 'exact')
            .map((r, i) => (
              <div key={i}>
                {t('part.panel.reference', { name: r.name, method: r.method, detail: r.detail })}
              </div>
            ))}
        </div>
      )}
      {missing.length > 0 && (
        <ul className="pm-missing" data-testid="panel-missing">
          {missing.map((m) => (
            <li key={m}>{m}</li>
          ))}
        </ul>
      )}
      <div className="pm-panel-buttons">
        <button type="button" data-testid="panel-preview" onClick={onPreview}>
          {t('part.panel.preview')}
        </button>
        <button type="button" className="pm-primary" data-testid="panel-ok" onClick={onOk}>
          {t('part.panel.ok')}
        </button>
        <button type="button" data-testid="panel-cancel" onClick={onCancel}>
          {t('part.panel.cancel')}
        </button>
      </div>
    </section>
  );
};

export default FeaturePanel;
