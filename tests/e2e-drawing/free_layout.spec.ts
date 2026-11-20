// Free placement of views and automatic arrangement of dimensions (docs/ui/DRAWING_UI.md,
// «Свободное размещение видов», «Автокомпоновка размеров»). On the reference bracket
// (tests/wasm/fixtures/bracket.part.json, A = 60, B = 50, Wd = 30) the user adds the front and top
// views and a section through the wall outside the projection link, sets its scale to 5:1 — larger
// than its auto-layout cell: the scale stays 5:1 and the kernel warns —, then 4:1 and drags it to
// the free upper right part of the sheet (it lands exactly where it was dropped, at 4:1, without
// warnings), then drags it onto the front view (an overlap warning, not an error). Two dimensions
// of the front view are arranged with the «Автокомпоновка размеров» button: the values stay, the
// texts move out of the view by the GOST 2.307 distances.
import { expect, test, Page } from '@playwright/test';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const bracket = path.join(root, 'tests/wasm/fixtures/bracket.part.json');

type Bounds = { minX: number; minY: number; maxX: number; maxY: number };
interface EditorState {
  error: { code: string; message: string } | null;
  message: string;
  description: {
    sheets: {
      id: string;
      width: number;
      height: number;
      views: { id: string; kind: string; bounds?: Bounds; scale: { label: string } | null; computedScale?: { label: string; ratio: [number, number] }; position: [number, number] | null }[];
      dimensions: { id: string; view: string; value?: number; offset: number; labelPosition?: [number, number] }[];
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

async function screenOf(page: Page, p: [number, number]) {
  const sheet = page.getByTestId('sheet');
  const box = (await sheet.boundingBox())!;
  const zoom = Number(await sheet.getAttribute('data-zoom'));
  const H = box.height / zoom;
  return { x: box.x + zoom * p[0], y: box.y + zoom * (H - p[1]), zoom };
}

const centre = (b: Bounds): [number, number] => [(b.minX + b.maxX) / 2, (b.minY + b.maxY) / 2];

async function edgeOf(page: Page, view: string, vertical: boolean, which: 'min' | 'max') {
  const c = vertical ? 0 : 1;
  const edges = page.locator(`[data-view="${view}"][data-kind="line"][data-visible="1"]`);
  const n = await edges.count();
  let best: { id: string; v: number; len: number } | null = null;
  for (let i = 0; i < n; ++i) {
    const pts = JSON.parse((await edges.nth(i).getAttribute('data-points'))!) as [number, number][];
    if (Math.abs(pts[0][c] - pts[1][c]) > 1e-6) continue;
    const v = pts[0][c];
    const len = Math.abs(pts[1][1 - c] - pts[0][1 - c]);
    if (!best || (which === 'min' ? v < best.v - 1e-6 : v > best.v + 1e-6) || (Math.abs(v - best.v) <= 1e-6 && len > best.len))
      best = { id: (await edges.nth(i).getAttribute('data-testid'))!, v, len };
  }
  expect(best).not.toBeNull();
  const b = (await page.getByTestId(best!.id).boundingBox())!;
  return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
}

async function drag(page: Page, from: { x: number; y: number }, to: { x: number; y: number }) {
  await page.mouse.move(from.x, from.y);
  await page.mouse.down();
  await page.mouse.move((from.x + to.x) / 2, (from.y + to.y) / 2, { steps: 4 });
  await page.mouse.move(to.x, to.y, { steps: 4 });
  await page.mouse.up();
}

test('a section outside the projection link at its own scale dragged freely; dimensions arranged by GOST 2.307', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });
  await page.getByTestId('open-model').setInputFiles(bracket);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  await page.getByTestId('add-base').click();
  await expect(page.getByTestId('view-V1')).toHaveCount(1);
  await page.getByTestId('add-top').click();
  await expect(page.getByTestId('view-list').locator('li')).toHaveCount(2);
  await expectNoError(page);

  // Section through the wall of the bracket (x ≈ 4 of T = 8), outside the projection link: an L
  // of 30 × 50 mm, at 5:1 taller than the column cell.
  await page.getByTestId('tool-section').click();
  await page.getByTestId('section-side').selectOption('none');
  let s = await state(page);
  const front = s.description!.sheets[0].views[0].bounds!;
  const p = await screenOf(page, [front.minX + 0.07 * (front.maxX - front.minX), front.minY + 0.5 * (front.maxY - front.minY)]);
  await page.mouse.click(p.x, p.y);
  await expect(page.getByTestId('view-list').locator('li')).toHaveCount(3);
  await expectNoError(page);
  s = await state(page);
  const sec0 = s.description!.sheets[0].views[2];
  expect(sec0.kind).toBe('section');
  const scale0 = sec0.computedScale!.ratio[0] / sec0.computedScale!.ratio[1];

  // Scale 5:1 chosen in the properties: kept although the view does not fit its cell.
  await page.getByTestId('tool-select').click();
  await page.getByTestId('view-list').locator('li').nth(2).click();
  await page.getByTestId('view-scale').selectOption('5:1');
  await expect(page.getByTestId('status')).toContainText('does not fit its cell');
  await expectNoError(page);
  s = await state(page);
  const sec1 = s.description!.sheets[0].views[2];
  expect(sec1.computedScale!.label).toBe('5:1');
  const w0 = sec0.bounds!.maxX - sec0.bounds!.minX;
  expect(sec1.bounds!.maxX - sec1.bounds!.minX).toBeCloseTo((w0 * 5) / scale0, 6);

  // 4:1 (120 × 214 mm with the label) and a drag to the free upper right part of the sheet, past the
  // column cell: the picture's centre lands at the drop point, no overlap and frame warnings.
  await page.getByTestId('view-scale').selectOption('4:1');
  await expectNoError(page);
  s = await state(page);
  const sec1b = s.description!.sheets[0].views[2];
  expect(sec1b.computedScale!.label).toBe('4:1');
  const sheet = s.description!.sheets[0];
  const w4 = sec1b.bounds!.maxX - sec1b.bounds!.minX, h4 = sec1b.bounds!.maxY - sec1b.bounds!.minY;
  expect(w4).toBeCloseTo((w0 * 4) / scale0, 6);
  const target: [number, number] = [sheet.width - 5 - w4 / 2 - 5, sheet.height - 5 - 14 - h4 / 2 - 5];
  const from = await screenOf(page, centre(sec1b.bounds!));
  const to = await screenOf(page, target);
  await drag(page, from, to);
  await expect(page.getByTestId('status')).toContainText('Вид перемещён');
  await expect(page.getByTestId('status')).not.toContainText('overlap');
  await expect(page.getByTestId('status')).not.toContainText('beyond');
  await expectNoError(page);
  s = await state(page);
  const sec2 = s.description!.sheets[0].views[2];
  expect(sec2.computedScale!.label).toBe('4:1');
  expect(sec2.position).not.toBeNull();
  const c2 = centre(sec2.bounds!);
  expect(c2[0]).toBeCloseTo(target[0], 0);
  expect(c2[1]).toBeCloseTo(target[1], 0);
  expect(sec2.bounds!.maxX - sec2.bounds!.minX).toBeCloseTo(w4, 6);

  // Dropped onto the front view: a warning in the status line, the drawing stays valid.
  const v1 = s.description!.sheets[0].views[0].bounds!;
  // Its left edge 10 mm into the front view; grabbed off its centre lines.
  const grab: [number, number] = [sec2.bounds!.minX + 0.3 * w4, sec2.bounds!.minY + 0.3 * h4];
  const into: [number, number] = [v1.maxX - 10 + 0.3 * w4, grab[1]];
  await drag(page, await screenOf(page, grab), await screenOf(page, into));
  await expect(page.getByTestId('status')).toContainText("views 'V1' and 'V3' overlap");
  await expectNoError(page);
  // Back to automatic placement for the rest of the test.
  await page.getByTestId('view-list').locator('li').nth(2).click();
  await page.getByTestId('view-scale').selectOption('');
  await page.getByRole('button', { name: 'Автоматическое положение' }).click();
  await expectNoError(page);

  // Two dimensions of the front view: A = 60 between the outer vertical edges, B = 50 between the
  // outer horizontal edges (put by the tool 10 mm from the picked points, inside the GOST distances).
  await page.getByTestId('tool-dimension').click();
  for (const vertical of [true, false]) {
    const a = await edgeOf(page, 'V1', vertical, 'min');
    const b = await edgeOf(page, 'V1', vertical, 'max');
    await page.mouse.click(a.x, a.y);
    await page.mouse.click(b.x, b.y);
  }
  await expect(page.getByTestId('dimension-D2')).toHaveCount(1);
  await expectNoError(page);
  s = await state(page);
  const before = s.description!.sheets[0].dimensions.map((d) => d.value);
  expect(before.map((v) => Math.round(v!))).toEqual([60, 50]);

  await page.getByTestId('tool-select').click();
  await page.getByTestId('view-list').locator('li').nth(0).click();
  await page.getByTestId('view-arrange-dimensions').click();
  await expect(page.getByTestId('status')).toContainText('Размеры вида расставлены');
  await expectNoError(page);
  s = await state(page);
  const vb = s.description!.sheets[0].views[0].bounds!;
  const dims = s.description!.sheets[0].dimensions;
  expect(dims.map((d) => d.value)).toEqual(before);
  for (const d of dims) {
    const [x, y] = d.labelPosition!;
    // The dimension line is 10 mm or more from the picture; its text (3,5 mm, 1 mm above the line)
    // is outside the picture by more than 4 mm.
    const outside = Math.max(vb.minX - x, x - vb.maxX, vb.minY - y, y - vb.maxY);
    expect(outside, d.id).toBeGreaterThan(4);
  }
  // Arranging again changes nothing.
  const offsets = dims.map((d) => d.offset);
  await page.getByTestId('view-arrange-dimensions').click();
  await expect(page.getByTestId('status')).toContainText('Размеры вида расставлены');
  s = await state(page);
  expect(s.description!.sheets[0].dimensions.map((d) => d.offset)).toEqual(offsets);
});
