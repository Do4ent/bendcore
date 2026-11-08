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
import {
  FEATURE_LABELS,
  OPTION_LABELS,
  OPTION_VALUE_LABELS,
  PARAMETER_LABELS,
  SLOT_LABELS,
  STATE_LABELS,
  label,
} from './labels';
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

const PROFILE_KINDS: { value: SketchTemplateForm['kind']; text: string }[] = [
  { value: 'rectangle', text: 'Прямоугольник' },
  { value: 'circle', text: 'Окружность' },
  { value: 'polygon', text: 'Полилиния' },
];

const TEMPLATE_FIELDS: Record<'rectangle' | 'circle', { key: keyof SketchTemplateForm; text: string }[]> = {
  rectangle: [
    { key: 'x', text: 'Угол X, мм' },
    { key: 'y', text: 'Угол Y, мм' },
    { key: 'width', text: 'Ширина (по X), мм' },
    { key: 'height', text: 'Высота (по Y), мм' },
  ],
  circle: [
    { key: 'cx', text: 'Центр X, мм' },
    { key: 'cy', text: 'Центр Y, мм' },
    { key: 'radius', text: 'Радиус, мм' },
  ],
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
  const missing = missingInputs(spec, form, (s) => label(SLOT_LABELS, s));
  const setTemplate = (patch: Partial<SketchTemplateForm>) =>
    form.template && onChange({ ...form, template: { ...form.template, ...patch } });
  const dims = view?.sketch && form.dimensions ? sketchDimensions(view.sketch) : [];

  return (
    <section className="pm-panel" data-testid="feature-panel" data-type={spec.type}>
      <h2>
        {mode === 'add' ? 'Новая операция' : 'Изменить'}: {label(FEATURE_LABELS, spec.type)}
        {view ? ` (${view.id})` : ''}
      </h2>
      <label className="pm-field">
        <span>Имя</span>
        <input
          value={form.label}
          placeholder={view?.id ?? 'по умолчанию'}
          data-testid="panel-label"
          onChange={(e) => onChange({ ...form, label: e.target.value })}
        />
      </label>

      {form.template && (
        <fieldset className="pm-group">
          <legend>Контур эскиза</legend>
          <label className="pm-field">
            <span>Форма</span>
            <select
              value={form.template.kind}
              data-testid="sketch-kind"
              onChange={(e) => setTemplate({ kind: e.target.value as SketchTemplateForm['kind'] })}
            >
              {PROFILE_KINDS.map((k) => (
                <option key={k.value} value={k.value}>
                  {k.text}
                </option>
              ))}
            </select>
          </label>
          {form.template.kind === 'polygon' ? (
            <label className="pm-field pm-field-wide">
              <span>Вершины «x; y», по одной в строке</span>
              <textarea
                rows={5}
                value={form.template.polygon}
                data-testid="sketch-polygon"
                onChange={(e) => setTemplate({ polygon: e.target.value })}
              />
            </label>
          ) : (
            TEMPLATE_FIELDS[form.template.kind].map((f) => (
              <label className="pm-field" key={f.key}>
                <span>{f.text}</span>
                <input
                  value={form.template![f.key]}
                  data-testid={`sketch-${f.key}`}
                  onChange={(e) => setTemplate({ [f.key]: e.target.value })}
                />
              </label>
            ))
          )}
        </fieldset>
      )}

      {dims.length > 0 && form.dimensions && (
        <fieldset className="pm-group">
          <legend>Размеры эскиза</legend>
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
          <legend>Параметры (выражения)</legend>
          {spec.parameters.map((p) => (
            <label className="pm-field" key={p.name}>
              <span>{label(PARAMETER_LABELS, p.name)}</span>
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
          <legend>Настройки</legend>
          {spec.options.map((o) => (
            <label className="pm-field" key={o.name}>
              <span>{label(OPTION_LABELS, o.name)}</span>
              <select
                value={form.options[o.name] ?? o.allowed[0]}
                data-testid={`option-${o.name}`}
                onChange={(e) => onChange({ ...form, options: { ...form.options, [o.name]: e.target.value } })}
              >
                {o.allowed.map((v) => (
                  <option key={v} value={v}>
                    {label(OPTION_VALUE_LABELS, v)}
                  </option>
                ))}
              </select>
            </label>
          ))}
        </fieldset>
      )}

      {spec.links.length > 0 && (
        <fieldset className="pm-group">
          <legend>Связи</legend>
          {spec.links.map((l) => (
            <label className="pm-field" key={l.name}>
              <span>{label(SLOT_LABELS, l.name)}</span>
              <select
                value={form.links[l.name] ?? ''}
                data-testid={`link-${l.name}`}
                onChange={(e) => onChange({ ...form, links: { ...form.links, [l.name]: e.target.value } })}
              >
                <option value="">{l.optional ? '— нет —' : '— выберите —'}</option>
                {eligibleLinks(l, features, position).map((f) => (
                  <option key={f.id} value={f.id}>
                    {f.label} ({label(FEATURE_LABELS, f.type)})
                  </option>
                ))}
              </select>
            </label>
          ))}
        </fieldset>
      )}

      {spec.references.length > 0 && (
        <fieldset className="pm-group">
          <legend>Элементы модели</legend>
          {spec.references.map((r) => {
            const picks = form.references[r.name] ?? [];
            const active = activeSlot === r.name;
            return (
              <div className={`pm-slot${active ? ' pm-slot-active' : ''}`} key={r.name} data-testid={`slot-${r.name}`}>
                <div className="pm-slot-head">
                  <span>
                    {label(SLOT_LABELS, r.name)} ({r.kind === 'edge' ? 'рёбра' : r.kind === 'face' ? 'грани' : 'вершины'}
                    {r.multiple ? ', несколько' : ''}
                    {r.optional ? ', необязательно' : ''})
                  </span>
                  <button
                    type="button"
                    data-testid={`slot-pick-${r.name}`}
                    disabled={r.kind === 'vertex'}
                    onClick={() => onActivateSlot(active ? null : r.name)}
                  >
                    {active ? 'Готово' : 'Выбрать во вьюпорте'}
                  </button>
                </div>
                <ul className="pm-picks" data-testid={`picks-${r.name}`}>
                  {picks.map((p, i) => (
                    <li key={`${p.ref.name}-${i}`}>
                      <code>{p.ref.name}</code>
                      <button
                        type="button"
                        aria-label="убрать"
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
                {active && <p className="pm-hint">Щёлкните по {r.kind === 'edge' ? 'ребру' : 'грани'} модели; повторный щелчок убирает.</p>}
              </div>
            );
          })}
        </fieldset>
      )}

      {view && (
        <div className={`pm-preview-state pm-state-${view.state}`} data-testid="panel-state" data-state={view.state}>
          Состояние: {label(STATE_LABELS, view.state)}
          {view.messages.length > 0 && <div>{view.messages.join('; ')}</div>}
          {view.referenceReports
            .filter((r) => r.method !== 'exact')
            .map((r, i) => (
              <div key={i}>
                ссылка {r.name}: {r.method} {r.detail}
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
          Предпросмотр
        </button>
        <button type="button" className="pm-primary" data-testid="panel-ok" onClick={onOk}>
          ОК
        </button>
        <button type="button" data-testid="panel-cancel" onClick={onCancel}>
          Отмена
        </button>
      </div>
    </section>
  );
};

export default FeaturePanel;
