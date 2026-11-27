// Selection sets and the section range of the assembly page (docs/ui/ASSEMBLY_UI.md, "Фильтры
// выбора, выбор рамкой и сечение"): faces and edges of occurrences as {path, index}, combined by the
// select operation, and the bounding box of the placed parts for the section slider.

import type { SelectOp } from '../kernel/selection';
import type { Box3 } from '../kernel/viewTools';

/** A face or an edge of the part at an occurrence path. */
export interface ElementRef {
  path: number[];
  index: number;
}

export const refKey = (r: ElementRef): string => `${r.path.join('/')}:${r.index}`;

/** The set after `found` was taken with `op` (no duplicates, order kept). */
export function combineRefs(current: readonly ElementRef[], found: readonly ElementRef[], op: SelectOp): ElementRef[] {
  const unique = (list: readonly ElementRef[]) => {
    const seen = new Set<string>();
    return list.filter((r) => {
      const k = refKey(r);
      if (seen.has(k)) return false;
      seen.add(k);
      return true;
    });
  };
  if (op === 'replace') return unique(found);
  if (op === 'remove') {
    const drop = new Set(found.map(refKey));
    return current.filter((r) => !drop.has(refKey(r)));
  }
  return unique([...current, ...found]);
}

/** Top-level occurrence ids of occurrence paths (a component is selected through its top-level occurrence). */
export function topLevelIds(paths: readonly (readonly number[])[]): number[] {
  return [...new Set(paths.filter((p) => p.length > 0).map((p) => p[0]))];
}

export interface PlacedMesh {
  positions: ArrayLike<number>;
  /** column-major 4x4 world matrix of the occurrence */
  matrix: readonly number[];
}

/** Bounding box of the vertices of the placed parts in world coordinates; null without vertices. */
export function placedBox(items: readonly PlacedMesh[]): Box3 | null {
  const min: [number, number, number] = [Infinity, Infinity, Infinity];
  const max: [number, number, number] = [-Infinity, -Infinity, -Infinity];
  let any = false;
  for (const { positions, matrix: m } of items) {
    for (let i = 0; i + 2 < positions.length; i += 3) {
      const x = positions[i];
      const y = positions[i + 1];
      const z = positions[i + 2];
      const w = [
        m[0] * x + m[4] * y + m[8] * z + m[12],
        m[1] * x + m[5] * y + m[9] * z + m[13],
        m[2] * x + m[6] * y + m[10] * z + m[14],
      ];
      for (let k = 0; k < 3; ++k) {
        if (w[k] < min[k]) min[k] = w[k];
        if (w[k] > max[k]) max[k] = w[k];
      }
      any = true;
    }
  }
  return any ? { min, max } : null;
}
