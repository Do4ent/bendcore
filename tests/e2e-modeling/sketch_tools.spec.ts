// Sketch editing tools end to end (docs/ui/SKETCH_EDITOR.md, "Редактирование", "Проекция
// рёбер"): an outward offset of a dimensioned rectangle with rounded corners is fully defined by
// one offset dimension and its ring has the area 2d(W + H) + πd²; changing the dimension gives
// the area for the new value. An ellipse crossed by a line is trimmed to an elliptical arc; a
// circular pattern puts the copies at the polar positions of the formula. A sketch on the top
// face of a block projects that face's edges; the extrusion of the projection follows the block
// when its sketch is widened. State is read through window.bendcoreSketchEditor and
// window.bendcorePartModeler; clicks go to sketch coordinates through sketchToClient.
import { expect, test, Page } from '@playwright/test';

interface EditorState {
  definition: string;
  dof: number;
  constraints: { type: string; a: number; b: number; c: number; value: string; driven: boolean; measured: number; state: string }[];
  points: { x: number; y: number; fixed: boolean; defined: boolean }[];
  curves: { entity: number; type: string; defined: boolean; projection: number }[];
  regions: { index: number; area: number; selected: boolean }[] | null;
  message: string | null;
}

declare global {
  interface Window {
    bendcoreSketchEditor?: { getState(): EditorState; sketchToClient(x: number, y: number): { x: number; y: number } };
    bendcorePartModeler?: {
      getState(): {
        features: { id: string; type: string; state: string; messages: string[] }[];
        properties: { volume: number | null } | null;
      };
      idle(): Promise<void>;
      worldToClient(x: number, y: number, z: number): { x: number; y: number };
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

async function click(page: Page, x: number, y: number, shift = false) {
  const p = await page.evaluate(([a, b]) => window.bendcoreSketchEditor!.sketchToClient(a, b), [x, y]);
  await page.mouse.move(p.x, p.y);
  if (shift) await page.keyboard.down('Shift');
  await page.mouse.down();
  await page.mouse.up();
  if (shift) await page.keyboard.up('Shift');
}

async function dimension(page: Page, picks: [number, number][], text: [number, number], value: string) {
  for (const [x, y] of picks) await click(page, x, y);
  await click(page, text[0], text[1]);
  const input = page.getByTestId('dim-input');
  await expect(input).toBeVisible();
  await input.fill(value);
  await input.press('Enter');
  await expect(input).toHaveCount(0);
}

async function expectVolume(page: Page, expected: number) {
  await expect
    .poll(async () => {
      const v = (await modeler(page)).properties?.volume;
      return v === null || v === undefined ? Infinity : Math.abs(v - expected) / expected;
    })
    .toBeLessThan(1e-7);
}

/** Rectangle from the origin, dimensioned W x H (fully defined). */
async function rectangle(page: Page, W: number, H: number) {
  await page.getByTestId('tool-rectangle').click();
  await click(page, 0.2, 0.1);
  await click(page, 50, 30);
  await page.getByTestId('tool-dimension').click();
  await dimension(page, [[25, 0]], [25, -8], String(W));
  await page.getByTestId('sketch-fit').click();
  await dimension(page, [[W, 15]], [W + 8, 15], String(H));
  expect((await editor(page)).definition).toBe('fully-defined');
  await page.getByTestId('sketch-fit').click();
}

test.beforeEach(async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
});

test('offset with rounded corners: one dimension, ring area by the formula; trim of an ellipse; circular pattern', async ({ page }) => {
  const W = 60;
  const H = 40;
  const ring = (d: number) => 2 * d * (W + H) + Math.PI * d * d;
  await page.getByTestId('sketch-draw').click();
  await expect(page.getByTestId('sketch-panel')).toBeVisible();
  await rectangle(page, W, H);

  // Select the four sides, offset by -5 (outward: the rectangle runs counter-clockwise), round corners.
  await page.getByTestId('tool-select').click();
  await click(page, W / 2, 0);
  for (const [x, y] of [[W, H / 2], [W / 2, H], [0, H / 2]]) await click(page, x, y, true);
  await page.getByTestId('sketch-param-offset').fill('-5');
  await page.getByTestId('sketch-offset-corners').selectOption('round');
  await page.getByTestId('sketch-offset').click();
  let s = await editor(page);
  expect(s.message).toBeNull();
  expect(s.curves.filter((c) => c.type === 'arc')).toHaveLength(4);
  expect(s.curves).toHaveLength(12);
  expect(s.definition).toBe('fully-defined');
  const areas = () => s.regions!.map((r) => r.area).sort((a, b) => a - b);
  expect(Math.abs(areas()[0] - ring(5))).toBeLessThan(1e-6);
  expect(Math.abs(areas()[1] - W * H)).toBeLessThan(1e-6);
  const master = s.constraints.findIndex((c) => c.type === 'offset-distance' && c.c < 1) + 1;
  expect(master).toBeGreaterThan(0);
  expect(s.constraints.filter((c) => c.type === 'offset-distance' && c.c < 1)).toHaveLength(1);
  await expect(page.getByTestId(`sketch-dim-${master}`)).toHaveText('5');

  // The offset dimension 5 -> 8: the ring follows.
  await page.getByTestId(`sketch-dim-${master}`).dblclick();
  await page.getByTestId('dim-input').fill('8');
  await page.getByTestId('dim-input').press('Enter');
  await expect(page.getByTestId('dim-input')).toHaveCount(0);
  s = await editor(page);
  expect(s.definition).toBe('fully-defined');
  expect(Math.abs(areas()[0] - ring(8))).toBeLessThan(1e-6);

  await page.getByTestId('sketch-cancel').click();
  await expect(page.getByTestId('sketch-panel')).toHaveCount(0);

  // New sketch: ellipse a = 20, b = 10 at (-30, 10), crossed by the line y = 15; trim its upper part.
  await page.getByTestId('sketch-draw').click();
  await expect(page.getByTestId('sketch-panel')).toBeVisible();
  await page.getByTestId('tool-ellipse').click();
  await click(page, -30, 10);
  await click(page, -10, 10);
  await click(page, -30, 20);
  await page.getByTestId('tool-line').click();
  await click(page, -55, 15);
  await click(page, -5, 15);
  await page.keyboard.press('Escape');
  s = await editor(page);
  expect(s.curves.map((c) => c.type).sort()).toEqual(['ellipse', 'line']);
  await page.getByTestId('tool-trim').click();
  await click(page, -30, 20);
  s = await editor(page);
  expect(s.message).toBeNull();
  expect(s.curves.map((c) => c.type).sort()).toEqual(['elliptical-arc', 'line']);
  // The new ends lie on y = 15 at x = -30 ± 20·√(1 − 1/4).
  const ends = s.points.filter((p) => Math.abs(p.y - 15) < 1e-6 && Math.abs(Math.abs(p.x + 30) - 20 * Math.sqrt(0.75)) < 1e-6);
  expect(ends).toHaveLength(2);

  // Circular pattern: a circle at (50, 0) around a point at (30, 0), 6 copies at 60°.
  await page.getByTestId('tool-point').click();
  await click(page, 30, 0);
  await page.getByTestId('tool-circle').click();
  await click(page, 50, 0);
  await click(page, 53, 0);
  await page.getByTestId('tool-select').click();
  await click(page, 50, 3);
  await click(page, 30, 0, true);
  await page.getByTestId('sketch-param-count').fill('6');
  await page.getByTestId('sketch-param-angle').fill('60');
  await page.getByTestId('sketch-circular-pattern').click();
  s = await editor(page);
  expect(s.message).toBeNull();
  expect(s.curves.filter((c) => c.type === 'circle')).toHaveLength(6);
  const o = s.points.find((p) => Math.hypot(p.x - 30, p.y) < 1e-6)!;
  const c0 = s.points.find((p) => Math.hypot(p.x - 50, p.y) < 1e-6)!;
  const r = Math.hypot(c0.x - o.x, c0.y - o.y);
  for (let k = 1; k < 6; ++k) {
    const ang = (k * 60 * Math.PI) / 180;
    const x = o.x + r * Math.cos(ang);
    const y = o.y + r * Math.sin(ang);
    expect(s.points.some((p) => Math.hypot(p.x - x, p.y - y) < 1e-9)).toBe(true);
  }
  await page.getByTestId('sketch-cancel').click();
  await expect(page.getByTestId('sketch-panel')).toHaveCount(0);
});

test('projection of a face of the model follows the model', async ({ page }) => {
  // Block 60 x 40 x 10 from a dimensioned sketch.
  await page.getByTestId('add-Sketch').click();
  await page.getByTestId('sketch-width').fill('60');
  await page.getByTestId('sketch-height').fill('40');
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
  await page.getByTestId('add-Extrude').click();
  await page.getByTestId('param-distance').fill('10');
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
  await expectVolume(page, 60 * 40 * 10);
  const baseSketch = (await modeler(page)).features[0].id;

  // Sketch on the top face (picked in the viewport); project the face by a click inside it.
  const top = await page.evaluate(() => window.bendcorePartModeler!.worldToClient(30, 20, 10));
  await page.mouse.click(top.x, top.y);
  await page.getByTestId('sketch-plane').selectOption('face');
  await page.getByTestId('sketch-draw').click();
  await expect(page.getByTestId('sketch-panel')).toBeVisible();
  await page.getByTestId('tool-project').click();
  // The face frame: X along the global X, origin at the global origin projected on the face, so
  // the middle of the face is (30, 20); the other quadrants guard against a reversed axis.
  await page.getByTestId('sketch-fit').click();
  let s = await editor(page);
  for (const [x, y] of [[30, 20], [-30, 20], [30, -20], [-30, -20]]) {
    await click(page, x, y);
    s = await editor(page);
    if (s.curves.length > 0) break;
  }
  expect(s.message).toContain('4');
  expect(s.curves).toHaveLength(4);
  expect(s.curves.every((c) => c.type === 'line' && c.projection >= 0)).toBe(true);
  expect(s.definition).toBe('fully-defined');
  expect(Math.abs(s.regions![0].area - 60 * 40)).toBeLessThan(1e-6);
  await page.getByTestId('sketch-exit').click();
  await expect(page.getByTestId('sketch-panel')).toHaveCount(0);
  const projSketch = (await modeler(page)).features[2].id;
  await page.getByTestId('add-Extrude').click();
  await page.getByTestId('link-sketch').selectOption(projSketch);
  await page.getByTestId('param-distance').fill('5');
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
  await expectVolume(page, 60 * 40 * 15);

  // Widen the block's sketch 60 -> 80: the projection is recomputed, the extrusion follows.
  await page.getByTestId(`tree-menu-${baseSketch}`).click();
  await page.getByTestId('action-edit').click();
  const widthField = page.locator('[data-testid^="dim-c"]').first();
  await expect(widthField).toHaveValue('60');
  await widthField.fill('80');
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
  await expectVolume(page, 80 * 40 * 15);
  expect((await modeler(page)).features.map((f) => f.state)).toEqual(['ok', 'ok', 'ok', 'ok']);
});
