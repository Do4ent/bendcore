// Text font and dissolved sub-assemblies end to end (docs/ui/DRAWING_UI.md, "Шрифт текста и
// разборные подсборки"). The shaft unit with the sub-assembly «Вал в сборе» (bushing + shaft;
// tests/assembly-fixtures/nested_unit.mjs) is opened in the drawing editor. The user switches the
// text font of the drawing — the kernel's SVG of the sheet has the line paths of the GOST 2.304
// font or none — and makes the sub-assembly "dissolved": its parts get positions of their own on
// the specification tab and as balloons on the sheet.
import { expect, test, Page } from '@playwright/test';
import { spawnSync } from 'child_process';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const out = path.join(root, 'test-results/drawing');
const assemblyDir = path.resolve(process.env.BENDCORE_ASSEMBLY_WASM_DIR || path.join(root, 'build-wasm-all/wasm_assembly'));

interface EditorState {
  status: string;
  error: { code: string; message: string } | null;
  description: {
    textFont: string;
    dissolvedAssemblies: string[];
    sheets: { balloons: { text: string; definition: number }[] }[];
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

/** The nested shaft unit made by the assembly module in node; written to test-results. */
function nestedUnit(): string {
  const script = `
    const dir = ${JSON.stringify(assemblyDir)};
    const { buildNestedUnit } = await import(${JSON.stringify(path.join(root, 'tests/assembly-fixtures/nested_unit.mjs'))});
    const api = await import(dir + '/bendcore_assembly_api.mjs');
    const k = await api.loadAssembly((await import(dir + '/bendcore_assembly.mjs')).default);
    process.stdout.write(buildNestedUnit(k));
  `;
  const run = spawnSync(process.execPath, ['--input-type=module', '-e', script], { maxBuffer: 1 << 28, encoding: 'utf8' });
  if (run.status !== 0) throw new Error(run.stderr);
  fs.mkdirSync(out, { recursive: true });
  const file = path.join(out, 'nested_unit.bcasm.json');
  fs.writeFileSync(file, run.stdout);
  return file;
}

const SUB = 'УВ.02.000';

test('text font of the sheets and a dissolved sub-assembly', async ({ page }) => {
  const file = nestedUnit();
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });
  await page.getByTestId('open-model').setInputFiles(file);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  await page.getByTestId('add-base').click();
  await expect(page.getByTestId('view-V1')).toHaveCount(1);
  await expectNoError(page);

  // GOST 2.304 type B by default: the texts of the title block are drawn with lines.
  const svg = page.getByTestId('sheet-svg');
  const strokeText = svg.locator('[data-linetype="gost2304"]');
  await expect(page.getByTestId('text-font')).toHaveValue('gost2304');
  expect((await state(page)).description!.textFont).toBe('gost2304');
  expect(await strokeText.count()).toBeGreaterThan(10);

  // DejaVu Sans: no line paths of the font, the texts are visible <text> elements.
  await page.getByTestId('text-font').selectOption('truetype');
  await expect(strokeText).toHaveCount(0);
  await expectNoError(page);
  expect((await state(page)).description!.textFont).toBe('truetype');
  await expect(svg.locator('text', { hasText: 'Узел вала' }).first()).toBeVisible();
  await expect(page.getByTestId('status')).toContainText('DejaVu Sans');

  // Undo returns the GOST font (the setting is a drawing edit).
  await page.getByTestId('tool-select').click();
  await page.locator('body').press('Control+z');
  await expect(page.getByTestId('status')).toContainText('Отменено');
  await expect.poll(async () => (await state(page)).description!.textFont).toBe('gost2304');
  await expect.poll(() => strokeText.count()).toBeGreaterThan(10);
  await expect(page.getByTestId('text-font')).toHaveValue('gost2304');

  // Positions by the specification on the front view: the sub-assembly is one position.
  await page.getByTestId('view-list').locator('li').first().click();
  await page.getByTestId('view-auto-balloons').click();
  await expect.poll(async () => (await state(page)).description!.sheets[0].balloons.length).toBe(4);
  await page.getByTestId('tab-specification').click();
  for (const p of [1, 2, 3, 4]) await expect(page.getByTestId(`spec-position-${p}`)).toHaveCount(1);
  await expect(page.getByTestId('spec-position-5')).toHaveCount(0);
  await expect(page.getByTestId('spec-position-1')).toContainText('Вал в сборе');
  const dissolve = page.getByTestId(`dissolve-${SUB}`);
  await expect(dissolve).not.toBeChecked();

  // Dissolved: the bushing and the shaft are entered with positions, no assembly units.
  await dissolve.click();
  await expect(page.getByTestId('spec-position-5')).toHaveCount(1);
  await expectNoError(page);
  await expect(dissolve).toBeChecked();
  expect((await state(page)).description!.dissolvedAssemblies).toEqual([SUB]);
  const specification = page.getByTestId('specification');
  await expect(specification.locator('tr.dr-spec-section', { hasText: 'Сборочные единицы' })).toHaveCount(0);
  await expect(specification.locator('tr.dr-spec-section', { hasText: 'Детали' })).toHaveCount(1);
  await expect(page.getByTestId('spec-position-1')).toContainText('Втулка');
  await expect(page.getByTestId('spec-position-2')).toContainText('Вал');
  await expect(specification.locator('tr', { hasText: 'Вал в сборе' })).toHaveCount(0);
  // Each part has its balloon on the sheet now.
  expect((await state(page)).description!.sheets[0].balloons.map((b) => Number(b.text)).sort()).toEqual([1, 2, 3, 4, 5]);
  for (const p of [1, 2, 3, 4, 5]) await expect(page.getByTestId(`spec-position-${p}`)).toHaveAttribute('data-balloons', '1');

  // Back to an assembly unit: one position for the sub-assembly again.
  await dissolve.click();
  await expect(page.getByTestId('spec-position-5')).toHaveCount(0);
  await expect(dissolve).not.toBeChecked();
  expect((await state(page)).description!.dissolvedAssemblies).toEqual([]);
  expect((await state(page)).description!.sheets[0].balloons.length).toBe(4);

  // Both settings are saved in the drawing file and come back when it is opened again.
  await dissolve.click();
  await expect(dissolve).toBeChecked();
  await page.getByTestId('text-font').selectOption('truetype');
  await expect.poll(async () => (await state(page)).description!.textFont).toBe('truetype');
  const [download] = await Promise.all([page.waitForEvent('download'), page.getByTestId('drawing-save').click()]);
  const saved = path.join(out, download.suggestedFilename());
  await download.saveAs(saved);
  const fileJson = JSON.parse(fs.readFileSync(saved, 'utf8'));
  expect(JSON.parse(fileJson.drawing).settings.textFont).toBe('truetype');
  expect(fileJson.specification).toEqual({ dissolvedAssemblies: [SUB] });

  await page.reload();
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });
  await page.getByTestId('open-drawing').setInputFiles(saved);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  await expectNoError(page);
  const reopened = (await state(page)).description!;
  expect(reopened.textFont).toBe('truetype');
  expect(reopened.dissolvedAssemblies).toEqual([SUB]);
  expect(reopened.sheets[0].balloons.length).toBe(5);
  await expect(strokeText).toHaveCount(0);
  await page.getByTestId('tab-specification').click();
  await expect(page.getByTestId(`dissolve-${SUB}`)).toBeChecked();
  await expect(page.getByTestId('spec-position-5')).toHaveCount(1);
});
