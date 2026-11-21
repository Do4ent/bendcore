// Material from the library, measurements and the section in the part page
// (docs/ui/PART_MODELING_UI.md, "Материал"; docs/ui/KERNEL_VIEWPORT.md, "Измерения и сечение").
// A 60 x 40 x 20 block is built through the operation panels; the numbers are compared with the
// formulas written out here: mass = V * 1e-9 * density, angle between the top and the front face
// 90 degrees, area of the top 60 * 40, length of the front bottom edge 60, section by z = 10 is
// 60 * 40.
import { expect, test, Page } from '@playwright/test';
import fs from 'fs';

interface ToolsState {
  measureActive: boolean;
  picks: { kind: string; index: number }[];
  result: { type: string; unit: string; value: number; diameter?: number } | null;
  section: { enabled: boolean; axis: string; fraction: number };
  sectionPosition: number | null;
  sectionArea: number | null;
}
interface ModelerState {
  status: string;
  error: { code: string; message: string } | null;
  properties: { volume: number | null; mass: number | null } | null;
  material: { id: string; density: number | null; libraryCount: number };
  tools: ToolsState;
}

declare global {
  interface Window {
    bendcorePartModeler?: {
      getState(): ModelerState;
      idle(): Promise<void>;
      worldToClient(x: number, y: number, z: number): { x: number; y: number };
    };
  }
}

const state = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcorePartModeler!.idle();
    return window.bendcorePartModeler!.getState();
  });

async function clickModelPoint(page: Page, x: number, y: number, z: number) {
  const p = await page.evaluate(([a, b, c]) => window.bendcorePartModeler!.worldToClient(a, b, c), [x, y, z]);
  await page.mouse.click(p.x, p.y);
}

async function openPanel(page: Page, type: string) {
  await page.getByTestId(`add-${type}`).click();
  await expect(page.getByTestId('feature-panel')).toHaveAttribute('data-type', type);
}

async function previewAndOk(page: Page): Promise<string> {
  await page.getByTestId('panel-preview').click();
  await expect(page.getByTestId('panel-state')).toHaveAttribute('data-state', 'ok');
  const id = (await state(page)).panel!.featureId!;
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
  return id;
}

async function block(page: Page) {
  await openPanel(page, 'Sketch');
  for (const [id, v] of Object.entries({ 'sketch-x': 0, 'sketch-y': 0, 'sketch-width': 60, 'sketch-height': 40 })) {
    await page.getByTestId(id).fill(String(v));
  }
  const sketch = await previewAndOk(page);
  await openPanel(page, 'Extrude');
  await expect(page.getByTestId('link-sketch')).toHaveValue(sketch);
  await page.getByTestId('param-distance').fill('20');
  await previewAndOk(page);
  await expect.poll(async () => (await state(page)).properties?.volume).toBeCloseTo(48000, 3);
}

test.beforeEach(async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await block(page);
});

test('a library material sets the density and the mass; the part file keeps it', async ({ page }) => {
  expect((await state(page)).material.libraryCount).toBe(21);
  await page.getByTestId('material').selectOption('al_6082_t6');
  await expect(page.getByTestId('density')).toHaveValue('2700');
  await expect.poll(async () => (await state(page)).properties?.mass).toBeCloseTo(48000e-9 * 2700, 9);
  expect((await state(page)).material).toMatchObject({ id: 'al_6082_t6', density: 2700 });

  const download = page.waitForEvent('download');
  await page.getByTestId('doc-save').click();
  const file = await (await download).path();
  const text = fs.readFileSync(file, 'utf8');
  expect(JSON.parse(text).material).toEqual({ id: 'al_6082_t6', density: 2700 });

  // A hand-typed density leaves the library; reopening the file restores the material.
  await page.getByTestId('density').fill('1000');
  await page.getByTestId('density').blur();
  await expect.poll(async () => (await state(page)).material.id).toBe('custom');
  await expect.poll(async () => (await state(page)).properties?.mass).toBeCloseTo(48000e-9 * 1000, 9);
  await page.getByTestId('doc-open').setInputFiles(file);
  await expect.poll(async () => (await state(page)).material).toMatchObject({ id: 'al_6082_t6', density: 2700 });
  await expect(page.getByTestId('material')).toHaveValue('al_6082_t6');
  await expect.poll(async () => (await state(page)).properties?.mass).toBeCloseTo(48000e-9 * 2700, 9);
});

test('angle between two faces, area of a face and length of an edge', async ({ page }) => {
  await page.getByTestId('measure-toggle').click();
  // Angle: the top face and the front face (y = 0).
  await page.getByTestId('measure-type').selectOption('angle');
  await clickModelPoint(page, 30, 20, 20);
  await expect.poll(async () => (await state(page)).tools.picks.length).toBe(1);
  await clickModelPoint(page, 30, 0, 10);
  await expect(page.getByTestId('measure-result')).toHaveAttribute('data-type', 'angle');
  const angle = (await state(page)).tools.result!;
  expect(angle.unit).toBe('deg');
  expect(angle.value).toBeCloseTo(90, 6);
  await expect(page.getByTestId('measure-value')).toHaveText('90°');
  await expect(page.getByTestId('measure-label')).toHaveAttribute('data-label', '90°');

  // Area of the top face.
  await page.getByTestId('measure-type').selectOption('area');
  await clickModelPoint(page, 30, 20, 20);
  await expect(page.getByTestId('measure-result')).toHaveAttribute('data-type', 'area');
  expect((await state(page)).tools.result!.value).toBeCloseTo(2400, 6);

  // Length of the front bottom edge.
  await page.getByTestId('measure-type').selectOption('length');
  await clickModelPoint(page, 30, 0, 0);
  await expect(page.getByTestId('measure-result')).toHaveAttribute('data-type', 'length');
  expect((await state(page)).tools.result!.value).toBeCloseTo(60, 6);

  // Distance between the bottom front edge picked twice is removed (toggle), then end measuring.
  await page.getByTestId('measure-toggle').click();
  await expect(page.getByTestId('measure-result')).toHaveCount(0);
  await expect(page.getByTestId('measure-label')).toHaveCount(0);
});

test('distance between two faces of the block that face away is the thickness', async ({ page }) => {
  // The top (z = 20) and the front (y = 0) faces touch: distance 0; the top and the bottom edge
  // of the front face are 20 apart, picked as two edges.
  await page.getByTestId('measure-toggle').click();
  await page.getByTestId('measure-kind-edge').click();
  await clickModelPoint(page, 30, 0, 0);
  await clickModelPoint(page, 30, 0, 20);
  await expect(page.getByTestId('measure-result')).toHaveAttribute('data-type', 'distance');
  expect((await state(page)).tools.result!.value).toBeCloseTo(20, 6);
});

test('the section plane with a slider clips the model and reports the section area', async ({ page }) => {
  await page.getByTestId('section-toggle').click();
  await page.getByTestId('section-axis').selectOption('Z');
  await expect(page.getByTestId('kernel-viewport')).toHaveAttribute('data-clipped', 'true');
  await page.getByTestId('section-slider').fill('500');
  await expect.poll(async () => (await state(page)).tools.sectionPosition).toBeCloseTo(10, 6);
  await expect.poll(async () => (await state(page)).tools.sectionArea).toBeCloseTo(2400, 6);
  await expect(page.getByTestId('kernel-viewport')).not.toHaveAttribute('data-section-lines', '0');

  // x = 15 (a quarter of the length): the section is 40 x 20.
  await page.getByTestId('section-axis').selectOption('X');
  await page.getByTestId('section-slider').fill('250');
  await expect.poll(async () => (await state(page)).tools.sectionPosition).toBeCloseTo(15, 6);
  await expect.poll(async () => (await state(page)).tools.sectionArea).toBeCloseTo(800, 6);

  await page.getByTestId('section-toggle').click();
  await expect(page.getByTestId('kernel-viewport')).toHaveAttribute('data-clipped', 'false');
  await expect(page.getByTestId('section-panel').getByTestId('section-slider')).toHaveCount(0);
});
