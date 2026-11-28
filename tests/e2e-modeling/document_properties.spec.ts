// Document properties of the part page (docs/ui/PART_MODELING_UI.md, "Свойства документа"): the
// bracket fixture is opened, the designation, name, developer, organization and note are typed in
// the panel; every field is one undo entry; the saved file keeps them next to the material and
// opening it again fills the panel. English captions — Jest src/ui/modeling/__tests__/documentProperties.test.tsx.
import { expect, test, Page } from '@playwright/test';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const bracket = path.join(root, 'tests/wasm/fixtures/bracket.part.json');

interface DocProps {
  designation: string;
  name: string;
  developer: string;
  organization: string;
  note: string;
  effective: { designation: string; name: string };
}
interface ModelerState {
  status: string;
  error: { code: string; message: string } | null;
  documentProperties: DocProps | null;
}

declare global {
  interface Window {
    bendcorePartModeler?: { getState(): ModelerState; idle(): Promise<void> };
  }
}

const state = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcorePartModeler!.idle();
    return window.bendcorePartModeler!.getState();
  });

const VALUES = {
  designation: 'АБВГ.715311.001',
  name: 'Кронштейн опорный',
  developer: 'Иванов',
  organization: 'ООО «Ромашка»',
  note: 'Покрытие Хим.Окс.прм',
};

async function type(page: Page, field: keyof typeof VALUES, value: string) {
  const input = page.getByTestId(`doc-prop-${field}`);
  await input.fill(value);
  await input.blur();
  await expect.poll(async () => (await state(page)).documentProperties?.[field]).toBe(value);
}

test('document properties: typed, undone, saved and opened again', async ({ page }) => {
  await page.goto('/');
  await expect.poll(async () => (await state(page)).status, { timeout: 120_000 }).toBe('ready');
  await page.getByTestId('doc-open').setInputFiles(bracket);
  await expect.poll(async () => (await state(page)).documentProperties?.designation).toBe('');
  await expect(page.getByTestId('document-properties')).toContainText('Свойства документа');

  for (const [field, value] of Object.entries(VALUES)) await type(page, field as keyof typeof VALUES, value);
  expect((await state(page)).error).toBeNull();

  // Each field is one undo entry: undo takes the note back, redo returns it.
  await page.getByTestId('undo').click();
  await expect.poll(async () => (await state(page)).documentProperties?.note).toBe('');
  await expect(page.getByTestId('doc-prop-note')).toHaveValue('');
  await page.getByTestId('redo').click();
  await expect.poll(async () => (await state(page)).documentProperties?.note).toBe(VALUES.note);

  const download = page.waitForEvent('download');
  await page.getByTestId('doc-save').click();
  const file = await (await download).path();
  const json = JSON.parse(fs.readFileSync(file, 'utf8'));
  expect(json.properties).toEqual(VALUES);
  expect(json.material).toBeTruthy();  // the page material is still written next to them
  fs.mkdirSync(path.join(root, 'test-results/part-modeler'), { recursive: true });
  fs.copyFileSync(file, path.join(root, 'test-results/part-modeler/bracket_with_properties.bcpart.json'));

  // A new part has empty properties; opening the file fills the panel again.
  await page.getByTestId('doc-new').click();
  await expect.poll(async () => (await state(page)).documentProperties?.name).toBe('');
  await page.getByTestId('doc-open').setInputFiles(file);
  await expect.poll(async () => (await state(page)).documentProperties?.name).toBe(VALUES.name);
  for (const [field, value] of Object.entries(VALUES)) await expect(page.getByTestId(`doc-prop-${field}`)).toHaveValue(value);
});

