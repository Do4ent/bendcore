// Sketch editor end to end (docs/ui/SKETCH_EDITOR.md): the user draws a rectangle from the
// origin, dimensions it, rounds its four corners with the corner fillet tool, draws a circle
// inside and dimensions it until the sketch is fully defined (status "Полностью определён",
// 0 degrees of freedom). A conflicting dimension is highlighted and listed; turned into a
// reference dimension it measures instead. Leaving the sketch rebuilds the part; the extrusion
// has the volume (W·H − (4 − π)·r² − π·d²/4)·D; changing the width in the sketch gives the
// volume for the new width. State is read through window.bendcoreSketchEditor and
// window.bendcorePartModeler; clicks go to sketch coordinates through sketchToClient.
import { expect, test, Page } from '@playwright/test';
import fs from 'fs';

interface EditorState {
  definition: string;
  dof: number;
  conflicting: number[];
  redundant: number[];
  constraints: { type: string; a: number; b: number; value: string; driven: boolean; measured: number; state: string }[];
  points: { x: number; y: number; fixed: boolean; defined: boolean }[];
  curves: { entity: number; type: string; defined: boolean }[];
  regions: { index: number; area: number; selected: boolean }[] | null;
  message: string | null;
  pendingDimension: boolean;
}

declare global {
  interface Window {
    bendcoreSketchEditor?: { getState(): EditorState; sketchToClient(x: number, y: number): { x: number; y: number } };
    bendcorePartModeler?: {
      getState(): {
        status: string;
        error: { code: string; message: string } | null;
        features: { id: string; type: string; state: string; messages: string[] }[];
        properties: { volume: number | null } | null;
        sketchMode: { featureId: string } | null;
      };
    };
  }
}

const editor = (page: Page) => page.evaluate(() => window.bendcoreSketchEditor!.getState());
const modeler = (page: Page) => page.evaluate(() => window.bendcorePartModeler!.getState());

async function at(page: Page, x: number, y: number) {
  return page.evaluate(([a, b]) => window.bendcoreSketchEditor!.sketchToClient(a, b), [x, y]);
}

async function click(page: Page, x: number, y: number) {
  const p = await at(page, x, y);
  await page.mouse.move(p.x, p.y);
  await page.mouse.down();
  await page.mouse.up();
}

/** Dimension tool: picks, then the text place; types the value and confirms. */
async function dimension(page: Page, picks: [number, number][], text: [number, number], value: string) {
  for (const [x, y] of picks) await click(page, x, y);
  await click(page, text[0], text[1]);
  const input = page.getByTestId('dim-input');
  await expect(input).toBeVisible();
  await input.fill(value);
  await input.press('Enter');
  await expect(input).toHaveCount(0);
  expect((await editor(page)).message).toBeNull();
}

async function expectVolume(page: Page, expected: number) {
  await expect
    .poll(async () => {
      const v = (await modeler(page)).properties?.volume;
      return v === null || v === undefined ? Infinity : Math.abs(v - expected) / expected;
    })
    .toBeLessThan(1e-7);
}

test('rounded rectangle with a hole: dimensions to full definition, conflict, extrusion, size change', async ({ page }) => {
  const W = 60;
  const H = 40;
  const r = 5;
  const d = 16;
  const D = 10;
  const volume = (w: number) => (w * H - (4 - Math.PI) * r * r - (Math.PI * d * d) / 4) * D;

  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await page.getByTestId('sketch-draw').click();
  await expect(page.getByTestId('sketch-panel')).toBeVisible();

  // Rectangle from the origin (snap) to (50, 30): H/V constraints are added automatically.
  await page.getByTestId('tool-rectangle').click();
  const o = await at(page, 0.2, 0.1);
  await page.mouse.move(o.x, o.y);
  await expect(page.getByTestId('sketch-snap')).toHaveAttribute('data-kind', 'origin');
  await click(page, 0.2, 0.1);
  await click(page, 50, 30);
  let s = await editor(page);
  expect(s.curves.map((c) => c.type)).toEqual(['line', 'line', 'line', 'line']);
  expect(s.constraints.map((c) => c.type)).toEqual(['horizontal', 'vertical', 'horizontal', 'vertical']);
  expect(s.points[0]).toMatchObject({ x: 0, y: 0, fixed: true });
  expect(s.dof).toBe(2);
  await expect(page.getByTestId('sketch-status')).toHaveAttribute('data-definition', 'under-defined');
  await expect(page.getByTestId('sketch-curve-1')).toHaveAttribute('data-state', 'under');

  // Width and height: click the line, then the place of the text.
  await page.getByTestId('tool-dimension').click();
  await dimension(page, [[25, 0]], [25, -8], String(W));
  await page.getByTestId('sketch-fit').click();
  await dimension(page, [[W, 15]], [W + 8, 15], String(H));
  s = await editor(page);
  expect(s.dof).toBe(0);
  expect(s.definition).toBe('fully-defined');
  expect(s.points[2].x).toBeCloseTo(W, 9);
  expect(s.points[2].y).toBeCloseTo(H, 9);
  await page.getByTestId('sketch-fit').click();

  // Fillets R5 at the four corners (kernel corner fillet: tangent arcs, radius, virtual sharps).
  await page.getByTestId('tool-fillet').click();
  await page.getByTestId('sketch-param-filletRadius').fill(String(r));
  for (const [x, y] of [[W, 0], [W, H], [0, H], [0, 0]]) await click(page, x, y);
  s = await editor(page);
  expect(s.message).toBeNull();
  expect(s.curves.filter((c) => c.type === 'arc')).toHaveLength(4);
  expect(s.definition).toBe('fully-defined');

  // Circle inside, then its diameter and its position from the origin corner (X above, Y beside).
  await page.getByTestId('tool-circle').click();
  await click(page, 27, 22);
  await click(page, 33, 22);
  s = await editor(page);
  expect(s.dof).toBe(3);
  await expect(page.getByTestId('sketch-status')).toHaveAttribute('data-definition', 'under-defined');
  const centre = s.points.length - 1;
  expect(s.points[centre].x).toBeCloseTo(27, 4);
  expect(s.points[centre].y).toBeCloseTo(22, 4);
  await page.getByTestId('tool-dimension').click();
  await dimension(page, [[27, 28]], [40, 36], String(d));
  await dimension(page, [[0, 0], [27, 22]], [13, H + 12], String(W / 2));
  await dimension(page, [[0, 0], [W / 2, 22]], [45, 10], String(H / 2));
  s = await editor(page);
  expect(s.dof).toBe(0);
  expect(s.definition).toBe('fully-defined');
  await expect(page.getByTestId('sketch-status')).toHaveAttribute('data-definition', 'fully-defined');
  await expect(page.getByTestId('sketch-status')).toContainText('Полностью определён');
  await expect(page.getByTestId('sketch-status')).toHaveAttribute('data-dof', '0');
  expect(s.points[centre].x).toBeCloseTo(W / 2, 9);
  expect(s.points[centre].y).toBeCloseTo(H / 2, 9);
  expect(s.curves.every((c) => c.defined)).toBe(true);
  expect(s.constraints.filter((c) => c.type === 'distance-x' || c.type === 'distance-y').map((c) => c.type)).toEqual(['distance-x', 'distance-y']);
  // Two regions: the plate with the hole (selected) and the disc.
  expect(s.regions!.map((g) => g.selected).sort()).toEqual([false, true]);
  const plate = s.regions!.find((g) => g.selected)!;
  expect(Math.abs(plate.area - volume(W) / D)).toBeLessThan(1e-6);

  const shot = process.env.BENDCORE_DOC_SCREENSHOT;
  if (shot) {
    await page.mouse.move(5, 5);
    await page.screenshot({ path: shot });
    expect(fs.statSync(shot).size).toBeGreaterThan(10_000);
  }

  // A second width 70 between the same corners conflicts: highlighted, listed.
  await dimension(page, [[0, 0], [W, 0]], [W / 2, -14], '70');
  s = await editor(page);
  const extra = s.constraints.length;
  expect(s.definition).toBe('over-defined');
  expect(s.conflicting).toContain(extra);
  expect(s.conflicting.length).toBeGreaterThanOrEqual(2);
  await expect(page.getByTestId('sketch-status')).toHaveAttribute('data-definition', 'over-defined');
  await expect(page.getByTestId(`sketch-dim-${extra}`)).toHaveAttribute('data-state', /sk-c-conflict/);
  await expect(page.getByTestId(`conflict-${extra}`)).toBeVisible();
  await expect(page.getByTestId(`constraint-row-${extra}`)).toHaveAttribute('data-state', 'conflict');
  // Its operands (the two corners) are drawn red.
  await expect(page.getByTestId('sketch-point-1')).toHaveAttribute('data-state', 'conflict');
  // Made a reference dimension: it measures 60 and the sketch is fully defined again.
  await page.getByTestId(`constraint-driven-${extra}`).click();
  await expect(page.getByTestId('sketch-status')).toHaveAttribute('data-definition', 'fully-defined');
  s = await editor(page);
  expect(s.constraints[extra - 1].driven).toBe(true);
  expect(s.constraints[extra - 1].measured).toBeCloseTo(W, 9);
  await expect(page.getByTestId(`sketch-dim-${extra}`)).toHaveText(`(${W})`);

  // Leave the sketch, extrude it.
  await page.getByTestId('sketch-exit').click();
  await expect(page.getByTestId('sketch-panel')).toHaveCount(0);
  const sketchId = (await modeler(page)).features[0].id;
  expect((await modeler(page)).features[0].state).toBe('ok');
  await page.getByTestId('add-Extrude').click();
  await expect(page.getByTestId('link-sketch')).toHaveValue(sketchId);
  await page.getByTestId('param-distance').fill(String(D));
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
  await expectVolume(page, volume(W));

  // Change the width in the sketch: double-click its text, new value; the part follows.
  await page.getByTestId(`tree-menu-${sketchId}`).click();
  await page.getByTestId('action-editSketch').click();
  await expect(page.getByTestId('sketch-panel')).toBeVisible();
  s = await editor(page);
  const width = s.constraints.findIndex((c) => c.type === 'distance' && c.value === String(W) && !c.driven) + 1;
  expect(width).toBeGreaterThan(0);
  await page.getByTestId(`sketch-dim-${width}`).dblclick();
  await page.getByTestId('dim-input').fill('80');
  await page.getByTestId('dim-input').press('Enter');
  await expect(page.getByTestId('dim-input')).toHaveCount(0);
  s = await editor(page);
  expect(s.definition).toBe('fully-defined');
  expect(s.constraints[extra - 1].measured).toBeCloseTo(80, 9);
  await page.getByTestId('sketch-exit').click();
  await expectVolume(page, volume(80));
  expect((await modeler(page)).features.map((f) => f.state)).toEqual(['ok', 'ok']);
  expect((await modeler(page)).error).toBeNull();
});

test('drawing tools, constraints panel, dragging, editing tools, regions, cancel', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await page.getByTestId('sketch-draw').click();
  await expect(page.getByTestId('sketch-panel')).toBeVisible();

  // Polyline of two segments: the second end snaps horizontally/vertically.
  await page.getByTestId('tool-line').click();
  await click(page, -40, -30);
  await click(page, -10, -30.3);
  await click(page, -10, -5);
  await page.keyboard.press('Escape');
  let s = await editor(page);
  expect(s.curves.map((c) => c.type)).toEqual(['line', 'line']);
  expect(s.constraints.map((c) => c.type)).toEqual(['horizontal', 'vertical']);

  // Every curve tool produces its curve.
  const draw = async (tool: string, picks: [number, number][]) => {
    await page.getByTestId(`tool-${tool}`).click();
    for (const [x, y] of picks) await click(page, x, y);
    if (tool === 'spline') await page.keyboard.press('Enter');
    const st = await editor(page);
    expect(st.message, tool).toBeNull();
    return st;
  };
  s = await draw('circle-3', [[20, 0], [30, 10], [40, 0]]);
  expect(s.curves.at(-1)!.type).toBe('circle');
  s = await draw('arc', [[0, 30], [10, 30], [0, 40]]);
  expect(s.curves.at(-1)!.type).toBe('arc');
  s = await draw('arc-3', [[20, 30], [40, 30], [30, 38]]);
  expect(s.curves.at(-1)!.type).toBe('arc');
  s = await draw('arc-tangent', [[-10, -5], [-20, 5]]);
  expect(s.curves.at(-1)!.type).toBe('arc');
  expect(s.constraints.at(-1)!.type).toBe('tangent');
  s = await draw('ellipse', [[-40, 30], [-25, 30], [-40, 36]]);
  expect(s.curves.at(-1)!.type).toBe('ellipse');
  s = await draw('spline', [[-50, -50], [-40, -40], [-30, -50], [-20, -40]]);
  expect(s.curves.at(-1)!.type).toBe('bspline');
  s = await draw('point', [[45, -40]]);
  const points = s.points.length;
  s = await draw('rectangle-center', [[30, -30], [38, -25]]);
  expect(s.points.length).toBe(points + 5);
  expect(s.constraints.at(-1)!.type).toBe('symmetric-point');

  // Constraints panel: two lines selected (Shift adds) offer parallel/perpendicular/angle/equal.
  await page.getByTestId('tool-select').click();
  await click(page, -25, -30);
  await page.keyboard.down('Shift');
  await click(page, -10, -20);
  await page.keyboard.up('Shift');
  await expect(page.getByTestId('sketch-selection')).toBeVisible();
  await expect(page.getByTestId('constraint-perpendicular')).toBeVisible();
  await expect(page.getByTestId('constraint-tangent')).toHaveCount(0);
  await page.getByTestId('constraint-equal').click();
  s = await editor(page);
  expect(s.constraints.at(-1)!.type).toBe('equal');
  expect(s.message).toBeNull();

  // Dragging the free end of the polyline: the kernel keeps the vertical side vertical.
  const before = s.points[2];
  const from = await at(page, before.x, before.y);
  const to = await at(page, before.x + 6, before.y + 4);
  await page.mouse.move(from.x, from.y);
  await page.mouse.down();
  await page.mouse.move((from.x + to.x) / 2, (from.y + to.y) / 2);
  await page.mouse.move(to.x, to.y);
  await page.mouse.up();
  s = await editor(page);
  expect(s.points[1].x).toBeCloseTo(s.points[2].x, 9);
  expect(Math.abs(s.points[2].y - before.y)).toBeGreaterThan(1);

  // Editing tools through the kernel: trim the 3-point circle at a crossing line, construction.
  const circles = s.curves.filter((c) => c.type === 'circle').length;
  await page.getByTestId('tool-line').click();
  await click(page, 10, 5);
  await click(page, 50, 5);
  await page.keyboard.press('Escape');
  await page.getByTestId('tool-trim').click();
  await click(page, 30, 10);
  s = await editor(page);
  expect(s.message).toBeNull();
  expect(s.curves.filter((c) => c.type === 'circle').length).toBe(circles - 1);

  await page.getByTestId('sketch-undo').click();
  s = await editor(page);
  expect(s.curves.filter((c) => c.type === 'circle').length).toBe(circles);

  // Leaving with Cancel drops the sketch (the transaction is aborted).
  await page.getByTestId('sketch-cancel').click();
  await expect(page.getByTestId('sketch-panel')).toHaveCount(0);
  expect((await modeler(page)).features).toHaveLength(0);
});
