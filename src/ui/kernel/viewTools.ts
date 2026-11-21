// Logic of the viewport measurement and section tools (docs/ui/KERNEL_VIEWPORT.md, "Измерения и
// сечение"): which elements a measurement needs, how picks accumulate, the measurement spec for the
// kernel, the readout, and the section plane and its clipping equation. No geometry is computed
// here: distances, angles, areas and section contours come from the kernel (docs/kernel/ANALYSIS.md).

export type Vec3 = [number, number, number];

export type MeasureType = 'distance' | 'angle' | 'length' | 'area' | 'radius';
export type PickKind = 'face' | 'edge';

export const MEASURE_TYPES: readonly MeasureType[] = ['distance', 'angle', 'length', 'area', 'radius'];

/** An element chosen for a measurement; `path` is set in assemblies (occurrence path). */
export interface MeasurePick {
  kind: PickKind;
  index: number;
  path?: number[];
}

/** Kernel result of a measurement (see Measurement in bendcore_kernel_api.d.mts). */
export interface MeasureResult {
  type: MeasureType;
  unit: 'mm' | 'mm2' | 'deg';
  value: number;
  points: Vec3[];
  diameter?: number;
}

/** Elements a measurement of this type needs. */
export function picksNeeded(type: MeasureType): 1 | 2 {
  return type === 'distance' || type === 'angle' ? 2 : 1;
}

/** The element kind a single-element measurement accepts (null: faces and edges alike). */
export function singleKind(type: MeasureType): PickKind | null {
  if (type === 'length') return 'edge';
  if (type === 'area') return 'face';
  return null;
}

/**
 * Adds a click to the picks of a measurement. Clicking an already chosen element removes it; with
 * all elements chosen, the next click starts a new measurement. An angle needs two faces or two
 * edges: a pick of the other kind than the first replaces it.
 */
export function addPick(picks: readonly MeasurePick[], pick: MeasurePick, type: MeasureType): MeasurePick[] {
  const same = (a: MeasurePick, b: MeasurePick) =>
    a.kind === b.kind && a.index === b.index && (a.path ?? []).join('/') === (b.path ?? []).join('/');
  const wanted = singleKind(type);
  if (wanted && pick.kind !== wanted) return [...picks];
  const existing = picks.findIndex((p) => same(p, pick));
  if (existing >= 0) return picks.filter((_, i) => i !== existing);
  const needed = picksNeeded(type);
  if (picks.length >= needed) return [pick];
  if (type === 'angle' && picks.length === 1 && picks[0].kind !== pick.kind) return [pick];
  return [...picks, pick];
}

export function isComplete(picks: readonly MeasurePick[], type: MeasureType): boolean {
  return picks.length === picksNeeded(type);
}

export interface MeasureSpecOut {
  type: MeasureType;
  a: MeasurePick;
  b?: MeasurePick;
}

/** The kernel's measurement spec for complete picks, else null. Part and kernel specs have no `path`. */
export function measureSpec(type: MeasureType, picks: readonly MeasurePick[], withPath: boolean): MeasureSpecOut | null {
  if (!isComplete(picks, type)) return null;
  const el = (p: MeasurePick): MeasurePick => (withPath ? { path: p.path ?? [], kind: p.kind, index: p.index } : { kind: p.kind, index: p.index });
  return picks.length === 2 ? { type, a: el(picks[0]), b: el(picks[1]) } : { type, a: el(picks[0]) };
}

const UNIT_SYMBOL: Record<MeasureResult['unit'], string> = { mm: 'mm', mm2: 'mm²', deg: '°' };

/** Value and unit as displayed: lengths and areas to 0.001, angles to 0.01 degrees. */
export function formatMeasure(m: Pick<MeasureResult, 'value' | 'unit'>): string {
  const digits = m.unit === 'deg' ? 2 : 3;
  const text = Number(m.value.toFixed(digits)).toString();
  return m.unit === 'deg' ? `${text}°` : `${text} ${UNIT_SYMBOL[m.unit]}`;
}

/** Polylines (flat xyz) to draw for a measurement, and the anchor of its label. */
export function measureOverlay(m: MeasureResult): { lines: Vec3[][]; anchor: Vec3 } {
  const p = m.points;
  const mid = (a: Vec3, b: Vec3): Vec3 => [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2];
  switch (m.type) {
    case 'distance':
      return { lines: [[p[0], p[1]]], anchor: mid(p[0], p[1]) };
    case 'angle':
      // points: vertex, point on the first element, point on the second
      return { lines: [[p[0], p[1]], [p[0], p[2]]], anchor: p[0] };
    case 'length':
      return { lines: [[p[0], p[1]]], anchor: p[2] };
    case 'radius':
      return { lines: [[p[0], p[1]]], anchor: mid(p[0], p[1]) };
    case 'area':
      return { lines: [], anchor: p[0] };
  }
}

// ---------------------------------------------------------------------------------- section

export type SectionAxis = 'X' | 'Y' | 'Z';
export const SECTION_AXES: readonly SectionAxis[] = ['X', 'Y', 'Z'];

export interface Box3 {
  min: Vec3;
  max: Vec3;
}

export interface SectionState {
  enabled: boolean;
  axis: SectionAxis;
  /** 0..1 along the bounding box on the axis. */
  fraction: number;
  /** Keep the other side. */
  flip: boolean;
}

export const DEFAULT_SECTION: SectionState = { enabled: false, axis: 'Z', fraction: 0.5, flip: false };

const AXIS_INDEX: Record<SectionAxis, 0 | 1 | 2> = { X: 0, Y: 1, Z: 2 };

export interface SectionPlaneOut {
  origin: Vec3;
  /** Unit normal; the model is kept where (p - origin) . normal <= 0, as the kernel's clipByPlane. */
  normal: Vec3;
  /** Position on the axis, mm. */
  position: number;
}

/** Plane of the section: through the box at `fraction` of its extent along the axis. */
export function sectionPlane(state: Pick<SectionState, 'axis' | 'fraction' | 'flip'>, box: Box3): SectionPlaneOut {
  const i = AXIS_INDEX[state.axis];
  const f = Math.min(1, Math.max(0, state.fraction));
  const position = box.min[i] + f * (box.max[i] - box.min[i]);
  const origin: Vec3 = [(box.min[0] + box.max[0]) / 2, (box.min[1] + box.max[1]) / 2, (box.min[2] + box.max[2]) / 2];
  origin[i] = position;
  const normal: Vec3 = [0, 0, 0];
  normal[i] = state.flip ? -1 : 1;
  return { origin, normal, position };
}

/**
 * Clipping plane in the form of three.js (kept side: normal . p + constant >= 0) for the model
 * kept at (p - origin) . n <= 0.
 */
export function clipEquation(plane: Pick<SectionPlaneOut, 'origin' | 'normal'>): { normal: Vec3; constant: number } {
  const n: Vec3 = [-plane.normal[0], -plane.normal[1], -plane.normal[2]];
  const constant = plane.normal[0] * plane.origin[0] + plane.normal[1] * plane.origin[1] + plane.normal[2] * plane.origin[2];
  return { normal: n, constant };
}

/** Signed side of a point: > 0 on the removed side of the section plane. */
export function removedSide(plane: Pick<SectionPlaneOut, 'origin' | 'normal'>, p: Vec3): number {
  return (p[0] - plane.origin[0]) * plane.normal[0] + (p[1] - plane.origin[1]) * plane.normal[1] + (p[2] - plane.origin[2]) * plane.normal[2];
}

/** Bounding box of a mesh position array (xyz triples); null for an empty mesh. */
export function boxOfPositions(positions: ArrayLike<number>): Box3 | null {
  if (positions.length < 3) return null;
  const min: Vec3 = [Infinity, Infinity, Infinity];
  const max: Vec3 = [-Infinity, -Infinity, -Infinity];
  for (let i = 0; i + 2 < positions.length; i += 3) {
    for (let k = 0; k < 3; ++k) {
      const v = positions[i + k];
      if (v < min[k]) min[k] = v;
      if (v > max[k]) max[k] = v;
    }
  }
  return { min, max };
}
