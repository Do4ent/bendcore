// Datum geometry end to end (docs/ui/PART_MODELING_UI.md, "Вспомогательная геометрия"): the user
// builds a block W×D×H, adds a plane through three vertices of its top face picked by three clicks
// in the viewport, a sketch on that plane, an extrusion, a datum point and a coordinate system.
// The volume follows W·D·H + a·b·h; after a change of the block width the plane and the pad
// follow the vertices; the viewport shows the point marker and the three axes of the system.
import { expect, test, Page } from '@playwright/test';

interface ModelerState {
  status: string;
  error: { code: string; message: string } | null;
  features: { id: string; type: string; state: string; messages: string[] }[];
  properties: { volume: number | null } | null;
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

async function expectVolume(page: Page, expected: number) {
  await expect
    .poll(async () => {
      const v = (await state(page)).properties?.volume;
      return v === null || v === undefined ? Infinity : Math.abs(v - expected) / expected;
    })
    .toBeLessThan(1e-9);
}

async function clickModelPoint(page: Page, x: number, y: number, z: number) {
  const p = await page.evaluate(([a, b, c]) => window.bendcorePartModeler!.worldToClient(a, b, c), [x, y, z]);
  await page.mouse.click(p.x, p.y);
}

async function ok(page: Page) {
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
  const s = await state(page);
  expect(s.features.filter((f) => f.state !== 'ok').map((f) => `${f.id}: ${f.messages.join('; ')}`)).toEqual([]);
}

test.beforeEach(async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
});

test('plane through three picked vertices carries a sketch; point and coordinate system are shown', async ({ page }, testInfo) => {
  const W = 60;
  const D = 40;
  const H = 20;
  const [a, b, h] = [12, 7, 9];

  // Block W×D×H.
  await page.getByTestId('add-Sketch').click();
  await page.getByTestId('sketch-width').fill(String(W));
  await page.getByTestId('sketch-height').fill(String(D));
  await ok(page);
  await page.getByTestId('add-Extrude').click();
  await page.getByTestId('param-distance').fill(String(H));
  await ok(page);
  await expectVolume(page, W * D * H);
  const sketchId = (await state(page)).features[0].id;

  // Plane through (W, D, H), (0, D, H), (W, 0, H): three clicks, the slot moves on by itself.
  await page.getByTestId('add-DatumPlane').click();
  await page.getByTestId('option-method').selectOption('threePoints');
  await page.getByTestId('slot-pick-vertex1').click();
  expect((await state(page)).panel!.activeSlot).toBe('vertex1');
  await expect(page.getByTestId('kernel-viewport')).toBeVisible();
  for (const [slot, p] of [
    ['vertex1', [W, D, H]],
    ['vertex2', [0, D, H]],
    ['vertex3', [W, 0, H]],
  ] as const) {
    await clickModelPoint(page, p[0], p[1], p[2]);
    await expect.poll(async () => (await state(page)).panel!.picks[slot].length).toBe(1);
  }
  const picks = (await state(page)).panel!.picks;
  // Vertex names come from the sketch entities meeting there (topological naming).
  for (const slot of ['vertex1', 'vertex2', 'vertex3']) expect(picks[slot][0]).toContain(sketchId);
  expect(new Set([picks.vertex1[0], picks.vertex2[0], picks.vertex3[0]]).size).toBe(3);
  await page.getByTestId('panel-preview').click();
  await expect(page.getByTestId('panel-state')).toHaveAttribute('data-state', 'ok');
  await ok(page);
  const planeId = (await state(page)).features[2].id;
  await expect(page.getByTestId('kernel-viewport')).toHaveAttribute('data-datum-planes', '1');

  // Sketch on the plane: rectangle a×b at local (2, 3); X of the plane is −X, Y is −Y.
  await page.getByTestId('add-Sketch').click();
  await page.getByTestId('option-plane').selectOption('datum');
  await page.getByTestId('link-plane').selectOption(planeId);
  await page.getByTestId('sketch-x').fill('2');
  await page.getByTestId('sketch-y').fill('3');
  await page.getByTestId('sketch-width').fill(String(a));
  await page.getByTestId('sketch-height').fill(String(b));
  await ok(page);
  const padSketch = (await state(page)).features[3].id;
  await page.getByTestId('add-Extrude').click();
  await page.getByTestId('link-sketch').selectOption(padSketch);
  await page.getByTestId('param-distance').fill(String(h));
  await ok(page);
  await expectVolume(page, W * D * H + a * b * h);

  // Datum point at the vertex (0, 0, H) and a coordinate system with its origin there.
  await page.getByTestId('add-DatumPoint').click();
  await page.getByTestId('option-method').selectOption('vertex');
  await page.getByTestId('slot-pick-vertex').click();
  await clickModelPoint(page, 0, 0, H);
  await expect.poll(async () => (await state(page)).panel!.picks.vertex.length).toBe(1);
  await ok(page);
  const pointId = (await state(page)).features[5].id;
  await page.getByTestId('add-DatumCsys').click();
  await page.getByTestId('option-origin').selectOption('datum');
  await page.getByTestId('link-point').selectOption(pointId);
  await page.getByTestId('option-xDirection').selectOption('X');
  await page.getByTestId('option-xyDirection').selectOption('Y');
  await ok(page);
  const viewport = page.getByTestId('kernel-viewport');
  await expect(viewport).toHaveAttribute('data-datum-points', '1');
  await expect(viewport).toHaveAttribute('data-datum-csys', '1');

  // Width 60 -> 80 from the tree: the plane follows its vertices, the pad stays on the top face.
  await page.getByTestId(`tree-menu-${sketchId}`).click();
  await page.getByTestId('action-edit').click();
  const widthField = page.locator('[data-testid^="dim-c"]').first();
  await expect(widthField).toHaveValue(String(W));
  await widthField.fill('80');
  await ok(page);
  await expectVolume(page, 80 * D * H + a * b * h);

  await page.screenshot({ path: process.env.BENDCORE_DOC_SCREENSHOT || testInfo.outputPath('datum_geometry.png') });
});
