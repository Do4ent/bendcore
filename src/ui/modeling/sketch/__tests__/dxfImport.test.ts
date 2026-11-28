/**
 * @jest-environment node
 */
// DXF import panel of the sketch editor (docs/ui/SKETCH_EDITOR.md, "Импорт DXF"): the options sent
// to ModelingKernel.dxfSketch and the summary of its answer.

import type { DxfSketchImport } from '../../../../wasm/bendcore_modeling_api.mjs';
import { translate } from '../../../app/i18n';
import type { MessageKey, Translate } from '../../../app/i18n';
import { DXF_UNITS, dxfOptions, dxfSummary, dxfTolerance, initialDxfForm, selectedLayers, toggleLayer, unitCaption } from '../dxfImport';

const t: Translate = (key: MessageKey, params) => translate('ru', key, params);
const tEn: Translate = (key: MessageKey, params) => translate('en', key, params);

function answer(over: Partial<DxfSketchImport> = {}): DxfSketchImport {
  return {
    sketch: { points: [], geometry: [], constraints: [], regions: [] },
    layers: [
      { name: 'CONTOUR', color: 7, frozen: false, off: false, curves: 4, selected: true },
      { name: 'DIMS', color: 2, frozen: false, off: false, curves: 1, selected: true },
      { name: 'FROZEN', color: 1, frozen: true, off: false, curves: 1, selected: false },
    ],
    version: 'AC1015',
    fileUnits: 1,
    units: 1,
    mmPerUnit: 25.4,
    tolerance: 0.001,
    imported: { lines: 4, arcs: 2, circles: 1, polylines: 1, polylineSegments: 4, degenerate: 0 },
    entities: new Int32Array([1, 2, 3, 4, 5, 6, 7]),
    points: 9,
    merged: 8,
    openEnds: new Int32Array([]),
    ignored: { texts: 1, paperSpace: 1, otherLayers: 1 },
    skipped: { SPLINE: 1, INSERT: 2 },
    warnings: [],
    bounds: { min: [0, 0], max: [101.6, 50.8] },
    ...over,
  };
}

describe('options of dxfSketch', () => {
  test('defaults: kernel layer choice, millimetres only for a file without units, tolerance', () => {
    const f = initialDxfForm();
    expect(dxfOptions(f)).toEqual({ fixed: false, defaultUnits: 4, tolerance: 0.001 });
    expect(dxfOptions({ ...f, units: 1, fixed: true, tolerance: '0,05' })).toEqual({ fixed: true, defaultUnits: 4, tolerance: 0.05, units: 1 });
    expect(dxfTolerance({ ...f, tolerance: '-1' })).toBeNull();
    expect(dxfTolerance({ ...f, tolerance: 'abc' })).toBeNull();
    expect(dxfOptions({ ...f, tolerance: '0' }).tolerance).toBeUndefined();
  });

  test('switching layers starts from the kernel selection', () => {
    const r = answer();
    expect(selectedLayers(r)).toEqual(['CONTOUR', 'DIMS']);
    const off = toggleLayer(initialDxfForm(), r, 'DIMS', false);
    expect(off.layers).toEqual(['CONTOUR']);
    expect(dxfOptions(off).layers).toEqual(['CONTOUR']);
    const on = toggleLayer(off, r, 'FROZEN', true);
    expect(on.layers).toEqual(['CONTOUR', 'FROZEN']);
    expect(toggleLayer(on, r, 'FROZEN', true).layers).toEqual(['CONTOUR', 'FROZEN']);
  });

  test('unit captions for every offered code', () => {
    for (const u of DXF_UNITS) expect(unitCaption(t, u)).not.toBe(String(u));
    expect(unitCaption(t, 1)).toBe('дюйм');
    expect(unitCaption(tEn, 6)).toBe('m');
    expect(unitCaption(t, 20)).toBe('20');
  });
});

describe('summary of the answer', () => {
  test('counts, size, closed contours, what is left out', () => {
    const lines = dxfSummary(answer(), t);
    expect(lines).toEqual([
      'Отрезков 4, дуг 2, окружностей 1, полилиний 1 (4 сегм.)',
      'Точек 9, совмещено концов 8',
      'Габарит 101.6 × 50.8 мм',
      'Все контуры замкнуты',
      'Не поддерживаются и не импортируются: SPLINE × 1, INSERT × 2',
      'Пропущено: текстов 1, в пространстве листа 1, на невыбранных слоях 1',
    ]);
  });

  test('open ends, degenerate curves, warnings, nothing selected', () => {
    const lines = dxfSummary(answer({ openEnds: new Int32Array([3, 5]), imported: { lines: 2, arcs: 0, circles: 0, polylines: 0, polylineSegments: 0, degenerate: 1 }, warnings: ['Z dropped'] }), t);
    expect(lines).toContain('Незамкнутых концов: 2');
    expect(lines).toContain('Вырожденных кривых (короче допуска): 1');
    expect(lines).toContain('Предупреждения чтения: Z dropped');
    // A warning of the DXF reader with a template is shown in the interface language.
    const known = dxfSummary(answer({ warnings: ['file has no EOF marker', '3 entities have non-zero Z or elevation; they are projected onto XY'] }), t);
    expect(known).toContain('Предупреждения чтения: В файле нет метки конца (EOF): возможно, он записан не полностью; Объектов с ненулевой координатой Z или возвышением: 3; они спроецированы на плоскость XY');
    const none = dxfSummary(answer({ entities: new Int32Array([]), bounds: null, skipped: {}, ignored: { texts: 0, paperSpace: 0, otherLayers: 0 } }), t);
    expect(none[0]).toBe('На выбранных слоях нет кривых');
    expect(none.some((l) => l.startsWith('Габарит'))).toBe(false);
    expect(none.some((l) => l.startsWith('Все контуры'))).toBe(false);
  });
});
