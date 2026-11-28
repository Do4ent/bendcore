// ISO sheet style, projection method and auxiliary view end to end (docs/ui/DRAWING_UI.md,
// «Оформление листа ISO 7200, метод проекций, дополнительный вид»). On the reference bracket
// (tests/wasm/fixtures/bracket.part.json, A = 60 along X, B = 50 along Z, Wd = 30) the user adds
// the front, top and left views; switches the projection method to the third angle — the top view
// moves above the front view and the left view to its left, the images stay —; chooses the ISO 7200
// sheet style and fills its fields — the kernel's SVG of the sheet has the ISO field names and the
// projection symbol (two circles left of the cone for the third angle), the DXF the field texts —;
// adds an auxiliary view with an arrow at −45° on the front view: the arrow with the letter «А»
// stands at the upper left of the front view, the label «А» above the new view, and «повернуто»
// turns it by −45° with the rotation sign in the label.
import { expect, test, Page } from '@playwright/test';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const bracket = path.join(root, 'tests/wasm/fixtures/bracket.part.json');

type Bounds = { minX: number; minY: number; maxX: number; maxY: number };
interface ViewState {
  id: string;
  kind: string;
  label?: string;
  letter?: string;
  rotated?: boolean;
  rotationDeg?: number;
  bounds?: Bounds;
  arrow?: { tip: [number, number]; direction: [number, number]; letterPosition: [number, number] };
}
interface EditorState {
  error: { code: string; message: string } | null;
  message: string;
  description: {
    projection: string;
    sheetStyle: string;
    sheets: {
      id: string;
      views: ViewState[];
      title: Record<string, unknown>;
      isoTitleShown?: Record<string, string | number>;
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

const centre = (b: Bounds): [number, number] => [(b.minX + b.maxX) / 2, (b.minY + b.maxY) / 2];
const viewOf = async (page: Page, id: string) => (await state(page)).description!.sheets[0].views.find((v) => v.id === id)!;

test('third-angle layout, ISO 7200 title block with the projection symbol, auxiliary view with an arrow', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });
  await page.getByTestId('open-model').setInputFiles(bracket);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  await page.getByTestId('add-base').click();
  await expect(page.getByTestId('view-V1')).toHaveCount(1);
  await page.getByTestId('add-top').click();
  await page.getByTestId('add-left').click();
  await expect(page.getByTestId('view-list').locator('li')).toHaveCount(3);
  await expectNoError(page);

  // First angle (ESKD): the top view V2 below the front view V1, the left view V3 right of it.
  await expect(page.getByTestId('projection-method')).toHaveValue('firstAngle');
  const f0 = centre((await viewOf(page, 'V1')).bounds!);
  const t0 = (await viewOf(page, 'V2')).bounds!;
  expect(centre(t0)[1]).toBeLessThan(f0[1]);
  expect(centre((await viewOf(page, 'V3')).bounds!)[0]).toBeGreaterThan(f0[0]);

  // Third angle (ISO A): the top view above, the left view on the left, same sizes; ESKD warns.
  await page.getByTestId('projection-method').selectOption('thirdAngle');
  await expect(page.getByTestId('status')).toContainText('третий угол');
  await expect(page.getByTestId('status')).toContainText('third-angle');
  await expect.poll(async () => (await state(page)).description!.projection).toBe('thirdAngle');
  const f1 = centre((await viewOf(page, 'V1')).bounds!);
  const t1 = (await viewOf(page, 'V2')).bounds!;
  expect(centre(t1)[1]).toBeGreaterThan(f1[1]);
  expect(Math.abs(centre(t1)[0] - f1[0])).toBeLessThan(1e-6);
  expect(Math.abs(t1.maxX - t1.minX - (t0.maxX - t0.minX))).toBeLessThan(1e-6);
  expect(centre((await viewOf(page, 'V3')).bounds!)[0]).toBeLessThan(f1[0]);

  // ISO 7200 sheet style: the ISO title block form; fields applied to the sheet.
  await page.getByTestId('sheet-style').selectOption('iso7200');
  await expect(page.getByTestId('title-block-iso')).toBeVisible();
  await expectNoError(page);
  await page.getByTestId('title-iso-organization').fill('ООО «Бенд»');
  await page.getByTestId('title-iso-documentStatus').fill('Выпущен');
  await page.getByTestId('title-iso-revision').fill('B');
  await page.getByTestId('title-iso-issueDate').fill('2026-11-21');
  await page.getByTestId('title-iso-developer').fill('Иванов');
  await page.getByTestId('title-iso-approver').fill('Петров');
  await page.getByTestId('title-iso-department').fill('КБ-3');
  await page.getByTestId('title-apply').click();
  await expect.poll(async () => (await state(page)).description!.sheets[0].title.revision).toBe('B');
  await expectNoError(page);
  const iso = (await state(page)).description!.sheets[0].isoTitleShown!;
  expect(iso.projectionSymbol).toBe('thirdAngle');
  expect(iso.legalOwner).toBe('ООО «Бенд»');
  expect(iso.documentType).toBe('Чертёж детали');
  expect(iso.sheet).toBe(1);
  // The kernel's SVG of the sheet: ISO field names and values, no GOST 2.104 headers.
  const svg = page.getByTestId('sheet-svg');
  for (const text of ['Владелец', 'Обозначение', 'Дата выпуска', 'Статус документа', 'Выпущен', '2026-11-21', 'КБ-3', 'Петров'])
    await expect(svg.locator(`[data-text="${text}"]`).first()).toHaveCount(1);
  await expect(svg.locator('[data-text="Разраб."]')).toHaveCount(0);
  // Projection symbol of the third angle: two circles (r 1,25 and 2,5) left of the cone, in the
  // cell (230…245) × (28…37) of A3: centre x = 237,5 − 3,75.
  const circles = await svg.locator('circle').evaluateAll((cs) => cs.map((c) => [Number(c.getAttribute('cx')), Number(c.getAttribute('r'))]));
  const symbol = circles.filter(([cx]) => cx > 230 && cx < 245);
  expect(symbol.map(([, r]) => r).sort()).toEqual([1.25, 2.5]);
  for (const [cx] of symbol) expect(Math.abs(cx - 233.75)).toBeLessThan(1e-3);
  // DXF of the sheet carries the field texts.
  const [dxfDownload] = await Promise.all([page.waitForEvent('download'), page.getByTestId('export-dxf').click()]);
  const dxf = fs.readFileSync((await dxfDownload.path())!, 'utf8');
  expect(dxf).toContain('2026-11-21');
  expect(dxf).toContain('\\U+0412\\U+043B\\U+0430\\U+0434\\U+0435\\U+043B\\U+0435\\U+0446'); // «Владелец»

  // Auxiliary view: arrow at −45° on the front view (selected), letter А.
  await page.getByTestId('view-list').locator('li').nth(0).click();
  await page.getByTestId('aux-angle').fill('-45');
  await page.getByTestId('add-auxiliary').click();
  await expect(page.getByTestId('view-list').locator('li')).toHaveCount(4);
  await expectNoError(page);
  const aux = await viewOf(page, 'V4');
  expect(aux.kind).toBe('auxiliary');
  expect(aux.letter).toBe('А');
  expect(aux.label).toBe('А');
  expect(aux.rotationDeg).toBe(0);
  const k = Math.SQRT1_2;
  expect(Math.abs(aux.arrow!.direction[0] - k)).toBeLessThan(1e-9);
  expect(Math.abs(aux.arrow!.direction[1] + k)).toBeLessThan(1e-9);
  const fb = (await viewOf(page, 'V1')).bounds!;
  expect(aux.arrow!.tip[0] < fb.minX || aux.arrow!.tip[1] > fb.maxY).toBe(true);
  expect(aux.arrow!.tip[0]).toBeLessThan(centre(fb)[0]);
  // The letter А twice on the sheet: at the arrow and above the view.
  await expect(svg.locator('[data-text="А"]')).toHaveCount(2);
  await expect(page.getByTestId('view-list').locator('li').nth(3)).toContainText('Дополнительный вид А');

  // Rotated: −45°, the list says «А (повернуто)», the label gets the rotation sign (an arc).
  await page.getByTestId('view-list').locator('li').nth(3).click();
  const arcsBefore = await svg.locator('path').count();
  await page.getByTestId('view-rotated').click();
  await expect.poll(async () => (await viewOf(page, 'V4')).rotationDeg).toBeCloseTo(-45, 9);
  await expect(page.getByTestId('view-list').locator('li').nth(3)).toContainText('А (повернуто)');
  expect(await svg.locator('path').count()).toBeGreaterThan(arcsBefore);
  await expectNoError(page);
  // Undo returns the unrotated view.
  await page.getByTestId('tool-select').click();
  await page.locator('body').press('Control+z');
  await expect.poll(async () => (await viewOf(page, 'V4')).rotationDeg).toBe(0);
});
