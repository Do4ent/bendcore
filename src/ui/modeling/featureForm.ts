// Form state of the feature panel and its conversion to the part document API
// (docs/ui/PART_MODELING_UI.md). The panel is generic: its fields come from the kernel catalog
// (parameters, options, link slots, reference slots of the feature type).

import type {
  FeatureEdits,
  FeatureSpecInput,
  FeatureTypeSpec,
  FeatureView,
  LinkSlotSpec,
  TopoReference,
} from '../../wasm/bendcore_modeling_api.mjs';
import { DEFAULT_TEMPLATE, SketchTemplateForm, sketchDimensions } from './sketchDimensions';

/** A reference with the element index it was picked at in the shown target, if known. */
export interface PickedReference {
  ref: TopoReference;
  index: number | null;
}

export interface FeatureForm {
  type: string;
  label: string;
  parameters: Record<string, string>;
  options: Record<string, string>;
  links: Record<string, string>;
  references: Record<string, PickedReference[]>;
  /** New sketch: profile template. */
  template: SketchTemplateForm | null;
  /** Existing sketch: dimension expressions by key (sketchDimensions). */
  dimensions: Record<string, string> | null;
}

/** Features a link slot may point to: earlier in the tree than `position`, of an accepted type. */
export function eligibleLinks(slot: LinkSlotSpec, features: readonly FeatureView[], position: number): FeatureView[] {
  return features.filter((f) => f.index < position && (slot.types.length === 0 || slot.types.includes(f.type)));
}

const NON_BODY = new Set(['Sketch', 'DatumPlane', 'DatumAxis']);

function defaultLink(slot: LinkSlotSpec, features: readonly FeatureView[], position: number): string {
  if (slot.optional) return '';
  const candidates = eligibleLinks(slot, features, position).filter(
    (f) => slot.types.length > 0 || !NON_BODY.has(f.type),
  );
  return candidates.length > 0 ? candidates[candidates.length - 1].id : '';
}

/** Form of a new feature inserted at `position`: catalog defaults, latest eligible links. */
export function newFeatureForm(spec: FeatureTypeSpec, features: readonly FeatureView[], position: number): FeatureForm {
  const parameters: Record<string, string> = {};
  for (const p of spec.parameters) parameters[p.name] = p.defaultExpression;
  const options: Record<string, string> = {};
  for (const o of spec.options) options[o.name] = o.allowed[0];
  const links: Record<string, string> = {};
  for (const l of spec.links) links[l.name] = defaultLink(l, features, position);
  const references: Record<string, PickedReference[]> = {};
  for (const r of spec.references) references[r.name] = [];
  return {
    type: spec.type,
    label: '',
    parameters,
    options,
    links,
    references,
    template: spec.hasSketch ? { ...DEFAULT_TEMPLATE } : null,
    dimensions: null,
  };
}

/** Form of an existing feature (edit panel). */
export function existingFeatureForm(spec: FeatureTypeSpec, view: FeatureView): FeatureForm {
  const references: Record<string, PickedReference[]> = {};
  for (const r of spec.references) {
    references[r.name] = (view.references[r.name] ?? []).map((ref) => ({ ref, index: null }));
  }
  const links: Record<string, string> = {};
  for (const l of spec.links) links[l.name] = view.links[l.name] ?? '';
  let dimensions: Record<string, string> | null = null;
  if (view.sketch) {
    dimensions = {};
    for (const d of sketchDimensions(view.sketch)) dimensions[d.key] = d.expression;
  }
  return {
    type: view.type,
    label: view.label,
    parameters: { ...view.parameters },
    options: { ...view.options },
    links,
    references,
    template: null,
    dimensions,
  };
}

function referenceMap(form: FeatureForm): Record<string, TopoReference[]> {
  const out: Record<string, TopoReference[]> = {};
  for (const [slot, picks] of Object.entries(form.references)) out[slot] = picks.map((p) => p.ref);
  return out;
}

function linkMap(form: FeatureForm): Record<string, string> {
  const out: Record<string, string> = {};
  for (const [slot, target] of Object.entries(form.links)) if (target) out[slot] = target;
  return out;
}

/** Spec for PartDocumentHandle.addFeature (the sketch data is built by the kernel and passed in). */
export function toFeatureSpec(form: FeatureForm, sketch?: FeatureSpecInput['sketch']): FeatureSpecInput {
  const spec: FeatureSpecInput = {
    type: form.type,
    parameters: { ...form.parameters },
    options: { ...form.options },
    links: linkMap(form),
    references: referenceMap(form),
  };
  if (form.label.trim()) spec.label = form.label.trim();
  if (sketch) spec.sketch = sketch;
  return spec;
}

/** Edits for PartDocumentHandle.editFeature. Links left empty are not sent (cannot be cleared). */
export function toFeatureEdits(form: FeatureForm, sketch?: FeatureEdits['sketch']): FeatureEdits {
  const edits: FeatureEdits = {
    parameters: { ...form.parameters },
    options: { ...form.options },
    links: linkMap(form),
    references: referenceMap(form),
  };
  if (form.label.trim()) edits.label = form.label.trim();
  if (sketch) edits.sketch = sketch;
  return edits;
}

/** Missing required inputs, as messages for the panel (empty when the form can be applied). */
export function missingInputs(spec: FeatureTypeSpec, form: FeatureForm, slotLabel: (s: string) => string): string[] {
  const out: string[] = [];
  for (const l of spec.links) {
    if (!l.optional && !form.links[l.name]) out.push(`не выбрано: ${slotLabel(l.name)}`);
  }
  for (const r of spec.references) {
    if (!r.optional && (form.references[r.name] ?? []).length === 0) out.push(`не выбрано: ${slotLabel(r.name)}`);
  }
  for (const [name, expr] of Object.entries(form.parameters)) {
    if (!expr.trim()) out.push(`пустое выражение: ${name}`);
  }
  return out;
}

/** Adds a pick to a slot (a single slot is replaced); a pick of an already chosen element removes it. */
export function togglePick(
  current: readonly PickedReference[],
  pick: PickedReference,
  multiple: boolean,
): PickedReference[] {
  const existing = current.findIndex((p) => p.ref.name === pick.ref.name && p.ref.kind === pick.ref.kind);
  if (existing >= 0) return current.filter((_, i) => i !== existing);
  return multiple ? [...current, pick] : [pick];
}
