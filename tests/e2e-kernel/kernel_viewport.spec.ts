// Kernel viewer end to end: the page loads bendcore_kernel_wasm, the user opens a corpus STEP
// file, the model is shown, and a click at a known model point selects the expected B-rep face.
// Selection is checked through the page state (window.bendcoreKernelViewer), not pixels; the
// expected faces follow from the model dimensions (tests/corpus/generated/manifest.json).
import { expect, test, Page } from '@playwright/test';
import path from 'path';

const corpus = path.resolve(__dirname, '../corpus/generated');

interface ViewerState {
  kernelStatus: string;
  busy: boolean;
  error: { code: string; message: string } | null;
  model: { fileName: string; counts: { faces: number; solids: number }; volume: number | null; triangles: number } | null;
  selection: {
    face: { index: number; surfaceType: string; area: number; parameters: Record<string, unknown> };
    point: [number, number, number];
  } | null;
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

// Kernel calls run in a worker (docs/ui/APP_SHELL.md): wait for the last result before reading.
const state = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcoreKernelViewer!.idle();
    return window.bendcoreKernelViewer!.getState();
  });

async function openModel(page: Page, file: string, faces: number) {
  await page.getByTestId('step-input').setInputFiles(path.join(corpus, file));
  await expect.poll(async () => (await state(page)).model?.fileName).toBe(file);
  const s = await state(page);
  expect(s.error).toBeNull();
  expect(s.model!.counts.faces).toBe(faces);
  expect(s.model!.triangles).toBeGreaterThan(0);
}

function expectUnitVector(actual: unknown, expected: number[]) {
  const v = actual as number[];
  expect(v).toHaveLength(3);
  v.forEach((c, i) => expect(c).toBeCloseTo(expected[i], 12));
}

async function clickModelPoint(page: Page, x: number, y: number, z: number) {
  const p = await page.evaluate(([a, b, c]) => window.bendcoreKernelViewer!.worldToClient(a, b, c), [x, y, z]);
  await page.mouse.click(p.x, p.y);
}

test.beforeEach(async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('kernel-status')).toHaveText('Ядро готово');
});

test('box: clicks select the top and front planes', async ({ page }) => {
  await openModel(page, 'box_100x60x20.step', 6);
  expect((await state(page)).model!.volume).toBeCloseTo(120000, 3);

  await clickModelPoint(page, 50, 30, 20);
  let s = await state(page);
  expect(s.selection!.face.surfaceType).toBe('Plane');
  expectUnitVector(s.selection!.face.parameters.normal, [0, 0, 1]);
  expect(s.selection!.face.area).toBeCloseTo(6000, 6);
  expect(s.selection!.point[2]).toBeCloseTo(20, 3);
  await expect(page.getByTestId('face-type')).toHaveText('Plane');
  await expect(page.getByTestId('face-index')).toHaveText(String(s.selection!.face.index));

  await clickModelPoint(page, 50, 0, 10);
  s = await state(page);
  expectUnitVector(s.selection!.face.parameters.normal, [0, -1, 0]);
  expect(s.selection!.face.area).toBeCloseTo(2000, 6);

  // A click on the background clears the selection.
  await page.mouse.click(700, 760);
  expect((await state(page)).selection).toBeNull();
});

test('cylinder: a click on the side selects the cylindrical face', async ({ page }) => {
  await openModel(page, 'cylinder_r20_h50.step', 3);
  // Point of the lateral surface facing the camera (view direction (1, -1.2, 0.9)).
  const a = Math.atan2(-1.2, 1);
  await clickModelPoint(page, 20 * Math.cos(a), 20 * Math.sin(a), 25);
  const s = await state(page);
  expect(s.selection!.face.surfaceType).toBe('Cylinder');
  expect(s.selection!.face.parameters.radius as number).toBeCloseTo(20, 9);
  expect(s.selection!.face.area).toBeCloseTo(2 * Math.PI * 20 * 50, 6);
});

test('filleted box: top face, documentation screenshot', async ({ page }, testInfo) => {
  await openModel(page, 'box_80x60x40_all_fillets_r5.step', 26);
  await clickModelPoint(page, 40, 30, 40);
  const s = await state(page);
  expect(s.selection!.face.surfaceType).toBe('Plane');
  expectUnitVector(s.selection!.face.parameters.normal, [0, 0, 1]);
  // Flat part of the top: (80 - 2r) x (60 - 2r), r = 5.
  expect(s.selection!.face.area).toBeCloseTo(70 * 50, 6);
  const target = process.env.BENDCORE_DOC_SCREENSHOT || testInfo.outputPath('kernel_viewport.png');
  await page.screenshot({ path: target });
});

test('a file that is not STEP shows the kernel ParseError', async ({ page }) => {
  await page.getByTestId('step-input').setInputFiles({
    name: 'broken.step',
    mimeType: 'application/octet-stream',
    buffer: Buffer.from('not a STEP file'),
  });
  await expect(page.getByTestId('kernel-error')).toContainText('ParseError');
  expect((await state(page)).model).toBeNull();
});
