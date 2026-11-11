// Drawing editor end to end (docs/ui/DRAWING_UI.md): the user opens the reference bracket
// (tests/wasm/fixtures/bracket.part.json, A = 60), adds the front, top and left views and an
// isometric view, puts a dimension by clicking the two outer vertical edges of the front view
// (the value equals A), drags the isometric view, changes A to 80 in the model panel, updates the
// drawing (the report lists "60 → 80", the dimension shows 80) and exports a PDF (header and,
// when available, pdfinfo of the downloaded file). The edges are clicked at their position on
// screen, taken from the overlay elements of the kernel's 2D edges.
import { expect, test, Page } from '@playwright/test';
import { spawnSync } from 'child_process';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const bracket = path.join(root, 'tests/wasm/fixtures/bracket.part.json');

interface EditorState {
  status: string;
  error: { code: string; message: string } | null;
  message: string;
  description: {
    sheets: { id: string; views: { id: string; kind: string; bounds?: { minX: number; minY: number; maxX: number; maxY: number } }[]; dimensions: { id: string; value?: number; text?: string }[] }[];
  } | null;
  staleness: { modelChanged: boolean } | null;
  report: { stale: string[]; dangling: string[] } | null;
}

const state = (page: Page) => page.evaluate(() => (window as unknown as { bendcoreDrawingEditor: EditorState }).bendcoreDrawingEditor);

async function expectNoError(page: Page) {
  const s = await state(page);
  expect(s.error, s.error ? `${s.error.code}: ${s.error.message}` : '').toBeNull();
}

// Screen point in the middle of the longest visible vertical (horizontal) edge of a view with the
// smallest or largest x (y).
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
    const better =
      !best ||
      (which === 'min' ? x < best.x - 1e-6 : x > best.x + 1e-6) ||
      (Math.abs(x - best.x) <= 1e-6 && length > best.length);
    if (better) best = { testId: (await e.getAttribute('data-testid'))!, x, length };
  }
  expect(best, `${which} ${vertical ? 'vertical' : 'horizontal'} edge of ${view}`).not.toBeNull();
  const box = (await page.getByTestId(best!.testId).boundingBox())!;
  return { testId: best!.testId, x: box.x + box.width / 2, y: box.y + box.height / 2 };
}

test('bracket drawing: views, dimension by two edges, update from the model, PDF export', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });

  await page.getByTestId('open-model').setInputFiles(bracket);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  await expectNoError(page);

  // Three views and the isometric.
  await page.getByTestId('add-base').click();
  await expect(page.getByTestId('view-V1')).toHaveCount(1);
  await page.getByTestId('add-top').click();
  await page.getByTestId('add-left').click();
  await page.getByTestId('add-isometric').click();
  await expect(page.getByTestId('view-list').locator('li')).toHaveCount(4);
  await expectNoError(page);
  const kinds = (await state(page)).description!.sheets[0].views.map((v) => v.kind);
  expect(kinds).toEqual(['base', 'projected', 'projected', 'axonometric']);

  // Dimension: two clicks on the outer vertical edges of the front view.
  await page.getByTestId('tool-dimension').click();
  const left = await extremeEdge(page, 'V1', true, 'min');
  const right = await extremeEdge(page, 'V1', true, 'max');
  await page.mouse.click(left.x, left.y);
  await expect(page.getByTestId('picks')).toHaveText('Выбрано рёбер: 1');
  await expect(page.getByTestId(left.testId)).toHaveClass(/dr-picked/);
  await page.mouse.click(right.x, right.y);
  const dim = page.getByTestId('dimension-D1');
  await expect(dim).toHaveCount(1);
  await expectNoError(page);
  await expect(dim).toHaveAttribute('data-value', '60');
  await expect(dim.locator('strong')).toHaveText('60');
  // The dimension text is on the sheet drawn by the kernel.
  await expect(page.getByTestId('sheet-svg').locator('text', { hasText: /^60$/ })).toHaveCount(1);

  // Datum А on the bottom line of the front view; title block fields.
  await page.getByTestId('tool-datum').click();
  const bottom = await extremeEdge(page, 'V1', false, 'min');
  await page.mouse.click(bottom.x, bottom.y);
  await expect(page.getByTestId('status')).toContainText('База добавлена');
  await expectNoError(page);
  await page.getByTestId('title-designation').fill('РД.10.001');
  await page.getByTestId('title-name').fill('Кронштейн');
  await page.getByTestId('title-material').fill('Сталь 20 ГОСТ 1050-2013');
  await page.getByTestId('title-developer').fill('Иванов');
  await page.getByTestId('title-apply').click();
  await expect(page.getByTestId('sheet-svg').locator('text', { hasText: 'РД.10.001' })).toHaveCount(1);
  await expect(page.getByTestId('sheet-svg').locator('text', { hasText: 'Иванов' })).toHaveCount(1);
  await expectNoError(page);

  // Drag the isometric view by 40 × 20 px: it moves alone, by that distance in sheet mm.
  await page.getByTestId('tool-select').click();
  const isoBefore = (await state(page)).description!.sheets[0].views[3].bounds!;
  const zoom = Number(await page.getByTestId('sheet').getAttribute('data-zoom'));
  const isoBox = (await page.getByTestId('view-V4').boundingBox())!;
  const start = { x: isoBox.x + isoBox.width / 2, y: isoBox.y + isoBox.height / 2 };
  await page.mouse.move(start.x, start.y);
  await page.mouse.down();
  await page.mouse.move(start.x + 20, start.y + 10, { steps: 4 });
  await page.mouse.move(start.x + 40, start.y + 20, { steps: 4 });
  await page.mouse.up();
  await expect(page.getByTestId('status')).toContainText('Вид перемещён');
  await expectNoError(page);
  const isoAfter = (await state(page)).description!.sheets[0].views[3].bounds!;
  expect(isoAfter.minX - isoBefore.minX).toBeCloseTo(40 / zoom, 6);
  expect(isoAfter.minY - isoBefore.minY).toBeCloseTo(-20 / zoom, 6);
  const frontBounds = (await state(page)).description!.sheets[0].views[0].bounds!;
  expect(frontBounds.maxX - frontBounds.minX).toBeCloseTo(60, 6); // the front view stays

  // Model change: A = 80; the drawing reports the change, the update recomputes the dimension.
  await page.getByTestId('variable-A').fill('80');
  await page.getByTestId('variable-A').press('Enter');
  await expect(page.getByTestId('model-state')).toContainText('Модель изменилась');
  await expect(dim).toHaveAttribute('data-value', '60');
  await page.getByTestId('update-from-model').click();
  await expect(page.getByTestId('report-stale')).toContainText('S1/D1: 60 → 80');
  await expect(page.getByTestId('report-dangling').locator('li')).toHaveCount(0);
  await expect(dim).toHaveAttribute('data-value', '80');
  await expect(page.getByTestId('model-state')).toHaveText('Чертёж соответствует модели');
  await expect(page.getByTestId('sheet-svg').locator('text', { hasText: /^80$/ })).toHaveCount(1);
  await expectNoError(page);

  // Screenshot of the editor for the documentation (BENDCORE_DOC_SCREENSHOT) or the test output.
  const shot = process.env.BENDCORE_DOC_SCREENSHOT || path.join(root, 'test-results/drawing/drawing_editor.png');
  fs.mkdirSync(path.dirname(shot), { recursive: true });
  await page.screenshot({ path: shot });

  // PDF export: the downloaded file is a one-page A3 PDF.
  const [download] = await Promise.all([page.waitForEvent('download'), page.getByTestId('export-pdf').click()]);
  const file = path.join(root, 'test-results/drawing', download.suggestedFilename());
  await download.saveAs(file);
  expect(download.suggestedFilename()).toMatch(/\.pdf$/);
  const bytes = fs.readFileSync(file);
  expect(bytes.subarray(0, 5).toString('latin1')).toBe('%PDF-');
  if (spawnSync('sh', ['-c', 'command -v pdfinfo']).status === 0) {
    const info = spawnSync('pdfinfo', [file], { encoding: 'utf8' });
    expect(info.status, info.stderr).toBe(0);
    expect(info.stdout).toMatch(/Pages:\s+1/);
    expect(info.stdout).toMatch(/\(A3\)/);
  }
  if (spawnSync('sh', ['-c', 'command -v pdftotext']).status === 0) {
    const text = spawnSync('pdftotext', [file, '-'], { encoding: 'utf8' });
    expect(text.status).toBe(0);
    expect(text.stdout).toContain('80');
    expect(text.stdout).toContain('РД.10.001');
    expect(text.stdout).toContain('Иванов');
  }

  // DXF of the sheet downloads as well.
  const [dxf] = await Promise.all([page.waitForEvent('download'), page.getByTestId('export-dxf').click()]);
  expect(dxf.suggestedFilename()).toMatch(/_S1\.dxf$/);
});
