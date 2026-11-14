// Feature types of the part editor's toolbar (docs/ui/PART_MODELING_UI.md). Captions of feature
// types, parameters, options, option values, reference slots and states are messages of
// src/ui/app/messages/part.ts (part.feature.*, part.param.*, part.option.*, part.value.*,
// part.slot.*, part.state.*); a name without a caption is shown as the kernel reports it.

/** Toolbar order of the feature types the editor knows; the kernel catalog decides what is shown. */
export const TOOLBAR_TYPES = [
  'Sketch',
  'Extrude',
  'Revolve',
  'Hole',
  'Fillet',
  'Chamfer',
  'LinearPattern',
  'CircularPattern',
  'Mirror',
  'Shell',
  'Draft',
  'DatumPlane',
  'DatumAxis',
];

/**
 * Primitive bodies of the catalog that are not offered on the toolbar: a part is built from
 * sketches and features; Box and Cylinder exist for documents made through the API.
 */
export const NOT_ON_TOOLBAR = new Set(['Box', 'Cylinder']);

/**
 * Toolbar buttons for the kernel catalog: the known types in TOOLBAR_TYPES order, then every
 * other catalog type in catalog order (a feature the kernel gains later appears without a change
 * here, under its caption or its kernel name).
 */
export function toolbarTypes(catalog: readonly string[]): string[] {
  const known = TOOLBAR_TYPES.filter((t) => catalog.includes(t));
  const added = catalog.filter((t) => !TOOLBAR_TYPES.includes(t) && !NOT_ON_TOOLBAR.has(t));
  return [...known, ...added];
}
