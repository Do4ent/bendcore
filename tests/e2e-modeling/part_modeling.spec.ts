// Part modeling end to end (docs/ui/PART_MODELING_UI.md): the user sketches a rectangle,
// extrudes it, picks a vertical edge in the viewport, fillets it, changes a sketch dimension;
// the volume in the properties panel follows the formula W·D·H − (1 − π/4)·r²·H; undo and redo
// (Ctrl+Z / Ctrl+Y) restore the previous volumes. State is read through
// window.bendcorePartModeler, the edge is clicked at its projected model position.
import { expect, test, Page } from '@playwright/test';
import fs from 'fs';

interface ModelerState {
  status: string;
  error: { code: string; message: string } | null;
  features: { id: string; type: string; label: string; state: string; messages: string[]; suppressed: boolean }[];
  doc: { canUndo: boolean; canRedo: boolean; inTransaction: boolean; undoHistory: { name: string }[] } | null;
  properties: { volume: number | null; mass: number | null } | null;
  panel: { type: string; featureId: string | null; activeSlot: string | null; picks: Record<string, string[]> } | null;
  shownTarget: string;
}

declare global {
  interface Window {
    bendcorePartModeler?: {
      getState(): ModelerState;
      worldToClient(x: number, y: number, z: number): { x: number; y: number };
    };
  }
}

const state = (page: Page) => page.evaluate(() => window.bendcorePartModeler!.getState());
const filletLoss = (r: number, L: number) => (1 - Math.PI / 4) * r * r * L;

async function expectVolume(page: Page, expected: number) {
  await expect
    .poll(async () => {
      const v = (await state(page)).properties?.volume;
      return v === null || v === undefined ? Infinity : Math.abs(v - expected) / expected;
    })
    .toBeLessThan(1e-9);
  // The panel shows the same value (rounded to 0.001 mm³).
  const shown = Number(await page.getByTestId('prop-volume').getAttribute('data-value'));
  expect(Math.abs(shown - expected) / expected).toBeLessThan(1e-9);
}

async function clickModelPoint(page: Page, x: number, y: number, z: number) {
  const p = await page.evaluate(([a, b, c]) => window.bendcorePartModeler!.worldToClient(a, b, c), [x, y, z]);
  await page.mouse.click(p.x, p.y);
}

async function buildBlock(page: Page, W: number, D: number, H: number) {
  await page.getByTestId('add-Sketch').click();
  await expect(page.getByTestId('feature-panel')).toHaveAttribute('data-type', 'Sketch');
  await page.getByTestId('sketch-width').fill(String(W));
  await page.getByTestId('sketch-height').fill(String(D));
  await page.getByTestId('panel-preview').click();
  await expect(page.getByTestId('panel-state')).toHaveAttribute('data-state', 'ok');
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);

  await page.getByTestId('add-Extrude').click();
  const sketchId = (await state(page)).features[0].id;
  await expect(page.getByTestId('link-sketch')).toHaveValue(sketchId);
  await page.getByTestId('param-distance').fill(String(H));
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
  await expectVolume(page, W * D * H);
  return sketchId;
}

test.beforeEach(async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
});

test('sketch, extrude, fillet a picked edge, change the sketch, undo/redo', async ({ page }, testInfo) => {
  const W = 60;
  const D = 40;
  const H = 20;
  const r = 5;
  const sketchId = await buildBlock(page, W, D, H);

  // Fillet: the edge slot is active on opening; the viewport shows the shape to pick from.
  await page.getByTestId('add-Fillet').click();
  let s = await state(page);
  expect(s.panel!.activeSlot).toBe('edges');
  expect(s.shownTarget).toBe('final');
  await page.getByTestId('param-radius').fill(String(r));
  // Vertical edge x = W, y = 0 (the corner facing the camera).
  await clickModelPoint(page, W, 0, H / 2);
  await expect.poll(async () => (await state(page)).panel!.picks.edges.length).toBe(1);
  s = await state(page);
  expect(s.panel!.picks.edges[0]).toBe(`G[${s.features[1].id}](${sketchId}:V1_2)`);
  await expect(page.getByTestId('picks-edges')).toContainText(sketchId);

  // Preview (inside the transaction): the volume already has the fillet.
  await page.getByTestId('panel-preview').click();
  await expect(page.getByTestId('panel-state')).toHaveAttribute('data-state', 'ok');
  await expectVolume(page, W * D * H - filletLoss(r, H));
  expect((await state(page)).doc!.inTransaction).toBe(true);
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
  await expectVolume(page, W * D * H - filletLoss(r, H));

  // Change the sketch width W = 60 -> 80 from the tree: the fillet follows its edge.
  await page.getByTestId(`tree-menu-${sketchId}`).click();
  await page.getByTestId('action-edit').click();
  const widthField = page.locator('[data-testid^="dim-c"]').first();
  await expect(widthField).toHaveValue(String(W));
  await widthField.fill('80');
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
  await expectVolume(page, 80 * D * H - filletLoss(r, H));
  s = await state(page);
  expect(s.features.map((f) => f.state)).toEqual(['ok', 'ok', 'ok']);

  // Ctrl+Z returns the previous volume; Ctrl+Y the edited one.
  await page.locator('body').click({ position: { x: 5, y: 840 } });
  await page.keyboard.press('Control+z');
  await expectVolume(page, W * D * H - filletLoss(r, H));
  await page.keyboard.press('Control+y');
  await expectVolume(page, 80 * D * H - filletLoss(r, H));
  await page.keyboard.press('Control+z');
  await expectVolume(page, W * D * H - filletLoss(r, H));

  // Documentation screenshot: the fillet selected in the tree, edge-picking view of the panel.
  await page.getByTestId(`tree-${s.features[2].id}`).click();
  const target = process.env.BENDCORE_DOC_SCREENSHOT || testInfo.outputPath('part_modeling.png');
  await page.screenshot({ path: target });
});

test('tree context actions, save and open', async ({ page }) => {
  const sketchId = await buildBlock(page, 50, 30, 10);
  const body = (await state(page)).features[1].id;

  // Suppress the extrusion: no body; undo brings it back.
  await page.getByTestId(`tree-menu-${body}`).click();
  await page.getByTestId('action-suppress').click();
  await expect(page.getByTestId(`tree-${body}`)).toHaveAttribute('data-state', 'suppressed');
  expect((await state(page)).properties!.volume).toBeNull();
  await page.getByTestId('undo').click();
  await expectVolume(page, 50 * 30 * 10);

  // Roll back to the sketch: the extrusion is behind the marker.
  await page.getByTestId(`tree-menu-${sketchId}`).click();
  await page.getByTestId('action-rollbackHere').click();
  await expect(page.getByTestId('rollback-marker')).toBeVisible();
  await expect(page.getByTestId(`tree-${body}`)).toHaveAttribute('data-state', 'rolled-back');
  await page.getByTestId('rollback-clear').click();
  await expectVolume(page, 50 * 30 * 10);

  // An invalid expression is refused by the document with its message.
  await page.getByTestId(`tree-menu-${body}`).click();
  await page.getByTestId('action-edit').click();
  await page.getByTestId('param-distance').fill('10 +');
  await page.getByTestId('panel-preview').click();
  await expect(page.getByTestId('modeling-error')).toContainText('InvalidInput');
  await page.getByTestId('panel-cancel').click();
  await expectVolume(page, 50 * 30 * 10);

  // Save to a file and open it again.
  const download = page.waitForEvent('download');
  await page.getByTestId('doc-save').click();
  const file = await (await download).path();
  const json = JSON.parse(fs.readFileSync(file!, 'utf8'));
  expect(json.format).toBe('bendcore-part');
  expect(json.version).toBe(3);
  await page.getByTestId('doc-new').click();
  await expect.poll(async () => (await state(page)).features.length).toBe(0);
  await page.getByTestId('doc-open').setInputFiles(file!);
  await expect.poll(async () => (await state(page)).features.length).toBe(2);
  await expectVolume(page, 50 * 30 * 10);
});
