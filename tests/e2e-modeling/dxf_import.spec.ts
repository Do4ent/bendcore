// DXF import into a sketch end to end (docs/ui/SKETCH_EDITOR.md, "Импорт DXF"). The user starts a
// sketch on XY, opens "Импорт DXF…", chooses tests/dxf/sketch/plate_inch.dxf (ezdxf, inches: a
// 4 × 2 in contour of separate LINE entities with 2e-5 in gaps, a 0.5 in hole, a dimension line on
// layer DIMS, a frozen layer). The kernel reports the layers and the units of the file; with DIMS
// switched off the summary says the contour is closed; "Insert" puts the curves into the sketch.
// The sketch leaves with one region, the extrusion of 10 mm has the volume
// (4·2 − π·0.25²)·25.4²·10 (the merged end points move the outline by at most the gap), undo and
// redo of the part return to one node and back. A second sketch from slot_mm.dxf with the layer
// SLOT and a tolerance below the gaps of nothing (bulged LWPOLYLINE) gives the stadium area; the
// R12 file without units asks for them. An unknown text of the kernel is shown localized by code.
import { expect, test, Page } from '@playwright/test';
import path from 'path';

const fixtures = path.resolve(__dirname, '../dxf/sketch');
const IN = 25.4;

interface EditorState {
  definition: string;
  points: { x: number; y: number; fixed: boolean }[];
  curves: { entity: number; type: string }[];
  regions: { index: number; area: number; selected: boolean }[] | null;
  selection: string[];
  message: string | null;
}

declare global {
  interface Window {
    bendcoreSketchEditor?: { getState(): EditorState };
    bendcorePartModeler?: {
      getState(): {
        status: string;
        error: { code: string; message: string } | null;
        features: { id: string; type: string; state: string; messages: string[] }[];
        properties: { volume: number | null } | null;
        canUndo: boolean;
        canRedo: boolean;
      };
      idle(): Promise<void>;
    };
  }
}

const editor = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcorePartModeler!.idle();
    return window.bendcoreSketchEditor!.getState();
  });
const modeler = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcorePartModeler!.idle();
    return window.bendcorePartModeler!.getState();
  });

async function expectVolume(page: Page, expected: number, rel: number) {
  await expect
    .poll(async () => {
      const v = (await modeler(page)).properties?.volume;
      return v === null || v === undefined ? Infinity : Math.abs(v - expected) / expected;
    })
    .toBeLessThan(rel);
}

async function openDxf(page: Page, file: string) {
  await page.getByTestId('sketch-dxf-open').click();
  await expect(page.getByTestId('sketch-dxf-panel')).toBeVisible();
  await page.getByTestId('sketch-dxf-file').setInputFiles(path.join(fixtures, file));
}

test('DXF contour into a sketch: layers, units, closed contour, extrusion volume, undo/redo', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await page.getByTestId('sketch-draw').click();
  await expect(page.getByTestId('sketch-panel')).toBeVisible();

  await openDxf(page, 'plate_inch.dxf');
  const summary = page.getByTestId('sketch-dxf-summary');
  await expect(summary).toBeVisible();
  // The units come from the file ($INSUNITS = 1, inches); the frozen layer is not chosen.
  await expect(page.getByTestId('sketch-dxf-units')).toHaveValue('file');
  await expect(page.getByTestId('sketch-dxf-units').locator('option[value="file"]')).toContainText('дюйм');
  await expect(page.getByTestId('sketch-dxf-layer-CONTOUR')).toBeChecked();
  await expect(page.getByTestId('sketch-dxf-layer-DIMS')).toBeChecked();
  await expect(page.getByTestId('sketch-dxf-layer-FROZEN')).not.toBeChecked();
  // With DIMS the dimension line is an open chain: two open ends.
  await expect(summary).toHaveAttribute('data-open-ends', '2');
  await expect(summary).toContainText('Незамкнутых концов: 2');
  await page.getByTestId('sketch-dxf-layer-DIMS').uncheck();
  await expect(summary).toHaveAttribute('data-open-ends', '0');
  await expect(summary).toHaveAttribute('data-curves', '5');
  await expect(summary).toContainText('Все контуры замкнуты');
  await expect(summary).toContainText('Отрезков 4, дуг 0, окружностей 1');
  await expect(summary).toContainText('совмещено концов 4');
  await expect(summary).toContainText(`Габарит ${Number((4 * IN).toFixed(3))} × ${Number((2 * IN).toFixed(3))} мм`);
  // A tolerance below the gaps opens the contour; back to the default closes it.
  await page.getByTestId('sketch-dxf-tolerance').fill('0.0001');
  await expect(summary).toHaveAttribute('data-open-ends', '6');
  await page.getByTestId('sketch-dxf-tolerance').fill('0.001');
  await expect(summary).toHaveAttribute('data-open-ends', '0');

  await page.getByTestId('sketch-dxf-insert').click();
  await expect(page.getByTestId('sketch-dxf-panel')).toHaveCount(0);
  await expect(page.getByTestId('sketch-dxf-inserted')).toContainText('Импортировано кривых: 5; незамкнутых концов: 0');
  let s = await editor(page);
  expect(s.message).toBeNull();
  expect(s.curves.map((c) => c.type).sort()).toEqual(['circle', 'line', 'line', 'line', 'line']);
  expect(s.points).toHaveLength(5);
  // Two regions: the plate with the hole (material, selected) and the disc of the hole.
  expect(s.regions).toHaveLength(2);
  const area = (8 - Math.PI * 0.25 * 0.25) * IN * IN;
  const bound = 12 * IN * 2e-5 * IN;
  const plate = s.regions!.filter((r) => r.selected);
  expect(plate).toHaveLength(1);
  expect(Math.abs(plate[0].area - area)).toBeLessThanOrEqual(bound);
  const disc = s.regions!.find((r) => !r.selected)!;
  expect(disc.area).toBeCloseTo(Math.PI * 0.25 * 0.25 * IN * IN, 6);

  await page.getByTestId('sketch-exit').click();
  await expect(page.getByTestId('sketch-panel')).toHaveCount(0);
  const sketchId = (await modeler(page)).features[0].id;
  expect((await modeler(page)).features[0].state).toBe('ok');
  await page.getByTestId('add-Extrude').click();
  await expect(page.getByTestId('link-sketch')).toHaveValue(sketchId);
  await page.getByTestId('param-distance').fill('10');
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
  await expectVolume(page, area * 10, (bound * 10) / (area * 10));
  expect((await modeler(page)).features.map((f) => f.state)).toEqual(['ok', 'ok']);

  // Undo the extrusion and the sketch, redo both: the same volume.
  await page.getByTestId('undo').click();
  await expect.poll(async () => (await modeler(page)).features.length).toBe(1);
  await page.getByTestId('undo').click();
  await expect.poll(async () => (await modeler(page)).features.length).toBe(0);
  await page.getByTestId('redo').click();
  await page.getByTestId('redo').click();
  await expect.poll(async () => (await modeler(page)).features.length).toBe(2);
  await expectVolume(page, area * 10, (bound * 10) / (area * 10));
});

test('DXF polyline with bulges, a file without units, kernel errors localized', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
  // A kernel ParseError in Russian by its template; the code and the original in the details.
  await page.getByTestId('doc-open').setInputFiles({ name: 'broken.json', mimeType: 'application/json', buffer: Buffer.from('not json at all') });
  const err = page.getByTestId('modeling-error');
  await expect(err.locator('.kernel-message-text')).toHaveText('Файл не является документом JSON');
  await expect(err.locator('.kernel-message-details')).toContainText('ParseError: not JSON');
  await page.getByTestId('sketch-draw').click();
  await expect(page.getByTestId('sketch-panel')).toBeVisible();

  // R12 without $INSUNITS: millimetres until the user chooses; the kernel would refuse the file.
  await openDxf(page, 'outline_r12.dxf');
  await expect(page.getByTestId('sketch-dxf-panel')).toContainText('В файле нет единиц');
  await expect(page.getByTestId('sketch-dxf-units')).toHaveValue('4');
  await expect(page.getByTestId('sketch-dxf-layer-КОНТУР')).toBeChecked();
  await page.getByTestId('sketch-dxf-units').selectOption('5');
  await expect(page.getByTestId('sketch-dxf-summary')).toContainText('Габарит');
  await expect(page.getByTestId('sketch-dxf-summary')).toContainText('× 300 мм');
  await page.getByTestId('sketch-dxf-close').click();

  // Stadium from a closed LWPOLYLINE with two bulge-1 ends, millimetres from the file.
  await openDxf(page, 'slot_mm.dxf');
  await expect(page.getByTestId('sketch-dxf-summary')).toContainText('SPLINE × 1');
  for (const l of ['SLOT_CW', 'ROUNDED']) await page.getByTestId(`sketch-dxf-layer-${l}`).uncheck();
  await expect(page.getByTestId('sketch-dxf-summary')).toHaveAttribute('data-curves', '4');
  // A negative tolerance is refused by the panel before the kernel is asked.
  await page.getByTestId('sketch-dxf-tolerance').fill('-1');
  await expect(page.getByTestId('sketch-dxf-insert')).toBeDisabled();
  await page.getByTestId('sketch-dxf-tolerance').fill('0.001');
  await page.getByTestId('sketch-dxf-insert').click();
  const s = await editor(page);
  expect(s.regions).toHaveLength(1);
  const slot = 50 * 20 + Math.PI * 100;
  expect(s.regions![0].area).toBeCloseTo(slot, 6);
  // Undo step inside the sketch removes the imported curves at once.
  await page.getByTestId('sketch-undo').click();
  expect((await editor(page)).curves).toHaveLength(0);
  await page.getByTestId('sketch-cancel').click();
  await expect(page.getByTestId('sketch-panel')).toHaveCount(0);
});
