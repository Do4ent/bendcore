// Sheet display of the drawing editor (docs/ui/DRAWING_UI.md): the mapping between sheet
// millimetres (origin lower left, y up — the coordinates of bendcore_drawing_wasm) and screen
// pixels with zoom and pan, SVG path text of the 2D edges the kernel returns, conversions from a
// clicked sheet point to view and model coordinates, and the pick state of the annotation tools.
// No geometry is made here: every curve comes from the kernel's sheetPicture().

import type { PictureCurve, PictureView, Point } from '../../wasm/bendcore_drawing_api.mjs';
import type { MessageKey } from '../app/i18n';

/** Screen = (panX + zoom·x, panY + zoom·(sheetHeight − y)); zoom in px per mm. */
export interface SheetTransform {
  zoom: number;
  panX: number;
  panY: number;
}

export const MIN_ZOOM = 0.2;
export const MAX_ZOOM = 60;

const fmt = (v: number) => (Math.abs(v) < 1e-9 ? '0' : String(Math.round(v * 1e4) / 1e4));

/** Transform that shows the whole sheet centred in a viewport of w × h px with a margin. */
export function fitTransform(sheetW: number, sheetH: number, w: number, h: number, margin = 16): SheetTransform {
  if (sheetW <= 0 || sheetH <= 0) throw new Error('fitTransform: empty sheet');
  const zoom = Math.max(MIN_ZOOM, Math.min((w - 2 * margin) / sheetW, (h - 2 * margin) / sheetH));
  return { zoom, panX: (w - zoom * sheetW) / 2, panY: (h - zoom * sheetH) / 2 };
}

export function sheetToScreen(t: SheetTransform, sheetH: number, p: Point): Point {
  return [t.panX + t.zoom * p[0], t.panY + t.zoom * (sheetH - p[1])];
}

export function screenToSheet(t: SheetTransform, sheetH: number, s: Point): Point {
  return [(s[0] - t.panX) / t.zoom, sheetH - (s[1] - t.panY) / t.zoom];
}

/** Zoom by factor keeping the sheet point under the screen point (sx, sy) in place. */
export function zoomAt(t: SheetTransform, factor: number, sx: number, sy: number): SheetTransform {
  const zoom = Math.min(MAX_ZOOM, Math.max(MIN_ZOOM, t.zoom * factor));
  const k = zoom / t.zoom;
  return { zoom, panX: sx - k * (sx - t.panX), panY: sy - k * (sy - t.panY) };
}

export function panBy(t: SheetTransform, dx: number, dy: number): SheetTransform {
  return { ...t, panX: t.panX + dx, panY: t.panY + dy };
}

/**
 * SVG path data of a kernel curve in sheet coordinates (y up; the overlay group flips y).
 * Arcs run counter-clockwise from startDeg to endDeg as the kernel defines them.
 */
export function curvePath(c: PictureCurve): string {
  switch (c.type) {
    case 'line':
      return `M${fmt(c.points[0][0])} ${fmt(c.points[0][1])}L${fmt(c.points[1][0])} ${fmt(c.points[1][1])}`;
    case 'polyline': {
      const [first, ...rest] = c.points;
      return `M${fmt(first[0])} ${fmt(first[1])}` + rest.map((p) => `L${fmt(p[0])} ${fmt(p[1])}`).join('') + (c.closed ? 'Z' : '');
    }
    case 'circle': {
      const [cx, cy] = c.center;
      const r = c.radius;
      // Two half circles: a full circle cannot be one arc command.
      return `M${fmt(cx + r)} ${fmt(cy)}A${fmt(r)} ${fmt(r)} 0 1 1 ${fmt(cx - r)} ${fmt(cy)}A${fmt(r)} ${fmt(r)} 0 1 1 ${fmt(cx + r)} ${fmt(cy)}Z`;
    }
    case 'arc': {
      const [cx, cy] = c.center;
      const r = c.radius;
      let sweep = (c.endDeg - c.startDeg) % 360;
      if (sweep <= 0) sweep += 360;
      const a0 = (c.startDeg * Math.PI) / 180;
      const a1 = ((c.startDeg + sweep) * Math.PI) / 180;
      const large = sweep > 180 ? 1 : 0;
      return (
        `M${fmt(cx + r * Math.cos(a0))} ${fmt(cy + r * Math.sin(a0))}` +
        `A${fmt(r)} ${fmt(r)} 0 ${large} 1 ${fmt(cx + r * Math.cos(a1))} ${fmt(cy + r * Math.sin(a1))}`
      );
    }
  }
}

/** Point of the view plane (model mm, view axes) under a sheet point. */
export function sheetToViewPoint(view: Pick<PictureView, 'anchor' | 'factor'>, p: Point): Point {
  return [(p[0] - view.anchor[0]) / view.factor, (p[1] - view.anchor[1]) / view.factor];
}

/** Model point on the view plane through the model origin: x·right + y·up. */
export function viewPointToModel(view: Pick<PictureView, 'frame'>, p: Point): [number, number, number] {
  const { right, up } = view.frame;
  const r = (i: number) => Math.round((p[0] * right[i] + p[1] * up[i]) * 1e6) / 1e6;
  return [r(0), r(1), r(2)];
}

/** The view whose bounds contain the sheet point (the smallest one when bounds overlap). */
export function viewAt(views: PictureView[], p: Point): PictureView | null {
  let best: PictureView | null = null;
  let bestArea = Infinity;
  for (const v of views) {
    const b = v.bounds;
    if (p[0] < b.minX || p[0] > b.maxX || p[1] < b.minY || p[1] > b.maxY) continue;
    const area = (b.maxX - b.minX) * (b.maxY - b.minY);
    if (area < bestArea) {
      best = v;
      bestArea = area;
    }
  }
  return best;
}

// ---------------------------------------------------------------- tools

export type Tool =
  | 'select'
  | 'dimension'
  | 'chain'
  | 'baseline'
  | 'roughness'
  | 'tolerance'
  | 'datum'
  | 'balloon'
  | 'thread'
  | 'section'
  | 'localSection'
  | 'detail';

export interface EdgePick {
  view: string;
  index: number;
  type: PictureCurve['type'];
}

export interface PickState {
  picks: EdgePick[];
  /** The picks form a complete input of the tool: the editor sends them to the kernel. */
  ready: boolean;
  /** Reason the last click was not taken, or what to click next (a message key). */
  message: MessageKey | null;
}

export const emptyPicks: PickState = { picks: [], ready: false, message: null };

/**
 * Next pick state of an edge tool after a click on an edge. Dimension: up to two edges of one
 * view, ready at two (a single circle or arc is sent with the "Размер" button / Enter). Symbols:
 * one straight edge. Position (balloon): one edge of any kind — the leader point; the shelf is the
 * next click on the sheet (balloonShelf). Chain and baseline dimensions: any number of edges of one
 * view, sent with the button / Enter (canDimensionGroup). A second click on a picked edge removes it.
 */
export function addPick(state: PickState, tool: Tool, pick: EdgePick): PickState {
  const same = state.picks.findIndex((p) => p.view === pick.view && p.index === pick.index);
  if (same >= 0) return { picks: state.picks.filter((_, i) => i !== same), ready: false, message: null };
  if (tool === 'dimension') {
    if (state.picks.length > 0 && state.picks[0].view !== pick.view) {
      return { picks: [pick], ready: false, message: 'drawing.pick.oneView' };
    }
    const picks = [...state.picks, pick].slice(-2);
    return { picks, ready: picks.length === 2, message: null };
  }
  if (tool === 'chain' || tool === 'baseline') {
    if (state.picks.length > 0 && state.picks[0].view !== pick.view) {
      return { picks: [pick], ready: false, message: 'drawing.pick.oneView' };
    }
    return { picks: [...state.picks, pick], ready: false, message: null };
  }
  if (tool === 'roughness' || tool === 'tolerance' || tool === 'datum') {
    if (pick.type === 'circle' || pick.type === 'arc') {
      return { ...state, ready: false, message: 'drawing.pick.straight' };
    }
    return { picks: [pick], ready: true, message: null };
  }
  if (tool === 'balloon') return { picks: [pick], ready: false, message: 'drawing.pick.shelf' };
  return { ...state, ready: false, message: null };
}

/**
 * The position tool after a click on the sheet at `p` (sheet mm): with a picked edge, the leader
 * edge and the shelf point to send to the kernel (addBalloon); without one, null.
 */
export function balloonShelf(state: PickState, p: Point): { view: string; index: number; at: Point } | null {
  if (state.picks.length !== 1) return null;
  return { view: state.picks[0].view, index: state.picks[0].index, at: [p[0], p[1]] };
}

/** Chain/baseline dimensions need at least two picked edges of one view. */
export function canDimensionGroup(state: PickState): boolean {
  return state.picks.length >= 2 && state.picks.every((p) => p.view === state.picks[0].view);
}

/**
 * Outline of a local section being drawn (sheet points): a click adds a point; a click on the
 * first point (within `closeTol` mm) or Enter closes the outline when it has three points or more.
 * Points closer than 0.5 mm to the previous one are ignored (double clicks).
 */
export interface Outline {
  points: Point[];
  closed: boolean;
}
export const emptyOutline: Outline = { points: [], closed: false };

export function addOutlinePoint(o: Outline, p: Point, closeTol = 2): Outline {
  if (o.closed) return { points: [p], closed: false };
  const last = o.points[o.points.length - 1];
  if (last && Math.hypot(p[0] - last[0], p[1] - last[1]) < 0.5) return o;
  const first = o.points[0];
  if (first && o.points.length >= 3 && Math.hypot(p[0] - first[0], p[1] - first[1]) <= closeTol) return { points: o.points, closed: true };
  return { points: [...o.points, p], closed: false };
}

/** SVG path of an outline in sheet coordinates (closed with Z). */
export function outlinePath(o: Outline): string {
  if (o.points.length === 0) return '';
  const [first, ...rest] = o.points;
  return `M${fmt(first[0])} ${fmt(first[1])}` + rest.map((q) => `L${fmt(q[0])} ${fmt(q[1])}`).join('') + (o.closed ? 'Z' : '');
}

/** A single picked circle or arc can be dimensioned on request (diameter / radius). */
export function canDimensionSingle(state: PickState): boolean {
  return state.picks.length === 1 && (state.picks[0].type === 'circle' || state.picks[0].type === 'arc');
}

// Latin transliteration of Russian letters for file names (Chromium replaces a download name it
// does not accept with "download").
const TRANSLIT: Record<string, string> = {
  а: 'a', б: 'b', в: 'v', г: 'g', д: 'd', е: 'e', ё: 'e', ж: 'zh', з: 'z', и: 'i', й: 'i', к: 'k', л: 'l',
  м: 'm', н: 'n', о: 'o', п: 'p', р: 'r', с: 's', т: 't', у: 'u', ф: 'f', х: 'kh', ц: 'ts', ч: 'ch',
  ш: 'sh', щ: 'shch', ъ: '', ы: 'y', ь: '', э: 'e', ю: 'iu', я: 'ia',
};

/** File name for a download: designation (or "drawing") + suffix, ASCII letters, digits, ".-_". */
export function downloadName(designation: string, suffix: string, ext: string): string {
  const latin = Array.from(designation.trim())
    .map((ch) => {
      const t = TRANSLIT[ch.toLowerCase()];
      if (t === undefined) return ch;
      return ch === ch.toLowerCase() ? t : t.charAt(0).toUpperCase() + t.slice(1);
    })
    .join('');
  const base = latin.replace(/[^A-Za-z0-9._-]+/g, '_').replace(/^[._]+|_+$/g, '') || 'drawing';
  return `${base}${suffix}.${ext}`;
}
