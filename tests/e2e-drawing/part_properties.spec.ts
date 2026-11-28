// Part document properties in the drawing editor (docs/drawing/DRAWING_DOCUMENT.md, «Свойства
// детали в основной надписи»; docs/modeling/PART_DOCUMENT.md, «Свойства документа»). The plate
// АБВГ.715311.001 «Плита опорная» and the unit of two plates are made with the modeling and
// assembly modules (tests/assembly-fixtures/part_properties.mjs). The part drawing shows the
// designation, name, material, organization, developer and the mass V·ρ = 60000·1e-9·2700 in the
// title block; a manual title field wins; the specification of the unit lists the plate with its
// designation, name, quantity 2 and the note of the part document.
import { expect, test, Page } from '@playwright/test';
import { spawnSync } from 'child_process';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const out = path.join(root, 'test-results/drawing');
const modelingDir = path.resolve(process.env.BENDCORE_MODELING_WASM_DIR || path.join(root, 'build-wasm-all/wasm_modeling'));
const assemblyDir = path.resolve(process.env.BENDCORE_ASSEMBLY_WASM_DIR || path.join(root, 'build-wasm-all/wasm_assembly'));

interface EditorState {
  status: string;
  error: { code: string; message: string } | null;
  description: {
    model: { kind: string };
    sheets: { titleShown?: Record<string, string | number> }[];
  } | null;
}

const state = (page: Page) =>
  page.evaluate(async () => {
    const w = window as unknown as { bendcoreDrawingEditor: EditorState; bendcoreDrawingEditorIdle: () => Promise<void> };
    await w.bendcoreDrawingEditorIdle();
    return w.bendcoreDrawingEditor;
  });

/** Part and assembly files written to test-results by the modules in node. */
function fixtures(): { part: string; assembly: string } {
  const script = `
    const load = async (dir, glue, api, fn) => (await import(dir + '/' + api))[fn]((await import(dir + '/' + glue)).default);
    const modeling = await load(${JSON.stringify(modelingDir)}, 'bendcore_modeling.mjs', 'bendcore_modeling_api.mjs', 'loadModeling');
    const assembly = await load(${JSON.stringify(assemblyDir)}, 'bendcore_assembly.mjs', 'bendcore_assembly_api.mjs', 'loadAssembly');
    const { buildPartProperties } = await import(${JSON.stringify(path.join(root, 'tests/assembly-fixtures/part_properties.mjs'))});
    process.stdout.write(JSON.stringify(buildPartProperties(modeling, assembly)));
  `;
  const run = spawnSync(process.execPath, ['--input-type=module', '-e', script], { maxBuffer: 1 << 28, encoding: 'utf8' });
  if (run.status !== 0) throw new Error(run.stderr);
  const { part, assembly } = JSON.parse(run.stdout);
  fs.mkdirSync(out, { recursive: true });
  const files = { part: path.join(out, 'plate_properties.bcpart.json'), assembly: path.join(out, 'plates_unit.bcasm.json') };
  fs.writeFileSync(files.part, part);
  fs.writeFileSync(files.assembly, assembly);
  return files;
}

test('part document properties: title block of the part drawing and the specification of the unit', async ({ page }) => {
  const files = fixtures();
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });

  // Part drawing: the title block is filled from the part document when the drawing is created.
  await page.getByTestId('open-model').setInputFiles(files.part);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  let s = await state(page);
  expect(s.error).toBeNull();
  expect(s.description!.model.kind).toBe('part');
  const title = s.description!.sheets[0].titleShown!;
  expect(title.designation).toBe('АБВГ.715311.001');
  expect(title.name).toBe('Плита опорная');
  expect(title.material).toBe('6061-T6');
  expect(title.organization).toBe('ООО «Ромашка»');
  expect(title.developer).toBe('Иванов');
  expect(title.mass).toBe('0,162');  // 100·60·10 mm³ · 1e-9 · 2700 kg/m³
  await expect(page.getByTestId('sheet-svg')).toContainText('Плита опорная');

  // A manual title field has priority over the part document.
  await page.getByTestId('title-designation').fill('РУЧН.000.001');
  await page.getByTestId('title-apply').click();
  await expect.poll(async () => (await state(page)).description!.sheets[0].titleShown!.designation).toBe('РУЧН.000.001');
  await page.getByTestId('update-from-model').click();
  s = await state(page);
  expect(s.description!.sheets[0].titleShown!.designation).toBe('РУЧН.000.001');
  expect(s.description!.sheets[0].titleShown!.name).toBe('Плита опорная');

  // Specification of the unit: the plate by its designation and name, two pieces, the note.
  await page.getByTestId('open-model').setInputFiles(files.assembly);
  await expect.poll(async () => (await state(page)).description?.model.kind).toBe('assembly');
  await page.getByTestId('add-base').click();
  await page.getByTestId('tab-specification').click();
  const row = page.getByTestId('spec-position-1');
  await expect(row).toBeVisible();
  const cells = await row.locator('td').allTextContents();
  expect(cells[3]).toBe('АБВГ.715311.001');
  expect(cells[4]).toBe('Плита опорная');
  expect(cells[5]).toBe('2');
  expect(cells[6]).toBe('Анодировать');
  await expect(page.getByTestId('specification').locator('tr.dr-spec-section', { hasText: 'Детали' })).toHaveCount(1);
  expect((await state(page)).error).toBeNull();
});
