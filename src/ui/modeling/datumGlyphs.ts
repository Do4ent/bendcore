// Datum geometry of the part shown in the viewport (docs/ui/PART_MODELING_UI.md,
// "Вспомогательная геометрия"): every built datum node gives one glyph — a point marker
// (DatumPoint), the three axes of a coordinate system (DatumCsys), a plane outline (DatumPlane)
// or an axis line (DatumAxis). A coordinate system also reports its XY plane, Z axis and origin;
// only its axes are drawn.

import type { FeatureView } from '../../wasm/bendcore_modeling_api.mjs';
import type { DatumGlyph } from '../kernel/KernelViewport';

const BUILT = new Set(['ok', 'warning']);

export function datumGlyphs(features: readonly FeatureView[]): DatumGlyph[] {
  const out: DatumGlyph[] = [];
  for (const f of features) {
    const d = f.datum;
    if (!d || !BUILT.has(f.state)) continue;
    if (d.csys) {
      out.push({ kind: 'csys', id: f.id, origin: d.csys.origin, x: d.csys.x, y: d.csys.y, z: d.csys.z });
    } else if (d.point) {
      out.push({ kind: 'point', id: f.id, point: d.point });
    } else if (d.planeOrigin && d.planeNormal) {
      out.push({ kind: 'plane', id: f.id, origin: d.planeOrigin, normal: d.planeNormal });
    } else if (d.axisOrigin && d.axisDirection) {
      out.push({ kind: 'axis', id: f.id, origin: d.axisOrigin, direction: d.axisDirection });
    }
  }
  return out;
}
