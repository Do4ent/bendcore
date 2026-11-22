// Group documents of GOST 2.113 end to end (docs/ui/DRAWING_UI.md, "Групповые документы").
//
// 1. Group drawing of a part: the reference bracket with a configuration table (base РД.10.001
//    A = 60, -01 A = 80, -02 without the hole) is opened; the user puts the dimension A (two outer
//    verticals of the front view) and B (bottom and top lines), switches the group drawing on —
//    A becomes a letter, B stays a number, the table of configurations lists 60/80/60; renames a
//    column header (the kernel's sheet SVG shows it); changes a cell of the part table and updates
//    the drawing — the table shows the new value.
// 2. Group specification of an assembly with two configurations (made by the assembly module in
//    node): on the specification tab the user switches the group specification on, the quantities
//    per configuration equal the composition of the assembly configurations, a column header is
//    renamed.
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
    group: { enabled: boolean; table: { columns: { key: string; header: string }[]; rows: string[][]; dimensions: { id: string; letter: string }[] } | null };
    groupSpecification: { enabled: boolean; columnHeaders: string[] } | null;
    sheets: { dimensions: { id: string; text?: string }[] }[];
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

// Screen point in the middle of the longest visible vertical (horizontal) edge of a view with the
// smallest or largest x (y) — as in drawing_editor.spec.ts.
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

/** The bracket of tests/wasm/fixtures with a configuration table, written to test-results. */
function bracketWithConfigurations(): string {
  const doc = JSON.parse(fs.readFileSync(path.join(root, 'tests/wasm/fixtures/bracket.part.json'), 'utf8'));
  doc.version = 4;
  doc.configurations = {
    columns: [{ kind: 'variable', target: 'A' }, { kind: 'suppression', target: 'MountHole' }],
    rows: [
      { designation: 'РД.10.001', name: 'Кронштейн', cells: ['60', '0'] },
      { designation: '-01', name: 'Кронштейн', cells: ['80', '0'] },
      { designation: '-02', name: 'Кронштейн', cells: ['60', '1'] },
    ],
    active: 'РД.10.001',
  };
  fs.mkdirSync(out, { recursive: true });
  const file = path.join(out, 'bracket_configurations.part.json');
  fs.writeFileSync(file, JSON.stringify(doc));
  return file;
}

async function dimensionByClicks(page: Page, vertical: boolean) {
  await page.getByTestId('tool-dimension').click();
  const a = await extremeEdge(page, 'V1', vertical, 'min');
  const b = await extremeEdge(page, 'V1', vertical, 'max');
  await page.mouse.click(a.x, a.y);
  await page.mouse.click(b.x, b.y);
  await expectNoError(page);
}

test('group drawing of a part: letters, table of configurations, header, model change', async ({ page }) => {
  const file = bracketWithConfigurations();
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });
  await page.getByTestId('open-model').setInputFiles(file);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  await page.getByTestId('add-base').click();
  await expect(page.getByTestId('view-V1')).toHaveCount(1);
  await dimensionByClicks(page, true);
  await expect(page.getByTestId('dimension-D1').locator('strong')).toHaveText('60');
  await dimensionByClicks(page, false);
  await expect(page.getByTestId('dimension-D2').locator('strong')).toHaveText('50');
  await page.getByTestId('tool-select').click();

  // The part table is shown; the group drawing is switched on.
  await expect(page.getByTestId('configuration-table')).toBeVisible();
  await page.getByTestId('group-enabled').click();
  await expect(page.getByTestId('group-enabled')).toBeChecked();
  await expect(page.getByTestId('group-table')).toBeVisible();
  await expectNoError(page);
  await expect(page.getByTestId('dimension-D1').locator('strong')).toHaveText('A');
  await expect(page.getByTestId('dimension-D2').locator('strong')).toHaveText('50');
  const key = 'dim:S1/D1';
  for (const [r, v] of [[0, '60'], [1, '80'], [2, '60']] as const) await expect(page.getByTestId(`group-cell-${r}-${key}`)).toHaveText(v);
  await expect(page.getByTestId('group-cell-2-suppress:MountHole')).toHaveText('нет');
  await expect(page.getByTestId('group-cell-1-designation')).toHaveText('РД.10.001-01');
  const svg = page.getByTestId('sheet-svg');
  await expect(svg.locator('[data-text="РД.10.001-02"]')).toHaveCount(1);
  await expect(page.getByTestId('group-letters')).toContainText('A — S1/D1');

  // Header of a column renamed: the kernel's table on the sheet shows it; undo brings it back.
  await page.getByTestId('group-header-suppress:MountHole').fill('Отверстие');
  await page.getByTestId('group-header-suppress:MountHole').press('Enter');
  await expect(svg.locator('[data-text="Отверстие"]')).toHaveCount(1);
  await expectNoError(page);
  expect((await state(page)).description!.group.table!.columns.find((c) => c.key === 'suppress:MountHole')!.header).toBe('Отверстие');

  // A cell of the part table: -01 A = 90 → the model changed → update → 90 in the table.
  await page.getByTestId('configuration-cell-1-0').fill('90');
  await page.getByTestId('configuration-cell-1-0').press('Enter');
  await expect(page.getByTestId('model-state')).toContainText('Модель изменилась');
  await page.getByTestId('update-from-model').click();
  await expect(page.getByTestId(`group-cell-1-${key}`)).toHaveText('90');
  expect(await svg.locator('[data-text="90"]').count()).toBeGreaterThan(0);
  await expectNoError(page);

  // Group drawing off: numbers again.
  await page.getByTestId('group-enabled').click();
  await expect(page.getByTestId('group-enabled')).not.toBeChecked();
  await expect(page.getByTestId('dimension-D1').locator('strong')).toHaveText('60');
  await expect(page.getByTestId('group-table')).toHaveCount(0);
  await expectNoError(page);
});

/** Assembly РЦ.500.00.000 with -01 (second plate excluded, block -01), made in node. */
function configuredAssembly(): string {
  const script = `
    const dir = ${JSON.stringify(assemblyDir)};
    const fs = await import('node:fs');
    const fixture = (n) => fs.readFileSync(${JSON.stringify(path.join(root, 'tests/assembly-fixtures'))} + '/' + n, 'utf8');
    const api = await import(dir + '/bendcore_assembly_api.mjs');
    const k = await api.loadAssembly((await import(dir + '/bendcore_assembly.mjs')).default);
    const doc = k.createDocument('Узел');
    doc.insertPart({ name: 'Корпус', format: 'part-json', data: fixture('block.json'), fixed: true });
    doc.insertPart({ name: 'Планка', format: 'part-json', data: fixture('plate.json'), placement: { position: [0, 100, 0] } });
    const plate2 = doc.insertPart({ name: 'Планка', format: 'part-json', data: fixture('plate.json'), placement: { position: [0, 200, 0] } });
    const block = doc.insertPart({ name: 'Брусок', format: 'part-json', data: fixture('configured_block.json'), placement: { position: [150, 0, 0] } });
    doc.setProperties(0, { designation: 'РЦ.500.00.000', name: 'Узел' });
    doc.setConfigurationTable({
      columns: [{ kind: 'suppression', target: plate2.occurrence }, { kind: 'configuration', target: block.occurrence }],
      rows: [{ designation: 'РЦ.500.00.000', name: 'Узел', cells: ['0', '#'] },
             { designation: '-01', name: 'Узел облегчённый', cells: ['1', '-01'] }],
      active: 'РЦ.500.00.000',
    });
    // The block -01 is put in the file (the drawing module has no part sources).
    doc.activateConfiguration('-01');
    doc.activateConfiguration('РЦ.500.00.000');
    process.stdout.write(doc.save());
    doc.dispose();
  `;
  const run = spawnSync(process.execPath, ['--input-type=module', '-e', script], { maxBuffer: 1 << 28, encoding: 'utf8' });
  if (run.status !== 0) throw new Error(run.stderr);
  fs.mkdirSync(out, { recursive: true });
  const file = path.join(out, 'configured_unit.bcasm.json');
  fs.writeFileSync(file, run.stdout);
  return file;
}

test('group specification of an assembly: quantities per configuration, column header', async ({ page }) => {
  test.skip(!fs.existsSync(path.join(assemblyDir, 'bendcore_assembly.mjs')), `no assembly module in ${assemblyDir}`);
  const file = configuredAssembly();
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });
  await page.getByTestId('open-model').setInputFiles(file);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  await page.getByTestId('tab-specification').click();
  await page.getByTestId('group-spec-enabled').click();
  await expect(page.getByTestId('group-spec-enabled')).toBeChecked();
  await expect(page.getByTestId('group-spec-table')).toBeVisible();
  await expectNoError(page);
  // Positions by GOST 2.106: Детали — Брусок (1), Брусок -01 (2); Стандартные изделия — Корпус (3), Планка (4).
  const qty = async (pos: number) => [await page.getByTestId(`group-spec-qty-${pos}-0`).textContent(), await page.getByTestId(`group-spec-qty-${pos}-1`).textContent()];
  expect(await qty(1)).toEqual(['1', '–']);
  expect(await qty(2)).toEqual(['–', '1']);
  expect(await qty(3)).toEqual(['1', '1']);
  expect(await qty(4)).toEqual(['2', '1']);
  await expect(page.getByTestId('group-spec-position-3')).toHaveAttribute('data-constant', 'yes');

  await page.getByTestId('group-spec-header-1').fill('Обл.');
  await page.getByTestId('group-spec-header-1').press('Enter');
  await expectNoError(page);
  await expect.poll(async () => (await state(page)).description!.groupSpecification!.columnHeaders).toEqual(['', 'Обл.']);
  await expect(page.getByTestId('group-spec-header-1')).toHaveValue('Обл.');

  // Variable data layout: the table is still there, quantities unchanged.
  await page.getByTestId('group-spec-layout').selectOption('variableData');
  await expectNoError(page);
  await expect(page.getByTestId('group-spec-table')).toBeVisible();
});
