// Ribbon of the drawing editor (docs/ui/DRAWING_UI.md, "Лента"): views from the "Виды" tab
// (projections unavailable until the front view exists), two dimensions with the tool of the
// "Размеры" tab, "Автокомпоновка размеров" of the selected view, the parts list shown from
// "Спецификация", the PDF from "Вывод". At 1440×900, scale 2 (like docs/ui/img/commands) the
// scenario takes the picture of the arrangement for docs/ui/img/commands/ribbon
// (BENDCORE_RIBBON_SCREENSHOTS=<dir>; otherwise into the test output).
import { expect, test, Page } from '@playwright/test';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const bracket = path.join(root, 'tests/wasm/fixtures/bracket.part.json');

const idle = (page: Page) =>
  page.evaluate(async () => {
    const w = window as unknown as { bendcoreDrawingEditorIdle: () => Promise<void>; bendcoreDrawingEditor: { error: unknown } };
    await w.bendcoreDrawingEditorIdle();
    return w.bendcoreDrawingEditor.error;
  });

// Screen point in the middle of the longest outer vertical (horizontal) visible edge of a view.
async function extremeEdge(page: Page, view: string, vertical: boolean, which: 'min' | 'max') {
  const c = vertical ? 0 : 1;
  const edges = page.locator(`[data-view="${view}"][data-kind="line"][data-visible="1"]`);
  const n = await edges.count();
  let best: { testId: string; x: number; length: number } | null = null;
  for (let i = 0; i < n; ++i) {
    const e = edges.nth(i);
    const pts = JSON.parse((await e.getAttribute('data-points'))!) as [number, number][];
    if (Math.abs(pts[0][c] - pts[1][c]) > 1e-6) continue;
    const x = pts[0][c];
    const length = Math.abs(pts[1][1 - c] - pts[0][1 - c]);
    const better = !best || (which === 'min' ? x < best.x - 1e-6 : x > best.x + 1e-6) || (Math.abs(x - best.x) <= 1e-6 && length > best.length);
    if (better) best = { testId: (await e.getAttribute('data-testid'))!, x, length };
  }
  expect(best).not.toBeNull();
  const box = (await page.getByTestId(best!.testId).boundingBox())!;
  return { x: box.x + box.width / 2, y: box.y + box.height / 2 };
}

async function dimensionBetween(page: Page, vertical: boolean) {
  const a = await extremeEdge(page, 'V1', vertical, 'min');
  const b = await extremeEdge(page, 'V1', vertical, 'max');
  await page.mouse.click(a.x, a.y);
  await page.mouse.click(b.x, b.y);
}

test.use({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });

test('drawing ribbon: views, dimensions, arrangement, parts list, PDF; picture of the arrangement', async ({ page }, testInfo) => {
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });
  const ribbon = page.getByTestId('drawing-ribbon');
  await expect(page.getByRole('tab')).toHaveText(['Виды', 'Разрезы', 'Размеры', 'Обозначения', 'Спецификация', 'Вывод']);
  // Without a drawing only the opening commands are available.
  await expect(page.getByTestId('add-base')).toBeDisabled();
  await expect(page.getByTestId('export-pdf')).toBeDisabled();

  await page.getByTestId('open-model').setInputFiles(bracket);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);

  // "Виды": projections wait for the front view.
  await expect(page.getByTestId('add-top')).toBeDisabled();
  await page.getByTestId('add-base').click();
  await expect(page.getByTestId('view-V1')).toHaveCount(1);
  await expect(page.getByTestId('add-base')).toBeDisabled();
  await page.getByTestId('add-top').click();
  await expect(page.getByTestId('view-list').locator('li')).toHaveCount(2);
  expect(await idle(page)).toBeNull();

  // "Размеры": the dimension tool, 60 between the outer vertical edges, 50 between the horizontal.
  await page.getByTestId('ribbon-tab-dimensions').click();
  await expect(ribbon).toHaveAttribute('data-active-tab', 'dimensions');
  await expect(page.getByTestId('view-arrange-dimensions')).toBeDisabled();
  await page.getByTestId('tool-dimension').click();
  await expect(page.getByTestId('tool-dimension')).toHaveAttribute('aria-pressed', 'true');
  await dimensionBetween(page, true);
  await expect(page.getByTestId('dimension-D1')).toHaveAttribute('data-value', '60');
  await dimensionBetween(page, false);
  await expect(page.getByTestId('dimension-D2')).toHaveAttribute('data-value', '50');

  // "Автокомпоновка размеров" of the selected front view.
  await page.getByTestId('tool-select').click();
  await page.getByTestId('view-list').locator('li').first().click();
  await expect(page.getByTestId('view-arrange-dimensions')).toBeEnabled();
  await page.getByTestId('view-arrange-dimensions').click();
  await expect(page.getByTestId('status')).toContainText('Размеры вида расставлены');
  expect(await idle(page)).toBeNull();
  await page.mouse.move(720, 880);
  const dir = process.env.BENDCORE_RIBBON_SCREENSHOTS;
  await page.screenshot({ path: dir ? path.join(dir, '5_arrange_dimensions.png') : testInfo.outputPath('5_arrange_dimensions.png') });

  // "Обозначения": balloons belong to assembly drawings.
  await page.getByTestId('ribbon-tab-annotations').click();
  await expect(page.getByTestId('tool-balloon')).toBeDisabled();
  await page.getByTestId('tool-roughness').click();
  await expect(page.getByTestId('tool-roughness')).toHaveAttribute('aria-pressed', 'true');
  await page.keyboard.press('Escape');

  // "Спецификация": the parts list and back to the sheet.
  await page.getByTestId('ribbon-show-specification').click();
  await expect(page.getByTestId('specification')).toBeVisible();
  await page.getByTestId('ribbon-show-sheet').click();
  await expect(page.getByTestId('sheet-svg')).toBeVisible();

  // "Вывод": PDF.
  await page.getByTestId('ribbon-tab-output').click();
  await expect(ribbon).toHaveAttribute('data-active-tab', 'output');
  const download = page.waitForEvent('download');
  await page.getByTestId('export-pdf').click();
  const file = await (await download).path();
  expect(fs.readFileSync(file!).subarray(0, 5).toString()).toBe('%PDF-');
});
