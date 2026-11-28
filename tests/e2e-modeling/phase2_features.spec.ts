// Phase 2 operations end to end (docs/ui/PART_MODELING_UI.md, "Операции фазы 2"): the user gives
// a sweep along an open path, a loft through two sections, a rib from an open contour, a tapped
// hole on a picked face, a new body and a boolean between bodies — only through the operation
// panels built from the kernel catalog. The volume in the properties panel is checked against a
// formula written out here; undo and redo (buttons and Ctrl+Z / Ctrl+Y) restore the volumes.
import { expect, test, Page } from '@playwright/test';

interface ModelerState {
  status: string;
  error: { code: string; message: string } | null;
  features: { id: string; type: string; label: string; state: string; messages: string[] }[];
  doc: { canUndo: boolean; canRedo: boolean; inTransaction: boolean } | null;
  properties: { volume: number | null; mass: number | null } | null;
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

/** Volume in the properties panel against `expected` (relative tolerance `tol`); null: no body. */
async function expectVolume(page: Page, expected: number | null, tol = 1e-9) {
  await expect
    .poll(async () => {
      const v = (await state(page)).properties?.volume ?? null;
      if (expected === null) return v === null ? 0 : Infinity;
      return v === null ? Infinity : Math.abs(v - expected) / expected;
    })
    .toBeLessThanOrEqual(tol);
  if (expected !== null) {
    const shown = Number(await page.getByTestId('prop-volume').getAttribute('data-value'));
    expect(Math.abs(shown - expected) / expected).toBeLessThan(Math.max(tol, 1e-6));
  }
}

async function clickModelPoint(page: Page, x: number, y: number, z: number) {
  const p = await page.evaluate(([a, b, c]) => window.bendcorePartModeler!.worldToClient(a, b, c), [x, y, z]);
  await page.mouse.click(p.x, p.y);
}

async function openPanel(page: Page, type: string) {
  await page.getByTestId(`add-${type}`).click();
  await expect(page.getByTestId('feature-panel')).toHaveAttribute('data-type', type);
}

/** Preview (the node must build), then OK; returns the id of the new node. */
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

async function circleSketch(page: Page, cx: number, cy: number, r: number, offset = 0) {
  await openPanel(page, 'Sketch');
  await page.getByTestId('sketch-kind').selectOption('circle');
  await fill(page, { 'sketch-cx': cx, 'sketch-cy': cy, 'sketch-radius': r, 'param-offset': offset });
  return previewAndOk(page);
}

/** Open path sketch in the plane `plane` (lines "x; y", arcs "x; y; cx; cy; cw|ccw"). */
async function pathSketch(page: Page, plane: string, offset: number, path: string) {
  await openPanel(page, 'Sketch');
  await page.getByTestId('option-plane').selectOption(plane);
  await page.getByTestId('sketch-kind').selectOption('path');
  // The open-chain template switches the sketch to an open contour.
  await expect(page.getByTestId('option-contour')).toHaveValue('open');
  await fill(page, { 'sketch-path': path, 'param-offset': offset });
  return previewAndOk(page);
}

/** Block length × width × height from the origin: rectangle sketch on XY, extrusion. */
async function box(page: Page, length: number, width: number, height: number) {
  await openPanel(page, 'Sketch');
  await fill(page, { 'sketch-x': 0, 'sketch-y': 0, 'sketch-width': length, 'sketch-height': width });
  const sketch = await previewAndOk(page);
  await openPanel(page, 'Extrude');
  await expect(page.getByTestId('link-sketch')).toHaveValue(sketch);
  await fill(page, { 'param-distance': height });
  return previewAndOk(page);
}

async function undoRedo(page: Page, before: number | null, after: number, tol = 1e-9) {
  await page.getByTestId('undo').click();
  await expectVolume(page, before, tol);
  await page.getByTestId('redo').click();
  await expectVolume(page, after, tol);
  // The same through the keyboard (outside input fields, panel closed).
  await page.locator('body').click({ position: { x: 5, y: 840 } });
  await page.keyboard.press('Control+z');
  await expectVolume(page, before, tol);
  await page.keyboard.press('Control+y');
  await expectVolume(page, after, tol);
}

test.beforeEach(async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
});

test('sweep of a circle along an open path of a line and an arc', async ({ page }) => {
  const r = 5;
  const L1 = 30;
  const Rb = 20;
  // Pappus: section area times the length of the path through its centre.
  const tube = (radius: number) => Math.PI * radius * radius * (L1 + (Math.PI / 2) * Rb);
  const profile = await circleSketch(page, 0, 0, r);
  // XZ plane: (u, v) -> (u, 0, v); up L1 from the profile centre, then a clockwise quarter arc.
  const path = await pathSketch(page, 'XZ', 0, `0; 0\n0; ${L1}\n${Rb}; ${L1 + Rb}; ${Rb}; ${L1}; cw`);

  await openPanel(page, 'Sweep');
  await page.getByTestId('link-sketch').selectOption(profile);
  await page.getByTestId('link-pathSketch').selectOption(path);
  const sweep = await previewAndOk(page);
  await expectVolume(page, tube(r), 1e-6);
  const s = await state(page);
  expect(s.features.map((f) => [f.type, f.state])).toEqual([
    ['Sketch', 'ok'],
    ['Sketch', 'ok'],
    ['Sweep', 'ok'],
  ]);
  expect(s.features[2].id).toBe(sweep);

  // The profile radius 5 -> 6 from the tree: the sweep follows.
  await page.getByTestId(`tree-menu-${profile}`).click();
  await page.getByTestId('action-edit').click();
  const radius = page.locator('[data-testid^="dim-c"]').first();
  await expect(radius).toHaveValue(String(r));
  await radius.fill('6');
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
  await expectVolume(page, tube(6), 1e-6);
  await undoRedo(page, tube(r), tube(6), 1e-6);
});

test('loft through two circular sections (ruled): a cone frustum', async ({ page }) => {
  const R1 = 20;
  const R2 = 10;
  const h = 30;
  const frustum = (Math.PI * h * (R1 * R1 + R1 * R2 + R2 * R2)) / 3;
  const bottom = await circleSketch(page, 0, 0, R1);
  const top = await circleSketch(page, 0, 0, R2, h);

  await openPanel(page, 'Loft');
  await page.getByTestId('link-section1').selectOption(bottom);
  await page.getByTestId('link-section2').selectOption(top);
  await page.getByTestId('option-surface').selectOption('ruled');
  await previewAndOk(page);
  await expectVolume(page, frustum);
  await undoRedo(page, null, frustum);
});

test('rib from an open contour and a tapped hole on a picked face', async ({ page }) => {
  // Angle bracket: base 40 × 20 × 5, wall 5 × 20 × 40.
  await box(page, 40, 20, 5);
  await expectVolume(page, 4000);
  await box(page, 5, 20, 40);
  const bracket = 40 * 20 * 5 + 5 * 20 * 35;
  await expectVolume(page, bracket);

  // Rib contour in the middle plane y = 10 (XZ offset −10) from the wall (5, 25) to the base
  // (22, 5): a triangle with legs 17 and 20, thickness 2.
  const contour = await pathSketch(page, 'XZ', -10, '5; 25\n22; 5');
  await openPanel(page, 'Rib');
  await expect(page.getByTestId('link-sketch')).toHaveValue(contour);
  await fill(page, { 'param-thickness': 2 });
  await previewAndOk(page);
  const ribbed = bracket + ((17 * 20) / 2) * 2;
  await expectVolume(page, ribbed);

  // Tapped hole M8 through the base: the face slot is active on opening; pick the top face.
  await openPanel(page, 'ThreadedHole');
  expect((await state(page)).panel!.activeSlot).toBe('face');
  await clickModelPoint(page, 34, 15, 5);
  await expect.poll(async () => (await state(page)).panel!.picks.face.length).toBe(1);
  await fill(page, { 'param-nominal': 8, 'param-threadLength': 0, 'param-x': 32, 'param-y': 10, 'param-z': 5 });
  await page.getByTestId('option-end').selectOption('through');
  await previewAndOk(page);
  const drill = 8 - 1.25; // M8 coarse: D − P
  const tapped = ribbed - (Math.PI * drill * drill * 5) / 4;
  await expectVolume(page, tapped);
  await undoRedo(page, ribbed, tapped);
});

test('new body by extrusion and a boolean subtraction between bodies', async ({ page }) => {
  await box(page, 40, 30, 10);
  await circleSketch(page, 20, 15, 5);
  await openPanel(page, 'Extrude');
  await page.getByTestId('option-mode').selectOption('newBody');
  await fill(page, { 'param-distance': 30 });
  await previewAndOk(page);
  // Two overlapping bodies: the properties sum their volumes.
  const twoBodies = 40 * 30 * 10 + Math.PI * 25 * 30;
  await expectVolume(page, twoBodies);

  await openPanel(page, 'BodyBoolean');
  expect((await state(page)).panel!.activeSlot).toBe('target');
  await clickModelPoint(page, 5, 5, 10); // top face of the plate
  await expect.poll(async () => (await state(page)).panel!.picks.target.length).toBe(1);
  await page.getByTestId('slot-pick-tools').click();
  const a = -Math.PI / 4; // side of the post facing the camera
  await clickModelPoint(page, 20 + 5 * Math.cos(a), 15 + 5 * Math.sin(a), 22);
  await expect.poll(async () => (await state(page)).panel!.picks.tools.length).toBe(1);
  await page.getByTestId('option-operation').selectOption('subtract');
  await previewAndOk(page);
  // The plate loses the overlap πr²·10; the tool body is consumed.
  const plate = 40 * 30 * 10 - Math.PI * 25 * 10;
  await expectVolume(page, plate);
  await undoRedo(page, twoBodies, plate);
});
