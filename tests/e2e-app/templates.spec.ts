// End-to-end scenarios of the document templates (docs/ui/APP_SHELL.md, "Шаблоны"):
//   - a new drawing from the built-in template "ISO A3 (third angle)": the sheet settings of the
//     template are in the drawing editor;
//   - a user template saved from an open part (a variable, a datum plane) makes a new part with
//     them; the template file exports, the template is deleted and imported back;
//   - a user drawing template keeps the constant fields of the title block.
//
//   npx playwright test -c web/app/playwright.config.ts tests/e2e-app/templates.spec.ts
import { expect, test } from '@playwright/test';
import type { Locator, Page } from '@playwright/test';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const out = path.join(root, 'test-results/app-templates');

interface ShellState {
  tabs: { id: string; kind: string; title: string; dirty: boolean }[];
  storage: string;
  recent: { id: string; kind: string; name: string }[];
  templates: { id: string; kind: string; name: string; builtin: boolean }[];
}

declare global {
  interface Window {
    bendcoreApp?: { getState(): ShellState; idle(): Promise<void> };
    bendcorePartModeler?: { getState(): { features: { id: string; type: string }[] } };
  }
}

const shell = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcoreApp!.idle();
    return window.bendcoreApp!.getState();
  });

const modeler = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcoreApp!.idle();
    return window.bendcorePartModeler!.getState();
  });

const activePanel = (page: Page): Locator => page.locator('[data-tab-panel][data-active="true"]');

async function freshShell(page: Page) {
  await page.goto('/');
  await page.evaluate(
    () =>
      new Promise<void>((resolve) => {
        const req = indexedDB.deleteDatabase('bendcore-app');
        req.onsuccess = req.onerror = req.onblocked = () => resolve();
      }),
  );
  await page.reload();
  await expect(page.getByTestId('start-page')).toBeVisible();
  await expect.poll(async () => (await shell(page)).storage).toBe('ready');
}

/** New part tab with a W×D×H block. */
async function buildBlock(page: Page, W: number, D: number, H: number) {
  await page.getByTestId('tab-start').click();
  await page.getByTestId('new-part').click();
  const P = activePanel(page);
  await expect(P.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await P.getByTestId('add-Sketch').click();
  await P.getByTestId('sketch-width').fill(String(W));
  await P.getByTestId('sketch-height').fill(String(D));
  await P.getByTestId('panel-preview').click();
  await expect(P.getByTestId('panel-state')).toHaveAttribute('data-state', 'ok');
  await P.getByTestId('panel-ok').click();
  await expect(P.getByTestId('feature-panel')).toHaveCount(0);
  await P.getByTestId('add-Extrude').click();
  await P.getByTestId('param-distance').fill(String(H));
  await P.getByTestId('panel-ok').click();
  await expect(P.getByTestId('feature-panel')).toHaveCount(0);
  return P;
}

test('new drawing from the template ISO A3 (third angle): sheet settings of the template', async ({ page }) => {
  await freshShell(page);
  const names = (await shell(page)).templates.map((x) => x.name);
  expect(names).toEqual(['Деталь ЕСКД, мм', 'Чертёж ЕСКД А3', 'Чертёж ЕСКД А4', 'Чертёж ISO A3 (первый угол)', 'Чертёж ISO A3 (третий угол)', 'Сборка']);

  // No model yet: a drawing cannot be made, the reason is shown.
  await expect(page.getByTestId('template-no-model')).toBeVisible();
  await expect(page.getByTestId('template-create-builtin:drawing-iso-a3-third')).toBeDisabled();

  await buildBlock(page, 60, 40, 20);
  await page.getByTestId('tab-start').click();
  await expect(page.getByTestId('template-no-model')).toHaveCount(0);
  await page.getByTestId('template-create-builtin:drawing-iso-a3-third').click();
  await expect(page.getByTestId('doc-tab-drawing')).toHaveCount(1);
  const D = activePanel(page);
  await expect(D.getByTestId('sheet-format')).toHaveValue('A3');
  await expect(D.getByTestId('sheet-orientation')).toHaveValue('landscape');
  await expect(D.getByTestId('sheet-style')).toHaveValue('iso7200');
  await expect(D.getByTestId('projection-method')).toHaveValue('thirdAngle');
  await expect(D.getByTestId('text-font')).toHaveValue('truetype');
  await expect(D.getByTestId('title-block-iso')).toBeVisible();
  // A drawing from a template is a new, not yet saved document and no recent file.
  await expect(page.getByTestId('doc-tab-drawing').getByTestId('tab-dirty')).toHaveCount(1);
  expect((await shell(page)).recent.map((x) => x.kind)).not.toContain('drawing');

  // The base view of the drawing is made on the model of the part.
  await expect(D.getByTestId('add-base')).toBeEnabled();
  await D.getByTestId('add-base').click();
  await expect(D.getByTestId('view-V1')).toHaveCount(1);
});

test('user part template: save from an open part, new part from it, export, delete, import', async ({ page }) => {
  fs.mkdirSync(out, { recursive: true });
  await freshShell(page);
  const P = await buildBlock(page, 50, 30, 10);
  await P.getByTestId('var-new-name').fill('T');
  await P.getByTestId('var-new-expression').fill('4');
  await P.getByTestId('var-add').click();
  await expect(P.getByTestId('var-T')).toBeVisible();
  await P.getByTestId('material').selectOption('steel_45');

  // Save as template from the open part.
  await page.getByTestId('shell-save-template').click();
  await expect(page.getByTestId('save-template-dialog')).toBeVisible();
  await page.getByTestId('template-name').fill('Bracket T');
  await page.getByTestId('template-save').click();
  await expect(page.getByTestId('save-template-dialog')).toHaveCount(0);
  await expect.poll(async () => (await shell(page)).templates.filter((x) => !x.builtin).map((x) => x.name)).toEqual(['Bracket T']);

  // New part from it: the variable and the material, no geometry.
  await page.getByTestId('tab-start').click();
  const row = page.locator('[data-testid^="template-user:"][data-kind="part"]');
  await expect(row).toHaveCount(1);
  await expect(row.getByTestId(/^template-summary-/)).toContainText('steel_45');
  await row.getByRole('button', { name: 'Создать' }).click();
  await expect(page.getByTestId('doc-tab-part')).toHaveCount(2);
  const P2 = activePanel(page);
  await expect(P2.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await expect(P2.getByTestId('var-T')).toBeVisible();
  await expect(P2.getByTestId('var-T')).toContainText('4');
  await expect(P2.getByTestId('material')).toHaveValue('steel_45');
  await expect.poll(async () => (await modeler(page)).features.length).toBe(0);

  // Template survives a reload (IndexedDB).
  await page.reload();
  await expect.poll(async () => (await shell(page)).templates.filter((x) => !x.builtin).length).toBe(1);

  // Export to a file, delete, import the file back.
  const del = page.locator('[data-testid^="template-delete-user:"]');
  const [download] = await Promise.all([page.waitForEvent('download'), page.locator('[data-testid^="template-export-user:"]').click()]);
  expect(download.suggestedFilename()).toBe('Bracket-T.bctemplate.json');
  const file = path.join(out, download.suggestedFilename());
  await download.saveAs(file);
  const json = JSON.parse(fs.readFileSync(file, 'utf8'));
  expect(json).toMatchObject({ format: 'bendcore-template', kind: 'part', name: 'Bracket T' });
  expect(json.settings.variables).toEqual([{ name: 'T', expression: '4' }]);

  page.once('dialog', (d) => void d.accept());
  await del.click();
  await expect.poll(async () => (await shell(page)).templates.filter((x) => !x.builtin).length).toBe(0);
  await page.getByTestId('template-import').setInputFiles(file);
  await expect.poll(async () => (await shell(page)).templates.filter((x) => !x.builtin).map((x) => x.name)).toEqual(['Bracket T']);

  // A broken file is refused with the reason.
  const bad = path.join(out, 'bad.bctemplate.json');
  fs.writeFileSync(bad, JSON.stringify({ format: 'bendcore-template', version: 1, kind: 'part', name: 'x', settings: { units: 'inch', material: null, variables: [], planes: [] } }));
  await page.getByTestId('template-import').setInputFiles(bad);
  await expect(page.getByTestId('notifications')).toContainText('bad.bctemplate.json');
  expect((await shell(page)).templates.filter((x) => !x.builtin)).toHaveLength(1);
});

test('user drawing template keeps the constant title block fields', async ({ page }) => {
  await freshShell(page);
  await buildBlock(page, 40, 40, 10);
  await page.getByTestId('tab-start').click();
  await page.getByTestId('template-create-builtin:drawing-iso-a3-first').click();
  const D = activePanel(page);
  await expect(D.getByTestId('title-block-iso')).toBeVisible();
  await D.getByTestId('title-iso-organization').fill('Bend Works');
  await D.getByTestId('title-iso-developer').fill('Ivanov');
  await D.getByTestId('title-apply').click();
  await expect(D.getByTestId('title-iso-organization')).toHaveValue('Bend Works');
  await expect.poll(async () => D.getByTestId('title-apply').isEnabled()).toBe(true);
  await page.getByTestId('shell-save-template').click();
  await page.getByTestId('template-name').fill('Works A3');
  await page.getByTestId('template-save').click();
  await expect.poll(async () => (await shell(page)).templates.filter((x) => !x.builtin).map((x) => x.name)).toEqual(['Works A3']);

  await page.getByTestId('tab-start').click();
  await expect(page.locator('[data-testid^="template-summary-user:"]')).toContainText('Bend Works');
  await page.locator('[data-testid^="template-create-user:"]').click();
  await expect(page.getByTestId('doc-tab-drawing')).toHaveCount(2);
  const D2 = activePanel(page);
  await expect(D2.getByTestId('sheet-style')).toHaveValue('iso7200');
  await expect(D2.getByTestId('projection-method')).toHaveValue('firstAngle');
  await expect(D2.getByTestId('title-iso-organization')).toHaveValue('Bend Works');
  await expect(D2.getByTestId('title-iso-developer')).toHaveValue('Ivanov');
});
