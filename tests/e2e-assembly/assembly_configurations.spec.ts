// Assembly configurations on the assembly page (docs/ui/ASSEMBLY_UI.md, "Исполнения сборки").
// Two components of tests/assembly-fixtures/configured_block.json (configurations РЦ.080.00.010
// 60×40×20 steel S235, -01 80×50×30 6061-T6, -02 50×30×15 without the hole). In the panel
// "Исполнения сборки" a configuration -01 is added in which the second component is excluded and
// the first one takes the block configuration -01. Activating it changes the composition: the BOM
// has one row РЦ.080.00.010-01 with the mass V·ρ, V = W·D·H − (1 − π/4)·4·W − π·25·H; back to the
// first row — two blocks of the base configuration. The table goes to CSV and back, undo returns
// the previous configuration.
import { expect, test } from '@playwright/test';
import type { Page } from '@playwright/test';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const fixture = path.join(root, 'tests/assembly-fixtures/configured_block.json');
const { PI } = Math;
const blockVolume = (w: number, d: number, h: number, hole: boolean) => w * d * h - (1 - PI / 4) * 4 * w - (hole ? PI * 25 * h : 0);

async function pageState(page: Page) {
  return page.evaluate(async () => {
    await window.bendcoreAssembly!.idle();
    const s = window.bendcoreAssembly!.getState();
    return { status: s.status, error: s.error, state: s.state };
  });
}

async function bomRows(page: Page) {
  await page.getByTestId('tab-bom').click();
  const table = page.getByTestId('bom-table');
  await expect(table).toBeVisible();
  await pageState(page);
  return table.locator('tbody tr').evaluateAll((trs) => trs.map((tr) => [...tr.querySelectorAll('td')].map((td) => td.textContent ?? '')));
}

test('assembly configurations: grid, activation, BOM per configuration, CSV and undo', async ({ page }) => {
  await page.goto('/');
  await expect.poll(async () => (await pageState(page)).status, { timeout: 120_000 }).toBe('ready');
  await page.getByTestId('insert-part-input').setInputFiles(fixture);
  await expect.poll(async () => (await pageState(page)).state?.occurrences.length).toBe(1);
  await page.getByTestId('insert-part-input').setInputFiles(fixture);
  await expect.poll(async () => (await pageState(page)).state?.occurrences.length).toBe(2);
  const [first, second] = (await pageState(page)).state!.occurrences;

  await page.getByTestId('tab-configurations').click();
  const panel = page.getByTestId('asm-configurations');
  await expect(panel.getByRole('heading')).toHaveText('Исполнения сборки');
  await expect(page.getByTestId('asm-config-active-designation')).toHaveText('Активны базовые значения (без исполнения)');

  // Columns: inclusion of the second component, configuration of the first one.
  await page.getByTestId('asm-config-new-col').selectOption(`suppression:${second.id}`);
  await page.getByTestId('asm-config-add-col').click();
  await expect(page.getByTestId('asm-config-col-0')).toContainText(`Включить: ${second.name}`);
  await page.getByTestId('asm-config-new-col').selectOption(`configuration:${first.id}`);
  await page.getByTestId('asm-config-add-col').click();
  await expect(page.getByTestId('asm-config-col-1')).toContainText(`Исполнение: ${first.name}`);
  await expect(page.getByTestId('asm-config-base-0')).toHaveText('включено');
  await expect(page.getByTestId('asm-config-base-1')).toHaveText('как в файле компонента');

  // Rows: the base configuration and -01.
  await page.getByTestId('asm-config-add-row').click();
  await expect(page.getByTestId('asm-config-designation-0')).toHaveValue('-00');
  await page.getByTestId('asm-config-designation-0').fill('РЦ.700.00.000');
  await page.getByTestId('asm-config-name-0').click();
  await expect(page.getByTestId('asm-config-designation-0')).toHaveValue('РЦ.700.00.000');
  await page.getByTestId('asm-config-add-row').click();
  await expect(page.getByTestId('asm-config-designation-1')).toHaveValue('-01');
  await page.getByTestId('asm-config-cell-1-0').selectOption('1');
  await expect(page.getByTestId('asm-config-cell-1-0')).toHaveValue('1');
  await expect(page.getByTestId('asm-config-cell-1-1').locator('option')).toHaveText([
    'как в базовом',
    'как в файле компонента',
    'РЦ.080.00.010 — Брусок',
    'РЦ.080.00.010-01 — Брусок большой',
    'РЦ.080.00.010-02 — Брусок без отверстия',
  ]);
  await page.getByTestId('asm-config-cell-1-1').selectOption('-01');
  await expect(page.getByTestId('asm-config-cell-1-1')).toHaveValue('-01');
  expect((await pageState(page)).error).toBeNull();

  // Activate -01: the second component is excluded, the first one is the block -01.
  await expect(page.locator('[data-testid^=asm-config-row-]')).toHaveCount(2);
  await page.getByTestId('asm-config-active-1').click();
  await expect.poll(async () => (await pageState(page)).state?.configuration.active).toBe('-01');
  await expect(page.getByTestId('asm-config-active-designation')).toHaveText('Активно исполнение РЦ.700.00.000-01');
  await expect(page.getByTestId('asm-config-report')).toContainText('Исключено компонентов: 1');
  let s = (await pageState(page)).state!;
  expect(s.occurrences.find((o) => o.id === second.id)!.suppressed).toBe(true);
  let rows = await bomRows(page);
  expect(rows.map((r) => r[1])).toEqual(['РЦ.080.00.010-01']);
  await expect(page.getByTestId('bom-total-mass')).toHaveText((blockVolume(80, 50, 30, true) * 1e-9 * 2700).toPrecision(3));

  // Back to the first row: two blocks of the base configuration.
  await page.getByTestId('tab-configurations').click();
  await page.getByTestId('asm-config-active-0').click();
  await expect.poll(async () => (await pageState(page)).state?.configuration.active).toBe('РЦ.700.00.000');
  rows = await bomRows(page);
  expect(rows.map((r) => [r[1], r[4]])).toEqual([['РЦ.080.00.010', '2']]);
  await expect(page.getByTestId('bom-total-mass')).toHaveText((2 * blockVolume(60, 40, 20, true) * 1e-9 * 7850).toPrecision(3));

  // Undo: -01 again (the activation was one step).
  await page.getByTestId('tab-configurations').click();
  await page.getByTestId('asm-config-undo').click();
  await expect.poll(async () => (await pageState(page)).state?.configuration.active).toBe('-01');

  // CSV: export, then import of an edited text (the name of -01 changed).
  await page.getByTestId('asm-config-decimal').selectOption(',');
  const [download] = await Promise.all([page.waitForEvent('download'), page.getByTestId('asm-config-export').click()]);
  const csv = fs.readFileSync((await download.path())!, 'utf8');
  expect(csv.charCodeAt(0)).toBe(0xfeff);
  // Both components carry the same name: the columns name them by id.
  expect(first.name).toBe(second.name);
  expect(csv).toContain(`designation;name;suppress:#${second.id};configuration:#${first.id}\r\n`);
  expect(csv).toContain('-01;;1;-01');
  const edited = csv.replace('-01;;1;-01', '-01;Без второго бруска;1;-01');
  const file = test.info().outputPath('configurations.csv');
  fs.writeFileSync(file, edited);
  await page.getByTestId('asm-config-import').setInputFiles(file);
  await expect(page.getByTestId('asm-config-name-1')).toHaveValue('Без второго бруска');
  s = (await pageState(page)).state!;
  expect(s.configuration.active).toBe('-01');
  expect((await pageState(page)).error).toBeNull();
});
