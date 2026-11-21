// Configuration of a component in the assembly page (docs/ui/ASSEMBLY_UI.md, "Исполнение
// компонента"). The block tests/assembly-fixtures/configured_block.json (configurations
// РЦ.080.00.010 60×40×20 steel S235, -01 80×50×30 6061-T6, -02 50×30×15 without the hole) is
// inserted twice; the second component gets "-01" in its properties. The BOM shows two rows with
// the configuration designations, the masses are V·ρ with V = W·D·H − (1 − π/4)·4·W − π·25·H.
import { expect, test } from '@playwright/test';
import type { Page } from '@playwright/test';
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

test('two components of different configurations: properties, BOM and mass', async ({ page }) => {
  await page.goto('/');
  await expect.poll(async () => (await pageState(page)).status, { timeout: 120_000 }).toBe('ready');
  await page.getByTestId('insert-part-input').setInputFiles(fixture);
  await expect.poll(async () => (await pageState(page)).state?.occurrences.length).toBe(1);
  await page.getByTestId('insert-part-input').setInputFiles(fixture);
  await expect.poll(async () => (await pageState(page)).state?.occurrences.length).toBe(2);
  let s = (await pageState(page)).state!;
  expect(s.occurrences[0].definition).toBe(s.occurrences[1].definition);
  const second = s.occurrences[1].id;

  await page.getByTestId(`component-${second}`).locator('.as-name').click();
  await page.getByTestId('tab-properties').click();
  const select = page.getByTestId('prop-configuration');
  await expect(select).toHaveValue('');
  await expect(select.locator('option')).toHaveText([
    'как в файле детали',
    'РЦ.080.00.010 — Брусок',
    'РЦ.080.00.010-01 — Брусок большой',
    'РЦ.080.00.010-02 — Брусок без отверстия',
  ]);
  await expect(page.getByTestId('prop-designation')).toHaveValue('РЦ.080.00.010');
  await select.selectOption('-01');
  await expect.poll(async () => {
    const st = (await pageState(page)).state!;
    const occ = st.occurrences.find((o) => o.id === second)!;
    return st.definitions.find((d) => d.id === occ.definition)!.properties.designation;
  }).toBe('РЦ.080.00.010-01');
  await expect(page.getByTestId('prop-configuration')).toHaveValue('-01');
  await expect(page.getByTestId('prop-designation')).toHaveValue('РЦ.080.00.010-01');
  await expect(page.getByTestId('prop-density')).toHaveValue('2700');
  expect((await pageState(page)).error).toBeNull();

  await page.getByTestId('tab-bom').click();
  const table = page.getByTestId('bom-table');
  await expect(table.locator('tbody tr')).toHaveCount(2);
  const rows = await table.locator('tbody tr').evaluateAll((trs) => trs.map((tr) => [...tr.querySelectorAll('td')].map((td) => td.textContent)));
  const byDesignation = Object.fromEntries(rows.map((r) => [r[1], r]));
  expect(Object.keys(byDesignation).sort()).toEqual(['РЦ.080.00.010', 'РЦ.080.00.010-01']);
  expect(byDesignation['РЦ.080.00.010'][2]).toBe('Брусок');
  expect(byDesignation['РЦ.080.00.010-01'][2]).toBe('Брусок большой');
  const m0 = blockVolume(60, 40, 20, true) * 1e-9 * 7850;
  const m1 = blockVolume(80, 50, 30, true) * 1e-9 * 2700;
  await expect(page.getByTestId('bom-total-mass')).toHaveText((m0 + m1).toPrecision(3));

  // Undo returns the component to the saved configuration (one BOM row, quantity 2).
  await page.getByTestId('undo').click();
  await expect(table.locator('tbody tr')).toHaveCount(1);
  await expect(table.locator('[data-testid=bom-quantity]')).toHaveText('2');
});
