// Positions on an assembly drawing end to end (docs/ui/DRAWING_UI.md, "Позиции"). The shaft unit
// (bushing, shaft, two bolts with washers and nuts) is assembled in the test with the assembly
// module (tests/assembly-fixtures/shaft_unit.mjs) and opened in the drawing editor. The user turns
// on the positions of the front view by the specification, puts a manual position with a leader
// on the top line of the shaft, checks the specification tab, saves the drawing file and opens it
// again.
import { expect, test, Page } from '@playwright/test';
import { spawnSync } from 'child_process';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const out = path.join(root, 'test-results/drawing');
const assemblyDir = path.resolve(process.env.BENDCORE_ASSEMBLY_WASM_DIR || path.join(root, 'build-wasm-all/wasm_assembly'));

interface Balloon {
  id: string | null;
  definition: number;
  text: string;
  at: [number, number];
}
interface EditorState {
  status: string;
  error: { code: string; message: string } | null;
  description: {
    model: { kind: string };
    sheets: {
      id: string;
      height: number;
      views: { id: string; bounds?: { minX: number; minY: number; maxX: number; maxY: number } }[];
      balloons: Balloon[];
      positions: { id: string; state: string | null; text?: string; definition?: number; shelf?: [number, number] }[];
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

/** The shaft unit assembled by the assembly module in node; written to test-results. */
function shaftUnit(): string {
  const script = `
    const dir = ${JSON.stringify(assemblyDir)};
    const { buildShaftUnit } = await import(${JSON.stringify(path.join(root, 'tests/assembly-fixtures/shaft_unit.mjs'))});
    const api = await import(dir + '/bendcore_assembly_api.mjs');
    const k = await api.loadAssembly((await import(dir + '/bendcore_assembly.mjs')).default);
    process.stdout.write(buildShaftUnit(k));
  `;
  const run = spawnSync(process.execPath, ['--input-type=module', '-e', script], { maxBuffer: 1 << 28, encoding: 'utf8' });
  if (run.status !== 0) throw new Error(run.stderr);
  fs.mkdirSync(out, { recursive: true });
  const file = path.join(out, 'shaft_unit.bcasm.json');
  fs.writeFileSync(file, run.stdout);
  return file;
}

/** Screen point of a sheet point (mm, y up). */
async function sheetToScreen(page: Page, p: [number, number], sheetHeight: number) {
  const zoom = Number(await page.getByTestId('sheet').getAttribute('data-zoom'));
  const box = (await page.getByTestId('sheet').boundingBox())!;
  return { x: box.x + zoom * p[0], y: box.y + zoom * (sheetHeight - p[1]) };
}

test('assembly drawing: positions by the specification and a manual position with a leader', async ({ page }) => {
  const file = shaftUnit();
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });
  await page.getByTestId('open-model').setInputFiles(file);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  await expectNoError(page);
  expect((await state(page)).description!.model.kind).toBe('assembly');

  // Specification by construction: 5 positions, none on the sheet yet.
  await page.getByTestId('add-base').click();
  await expect(page.getByTestId('view-V1')).toHaveCount(1);
  await page.getByTestId('tab-specification').click();
  for (const p of [1, 2, 3, 4, 5]) await expect(page.getByTestId(`spec-position-${p}`)).toHaveAttribute('data-balloons', '0');
  await expect(page.getByTestId('balloon-missing')).toContainText('1, 2, 3, 4, 5');

  // The "Позиции" switch of the view: one balloon per position of the specification.
  await page.getByTestId('tab-specification').click();
  await page.locator('.dr-tabs button').first().click();
  await page.getByTestId('view-list').locator('li').first().click();
  // A controlled checkbox: it shows the state of the kernel after the call.
  await page.getByTestId('view-auto-balloons').click();
  await expect.poll(async () => (await state(page)).description!.sheets[0].balloons.length).toBe(5);
  await expectNoError(page);
  let sheet = (await state(page)).description!.sheets[0];
  expect(sheet.balloons.map((b) => Number(b.text)).sort()).toEqual([1, 2, 3, 4, 5]);
  const shaftDefinition = sheet.balloons.find((b) => b.text === '2')!.definition;

  // Manual position: the top line of the shaft (highest horizontal visible line of the front
  // view), then the shelf to the upper right of the view.
  await page.getByTestId('tool-balloon').click();
  const edges = page.locator('[data-view="V1"][data-kind="line"][data-visible="1"]');
  let top: { testId: string; y: number } | null = null;
  for (let i = 0; i < (await edges.count()); ++i) {
    const pts = JSON.parse((await edges.nth(i).getAttribute('data-points'))!) as [number, number][];
    if (Math.abs(pts[0][1] - pts[1][1]) > 1e-6) continue;
    if (!top || pts[0][1] > top.y) top = { testId: (await edges.nth(i).getAttribute('data-testid'))!, y: pts[0][1] };
  }
  expect(top).not.toBeNull();
  const edgeBox = (await page.getByTestId(top!.testId).boundingBox())!;
  await page.mouse.click(edgeBox.x + edgeBox.width / 2, edgeBox.y + edgeBox.height / 2);
  await expect(page.getByTestId('status')).toContainText('место полки');
  const bounds = sheet.views[0].bounds!;
  const shelf: [number, number] = [bounds.maxX + 50, bounds.maxY - 5];
  const at = await sheetToScreen(page, shelf, sheet.height);
  await page.mouse.click(at.x, at.y);
  await expect(page.getByTestId('status')).toContainText('Позиция добавлена');
  await expectNoError(page);
  const manual = page.getByTestId('balloon-P1');
  await expect(manual).toHaveAttribute('data-state', 'ok');
  await expect(manual).toHaveAttribute('data-number', '2');
  sheet = (await state(page)).description!.sheets[0];
  const p1 = sheet.positions[0];
  expect(p1.definition).toBe(shaftDefinition);
  expect(p1.shelf![0]).toBeCloseTo(shelf[0], 0);
  expect(p1.shelf![1]).toBeCloseTo(shelf[1], 0);
  // The shaft keeps one position: the automatic one gave way to the manual one.
  expect(sheet.balloons.filter((b) => b.text === '2').map((b) => b.id)).toEqual(['P1']);
  expect(sheet.balloons.map((b) => Number(b.text)).sort()).toEqual([1, 2, 3, 4, 5]);
  // The number is on the sheet drawn by the kernel.
  await expect(page.getByTestId('sheet-svg').locator('text', { hasText: /^2$/ })).toHaveCount(1);

  // The specification tab: every position once on the sheet.
  await page.getByTestId('tab-specification').click();
  for (const p of [1, 2, 3, 4, 5]) await expect(page.getByTestId(`spec-position-${p}`)).toHaveAttribute('data-balloons', '1');
  await expect(page.getByTestId('balloon-missing')).toHaveCount(0);
  await expect(page.getByTestId('balloon-summary')).toContainText('позиций: 5');
  await page.locator('.dr-tabs button').first().click();

  // Saved as one drawing file with the model; opened again: the positions stay.
  const [download] = await Promise.all([page.waitForEvent('download'), page.getByTestId('drawing-save').click()]);
  expect(download.suggestedFilename()).toMatch(/\.bcdrw\.json$/);
  const saved = path.join(out, download.suggestedFilename());
  await download.saveAs(saved);
  const fileJson = JSON.parse(fs.readFileSync(saved, 'utf8'));
  expect(fileJson.format).toBe('bendcore-drawing-file');
  expect(JSON.parse(fileJson.model).format).toBe('bendcore-assembly');
  const drawingDoc = JSON.parse(fileJson.drawing);
  expect(drawingDoc.sheets[0].balloons.map((b: { id: string }) => b.id)).toEqual(['P1']);

  await page.getByTestId('open-model').setInputFiles(path.join(root, 'tests/wasm/fixtures/bracket.part.json'));
  await expect(page.getByTestId('balloon-P1')).toHaveCount(0);
  await page.getByTestId('open-drawing').setInputFiles(saved);
  await expect(page.getByTestId('balloon-P1')).toHaveAttribute('data-number', '2');
  await expectNoError(page);
  expect((await state(page)).description!.sheets[0].balloons.length).toBe(5);

  // Removing the manual position brings back the automatic one of the shaft.
  await page.getByTestId('balloon-P1').locator('button[title="Удалить"]').click();
  await expect(page.getByTestId('balloon-P1')).toHaveCount(0);
  sheet = (await state(page)).description!.sheets[0];
  expect(sheet.balloons.filter((b) => b.text === '2').map((b) => b.id)).toEqual([null]);
});
