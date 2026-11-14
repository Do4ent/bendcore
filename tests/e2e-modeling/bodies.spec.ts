// Multi-body operations end to end (docs/ui/PART_MODELING_UI.md, "Операции над телами"): the user
// repeats a body by a linear and a circular pattern, mirrors one body, deletes another and sweeps
// a profile along an edge of the model — only through the operation panels of the kernel catalog
// and picks in the viewport. The number of bodies and the volume in the properties are checked
// against formulas written out here; undo and redo return the previous counts and volumes.
import { expect, test, Page } from '@playwright/test';

interface ModelerState {
  status: string;
  error: { code: string; message: string } | null;
  features: { id: string; type: string; state: string; messages: string[] }[];
  properties: { volume: number | null; counts: { solids: number } } | null;
  panel: { type: string; featureId: string | null; activeSlot: string | null; picks: Record<string, string[]> } | null;
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

/** Number of bodies and volume against the formula (relative tolerance tol). */
async function expectBodies(page: Page, solids: number, volume: number, tol = 1e-9) {
  await expect
    .poll(async () => {
      const p = (await state(page)).properties;
      if (!p || p.volume === null) return `no body`;
      const rel = Math.abs(p.volume - volume) / volume;
      return rel <= tol ? `${p.counts.solids}` : `${p.counts.solids} bodies, volume ${p.volume}`;
    })
    .toBe(`${solids}`);
}

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

async function fill(page: Page, fields: Record<string, string | number>) {
  for (const [testId, value] of Object.entries(fields)) await page.getByTestId(testId).fill(String(value));
}

/** Block from (x, y, 0): rectangle sketch on XY, extrusion. */
async function box(page: Page, x: number, y: number, length: number, width: number, height: number) {
  await openPanel(page, 'Sketch');
  await fill(page, { 'sketch-x': x, 'sketch-y': y, 'sketch-width': length, 'sketch-height': width });
  const sketch = await previewAndOk(page);
  await openPanel(page, 'Extrude');
  await expect(page.getByTestId('link-sketch')).toHaveValue(sketch);
  await fill(page, { 'param-distance': height });
  return previewAndOk(page);
}

/** "Show all" (F, outside input fields): the copies may lie outside the view framed earlier. */
async function showAll(page: Page) {
  await page.locator('body').click({ position: { x: 5, y: 840 } });
  await page.keyboard.press('f');
}

/** Picks a body of the active "bodies" slot by a point of its top face. */
async function pickBody(page: Page, x: number, y: number, z: number, count: number) {
  expect((await state(page)).panel!.activeSlot).toBe('bodies');
  await showAll(page);
  await clickModelPoint(page, x, y, z);
  await expect.poll(async () => (await state(page)).panel!.picks.bodies.length).toBe(count);
}

async function undoRedo(page: Page, before: [number, number], after: [number, number]) {
  await page.getByTestId('undo').click();
  await expectBodies(page, ...before);
  await page.getByTestId('redo').click();
  await expectBodies(page, ...after);
}

test.beforeEach(async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
});

test('linear pattern of a body, mirror of one copy, deletion of another', async ({ page }) => {
  const block = 20 * 10 * 10;
  await box(page, 0, 0, 20, 10, 10);
  await expectBodies(page, 1, block);

  // Three bodies along X, 30 apart: x in [0, 20], [30, 50], [60, 80].
  await openPanel(page, 'BodyLinearPattern');
  await pickBody(page, 10, 5, 10, 1);
  await expect(page.getByTestId('option-direction1')).toHaveValue('X');
  await fill(page, { 'param-count1': 3, 'param-spacing1': 30 });
  await previewAndOk(page);
  await expectBodies(page, 3, 3 * block);
  await undoRedo(page, [1, block], [3, 3 * block]);

  // Mirror of the last copy about YZ: a fourth body in x in [-80, -60].
  await openPanel(page, 'BodyMirror');
  await pickBody(page, 70, 5, 10, 1);
  await expect(page.getByTestId('option-plane')).toHaveValue('YZ');
  await previewAndOk(page);
  await expectBodies(page, 4, 4 * block);
  await undoRedo(page, [3, 3 * block], [4, 4 * block]);

  // Delete the middle copy.
  await openPanel(page, 'DeleteBody');
  await pickBody(page, 40, 5, 10, 1);
  await previewAndOk(page);
  await expectBodies(page, 3, 3 * block);
  await undoRedo(page, [4, 4 * block], [3, 3 * block]);
  expect((await state(page)).features.map((f) => f.state)).toEqual(['ok', 'ok', 'ok', 'ok', 'ok']);

  // Undo everything back to the single block and redo it all.
  for (let i = 0; i < 3; i++) await page.getByTestId('undo').click();
  await expectBodies(page, 1, block);
  for (let i = 0; i < 3; i++) await page.getByTestId('redo').click();
  await expectBodies(page, 3, 3 * block);
  expect((await state(page)).error).toBeNull();
});

test('circular pattern of a body about Z', async ({ page }) => {
  const block = 10 * 10 * 10;
  await box(page, 20, 0, 10, 10, 10);
  await openPanel(page, 'BodyCircularPattern');
  await pickBody(page, 25, 5, 10, 1);
  await expect(page.getByTestId('option-axis')).toHaveValue('Z');
  await fill(page, { 'param-count': 4, 'param-angle': 360 });
  await previewAndOk(page);
  // Four copies 90° apart, none overlapping (the block lies at x >= 20).
  await expectBodies(page, 4, 4 * block);
  await undoRedo(page, [1, block], [4, 4 * block]);
});

test('sweep along an edge of the model as a new body', async ({ page }) => {
  const r = 2;
  const L = 40;
  await box(page, 0, 0, L, 30, 10);
  // Profile: circle r on YZ around the start of the bottom front edge (0, 0, 0) -> (L, 0, 0).
  await openPanel(page, 'Sketch');
  await page.getByTestId('option-plane').selectOption('YZ');
  await page.getByTestId('sketch-kind').selectOption('circle');
  await fill(page, { 'sketch-cx': 0, 'sketch-cy': 0, 'sketch-radius': r });
  const profile = await previewAndOk(page);

  await openPanel(page, 'Sweep');
  await expect(page.getByTestId('link-sketch')).toHaveValue(profile);
  await page.getByTestId('option-path').selectOption('edges');
  await page.getByTestId('option-mode').selectOption('newBody');
  await page.getByTestId('slot-pick-pathEdges').click();
  await clickModelPoint(page, L / 2, 0, 0);
  await expect.poll(async () => (await state(page)).panel!.picks.pathEdges.length).toBe(1);
  await previewAndOk(page);
  // The tube is a second body: the properties sum the volumes of the bodies.
  const block = L * 30 * 10;
  const tube = Math.PI * r * r * L;
  await expectBodies(page, 2, block + tube, 1e-6);
  await undoRedo(page, [1, block], [2, block + tube]);
});
