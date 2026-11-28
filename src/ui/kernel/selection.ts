// Selection filters and box selection of the viewports (docs/ui/KERNEL_VIEWPORT.md, "Фильтры
// выбора и выбор рамкой"). Pure logic: which elements the filter lets through, which elements a
// screen rectangle takes. The geometry is the kernel's tessellation (positions, indices, faceSpans,
// edge polylines) projected to the screen by the caller; nothing is generated here.

export type ElementMode = 'face' | 'edge' | 'vertex' | 'body';
export const ELEMENT_MODES: readonly ElementMode[] = ['face', 'edge', 'vertex', 'body'];

/** Face and edge types the filters name (surface/curve types of the kernel: KernelShape.faceTypes()). */
export type FaceFilterType = 'Plane' | 'Cylinder';
export type EdgeFilterType = 'Line' | 'Circle';
export const FACE_FILTER_TYPES: readonly FaceFilterType[] = ['Plane', 'Cylinder'];
export const EDGE_FILTER_TYPES: readonly EdgeFilterType[] = ['Line', 'Circle'];

export interface SelectionFilter {
  mode: ElementMode;
  /** Face types let through; empty: every face. */
  faceTypes: readonly FaceFilterType[];
  /** Edge types let through; empty: every edge. */
  edgeTypes: readonly EdgeFilterType[];
}

export function defaultFilter(mode: ElementMode = 'face'): SelectionFilter {
  return { mode, faceTypes: [], edgeTypes: [] };
}

export function setMode(filter: SelectionFilter, mode: ElementMode): SelectionFilter {
  return { ...filter, mode };
}

/** Switches a type on or off (an empty list means "all"). */
export function toggleFaceType(filter: SelectionFilter, type: FaceFilterType): SelectionFilter {
  const on = filter.faceTypes.includes(type);
  return { ...filter, faceTypes: on ? filter.faceTypes.filter((t) => t !== type) : [...filter.faceTypes, type] };
}

export function toggleEdgeType(filter: SelectionFilter, type: EdgeFilterType): SelectionFilter {
  const on = filter.edgeTypes.includes(type);
  return { ...filter, edgeTypes: on ? filter.edgeTypes.filter((t) => t !== type) : [...filter.edgeTypes, type] };
}

/** Does the filter let a face of this surface type through? An unknown type passes only an empty filter. */
export function faceAllowed(filter: SelectionFilter, surfaceType: string | undefined): boolean {
  if (filter.faceTypes.length === 0) return true;
  return surfaceType !== undefined && (filter.faceTypes as readonly string[]).includes(surfaceType);
}

export function edgeAllowed(filter: SelectionFilter, curveType: string | undefined): boolean {
  if (filter.edgeTypes.length === 0) return true;
  return curveType !== undefined && (filter.edgeTypes as readonly string[]).includes(curveType);
}

// ------------------------------------------------------------------ select operation

export type SelectOp = 'replace' | 'add' | 'remove';

/** Shift adds to the selection, Ctrl (Cmd) removes from it; Ctrl wins when both are held. */
export function selectOp(mods: { shiftKey?: boolean; ctrlKey?: boolean; metaKey?: boolean }): SelectOp {
  if (mods.ctrlKey || mods.metaKey) return 'remove';
  if (mods.shiftKey) return 'add';
  return 'replace';
}

/** The selection after `found` was taken with `op` (no duplicates, order kept). */
export function combineSelection(current: readonly number[], found: readonly number[], op: SelectOp): number[] {
  if (op === 'replace') return [...new Set(found)];
  if (op === 'remove') {
    const drop = new Set(found);
    return current.filter((i) => !drop.has(i));
  }
  return [...new Set([...current, ...found])];
}

// ------------------------------------------------------------------ rectangle

export interface Rect {
  x0: number;
  y0: number;
  x1: number;
  y1: number;
}

/**
 * 'inside': the rectangle was dragged left to right, elements entirely inside are taken.
 * 'cross': right to left, elements the rectangle touches are taken too.
 */
export type BoxKind = 'inside' | 'cross';

export function boxKind(start: { x: number }, end: { x: number }): BoxKind {
  return end.x >= start.x ? 'inside' : 'cross';
}

/** Rectangle with x0 <= x1 and y0 <= y1 from two corners. */
export function normalizeRect(a: { x: number; y: number }, b: { x: number; y: number }): Rect {
  return { x0: Math.min(a.x, b.x), y0: Math.min(a.y, b.y), x1: Math.max(a.x, b.x), y1: Math.max(a.y, b.y) };
}

export interface ScreenPoint {
  x: number;
  y: number;
  /** false for a point behind the camera (or outside the clip range). */
  visible: boolean;
}

export type Projector = (x: number, y: number, z: number) => ScreenPoint;

export function pointInRect(p: { x: number; y: number }, r: Rect): boolean {
  return p.x >= r.x0 && p.x <= r.x1 && p.y >= r.y0 && p.y <= r.y1;
}

type P2 = { x: number; y: number };

/** Does the segment ab touch the rectangle (Liang-Barsky clipping)? */
export function segmentTouchesRect(a: P2, b: P2, r: Rect): boolean {
  let t0 = 0;
  let t1 = 1;
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const checks: [number, number][] = [
    [-dx, a.x - r.x0],
    [dx, r.x1 - a.x],
    [-dy, a.y - r.y0],
    [dy, r.y1 - a.y],
  ];
  for (const [p, q] of checks) {
    if (p === 0) {
      if (q < 0) return false;
    } else {
      const t = q / p;
      if (p < 0) {
        if (t > t1) return false;
        if (t > t0) t0 = t;
      } else {
        if (t < t0) return false;
        if (t < t1) t1 = t;
      }
    }
  }
  return t0 <= t1;
}

function pointInTriangle(p: P2, a: P2, b: P2, c: P2): boolean {
  const s1 = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
  const s2 = (c.x - b.x) * (p.y - b.y) - (c.y - b.y) * (p.x - b.x);
  const s3 = (a.x - c.x) * (p.y - c.y) - (a.y - c.y) * (p.x - c.x);
  const neg = s1 < 0 || s2 < 0 || s3 < 0;
  const pos = s1 > 0 || s2 > 0 || s3 > 0;
  return !(neg && pos);
}

/** Does the projected triangle touch the rectangle (a corner inside, an edge crossing, or the rectangle inside it)? */
export function triangleTouchesRect(a: P2, b: P2, c: P2, r: Rect): boolean {
  if (pointInRect(a, r) || pointInRect(b, r) || pointInRect(c, r)) return true;
  if (segmentTouchesRect(a, b, r) || segmentTouchesRect(b, c, r) || segmentTouchesRect(c, a, r)) return true;
  return pointInTriangle({ x: r.x0, y: r.y0 }, a, b, c);
}

// ------------------------------------------------------------------ box selection

/** The tessellation parts a box selection reads (a KernelMesh, a PartMesh or an assembly part's mesh). */
export interface BoxMesh {
  positions: ArrayLike<number>;
  indices: ArrayLike<number>;
  /** triples (faceIndex, firstIndex, indexCount) */
  faceSpans: ArrayLike<number>;
  edges: readonly { edgeIndex: number; points: ArrayLike<number> }[];
  /** xyz triples of the vertices (index = vertex index), when the mesh has them */
  vertices?: ArrayLike<number> | null;
  faceTypes?: readonly string[];
  edgeTypes?: readonly string[];
  /** body of every face (NO_BODY: none) */
  faceBodies?: ArrayLike<number>;
}

export interface BoxResult {
  faces: number[];
  edges: number[];
  vertices: number[];
  bodies: number[];
}

export const NO_BODY = 0xffffffff;

/** Faces of the mesh the rectangle takes: entirely inside ('inside') or touched ('cross'). */
export function boxFaces(mesh: BoxMesh, project: Projector, rect: Rect, kind: BoxKind, filter?: SelectionFilter): number[] {
  const out: number[] = [];
  const cache = new Map<number, ScreenPoint>();
  const at = (v: number): ScreenPoint => {
    let p = cache.get(v);
    if (!p) {
      p = project(mesh.positions[v * 3], mesh.positions[v * 3 + 1], mesh.positions[v * 3 + 2]);
      cache.set(v, p);
    }
    return p;
  };
  for (let s = 0; s + 2 < mesh.faceSpans.length; s += 3) {
    const face = mesh.faceSpans[s];
    if (filter && filter.mode === 'face' && !faceAllowed(filter, mesh.faceTypes?.[face])) continue;
    const first = mesh.faceSpans[s + 1];
    const end = first + mesh.faceSpans[s + 2];
    let all = end > first;
    let any = false;
    for (let i = first; i + 2 < end; i += 3) {
      const a = at(mesh.indices[i]);
      const b = at(mesh.indices[i + 1]);
      const c = at(mesh.indices[i + 2]);
      const visible = a.visible && b.visible && c.visible;
      if (!(visible && pointInRect(a, rect) && pointInRect(b, rect) && pointInRect(c, rect))) all = false;
      if (kind === 'cross' && visible && triangleTouchesRect(a, b, c, rect)) {
        any = true;
        break;
      }
      if (kind === 'inside' && !all) break;
    }
    if (kind === 'inside' ? all : any) out.push(face);
  }
  return out;
}

/** Edges of the mesh the rectangle takes (all polyline points inside, or a polyline segment touching it). */
export function boxEdges(mesh: BoxMesh, project: Projector, rect: Rect, kind: BoxKind, filter?: SelectionFilter): number[] {
  const out: number[] = [];
  for (const edge of mesh.edges) {
    if (filter && filter.mode === 'edge' && !edgeAllowed(filter, mesh.edgeTypes?.[edge.edgeIndex])) continue;
    const pts = edge.points;
    const n = Math.floor(pts.length / 3);
    if (n === 0) continue;
    const proj: ScreenPoint[] = [];
    for (let i = 0; i < n; ++i) proj.push(project(pts[i * 3], pts[i * 3 + 1], pts[i * 3 + 2]));
    let hit: boolean;
    if (kind === 'inside') {
      hit = proj.every((p) => p.visible && pointInRect(p, rect));
    } else {
      hit = n === 1 ? proj[0].visible && pointInRect(proj[0], rect) : false;
      for (let i = 0; !hit && i + 1 < n; ++i) {
        if (proj[i].visible && proj[i + 1].visible && segmentTouchesRect(proj[i], proj[i + 1], rect)) hit = true;
      }
    }
    if (hit) out.push(edge.edgeIndex);
  }
  return out;
}

/** Vertices whose projection lies in the rectangle (the same for both directions). */
export function boxVertices(mesh: BoxMesh, project: Projector, rect: Rect): number[] {
  const out: number[] = [];
  const v = mesh.vertices;
  if (!v) return out;
  for (let i = 0; i * 3 + 2 < v.length; ++i) {
    const p = project(v[i * 3], v[i * 3 + 1], v[i * 3 + 2]);
    if (p.visible && pointInRect(p, rect)) out.push(i);
  }
  return out;
}

/**
 * Bodies the rectangle takes: 'inside' when every face of the body is entirely inside, 'cross'
 * when any face of it is touched.
 */
export function boxBodies(mesh: BoxMesh, project: Projector, rect: Rect, kind: BoxKind): number[] {
  const bodies = mesh.faceBodies;
  if (!bodies) return [];
  const found = new Set(boxFaces(mesh, project, rect, kind));
  const total = new Map<number, number>();
  const hit = new Map<number, number>();
  for (let f = 0; f < bodies.length; ++f) {
    const b = bodies[f];
    if (b === NO_BODY) continue;
    total.set(b, (total.get(b) ?? 0) + 1);
    if (found.has(f)) hit.set(b, (hit.get(b) ?? 0) + 1);
  }
  const out: number[] = [];
  for (const [b, n] of total) {
    const h = hit.get(b) ?? 0;
    if (kind === 'inside' ? h === n : h > 0) out.push(b);
  }
  return out.sort((a, b) => a - b);
}

/** Everything the rectangle takes in the element mode of the filter (other kinds are left empty). */
export function boxSelect(mesh: BoxMesh, project: Projector, rect: Rect, kind: BoxKind, filter: SelectionFilter): BoxResult {
  const out: BoxResult = { faces: [], edges: [], vertices: [], bodies: [] };
  if (filter.mode === 'face') out.faces = boxFaces(mesh, project, rect, kind, filter);
  else if (filter.mode === 'edge') out.edges = boxEdges(mesh, project, rect, kind, filter);
  else if (filter.mode === 'vertex') out.vertices = boxVertices(mesh, project, rect);
  else out.bodies = boxBodies(mesh, project, rect, kind);
  return out;
}

/** Faces of the given bodies (a body highlight is the highlight of its faces). */
export function facesOfBodies(faceBodies: ArrayLike<number> | undefined, bodies: readonly number[]): number[] {
  if (!faceBodies) return [];
  const set = new Set(bodies);
  const out: number[] = [];
  for (let f = 0; f < faceBodies.length; ++f) if (set.has(faceBodies[f])) out.push(f);
  return out;
}
