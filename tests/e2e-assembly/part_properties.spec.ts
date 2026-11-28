// Part document properties in the assembly page (docs/ui/ASSEMBLY_UI.md; docs/modeling/PART_DOCUMENT.md,
// «Свойства документа»): the plate АБВГ.715311.001 «Плита опорная» of 6061-T6 (made with the
// modeling module, tests/assembly-fixtures/part_properties.mjs) is inserted twice. The component
// is named after the part, its properties come from the part document, the BOM shows one row with
// the designation, the name, quantity 2 and the mass 2 · 100·60·10·1e-9·2700 kg.
import { expect, test } from '@playwright/test';
import type { Page } from '@playwright/test';
import { spawnSync } from 'child_process';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const modelingDir = path.resolve(process.env.BENDCORE_MODELING_WASM_DIR || path.join(root, 'build-wasm-all/wasm_modeling'));
const assemblyDir = path.resolve(process.env.BENDCORE_ASSEMBLY_WASM_DIR || path.join(root, 'build-wasm-all/wasm_assembly'));

async function pageState(page: Page) {
  return page.evaluate(async () => {
    await window.bendcoreAssembly!.idle();
    const s = window.bendcoreAssembly!.getState();
    return { status: s.status, error: s.error, state: s.state };
  });
}

function plateFile(): string {
  const script = `
    const load = async (dir, glue, api, fn) => (await import(dir + '/' + api))[fn]((await import(dir + '/' + glue)).default);
    const modeling = await load(${JSON.stringify(modelingDir)}, 'bendcore_modeling.mjs', 'bendcore_modeling_api.mjs', 'loadModeling');
    const assembly = await load(${JSON.stringify(assemblyDir)}, 'bendcore_assembly.mjs', 'bendcore_assembly_api.mjs', 'loadAssembly');
    const { buildPartProperties } = await import(${JSON.stringify(path.join(root, 'tests/assembly-fixtures/part_properties.mjs'))});
    process.stdout.write(buildPartProperties(modeling, assembly).part);
  `;
  const run = spawnSync(process.execPath, ['--input-type=module', '-e', script], { maxBuffer: 1 << 28, encoding: 'utf8' });
  if (run.status !== 0) throw new Error(run.stderr);
  const dir = path.join(root, 'test-results/assembly');
  fs.mkdirSync(dir, { recursive: true });
  const file = path.join(dir, 'plate.bcpart.json');
  fs.writeFileSync(file, run.stdout);
  return file;
}

test('component name, properties and BOM from the part document properties', async ({ page }) => {
  const file = plateFile();
  await page.goto('/');
  await expect.poll(async () => (await pageState(page)).status, { timeout: 120_000 }).toBe('ready');
  await page.getByTestId('insert-part-input').setInputFiles(file);
  await expect.poll(async () => (await pageState(page)).state?.occurrences.length).toBe(1);
  const s = (await pageState(page)).state!;
  expect(s.occurrences[0].name).toBe('Плита опорная');
  const def = s.definitions.find((d) => d.id === s.occurrences[0].definition)!;
  expect(def.properties.designation).toBe('АБВГ.715311.001');
  expect(def.properties.material).toBe('6061-T6');
  expect(def.properties.density).toBe(2700);

  await page.getByTestId(`component-${s.occurrences[0].id}`).locator('.as-name').click();
  await page.getByTestId('tab-properties').click();
  await expect(page.getByTestId('prop-designation')).toHaveValue('АБВГ.715311.001');
  await expect(page.getByTestId('prop-density')).toHaveValue('2700');

  // The file inserted again: a second component with the same designation.
  await page.getByTestId('insert-part-input').setInputFiles(file);
  await expect.poll(async () => (await pageState(page)).state?.occurrences.length).toBe(2);

  await page.getByTestId('tab-bom').click();
  const table = page.getByTestId('bom-table');
  const rows = await table.locator('tbody tr').evaluateAll((trs) => trs.map((tr) => [...tr.querySelectorAll('td')].map((td) => td.textContent)));
  expect(rows.every((r) => r[1] === 'АБВГ.715311.001' && r[2] === 'Плита опорная')).toBe(true);
  const m = 100 * 60 * 10 * 1e-9 * 2700;
  await expect(page.getByTestId('bom-total-mass')).toHaveText((2 * m).toPrecision(3));
  expect((await pageState(page)).error).toBeNull();
});
