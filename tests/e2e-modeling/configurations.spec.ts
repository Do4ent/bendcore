// Panel "Таблица параметров" of the part page (docs/ui/PART_MODELING_UI.md, "Таблица параметров").
// The block tests/assembly-fixtures/configured_block.json (W×D×H, fillet R = 2 on the top front
// edge, through hole r = 5; configurations РЦ.080.00.010 60×40×20, -01 80×50×30, -02 50×30×15
// without the hole) is opened; the active configuration is switched with the radio buttons and
// the volume is compared with V = W·D·H − (1 − π/4)·R²·W − π·r²·H; a cell is edited, a row and a
// column are added and removed, undo restores them, every configuration is checked, the table
// goes to CSV and comes back from an edited CSV with a decimal comma.
import { expect, test, Page } from '@playwright/test';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const fixture = path.join(root, 'tests/assembly-fixtures/configured_block.json');

interface ConfigurationTableView {
  columns: { kind: string; target: string }[];
  rows: { designation: string; fullDesignation?: string; name: string; cells: string[] }[];
  active: string;
}
interface ModelerState {
  status: string;
  error: { code: string; message: string } | null;
  features: { id: string; state: string }[];
  properties: { volume: number | null; mass: number | null } | null;
  material: { id: string; density: number | null };
  configurations: ConfigurationTableView | null;
  configurationChecks: { designation: string; ok: boolean; problems: string[] }[] | null;
}

declare global {
  interface Window {
    bendcorePartModeler?: {
      getState(): ModelerState;
      idle(): Promise<void>;
    };
  }
}

const state = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcorePartModeler!.idle();
    return window.bendcorePartModeler!.getState();
  });

const { PI } = Math;
const blockVolume = (w: number, d: number, h: number, hole: boolean) => w * d * h - (1 - PI / 4) * 4 * w - (hole ? PI * 25 * h : 0);
const volumeOf = async (page: Page) => (await state(page)).properties?.volume ?? null;

test.beforeEach(async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await page.getByTestId('doc-open').setInputFiles(fixture);
  await expect.poll(async () => (await state(page)).configurations?.rows.length).toBe(3);
});

test('switching configurations rebuilds the part; editing the grid; undo', async ({ page }) => {
  const panel = page.getByTestId('configurations');
  await expect(panel.getByRole('heading')).toHaveText('Таблица параметров');
  await expect(page.getByTestId('config-active-0')).toBeChecked();
  expect(await volumeOf(page)).toBeCloseTo(blockVolume(60, 40, 20, true), 4);
  await expect(page.getByTestId('config-designation-1')).toHaveAttribute('title', 'РЦ.080.00.010-01');

  await page.getByTestId('config-active-1').click();
  await expect.poll(() => volumeOf(page)).toBeCloseTo(blockVolume(80, 50, 30, true), 4);
  // The material of the configuration becomes the material of the page: mass V·ρ of 6061-T6.
  await expect.poll(async () => (await state(page)).material.id).toBe('al_6061_t6');
  await expect.poll(async () => (await state(page)).properties?.mass).toBeCloseTo(blockVolume(80, 50, 30, true) * 1e-9 * 2700, 9);
  await page.getByTestId('config-active-2').click();
  await expect.poll(() => volumeOf(page)).toBeCloseTo(blockVolume(50, 30, 15, false), 4);
  expect((await state(page)).features.find((f) => f.id === 'Hole')!.state).toBe('suppressed');
  expect((await state(page)).features.find((f) => f.id === 'Round')!.state).toBe('ok');
  await page.getByTestId('config-active-0').click();
  await expect.poll(() => volumeOf(page)).toBeCloseTo(blockVolume(60, 40, 20, true), 4);

  // Cell H of the active row: 20 → 25.
  await page.getByTestId('config-cell-0-2').fill('25');
  await page.getByTestId('config-cell-0-2').blur();
  await expect.poll(() => volumeOf(page)).toBeCloseTo(blockVolume(60, 40, 25, true), 4);
  // Suppress the hole in the base configuration from the grid.
  await page.getByTestId('config-cell-0-3').selectOption('1');
  await expect.poll(() => volumeOf(page)).toBeCloseTo(blockVolume(60, 40, 25, false), 4);
  // Undo twice: the hole and H = 20 come back.
  await page.getByTestId('undo').click();
  await expect.poll(() => volumeOf(page)).toBeCloseTo(blockVolume(60, 40, 25, true), 4);
  await page.getByTestId('undo').click();
  await expect.poll(() => volumeOf(page)).toBeCloseTo(blockVolume(60, 40, 20, true), 4);

  // A new row copies the last one with the next suffix; it is removed again.
  await page.getByTestId('config-add-row').click();
  await expect.poll(async () => (await state(page)).configurations!.rows.map((r) => r.designation)).toEqual(['РЦ.080.00.010', '-01', '-02', '-03']);
  await page.getByTestId('config-name-3').fill('Брусок короткий');
  await page.getByTestId('config-name-3').blur();
  await expect.poll(async () => (await state(page)).configurations!.rows[3].name).toBe('Брусок короткий');
  await page.getByTestId('config-remove-row-3').click();
  await expect.poll(async () => (await state(page)).configurations!.rows.length).toBe(3);

  // New column: suppression of the fillet; every row takes the base value "0".
  await page.getByTestId('config-new-col').selectOption('suppression:Round');
  await page.getByTestId('config-add-col').click();
  await expect.poll(async () => (await state(page)).configurations!.columns.length).toBe(6);
  expect((await state(page)).configurations!.rows.map((r) => r.cells[5])).toEqual(['0', '0', '0']);
  await expect(page.getByTestId('config-col-5')).toContainText('Подавить: Round');
  await page.getByTestId('config-cell-0-5').selectOption('1');
  await expect.poll(() => volumeOf(page)).toBeCloseTo(60 * 40 * 20 - PI * 25 * 20, 4);
  await page.getByTestId('config-remove-col-5').click();
  await expect.poll(async () => (await state(page)).configurations!.columns.length).toBe(5);
  await expect.poll(() => volumeOf(page)).toBeCloseTo(blockVolume(60, 40, 20, true), 4);
  await page.getByTestId('undo').click();
  await expect.poll(async () => (await state(page)).configurations!.columns.length).toBe(6);

  // An invalid value is refused with an explicit error; the cell keeps its value.
  await page.getByTestId('config-cell-1-0').fill('8O+');
  await page.getByTestId('config-cell-1-0').blur();
  await expect(page.getByTestId('modeling-error')).toContainText('8O+');
  expect((await state(page)).configurations!.rows[1].cells[0]).toBe('80');
});

test('check of every configuration; CSV export and import with a decimal comma', async ({ page }) => {
  // H = −5 in "-02": the check names the configuration that does not build and the cause.
  await page.getByTestId('config-cell-2-2').fill('-5');
  await page.getByTestId('config-cell-2-2').blur();
  await expect.poll(async () => (await state(page)).configurations!.rows[2].cells[2]).toBe('-5');
  await page.getByTestId('config-check').click();
  await expect.poll(async () => (await state(page)).configurationChecks?.map((c) => c.ok)).toEqual([true, true, false]);
  await expect(page.getByTestId('config-check-2')).toHaveText('!');
  await expect(page.getByTestId('config-check-2')).toHaveAttribute('title', /feature 'Body'/);
  await expect(page.getByTestId('config-check-0')).toHaveText('✓');
  // The check leaves the active configuration as it was.
  expect(await volumeOf(page)).toBeCloseTo(blockVolume(60, 40, 20, true), 4);

  await page.getByTestId('config-decimal').selectOption(',');
  const download = page.waitForEvent('download');
  await page.getByTestId('config-export').click();
  const file = await (await download).path();
  const csv = fs.readFileSync(file!, 'utf8').replace(/^﻿/, '');
  const lines = csv.trim().split(/\r?\n/);
  expect(lines[0]).toBe('designation;name;W;D;H;suppress:Hole;material');
  expect(lines[2]).toBe('-01;Брусок большой;80;50;30;0;al_6061_t6');

  // Edited in a spreadsheet: -02 back to H = 15, -01 with W = 82,5 (decimal comma).
  const edited = csv.replace('-01;Брусок большой;80;', '-01;Брусок большой;82,5;').replace(';50;30;-5;1;', ';50;30;15;1;');
  await page.getByTestId('config-import').setInputFiles({ name: 'block.csv', mimeType: 'text/csv', buffer: Buffer.from(edited, 'utf8') });
  await expect.poll(async () => (await state(page)).configurations!.rows[1].cells[0]).toBe('82.5');
  expect((await state(page)).configurations!.rows[2].cells[2]).toBe('15');
  await page.getByTestId('config-active-1').click();
  await expect.poll(() => volumeOf(page)).toBeCloseTo(blockVolume(82.5, 50, 30, true), 4);

  // A broken CSV is refused with the line number.
  await page.getByTestId('config-import').setInputFiles({ name: 'bad.csv', mimeType: 'text/csv', buffer: Buffer.from('designation;name;W\nA\n') });
  await expect(page.getByTestId('modeling-error')).toContainText('2');
  expect((await state(page)).configurations!.rows.length).toBe(3);

  // The part file keeps the table and the active configuration.
  const save = page.waitForEvent('download');
  await page.getByTestId('doc-save').click();
  const saved = JSON.parse(fs.readFileSync((await (await save).path())!, 'utf8'));
  expect(saved.version).toBe(4);
  expect(saved.configurations.active).toBe('-01');
  expect(saved.configurations.rows[1].cells[0]).toBe('82.5');
});
