// Selection filters and box selection in the STEP viewer (docs/ui/KERNEL_VIEWPORT.md, "Фильтры
// выбора и выбор рамкой"). Counts are compared with the topology of the corpus models: the box has
// 6 faces, 12 edges, 8 vertices; the box with all edges rounded has 6 planes, 12 cylinders and 8
// spheres; the cylinder has 2 circular edges. The rectangle is placed through the projection of
// model points (worldToClient), so the expected sets follow from the geometry, not from the code.
import { expect, test, Page } from '@playwright/test';
import path from 'path';

const corpus = path.resolve(__dirname, '../corpus/generated');

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
interface ViewerState {
  error: { code: string; message: string } | null;
  model: { fileName: string; counts: { solids: number; faces: number; edges: number; vertices: number } } | null;
  elements: Elements | null;
}

declare global {
  interface Window {
    bendcoreKernelViewer?: {
      getState(): ViewerState;
      idle(): Promise<void>;
      worldToClient(x: number, y: number, z: number): { x: number; y: number };
    };
  }
}

const state = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcoreKernelViewer!.idle();
    return window.bendcoreKernelViewer!.getState();
  });

const elements = async (page: Page) => (await state(page)).elements!;

async function openModel(page: Page, file: string) {
  await page.getByTestId('step-input').setInputFiles(path.join(corpus, file));
  await expect.poll(async () => (await state(page)).model?.fileName).toBe(file);
}

const client = (page: Page, p: number[]) => page.evaluate(([a, b, c]) => window.bendcoreKernelViewer!.worldToClient(a, b, c), p);

/** Screen rectangle around the whole model (the corners of its bounding box) with a margin. */
async function modelRect(page: Page, max: number[], margin = 14, min: number[] = [0, 0, 0]) {
  const xs: number[] = [];
  const ys: number[] = [];
  for (const x of [min[0], max[0]]) for (const y of [min[1], max[1]]) for (const z of [min[2], max[2]]) {
    const p = await client(page, [x, y, z]);
    xs.push(p.x);
    ys.push(p.y);
  }
  return { x0: Math.min(...xs) - margin, y0: Math.min(...ys) - margin, x1: Math.max(...xs) + margin, y1: Math.max(...ys) + margin };
}

type Rect = { x0: number; y0: number; x1: number; y1: number };

/** Drags a rectangle: left to right ('inside') or right to left ('cross'), optionally with a modifier. */
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
  await expect(page.getByTestId('kernel-status')).toHaveText('Ядро готово');
});

test('box: left to right takes whole faces, right to left the touched ones', async ({ page }) => {
  await openModel(page, 'box_100x60x20.step');
  await page.getByTestId('sel-box').click();
  const whole = await modelRect(page, [100, 60, 20]);

  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(6);
  // The kernel's area of the six faces: 2 (100 60 + 100 20 + 60 20).
  await expect(page.getByTestId('sel-count')).toHaveAttribute('data-count', '6');
  await expect.poll(async () => (await elements(page)).total).toBeCloseTo(2 * (6000 + 2000 + 1200), 3);
  expect((await state(page)).error).toBeNull();

  // The same rectangle from right to left takes the same six faces.
  await page.getByTestId('sel-clear').click();
  expect((await elements(page)).faces).toEqual([]);
  await drag(page, whole, 'cross');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(6);

  // A small rectangle inside the top face: nothing lies wholly in it. Right to left it touches the
  // top face and, behind it, at least the face the ray leaves the solid through (a convex solid:
  // two faces per ray), never all six.
  await page.getByTestId('sel-clear').click();
  const c = await client(page, [50, 30, 20]);
  await page.mouse.click(c.x, c.y);
  await expect.poll(async () => (await elements(page)).faces.length).toBe(1);
  const topFace = (await elements(page)).faces[0];
  await page.getByTestId('sel-clear').click();
  const small = { x0: c.x - 2, y0: c.y - 2, x1: c.x + 2, y1: c.y + 2 };
  await drag(page, small, 'inside');
  expect((await elements(page)).faces).toEqual([]);
  await drag(page, small, 'cross');
  await expect.poll(async () => (await elements(page)).faces.length).toBeGreaterThanOrEqual(2);
  const touched = (await elements(page)).faces;
  expect(touched).toContain(topFace);
  expect(touched.length).toBeLessThan(6);
});

test('box with rounded edges: the type filter chooses planes or cylinders', async ({ page }) => {
  await openModel(page, 'box_80x60x40_all_fillets_r5.step');
  expect((await state(page)).model!.counts.faces).toBe(26);
  await page.getByTestId('sel-box').click();
  const whole = await modelRect(page, [80, 60, 40]);

  // No filter: every face (6 planes, 12 cylinders, 8 spheres).
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(26);

  await page.getByTestId('sel-filter-Plane').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(6);
  expect((await elements(page)).faceTypes).toEqual(['Plane']);

  await page.getByTestId('sel-filter-Plane').click();
  await page.getByTestId('sel-filter-Cylinder').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(12);

  await page.getByTestId('sel-filter-Plane').click();
  await drag(page, whole, 'cross');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(18);
});

test('Shift adds to the selection, Ctrl removes from it', async ({ page }) => {
  await openModel(page, 'box_80x60x40_all_fillets_r5.step');
  await page.getByTestId('sel-box').click();
  const whole = await modelRect(page, [80, 60, 40]);

  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(26);

  // Ctrl with the plane filter removes the six planes.
  await page.getByTestId('sel-filter-Plane').click();
  await drag(page, whole, 'inside', 'Control');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(20);

  // Shift with the plane filter adds them back.
  await drag(page, whole, 'inside', 'Shift');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(26);

  // A plain box replaces the selection.
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).faces.length).toBe(6);
});

test('only allowed elements are highlighted and picked', async ({ page }) => {
  await openModel(page, 'box_80x60x40_all_fillets_r5.step');
  const top = await client(page, [40, 30, 40]);
  await page.mouse.move(top.x, top.y);
  await expect(page.getByTestId('kernel-viewport')).toHaveAttribute('data-hover', /^face:\d+$/);

  // Only cylinders are allowed: the top plane is neither highlighted nor picked.
  await page.getByTestId('sel-filter-Cylinder').click();
  await page.mouse.move(top.x + 1, top.y + 1);
  await expect(page.getByTestId('kernel-viewport')).toHaveAttribute('data-hover', '');
  await page.mouse.click(top.x, top.y);
  expect((await elements(page)).faces).toEqual([]);
  await expect(page.getByTestId('face-panel')).toHaveCount(0);

  // The filter is lifted for planes: the click selects the top face.
  await page.getByTestId('sel-filter-Cylinder').click();
  await page.getByTestId('sel-filter-Plane').click();
  await page.mouse.click(top.x, top.y);
  await expect.poll(async () => (await elements(page)).faces.length).toBe(1);
  await expect(page.getByTestId('face-type')).toHaveText('Plane');
});

test('edges, vertices and bodies of the box', async ({ page }) => {
  await openModel(page, 'box_100x60x20.step');
  await page.getByTestId('sel-box').click();
  const whole = await modelRect(page, [100, 60, 20]);

  await page.getByTestId('sel-mode-edge').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).edges.length).toBe(12);
  await expect(page.getByTestId('sel-count')).toHaveAttribute('data-count', '12');
  // The kernel's length of the 12 edges: 4 (100 + 60 + 20).
  await expect.poll(async () => (await elements(page)).total).toBeCloseTo(4 * 180, 3);
  await page.getByTestId('sel-filter-Circle').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).edges.length).toBe(0);
  await page.getByTestId('sel-filter-Circle').click();
  await page.getByTestId('sel-filter-Line').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).edges.length).toBe(12);

  await page.getByTestId('sel-mode-vertex').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).vertices.length).toBe(8);

  await page.getByTestId('sel-mode-body').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await elements(page)).bodies).toEqual([0]);
});

test('cylinder: circular and straight edges', async ({ page }) => {
  await openModel(page, 'cylinder_r20_h50.step');
  const edges = (await state(page)).model!.counts.edges;
  await page.getByTestId('sel-box').click();
  await page.getByTestId('sel-mode-edge').click();
  const rect = await modelRect(page, [20, 20, 50], 14, [-20, -20, 0]);

  await page.getByTestId('sel-filter-Circle').click();
  await drag(page, rect, 'cross');
  await expect.poll(async () => (await elements(page)).edges.length).toBe(2);
  await page.getByTestId('sel-filter-Circle').click();
  await page.getByTestId('sel-filter-Line').click();
  await drag(page, rect, 'cross');
  await expect.poll(async () => (await elements(page)).edges.length).toBe(edges - 2);
});
