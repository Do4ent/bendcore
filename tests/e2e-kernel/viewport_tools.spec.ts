// Measurements and section in the STEP viewer (docs/ui/KERNEL_VIEWPORT.md, "Измерения и сечение"):
// values come from the kernel and are compared with the analytic ones of the corpus models.
import { expect, test, Page } from '@playwright/test';
import path from 'path';

const corpus = path.resolve(__dirname, '../corpus/generated');

interface ViewerState {
  error: { code: string; message: string } | null;
  model: { fileName: string } | null;
  tools: {
    picks: { kind: string; index: number }[];
    result: { type: string; value: number; diameter?: number } | null;
    sectionPosition: number | null;
    sectionArea: number | null;
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

const state = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcoreKernelViewer!.idle();
    return window.bendcoreKernelViewer!.getState();
  });

async function openModel(page: Page, file: string) {
  await page.getByTestId('step-input').setInputFiles(path.join(corpus, file));
  await expect.poll(async () => (await state(page)).model?.fileName).toBe(file);
}

async function clickModelPoint(page: Page, x: number, y: number, z: number) {
  const p = await page.evaluate(([a, b, c]) => window.bendcoreKernelViewer!.worldToClient(a, b, c), [x, y, z]);
  await page.mouse.click(p.x, p.y);
}

test.beforeEach(async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('kernel-status')).toHaveText('Ядро готово');
});

test('box: angle between the top and the front face, area of the top', async ({ page }) => {
  await openModel(page, 'box_100x60x20.step');
  await page.getByTestId('measure-toggle').click();
  await page.getByTestId('measure-type').selectOption('angle');
  await clickModelPoint(page, 50, 30, 20);
  await clickModelPoint(page, 50, 0, 10);
  await expect(page.getByTestId('measure-result')).toHaveAttribute('data-type', 'angle');
  expect((await state(page)).tools!.result!.value).toBeCloseTo(90, 6);
  await expect(page.getByTestId('measure-label')).toHaveAttribute('data-label', '90°');

  await page.getByTestId('measure-type').selectOption('area');
  await clickModelPoint(page, 50, 30, 20);
  await expect(page.getByTestId('measure-result')).toHaveAttribute('data-type', 'area');
  expect((await state(page)).tools!.result!.value).toBeCloseTo(6000, 6);
  // While measuring, a click does not open the face panel.
  await expect(page.getByTestId('face-panel')).toHaveCount(0);
});

test('cylinder: radius of the side face and the section area', async ({ page }) => {
  await openModel(page, 'cylinder_r20_h50.step');
  await page.getByTestId('measure-toggle').click();
  await page.getByTestId('measure-type').selectOption('radius');
  const a = Math.atan2(-1.2, 1);
  await clickModelPoint(page, 20 * Math.cos(a), 20 * Math.sin(a), 25);
  await expect(page.getByTestId('measure-result')).toHaveAttribute('data-type', 'radius');
  const r = (await state(page)).tools!.result!;
  expect(r.value).toBeCloseTo(20, 6);
  expect(r.diameter).toBeCloseTo(40, 6);

  await page.getByTestId('section-toggle').click();
  await page.getByTestId('section-axis').selectOption('Z');
  await page.getByTestId('section-slider').fill('500');
  await expect.poll(async () => (await state(page)).tools!.sectionPosition).toBeCloseTo(25, 6);
  await expect.poll(async () => (await state(page)).tools!.sectionArea).toBeCloseTo(Math.PI * 400, 1);
  await expect(page.getByTestId('kernel-viewport')).toHaveAttribute('data-clipped', 'true');
  // A section across the axis: x = 0 gives the 40 x 50 rectangle.
  await page.getByTestId('section-axis').selectOption('X');
  await expect.poll(async () => (await state(page)).tools!.sectionArea).toBeCloseTo(2000, 1);
});
