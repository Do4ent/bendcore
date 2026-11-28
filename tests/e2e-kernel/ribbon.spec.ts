// Ribbon of the STEP viewer (docs/ui/KERNEL_VIEWPORT.md, "Лента"): opening from the quick access,
// the "Просмотр" tab with a measurement switched on from the ribbon (area of the box top, from
// the kernel), the section toggled from the ribbon, retessellation with new tolerances.
import { expect, test, Page } from '@playwright/test';
import path from 'path';

const corpus = path.resolve(__dirname, '../corpus/generated');

interface ViewerApi {
  getState(): { model: { fileName: string } | null; tools: { result: { type: string; value: number } | null; sectionArea: number | null } | null };
  idle(): Promise<void>;
  worldToClient(x: number, y: number, z: number): { x: number; y: number };
}
const state = (page: Page) =>
  page.evaluate(async () => {
    const v = (window as unknown as { bendcoreKernelViewer: ViewerApi }).bendcoreKernelViewer;
    await v.idle();
    return v.getState();
  });

test('viewer ribbon: open, measure and section from the ribbon, retessellate', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('kernel-status')).toHaveText('Ядро готово');
  const ribbon = page.getByTestId('viewer-ribbon');
  await expect(page.getByRole('tab')).toHaveText(['Просмотр']);
  await expect(ribbon).toHaveAttribute('data-active-tab', 'view');
  await expect(page.getByTestId('viewer-export')).toBeDisabled();
  await expect(page.getByTestId('ribbon-measure')).toBeDisabled();

  await page.getByTestId('step-input').setInputFiles(path.join(corpus, 'box_100x60x20.step'));
  await expect.poll(async () => (await state(page)).model?.fileName).toBe('box_100x60x20.step');
  await expect(page.getByTestId('viewer-export')).toBeEnabled();

  // Measurement switched on from the ribbon: the area of the top face.
  await page.getByTestId('ribbon-measure').click();
  await expect(page.getByTestId('ribbon-measure')).toHaveAttribute('aria-pressed', 'true');
  await page.getByTestId('measure-type').selectOption('area');
  const p = await page.evaluate(() => (window as unknown as { bendcoreKernelViewer: ViewerApi }).bendcoreKernelViewer.worldToClient(50, 30, 20));
  await page.mouse.click(p.x, p.y);
  await expect(page.getByTestId('measure-result')).toHaveAttribute('data-type', 'area');
  expect((await state(page)).tools!.result!.value).toBeCloseTo(6000, 6);
  await page.getByTestId('ribbon-measure').click();
  await expect(page.getByTestId('measure-toggle')).toHaveAttribute('aria-pressed', 'false');

  // Section from the ribbon: the kernel reports the area of the cut.
  await page.getByTestId('ribbon-section').click();
  await expect(page.getByTestId('ribbon-section')).toHaveAttribute('aria-pressed', 'true');
  await expect.poll(async () => (await state(page)).tools!.sectionArea ?? 0).toBeGreaterThan(0);
  await page.getByTestId('ribbon-section').click();
  await expect(page.getByTestId('ribbon-section')).toHaveAttribute('aria-pressed', 'false');

  // Retessellation with a finer tolerance from the "Тесселяция" group.
  await page.getByRole('group', { name: 'Тесселяция' }).locator('input').first().fill('0.02');
  await page.getByTestId('viewer-retessellate').click();
  await expect(page.getByTestId('kernel-status')).toHaveText('Ядро готово');
  expect((await state(page)).model?.fileName).toBe('box_100x60x20.step');
});
