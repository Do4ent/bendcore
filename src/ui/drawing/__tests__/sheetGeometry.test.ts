// Sheet display logic of the drawing editor (src/ui/drawing/sheetGeometry.ts): transforms are
// checked by round trips and fixed points, SVG paths by parsing their end points against the
// curve definitions, picks by the tool rules of docs/ui/DRAWING_UI.md.

import {
  addPick,
  balloonShelf,
  canDimensionSingle,
  curvePath,
  downloadName,
  emptyPicks,
  fitTransform,
  panBy,
  screenToSheet,
  sheetToScreen,
  sheetToViewPoint,
  viewAt,
  viewPointToModel,
  zoomAt,
} from '../sheetGeometry';
import type { PictureView } from '../../../wasm/bendcore_drawing_api.mjs';

const numbers = (d: string) => (d.match(/-?\d+(\.\d+)?(e-?\d+)?/g) ?? []).map(Number);

describe('sheet transform', () => {
  test('fit: A3 landscape in 1000 × 600 px is limited by the height and centred', () => {
    const t = fitTransform(420, 297, 1000, 600, 16);
    expect(t.zoom).toBeCloseTo((600 - 32) / 297, 12);
    // Sheet corners land symmetric in the viewport.
    const [x0, y0] = sheetToScreen(t, 297, [0, 297]);
    const [x1, y1] = sheetToScreen(t, 297, [420, 0]);
    expect(x0).toBeCloseTo(1000 - x1, 9);
    expect(y0).toBeCloseTo(16, 9);
    expect(y1).toBeCloseTo(600 - 16, 9);
  });

  test('screen and sheet points are inverse; y of the sheet points up', () => {
    const t = { zoom: 2.5, panX: 40, panY: -12 };
    for (const p of [[0, 0], [420, 297], [123.25, 77.5]] as [number, number][]) {
      const back = screenToSheet(t, 297, sheetToScreen(t, 297, p));
      expect(back[0]).toBeCloseTo(p[0], 12);
      expect(back[1]).toBeCloseTo(p[1], 12);
    }
    expect(sheetToScreen(t, 297, [0, 0])[1]).toBeGreaterThan(sheetToScreen(t, 297, [0, 10])[1]);
  });

  test('zoom keeps the point under the cursor; limits; pan', () => {
    const t = { zoom: 2, panX: 10, panY: 20 };
    const before = screenToSheet(t, 297, [300, 200]);
    const z = zoomAt(t, 1.7, 300, 200);
    expect(z.zoom).toBeCloseTo(3.4, 12);
    const after = screenToSheet(z, 297, [300, 200]);
    expect(after[0]).toBeCloseTo(before[0], 9);
    expect(after[1]).toBeCloseTo(before[1], 9);
    expect(zoomAt(t, 1e6, 0, 0).zoom).toBe(60);
    expect(zoomAt(t, 1e-6, 0, 0).zoom).toBe(0.2);
    expect(panBy(t, 5, -3)).toEqual({ zoom: 2, panX: 15, panY: 17 });
  });

  test('fit rejects an empty sheet', () => {
    expect(() => fitTransform(0, 297, 100, 100)).toThrow();
  });
});

describe('curve paths', () => {
  test('line and polyline', () => {
    expect(curvePath({ type: 'line', points: [[1, 2], [3.5, -4]] })).toBe('M1 2L3.5 -4');
    expect(curvePath({ type: 'polyline', points: [[0, 0], [1, 0], [1, 1]], closed: true })).toBe('M0 0L1 0L1 1Z');
  });

  test('circle: two half arcs through the points at 0° and 180°', () => {
    const d = curvePath({ type: 'circle', center: [10, 20], radius: 4 });
    const n = numbers(d);
    expect(n.slice(0, 2)).toEqual([14, 20]);
    expect(d.match(/A/g)).toHaveLength(2);
    expect(n.slice(7, 9)).toEqual([6, 20]);
  });

  test('arc: end points by start/end angle, large-arc flag by the sweep', () => {
    // Quarter arc 0°..90° of R 5 at (0, 0): from (5, 0) to (0, 5), small.
    const q = numbers(curvePath({ type: 'arc', center: [0, 0], radius: 5, startDeg: 0, endDeg: 90 }));
    expect(q.slice(0, 2)).toEqual([5, 0]);
    expect(q.slice(2, 7)).toEqual([5, 5, 0, 0, 1]);
    expect(q[7]).toBeCloseTo(0, 9);
    expect(q[8]).toBeCloseTo(5, 9);
    // 300° sweep across 0° (from 330° to 270°): large arc.
    const l = numbers(curvePath({ type: 'arc', center: [0, 0], radius: 2, startDeg: 330, endDeg: 270 }));
    expect(l[5]).toBe(1);
    expect(l[7]).toBeCloseTo(0, 9);
    expect(l[8]).toBeCloseTo(-2, 9);
  });
});

describe('view coordinates', () => {
  const front: PictureView = {
    id: 'V1',
    kind: 'base',
    inGroup: true,
    label: '',
    scale: '1:2',
    bounds: { minX: 100, minY: 100, maxX: 130, maxY: 125 },
    anchor: [100, 100],
    factor: 0.5,
    frame: { toward: [0, -1, 0], right: [1, 0, 0], up: [0, 0, 1] },
    edges: [],
  };

  test('sheet point to view point and model point (front view: x → X, y → Z)', () => {
    const vp = sheetToViewPoint(front, [120, 110]);
    expect(vp).toEqual([40, 20]);
    expect(viewPointToModel(front, vp)).toEqual([40, 0, 20]);
  });

  test('viewAt picks the smallest view containing the point', () => {
    const detail = { ...front, id: 'V2', bounds: { minX: 110, minY: 105, maxX: 115, maxY: 110 } };
    expect(viewAt([front, detail], [112, 107])!.id).toBe('V2');
    expect(viewAt([front, detail], [101, 101])!.id).toBe('V1');
    expect(viewAt([front, detail], [10, 10])).toBeNull();
  });
});

describe('picks of the edge tools', () => {
  const line = (index: number, view = 'V1') => ({ view, index, type: 'line' as const });

  test('dimension: ready at two edges of one view; a click on a picked edge removes it', () => {
    let s = addPick(emptyPicks, 'dimension', line(1));
    expect(s.ready).toBe(false);
    s = addPick(s, 'dimension', line(1));
    expect(s.picks).toEqual([]);
    s = addPick(addPick(emptyPicks, 'dimension', line(1)), 'dimension', line(4));
    expect(s.ready).toBe(true);
    expect(s.picks.map((p) => p.index)).toEqual([1, 4]);
  });

  test('dimension: an edge of another view starts the selection again', () => {
    const s = addPick(addPick(emptyPicks, 'dimension', line(1)), 'dimension', line(2, 'V2'));
    expect(s.picks).toEqual([line(2, 'V2')]);
    expect(s.ready).toBe(false);
    expect(s.message).toBe('drawing.pick.oneView');
  });

  test('a single circle or arc can be dimensioned, a single line cannot', () => {
    expect(canDimensionSingle(addPick(emptyPicks, 'dimension', { view: 'V1', index: 3, type: 'circle' }))).toBe(true);
    expect(canDimensionSingle(addPick(emptyPicks, 'dimension', { view: 'V1', index: 3, type: 'arc' }))).toBe(true);
    expect(canDimensionSingle(addPick(emptyPicks, 'dimension', line(3)))).toBe(false);
  });

  test('symbols: one straight edge; curves are refused', () => {
    expect(addPick(emptyPicks, 'roughness', line(2)).ready).toBe(true);
    const c = addPick(emptyPicks, 'datum', { view: 'V1', index: 3, type: 'circle' });
    expect(c.ready).toBe(false);
    expect(c.picks).toEqual([]);
    expect(c.message).toBe('drawing.pick.straight');
  });

  test('position: an edge of any kind, then the shelf point on the sheet', () => {
    expect(balloonShelf(emptyPicks, [10, 20])).toBeNull();
    const s = addPick(emptyPicks, 'balloon', { view: 'V1', index: 3, type: 'circle' });
    expect(s.ready).toBe(false);
    expect(s.message).toBe('drawing.pick.shelf');
    expect(balloonShelf(s, [150.5, 80])).toEqual({ view: 'V1', index: 3, at: [150.5, 80] });
    // Another edge replaces the leader edge.
    const t = addPick(s, 'balloon', line(7, 'V2'));
    expect(balloonShelf(t, [1, 2])).toEqual({ view: 'V2', index: 7, at: [1, 2] });
  });

  test('download names', () => {
    expect(downloadName('РД.10.001', '_S1', 'dxf')).toBe('RD.10.001_S1.dxf');
    expect(downloadName('ВТ.01.000 СБ', '', 'pdf')).toBe('VT.01.000_SB.pdf');
    expect(downloadName('  ', '', 'pdf')).toBe('drawing.pdf');
    expect(downloadName('Щит/Ёж: 1', '', 'svg')).toBe('Shchit_Ezh_1.svg');
  });
});
