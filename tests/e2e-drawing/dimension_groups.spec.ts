// Chain and baseline dimensions (ГОСТ 2.307) in the drawing editor end to end
// (docs/ui/DRAWING_UI.md, "Размеры цепочкой и от базы"). On the reference bracket
// (tests/wasm/fixtures/bracket.part.json, A = 60, T = 8) the user picks three vertical edges of the
// front view with the «Цепочка» tool — two dimensions T and A − T on one dimension line, one group;
// drags one of them — the whole chain moves; with «От базы» a step below 7 mm is refused, 7 mm gives
// T and A with the smaller one nearer to the contour and the lines 7 mm apart; the model change
// T = 20 updates the values and the groups keep their line and step; the saved file keeps the groups.
import { expect, test, Page } from '@playwright/test';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const bracket = path.join(root, 'tests/wasm/fixtures/bracket.part.json');
const out = path.join(root, 'test-results/drawing');

type Bounds = { minX: number; minY: number; maxX: number; maxY: number };
interface Dim {
  id: string;
  value?: number;
  labelPosition?: [number, number];
  dimensionLine?: [[number, number], [number, number]];
  group: string | null;
  groupMode?: string;
}
interface EditorState {
  error: { code: string; message: string } | null;
  description: { sheets: { id: string; views: { id: string; bounds?: Bounds }[]; dimensions: Dim[] }[] } | null;
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

/** Screen point in the middle of the longest visible vertical edge of the view at sheet x. */
async function verticalEdgeAt(page: Page, view: string, x: number) {
  const edges = page.locator(`[data-view="${view}"][data-kind="line"][data-visible="1"]`);
  let best: { id: string; len: number } | null = null;
  for (let i = 0; i < (await edges.count()); ++i) {
    const pts = JSON.parse((await edges.nth(i).getAttribute('data-points'))!) as [number, number][];
    if (Math.abs(pts[0][0] - pts[1][0]) > 1e-6 || Math.abs(pts[0][0] - x) > 1e-6) continue;
    const len = Math.abs(pts[1][1] - pts[0][1]);
    if (!best || len > best.len) best = { id: (await edges.nth(i).getAttribute('data-testid'))!, len };
  }
  expect(best, `vertical edge of ${view} at x = ${x}`).not.toBeNull();
  const b = (await page.getByTestId(best!.id).boundingBox())!;
  return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
}

const lineY = (d: Dim) => d.dimensionLine![0][1];
const groupOf = async (page: Page, group: string) =>
  (await state(page)).description!.sheets[0].dimensions.filter((d) => d.group === group).sort((a, b) => a.value! - b.value!);

test('chain and baseline dimensions: one line, 7 mm step, group drag, associativity', async ({ page }) => {
  fs.mkdirSync(out, { recursive: true });
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });
  await page.getByTestId('open-model').setInputFiles(bracket);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  await page.getByTestId('add-base').click();
  await expect(page.getByTestId('view-V1')).toHaveCount(1);
  await expectNoError(page);
  const b = (await state(page)).description!.sheets[0].views[0].bounds!;
  const k = (b.maxX - b.minX) / 60;
  const xs = [0, 8, 60].map((x) => b.minX + k * x);
  const pick = async () => {
    for (const x of xs) {
      const p = await verticalEdgeAt(page, 'V1', x);
      await page.mouse.click(p.x, p.y);
    }
    await expect(page.getByTestId('picks')).toHaveText('Выбрано рёбер: 3');
  };

  // «Цепочка»: T and A − T on one dimension line, one group.
  await page.getByTestId('tool-chain').click();
  await pick();
  await page.getByTestId('group-create').click();
  await expect(page.getByTestId('status')).toContainText('цепочкой');
  await expectNoError(page);
  const chainGroup = (await state(page)).description!.sheets[0].dimensions[0].group!;
  expect(chainGroup).toMatch(/^G\d+$/);
  let chain = await groupOf(page, chainGroup);
  expect(chain.map((d) => d.value)).toEqual([8, 52]);
  expect(chain.every((d) => d.groupMode === 'chain')).toBe(true);
  expect(lineY(chain[0])).toBeCloseTo(lineY(chain[1]), 6);
  for (const d of chain) await expect(page.getByTestId(`dimension-${d.id}`)).toHaveAttribute('data-group', chainGroup);
  await expect(page.getByTestId(`dimension-${chain[0].id}`)).toContainText(`цепочка ${chainGroup}`);

  // Drag of one link by 20 px away from the view: the whole chain moves, still one line.
  await page.getByTestId('tool-select').click();
  const zoom = Number(await page.getByTestId('sheet').getAttribute('data-zoom'));
  const below = lineY(chain[0]) < b.minY;
  const handle = page.getByTestId(`handle-dimension-${chain[1].id}`);
  const hb = (await handle.boundingBox())!;
  const start = { x: hb.x + hb.width / 2, y: hb.y + hb.height / 2 };
  const dy = below ? 20 : -20; // screen y grows downwards
  await page.mouse.move(start.x, start.y);
  await page.mouse.down();
  await page.mouse.move(start.x, start.y + dy / 2, { steps: 3 });
  await page.mouse.move(start.x, start.y + dy, { steps: 3 });
  await page.mouse.up();
  await expect(page.getByTestId('status')).toContainText('Размер перенесён');
  await expectNoError(page);
  const moved = await groupOf(page, chainGroup);
  expect(lineY(moved[0])).toBeCloseTo(lineY(moved[1]), 6);
  expect(Math.abs(lineY(moved[0]) - lineY(chain[0]))).toBeCloseTo(20 / zoom, 1);
  chain = moved;

  // «От базы»: a step below 7 mm is refused (ГОСТ 2.307), 7 mm — T nearer to the contour.
  await page.getByTestId('tool-baseline').click();
  await pick();
  await page.getByTestId('group-spacing').fill('5');
  await expect(page.getByTestId('group-create')).toBeDisabled();
  await page.getByTestId('group-spacing').fill('7');
  await expect(page.getByTestId('group-create')).toBeEnabled();
  await page.getByTestId('group-create').click();
  await expect(page.getByTestId('status')).toContainText('от базы');
  await expectNoError(page);
  const all = (await state(page)).description!.sheets[0].dimensions;
  const baseGroup = all.find((d) => d.groupMode === 'baseline')!.group!;
  expect(baseGroup).not.toBe(chainGroup);
  const checkBaseline = async (small: number) => {
    const base = await groupOf(page, baseGroup);
    expect(base.map((d) => d.value)).toEqual([small, 60]);
    const view = (await state(page)).description!.sheets[0].views[0].bounds!;
    const dist = (d: Dim) => Math.min(Math.abs(lineY(d) - view.minY), Math.abs(lineY(d) - view.maxY));
    expect(dist(base[0])).toBeLessThan(dist(base[1]));
    expect(Math.abs(lineY(base[1]) - lineY(base[0]))).toBeCloseTo(7, 6);
  };
  await checkBaseline(8);

  // Model change T = 20: values follow, the chain stays on its line, the baseline keeps its step.
  await page.getByTestId('variable-T').fill('20');
  await page.getByTestId('variable-T').press('Enter');
  await expect(page.getByTestId('model-state')).toContainText('Модель изменилась');
  await page.getByTestId('update-from-model').click();
  await expect(page.getByTestId('model-state')).toHaveText('Чертёж соответствует модели');
  await expect(page.getByTestId('report-dangling').locator('li')).toHaveCount(0);
  await expectNoError(page);
  const chain2 = await groupOf(page, chainGroup);
  expect(chain2.map((d) => d.value)).toEqual([20, 40]);
  expect(lineY(chain2[0])).toBeCloseTo(lineY(chain2[1]), 6);
  expect(lineY(chain2[0])).toBeCloseTo(lineY(chain[0]), 6);
  await checkBaseline(20);

  // Saved file: the groups with their mode and step.
  const [download] = await Promise.all([page.waitForEvent('download'), page.getByTestId('drawing-save').click()]);
  const saved = path.join(out, download.suggestedFilename());
  await download.saveAs(saved);
  const doc = JSON.parse(JSON.parse(fs.readFileSync(saved, 'utf8')).drawing);
  const dims = doc.sheets[0].dimensions as { group?: string; groupMode?: string; groupSpacing?: number }[];
  expect(dims.filter((d) => d.group === chainGroup && d.groupMode === 'chain')).toHaveLength(2);
  expect(dims.filter((d) => d.group === baseGroup && d.groupMode === 'baseline' && d.groupSpacing === 7)).toHaveLength(2);
});
