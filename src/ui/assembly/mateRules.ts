// Mate types allowed for two picked elements (docs/ui/ASSEMBLY_UI.md, "Панель сопряжений").
//
// The rules mirror prepareMate of the solver (docs/assembly/SOLVER.md, section 2): which entity
// pairs each mate type accepts. The panel enables only the allowed types; the kernel still
// validates the mate when it is added and its error is shown if the rules disagree.

import type { EntityInfo, GeometryRef, MateType } from '../../wasm/bendcore_assembly_api.mjs';

export type EntityType = EntityInfo['type'];

export interface PickedElement {
  ref: GeometryRef;
  /** Kernel geometry type of the element (Plane, Cylinder, Line, Circle, ...). */
  geometry: string;
  /** Mate entity in world coordinates, or null when the element has none. */
  entity: EntityInfo | null;
  /** Display name: component path name and element. */
  label: string;
}

const pointLike = (t: EntityType) => t === 'point' || t === 'sphere';
const lineLike = (t: EntityType) => t === 'axis' || t === 'cylinder';
const hasDirection = (t: EntityType) => lineLike(t) || t === 'plane';

/** True when the mate type accepts the two entity types (in either order). */
export function mateAccepts(type: MateType, a: EntityType, b: EntityType): boolean {
  const either = (p: (x: EntityType, y: EntityType) => boolean) => p(a, b) || p(b, a);
  switch (type) {
    case 'coincident':
      return either(
        (x, y) =>
          (pointLike(x) && (pointLike(y) || y === 'plane' || lineLike(y))) ||
          (x === 'plane' && y === 'plane') ||
          (lineLike(x) && lineLike(y)),
      );
    case 'concentric':
      return lineLike(a) && lineLike(b);
    case 'distance':
      return either((x, y) => (x === 'plane' && y === 'plane') || (pointLike(x) && (y === 'plane' || pointLike(y))));
    case 'angle':
    case 'parallel':
    case 'perpendicular':
      return hasDirection(a) && hasDirection(b);
    case 'tangent':
      return either(
        (x, y) => (x === 'plane' && (y === 'cylinder' || y === 'sphere')) || (x === 'cylinder' && y === 'cylinder'),
      );
    case 'rigid':
      return true;
  }
  return false;
}

export const MATE_TYPES: MateType[] = [
  'coincident',
  'concentric',
  'distance',
  'angle',
  'parallel',
  'perpendicular',
  'tangent',
  'rigid',
];

/** Types that need a value field, and its unit. */
export function mateValueUnit(type: MateType): 'mm' | 'deg' | null {
  if (type === 'distance') return 'mm';
  if (type === 'angle') return 'deg';
  return null;
}

/** Allowed types for the picks; rigid needs two different top-level components. */
export function allowedMateTypes(a: PickedElement | null, b: PickedElement | null): MateType[] {
  if (!a || !b) return [];
  const differentComponents = a.ref.path[0] !== b.ref.path[0];
  if (!differentComponents) return [];
  return MATE_TYPES.filter((t) => {
    if (t === 'rigid') return true;
    if (!a.entity || !b.entity) return false;
    return mateAccepts(t, a.entity.type, b.entity.type);
  });
}

/**
 * Type preselected for the picks: concentric for two axes/cylinders, tangent for a plane and a
 * cylinder or sphere, coincident otherwise (when allowed).
 */
export function suggestMateType(a: PickedElement | null, b: PickedElement | null): MateType | null {
  const allowed = allowedMateTypes(a, b);
  if (allowed.length === 0) return null;
  for (const t of ['concentric', 'coincident', 'tangent', 'parallel'] as MateType[]) {
    if (allowed.includes(t)) return t;
  }
  return allowed[0];
}

export function sameRef(x: GeometryRef, y: GeometryRef): boolean {
  return (
    x.kind === y.kind &&
    x.index === y.index &&
    x.path.length === y.path.length &&
    x.path.every((id, i) => id === y.path[i])
  );
}

/**
 * Next selection of the two mate slots after a pick: a pick equal to a slot clears it; otherwise
 * it fills the first empty slot, or replaces B when both are full.
 */
export function nextMateSlots(
  slots: [PickedElement | null, PickedElement | null],
  pick: PickedElement,
): [PickedElement | null, PickedElement | null] {
  const [a, b] = slots;
  if (a && sameRef(a.ref, pick.ref)) return [b, null];
  if (b && sameRef(b.ref, pick.ref)) return [a, null];
  if (!a) return [pick, b];
  return [a, pick];
}
