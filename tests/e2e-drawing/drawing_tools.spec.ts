// Drawing editor end to end, v1.0 tools (docs/ui/DRAWING_UI.md): on the reference bracket
// (tests/wasm/fixtures/bracket.part.json, A = 60, T = 8, hole Ø8 at x = 40 through the plate
// z ∈ [0, 8], width Wd = 30) the user draws a local section around the hole (four clicks, depth 15
// — the plane through the hole axis), undoes and redoes it with Ctrl+Z / Ctrl+Y, puts chain and
// baseline dimensions by clicking three vertical edges (values T, A − T and T, A), drags a
// dimension text with the mouse, prints the sheet (the print document has @page = 420×297 mm; it
// is rendered to PDF by Chromium and pdfinfo reads the page size) and downloads the 1:1 PDF of the
// sheet. Optional: a part with a threaded hole (tests/wasm/fixtures/threaded_block.part.json) gets
// a thread dimension with the tolerance class typed in the panel.
import { expect, test, Page } from '@playwright/test';
import { spawnSync } from 'child_process';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const bracket = path.join(root, 'tests/wasm/fixtures/bracket.part.json');
const threaded = path.join(root, 'tests/wasm/fixtures/threaded_block.part.json');
const out = path.join(root, 'test-results/drawing');

type Bounds = { minX: number; minY: number; maxX: number; maxY: number };
interface EditorState {
  error: { code: string; message: string } | null;
  description: {
    history: { canUndo: boolean; canRedo: boolean; undoLabel: string };
    sheets: {
      id: string;
      width: number;
      height: number;
      views: { id: string; kind: string; bounds?: Bounds; localSections: { points: [number, number][]; depth: number }[] }[];
      dimensions: { id: string; value?: number; labelPosition?: [number, number] }[];
      threadNotes: { id: string; text?: string; state: string | null; toleranceField: string }[];
    }[];
  } | null;
}

const state = (page: Page) =>
  page.evaluate(async () => {
    const w = window as unknown as { bendcoreDrawingEditor: EditorState; bendcoreDrawingEditorIdle: () => Promise<void> };
    await w.bendcoreDrawingEditorIdle();
    return w.bendcoreDrawingEditor;
  });

async function expectNoError(page: Page) {
  const s = await state(page);
  expect(s.error, s.error ? `${s.error.code}: ${s.error.message}` : '').toBeNull();
}

/** Screen point of a sheet point (mm, y up): the sheet element spans W × H mm at data-zoom px/mm. */
async function screenOf(page: Page, p: [number, number]) {
  const sheet = page.getByTestId('sheet');
  const box = (await sheet.boundingBox())!;
  const zoom = Number(await sheet.getAttribute('data-zoom'));
  const H = box.height / zoom;
  return { x: box.x + zoom * p[0], y: box.y + zoom * (H - p[1]), zoom };
}

/** Screen point in the middle of a visible vertical edge of the view at sheet x (± 1e-6). */
async function verticalEdgeAt(page: Page, view: string, x: number) {
  const edges = page.locator(`[data-view="${view}"][data-kind="line"][data-visible="1"]`);
  const n = await edges.count();
  let best: { id: string; len: number } | null = null;
  for (let i = 0; i < n; ++i) {
    const pts = JSON.parse((await edges.nth(i).getAttribute('data-points'))!) as [number, number][];
    if (Math.abs(pts[0][0] - pts[1][0]) > 1e-6 || Math.abs(pts[0][0] - x) > 1e-6) continue;
    const len = Math.abs(pts[1][1] - pts[0][1]);
    if (!best || len > best.len) best = { id: (await edges.nth(i).getAttribute('data-testid'))!, len };
  }
  expect(best, `vertical edge of ${view} at x = ${x}`).not.toBeNull();
  const b = (await page.getByTestId(best!.id).boundingBox())!;
  return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
}

function pdfinfo(file: string): string | null {
  if (spawnSync('sh', ['-c', 'command -v pdfinfo']).status !== 0) return null;
  const info = spawnSync('pdfinfo', [file], { encoding: 'utf8' });
  expect(info.status, info.stderr).toBe(0);
  return info.stdout;
}

test('local section, undo/redo, chain and baseline, drag of a dimension, print at 1:1', async ({ page }) => {
  fs.mkdirSync(out, { recursive: true });
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });
  await page.getByTestId('open-model').setInputFiles(bracket);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  await page.getByTestId('add-base').click();
  await expect(page.getByTestId('view-V1')).toHaveCount(1);
  await page.getByTestId('add-top').click();
  await expect(page.getByTestId('view-list').locator('li')).toHaveCount(2);
  await expectNoError(page);

  // Front view V1: bounds = profile 60 × 50 at the sheet scale; view point (x, z) → sheet.
  let s = await state(page);
  const b = s.description!.sheets[0].views[0].bounds!;
  const k = (b.maxX - b.minX) / 60;
  const sheetPt = (x: number, z: number): [number, number] => [b.minX + k * x, b.minY + k * z];

  // Local section: four clicks around the hole, depth 15, then «Готово».
  await page.getByTestId('tool-localSection').click();
  await page.getByTestId('local-depth').fill('15');
  for (const [x, z] of [[33, 1], [47, 1], [47, 7], [33, 7]] as [number, number][]) {
    const p = await screenOf(page, sheetPt(x, z));
    await page.mouse.click(p.x, p.y);
  }
  await expect(page.getByTestId('outline')).toHaveCount(1);
  await page.getByTestId('local-done').click();
  await expect(page.getByTestId('status')).toContainText('Местный разрез добавлен');
  await expectNoError(page);
  s = await state(page);
  const ls = s.description!.sheets[0].views[0].localSections;
  expect(ls).toHaveLength(1);
  expect(ls[0].depth).toBe(15);
  expect(ls[0].points[0][0]).toBeCloseTo(33, 1);
  expect(ls[0].points[0][1]).toBeCloseTo(1, 1);
  // The walls of the hole inside the region are visible lines now (x = 36 and 44).
  await verticalEdgeAt(page, 'V1', sheetPt(36, 0)[0]);
  await verticalEdgeAt(page, 'V1', sheetPt(44, 0)[0]);
  expect(s.description!.history.undoLabel).toBe('addLocalSection');

  // Ctrl+Z removes it, Ctrl+Y brings it back.
  await page.getByTestId('tool-select').click();
  await page.locator('body').press('Control+z');
  await expect(page.getByTestId('status')).toContainText('Отменено');
  s = await state(page);
  expect(s.description!.sheets[0].views[0].localSections).toHaveLength(0);
  expect(s.description!.history.canRedo).toBe(true);
  await page.locator('body').press('Control+y');
  await expect(page.getByTestId('status')).toContainText('Повторено');
  s = await state(page);
  expect(s.description!.sheets[0].views[0].localSections).toHaveLength(1);
  await expectNoError(page);

  // Chain: x = 0, x = T = 8, x = A = 60 → 8 and 52; baseline → 8 and 60.
  const xs = [0, 8, 60].map((x) => sheetPt(x, 0)[0]);
  for (const [tool, expected] of [['chain', [8, 52]], ['baseline', [8, 60]]] as [string, number[]][]) {
    const before = (await state(page)).description!.sheets[0].dimensions.length;
    await page.getByTestId(`tool-${tool}`).click();
    for (const x of xs) {
      const p = await verticalEdgeAt(page, 'V1', x);
      await page.mouse.click(p.x, p.y);
    }
    await expect(page.getByTestId('picks')).toHaveText('Выбрано рёбер: 3');
    await page.getByTestId('group-create').click();
    await expect(page.getByTestId('status')).toContainText(tool === 'chain' ? 'цепочкой' : 'от базы');
    await expectNoError(page);
    const dims = (await state(page)).description!.sheets[0].dimensions.slice(before);
    expect(dims.map((d) => Math.round(d.value! * 1e6) / 1e6).sort((a, c) => a - c)).toEqual(expected);
  }

  // Drag of the A = 60 text by (30, −20) px: the text follows the pointer (a text that does not fit
  // between the extension lines stands outside them and moves only across the dimension line).
  await page.getByTestId('tool-select').click();
  const all = (await state(page)).description!.sheets[0].dimensions;
  const d0 = all.reduce((a, c) => (c.value! > a.value! ? c : a));
  const handle = page.getByTestId(`handle-dimension-${d0.id}`);
  await expect(handle).toHaveCount(1);
  const hb = (await handle.boundingBox())!;
  const start = { x: hb.x + hb.width / 2, y: hb.y + hb.height / 2 };
  const zoom = Number(await page.getByTestId('sheet').getAttribute('data-zoom'));
  await page.mouse.move(start.x, start.y);
  await page.mouse.down();
  await page.mouse.move(start.x + 15, start.y - 10, { steps: 3 });
  await page.mouse.move(start.x + 30, start.y - 20, { steps: 3 });
  await page.mouse.up();
  await expect(page.getByTestId('status')).toContainText('Размер перенесён');
  await expectNoError(page);
  const d1 = (await state(page)).description!.sheets[0].dimensions.find((d) => d.id === d0.id)!;
  expect(d1.labelPosition![0] - d0.labelPosition![0]).toBeCloseTo(30 / zoom, 3);
  expect(d1.labelPosition![1] - d0.labelPosition![1]).toBeCloseTo(20 / zoom, 3);
  expect(d1.value).toBe(d0.value);

  // Print: the print document has the page size of the sheet (A3 landscape 420 × 297 mm).
  await page.evaluate(() => {
    const w = window as unknown as { bendcoreDrawingPrintHook: (html: string) => void; printed: string };
    w.bendcoreDrawingPrintHook = (html) => {
      w.printed = html;
    };
  });
  await page.getByTestId('drawing-print').click();
  await expect(page.getByTestId('status')).toContainText('отправлен на печать (420×297 мм)');
  const html = await page.evaluate(() => (window as unknown as { printed: string }).printed);
  expect(html).toContain('@page { size: 420mm 297mm; margin: 0; }');
  expect(html).toMatch(/<svg width="420mm" height="297mm"/);
  // Chromium's print of that document: the PDF page is 420 × 297 mm (1190.55 × 841.89 pt).
  const printPage = await page.context().newPage();
  await printPage.setContent(html, { waitUntil: 'load' });
  const printed = path.join(out, 'print_a3.pdf');
  await printPage.pdf({ path: printed, preferCSSPageSize: true, printBackground: true });
  await printPage.close();
  const info = pdfinfo(printed);
  if (info) {
    const m = /Page size:\s+([\d.]+) x ([\d.]+) pts/.exec(info)!;
    // Chromium rounds the page to whole CSS pixels (1 px = 0,26 mm): within 1 pt = 0,35 mm.
    expect(Math.abs(Number(m[1]) - (420 * 72) / 25.4)).toBeLessThan(1);
    expect(Math.abs(Number(m[2]) - (297 * 72) / 25.4)).toBeLessThan(1);
    expect(info).toMatch(/Pages:\s+1/);
  }

  // PDF of the sheet at 1:1: one A3 page.
  const [download] = await Promise.all([page.waitForEvent('download'), page.getByTestId('export-sheet-pdf').click()]);
  expect(download.suggestedFilename()).toMatch(/_S1\.pdf$/);
  const file = path.join(out, download.suggestedFilename());
  await download.saveAs(file);
  expect(fs.readFileSync(file).subarray(0, 5).toString('latin1')).toBe('%PDF-');
  const sheetInfo = pdfinfo(file);
  if (sheetInfo) {
    expect(sheetInfo).toMatch(/Pages:\s+1/);
    expect(sheetInfo).toMatch(/\(A3\)/);
  }
});

test('thread dimension with the tolerance class from the panel', async ({ page }) => {
  test.skip(!fs.existsSync(threaded), 'no threaded_block.part.json fixture');
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });
  await page.getByTestId('open-model').setInputFiles(threaded);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  await page.getByTestId('add-base').click();
  await expect(page.getByTestId('view-V1')).toHaveCount(1);
  await expectNoError(page);
  // Front view of the block 60 × 20 (x, z): the M10 thread at x = 20 from the top z = 20.
  const s = await state(page);
  const b = s.description!.sheets[0].views[0].bounds!;
  const k = (b.maxX - b.minX) / 60;
  await page.getByTestId('tool-thread').click();
  await page.getByTestId('thread-tolerance').fill('6H');
  const p = await screenOf(page, [b.minX + k * 25, b.minY + k * 15]);
  await page.mouse.click(p.x, p.y);
  await expect(page.getByTestId('status')).toContainText('Размер резьбы добавлен');
  await expectNoError(page);
  const note = page.getByTestId('thread-TH1');
  await expect(note).toHaveAttribute('data-text', 'M10×1,25LH-6H');
  await expect(page.getByTestId('sheet-svg').locator('text', { hasText: 'M10×1,25LH-6H' })).toHaveCount(1);
  // Tolerance class changed in the list.
  await page.getByTestId('thread-tolerance-TH1').fill('7H');
  await page.getByTestId('thread-tolerance-TH1').press('Enter');
  await expect(note).toHaveAttribute('data-text', 'M10×1,25LH-7H');
  await expectNoError(page);
});
