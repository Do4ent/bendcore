// Dimensions of a sketch for the edit panel (docs/ui/PART_MODELING_UI.md, "Эскиз"): the
// expressions that drive the solved geometry — coordinates of fixed points and the values of
// Distance, Radius and Angle constraints. Editing keeps the structure (point and entity
// numbers), so names of the faces and edges built from the sketch stay the same.

import type { SketchData, SketchGeometryDef } from '../../wasm/bendcore_modeling_api.mjs';
import { translate } from '../app/i18n';
import type { Translate } from '../app/i18n';

/** Russian captions when the caller passes no translator (the reference dictionary). */
const RU: Translate = (key, params) => translate('ru', key, params);

export interface SketchDimension {
  /** p<i>.x, p<i>.y (point i) or c<i> (constraint i). */
  key: string;
  label: string;
  expression: string;
}

function samePair(c: { a: number; b: number }, a: number, b: number): boolean {
  return (c.a === a && c.b === b) || (c.a === b && c.b === a);
}

export function sketchDimensions(data: SketchData, t: Translate = RU): SketchDimension[] {
  const out: SketchDimension[] = [];
  data.points.forEach((p, i) => {
    if (!p.fixed) return;
    out.push({ key: `p${i}.x`, label: t('part.dim.pointX', { n: i + 1 }), expression: p.x });
    out.push({ key: `p${i}.y`, label: t('part.dim.pointY', { n: i + 1 }), expression: p.y });
  });
  data.constraints.forEach((c, i) => {
    if (c.type === 'distance') {
      const horizontal = data.constraints.some((o) => o.type === 'horizontal' && samePair(o, c.a, c.b));
      const vertical = data.constraints.some((o) => o.type === 'vertical' && samePair(o, c.a, c.b));
      const what = horizontal ? 'part.dim.lengthX' : vertical ? 'part.dim.lengthY' : 'part.dim.distance';
      out.push({ key: `c${i}`, label: t(what, { a: c.a + 1, b: c.b + 1 }), expression: c.value });
    } else if (c.type === 'radius') {
      out.push({ key: `c${i}`, label: t('part.dim.radius', { a: c.a }), expression: c.value });
    } else if (c.type === 'angle') {
      out.push({ key: `c${i}`, label: t('part.dim.angle', { a: c.a, b: c.b }), expression: c.value });
    }
  });
  return out;
}

/** Copy of `data` with the given dimension expressions; unknown keys are an error. */
export function applySketchDimensions(data: SketchData, values: Record<string, string>): SketchData {
  const next: SketchData = JSON.parse(JSON.stringify(data));
  for (const [key, expression] of Object.entries(values)) {
    const point = /^p(\d+)\.(x|y)$/.exec(key);
    const constraint = /^c(\d+)$/.exec(key);
    if (point) {
      const p = next.points[Number(point[1])];
      if (!p || !p.fixed) throw new Error(`sketch dimension ${key}: no fixed point`);
      p[point[2] as 'x' | 'y'] = expression;
    } else if (constraint) {
      const c = next.constraints[Number(constraint[1])];
      if (!c || !['distance', 'radius', 'angle'].includes(c.type)) throw new Error(`sketch dimension ${key}: no dimension`);
      c.value = expression;
    } else {
      throw new Error(`unknown sketch dimension ${key}`);
    }
  }
  return next;
}

/** Profile template of the "new sketch" panel. */
export interface SketchTemplateForm {
  /** path: an open chain of lines and arcs (sweep path, rib contour); the sketch gets contour = open. */
  kind: 'rectangle' | 'circle' | 'polygon' | 'path';
  x: string;
  y: string;
  width: string;
  height: string;
  cx: string;
  cy: string;
  radius: string;
  /** One corner per line: "x; y" (expressions may contain commas). */
  polygon: string;
  /** Open chain, one vertex per line: "x; y" — a line from the previous vertex; "x; y; cx; cy"
   * (optionally "; cw" or "; ccw", default ccw) — an arc around (cx, cy) from the previous vertex. */
  path: string;
}

export const DEFAULT_TEMPLATE: SketchTemplateForm = {
  kind: 'rectangle',
  x: '0',
  y: '0',
  width: '60',
  height: '40',
  cx: '0',
  cy: '0',
  radius: '20',
  polygon: '0; 0\n60; 0\n60; 20\n0; 40',
  path: '0; 0\n0; 30\n20; 50; 20; 30; cw',
};

export type ProfileTemplate =
  | { kind: 'rectangle'; x: string; y: string; width: string; height: string }
  | { kind: 'circle'; cx: string; cy: string; radius: string }
  | { kind: 'polygon'; points: [string, string][] };

/** The template spec for ModelingKernel.sketchTemplate; polygon text errors name the line. */
export function templateProfiles(form: SketchTemplateForm, t: Translate = RU): { profiles: ProfileTemplate[] } {
  if (form.kind === 'path') throw new Error('an open path is not a profile template: build it with pathSketch');
  if (form.kind === 'rectangle') {
    return { profiles: [{ kind: 'rectangle', x: form.x, y: form.y, width: form.width, height: form.height }] };
  }
  if (form.kind === 'circle') {
    return { profiles: [{ kind: 'circle', cx: form.cx, cy: form.cy, radius: form.radius }] };
  }
  const points: [string, string][] = [];
  form.polygon
    .split('\n')
    .map((l) => l.trim())
    .forEach((line, i) => {
      if (!line) return;
      const parts = line.split(';').map((p) => p.trim());
      if (parts.length !== 2 || !parts[0] || !parts[1]) {
        throw new Error(t('part.dim.badVertex', { n: i + 1, line }));
      }
      points.push([parts[0], parts[1]]);
    });
  if (points.length < 3) throw new Error(t('part.dim.fewVertices'));
  return { profiles: [{ kind: 'polygon', points }] };
}

/**
 * Sketch data of an open chain ("path" template): every vertex is a fixed point (a dimension of
 * the sketch), lines and arcs join consecutive vertices; an arc's centre is a fixed point too.
 * Errors name the line of the text.
 */
export function pathSketch(text: string, t: Translate = RU): SketchData {
  const data: SketchData = { points: [], geometry: [], constraints: [], regions: [] };
  const point = (x: string, y: string) => {
    data.points.push({ x, y, fixed: true });
    return data.points.length - 1;
  };
  const curve = (g: Partial<SketchGeometryDef> & Pick<SketchGeometryDef, 'type'>) =>
    data.geometry.push({
      start: -1,
      end: -1,
      center: -1,
      radius: '',
      counterClockwise: true,
      construction: false,
      projection: -1,
      ...g,
    });
  let previous = -1;
  text
    .split('\n')
    .map((l) => l.trim())
    .forEach((line, i) => {
      if (!line) return;
      const parts = line.split(';').map((p) => p.trim());
      const direction = parts.length === 5 ? parts[4].toLowerCase() : 'ccw';
      const valid =
        (parts.length === 2 || parts.length === 4 || parts.length === 5) &&
        parts.slice(0, 4).every((p) => p.length > 0) &&
        (direction === 'cw' || direction === 'ccw') &&
        (parts.length === 2 || previous >= 0);
      if (!valid) throw new Error(t('part.dim.badPathVertex', { n: i + 1, line }));
      const vertex = point(parts[0], parts[1]);
      if (parts.length === 2) {
        if (previous >= 0) curve({ type: 'line', start: previous, end: vertex });
      } else {
        const center = point(parts[2], parts[3]);
        curve({ type: 'arc', center, start: previous, end: vertex, counterClockwise: direction === 'ccw' });
      }
      previous = vertex;
    });
  if (data.geometry.length === 0) throw new Error(t('part.dim.fewPathVertices'));
  return data;
}
