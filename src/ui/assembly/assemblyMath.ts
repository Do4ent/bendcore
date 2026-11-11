// Small vector helpers of the assembly page (docs/ui/ASSEMBLY_UI.md): drag plane, fastener
// insertion direction, flattening of the occurrence tree and of the BOM.

import type { BomRow, OccurrenceInfo } from '../../wasm/bendcore_assembly_api.mjs';

export type V3 = [number, number, number];

export const dot = (a: V3, b: V3) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
export const sub = (a: V3, b: V3): V3 => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
export const add = (a: V3, b: V3): V3 => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
export const scale = (a: V3, s: number): V3 => [a[0] * s, a[1] * s, a[2] * s];

/** Intersection of the ray origin + t·dir (t ≥ 0) with a plane; null when parallel or behind. */
export function rayPlane(origin: V3, dir: V3, point: V3, normal: V3): V3 | null {
  const denom = dot(dir, normal);
  if (Math.abs(denom) < 1e-12) return null;
  const t = dot(sub(point, origin), normal) / denom;
  if (t < 0) return null;
  return add(origin, scale(dir, t));
}

/**
 * Fastener insertion direction (head → tip) for a hole axis seen along viewDir: the axis sign
 * that points away from the viewer, so the head sits on the visible side.
 */
export function insertionDirection(axis: V3, viewDir: V3): V3 {
  return dot(axis, viewDir) >= 0 ? axis : scale(axis, -1);
}

/** Occurrences depth first with their path and depth. */
export function flattenOccurrences(
  list: OccurrenceInfo[],
  prefix: number[] = [],
  depth = 0,
): { occurrence: OccurrenceInfo; path: number[]; depth: number }[] {
  const out: { occurrence: OccurrenceInfo; path: number[]; depth: number }[] = [];
  for (const o of list) {
    const path = [...prefix, o.id];
    out.push({ occurrence: o, path, depth });
    if (o.children) out.push(...flattenOccurrences(o.children, path, depth + 1));
  }
  return out;
}

/** BOM rows in table order (children after their assembly row). */
export function bomTableRows(rows: BomRow[]): BomRow[] {
  const out: BomRow[] = [];
  const walk = (list: BomRow[]) => {
    for (const r of list) {
      out.push(r);
      walk(r.children);
    }
  };
  walk(rows);
  return out;
}

/** Clamp of the exploded-view slider: 0 … number of steps. */
export function clampProgress(progress: number, steps: number): number {
  if (!Number.isFinite(progress)) return 0;
  return Math.min(Math.max(progress, 0), Math.max(steps, 0));
}
