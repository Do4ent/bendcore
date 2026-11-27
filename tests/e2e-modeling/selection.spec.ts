// Selection filters and box selection in the part page (docs/ui/PART_MODELING_UI.md, "Фильтры
// выбора и выбор рамкой"). A 60 x 40 x 20 block is built through the operation panels (6 planar
// faces, 12 straight edges, 8 vertices, one body). Counts and totals are compared with the
// formulas written out here: the faces' area is 2 (60 40 + 60 20 + 40 20) = 8800 mm2, the edges'
// length is 4 (60 + 40 + 20) = 480 mm.
import { expect, test, Page } from '@playwright/test';

interface Elements {
  mode: string;
  faceTypes: string[];
  edgeTypes: string[];
  boxSelect: boolean;
  faces: number[];
  edges: number[];
  vertices: number[];
  bodies: number[];
  total: number | null;
}
interface ModelerState {
  status: string;
  error: { code: string; message: string } | null;
  properties: { volume: number | null } | null;
  panel: { type: string; featureId: string | null; activeSlot: string | null; picks: Record<string, string[]> } | null;
  elements: Elements;
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
const elements = async (page: Page) => (await state(page)).elements;
const client = (page: Page, p: number[]) => page.evaluate(([a, b, c]) => window.bendcorePartModeler!.worldToClient(a, b, c), p);

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

type Rect = { x0: number; y0: number; x1: number; y1: number };

async function wholeRect(page: Page, margin = 14): Promise<Rect> {
  const xs: number[] = [];
  const ys: number[] = [];
  for (const x of [0, 60]) for (const y of [0, 40]) for (const z of [0, 20]) {
    const p = await client(page, [x, y, z]);
    xs.push(p.x);
    ys.push(p.y);
  }
  return { x0: Math.min(...xs) - margin, y0: Math.min(...ys) - margin, x1: Math.max(...xs) + margin, y1: Math.max(...ys) + margin };
}

async function drag(page: Page, r: Rect, direction: 'inside' | 'cross', modifier?: 'Shift' | 'Control') {
  const [ax, bx] = direction === 'inside' ? [r.x0, r.x1] : [r.x1, r.x0];
  if (modifier) await page.keyboard.down(modifier);
  await page.mouse.move(ax, r.y0);
  await page.mouse.down();
  await page.mouse.move((ax + bx) / 2, (r.y0 + r.y1) / 2, { steps: 4 });
  await page.mouse.move(bx, r.y1, { steps: 4 });
  await expect(page.getByTestId('box-rect')).toHaveAttribute('data-direction', direction);
  await page.mouse.up();
  if (modifier) await page.keyboard.up(modifier);
  await expect(page.getByTestId('box-rect')).toHaveCount(0);
}

test.beforeEach(async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await block(page);
});

test('box: faces by type with the kernel total area, then edges, vertices and the body', async ({ page }) => {
  await page.getByTestId('sel-box').click();
  const whole = await wholeRect(page);

  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(6);
  await expect.poll(async () => (await elements(page)).total).toBeCloseTo(8800, 3);
  await expect(page.getByTestId('sel-count')).toHaveAttribute('data-count', '6');

  // No cylindrical face in a block: the filter leaves nothing; planes leave all six.
  await page.getByTestId('sel-filter-Cylinder').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(0);
  await page.getByTestId('sel-filter-Cylinder').click();
  await page.getByTestId('sel-filter-Plane').click();
  await drag(page, whole, 'cross');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(6);

  // Ctrl removes, Shift adds back.
  await page.getByTestId('sel-filter-Plane').click();
  const top = await client(page, [30, 20, 20]);
  await page.mouse.click(top.x, top.y);
  await expect.poll(async () => (await elements(page)).faces.length).toBe(1);
  await drag(page, whole, 'inside', 'Shift');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(6);
  await drag(page, whole, 'inside', 'Control');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(0);

  await page.getByTestId('sel-mode-edge').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).edges.length).toBe(12);
  await expect.poll(async () => (await elements(page)).total).toBeCloseTo(480, 3);
  await page.getByTestId('sel-filter-Circle').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).edges.length).toBe(0);
  await page.getByTestId('sel-filter-Circle').click();

  await page.getByTestId('sel-mode-vertex').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).vertices.length).toBe(8);

  await page.getByTestId('sel-mode-body').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).bodies).toEqual([0]);
  expect((await state(page)).error).toBeNull();
});

test('an operation slot takes the elements of its kind by the filter, from a box', async ({ page }) => {
  await page.getByTestId('add-Fillet').click();
  expect((await state(page)).panel!.activeSlot).toBe('edges');
  // The slot dictates the element kind: the mode buttons are disabled, the filter stays.
  await expect(page.getByTestId('sel-forced')).toBeVisible();
  await expect(page.getByTestId('sel-mode-face')).toBeDisabled();
  await page.getByTestId('sel-box').click();
  const whole = await wholeRect(page);

  // Only circular edges are allowed: a block has none, the slot stays empty.
  await page.getByTestId('sel-filter-Circle').click();
  await drag(page, whole, 'inside');
  expect((await state(page)).panel!.picks.edges ?? []).toHaveLength(0);

  // Straight edges: all twelve go to the slot; Ctrl removes the ones the rectangle takes again.
  await page.getByTestId('sel-filter-Circle').click();
  await page.getByTestId('sel-filter-Line').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await state(page)).panel!.picks.edges.length).toBe(12);
  await drag(page, whole, 'inside', 'Control');
  await expect.poll(async () => (await state(page)).panel!.picks.edges.length).toBe(0);
  await drag(page, whole, 'cross', 'Shift');
  await expect.poll(async () => (await state(page)).panel!.picks.edges.length).toBe(12);

  // The twelve edges are filleted by the kernel: 60 x 40 x 20 with r = 2 loses volume.
  await page.getByTestId('param-radius').fill('2');
  await page.getByTestId('panel-preview').click();
  await expect(page.getByTestId('panel-state')).toHaveAttribute('data-state', 'ok');
  await expect.poll(async () => (await state(page)).properties?.volume).toBeLessThan(48000);
});
