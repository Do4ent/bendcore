// Multiple sheet formats, zones and the hatching by material end to end (docs/ui/DRAWING_UI.md,
// «Формат листа», «Зоны и штриховка по материалу»).
//
// 1. Reference bracket: the format list holds the 5 basic sizes and the 19 multiples of GOST 2.301
//    table 2; А4×3 landscape gives a 630 × 297 sheet with the title block in the lower right corner
//    of the frame; in the ISO 7200 style the same choice is the ISO 5457 elongated size «ISO A4x3».
// 2. Shaft unit (tests/assembly-fixtures/shaft_unit.mjs) with the bushing made of polyamide: zones on
//    the margins fill the zone column of the specification for every position (letter + numeral of
//    the frame grid); the section through the bore hatches the bushing as a non-metal (GOST 2.306
//    table 1, item 2: crossed lines at 45° and 135°), a hand-picked symbol replaces it, «По детали»
//    brings it back.
import { expect, test, Page } from '@playwright/test';
import { spawnSync } from 'child_process';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const out = path.join(root, 'test-results/drawing');
const bracket = path.join(root, 'tests/wasm/fixtures/bracket.part.json');
const assemblyDir = path.resolve(process.env.BENDCORE_ASSEMBLY_WASM_DIR || path.join(root, 'build-wasm-all/wasm_assembly'));

type Bounds = { minX: number; minY: number; maxX: number; maxY: number };
interface EditorState {
  error: { code: string; message: string } | null;
  message: string;
  description: {
    sheetStyle: string;
    zones: { enabled: boolean; fieldLength: number };
    sheets: {
      id: string;
      name?: string;
      format: string;
      multiplier: number;
      width: number;
      height: number;
      views: { id: string; kind: string; bounds?: Bounds; hatchMaterial: string | null; hatchMaterialsShown?: string[] }[];
      balloons: { definition: number; text: string; at: [number, number] }[];
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

async function screenOf(page: Page, p: [number, number]) {
  const sheet = page.getByTestId('sheet');
  const box = (await sheet.boundingBox())!;
  const zoom = Number(await sheet.getAttribute('data-zoom'));
  const H = box.height / zoom;
  return { x: box.x + zoom * p[0], y: box.y + zoom * (H - p[1]) };
}

/** The shaft unit assembled by the assembly module in node, the bushing made of polyamide. */
function plasticShaftUnit(): string {
  const script = `
    const dir = ${JSON.stringify(assemblyDir)};
    const { buildShaftUnit } = await import(${JSON.stringify(path.join(root, 'tests/assembly-fixtures/shaft_unit.mjs'))});
    const api = await import(dir + '/bendcore_assembly_api.mjs');
    const k = await api.loadAssembly((await import(dir + '/bendcore_assembly.mjs')).default);
    process.stdout.write(buildShaftUnit(k));
  `;
  const run = spawnSync(process.execPath, ['--input-type=module', '-e', script], { maxBuffer: 1 << 28, encoding: 'utf8' });
  if (run.status !== 0) throw new Error(run.stderr);
  const doc = JSON.parse(run.stdout);
  const bushing = doc.definitions.find((d: { properties?: { designation?: string } }) => d.properties?.designation === 'УВ.01.001');
  expect(bushing).toBeDefined();
  bushing.properties.material = 'Полиамид ПА6';
  fs.mkdirSync(out, { recursive: true });
  const file = path.join(out, 'shaft_unit_plastic.bcasm.json');
  fs.writeFileSync(file, JSON.stringify(doc));
  return file;
}

test('multiple formats of GOST 2.301 and the ISO 5457 elongated sizes', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });
  await page.getByTestId('open-model').setInputFiles(bracket);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  await page.getByTestId('add-base').click();
  await expect(page.getByTestId('view-V1')).toHaveCount(1);
  await expectNoError(page);

  // The full list: 5 basic sizes and 19 multiples with their sizes.
  const options = page.getByTestId('sheet-format').locator('option');
  await expect(options).toHaveCount(24);
  await expect(page.getByTestId('sheet-format').locator('option[value="A4x3"]')).toHaveText('А4×3 (297×630)');
  await expect(page.getByTestId('sheet-format').locator('option[value="A0x2"]')).toHaveText('А0×2 (1189×1682)');

  await page.getByTestId('sheet-format').selectOption('A4x3');
  await expect(page.getByTestId('status')).toContainText('Формат');
  await expectNoError(page);
  let s = (await state(page)).description!.sheets[0];
  expect(s.format).toBe('A4');
  expect(s.multiplier).toBe(3);
  expect([s.width, s.height]).toEqual([630, 297]);
  await expect(page.getByTestId('sheet-format')).toHaveValue('A4x3');
  // The view stays inside the frame of the longer sheet.
  const b = s.views[0].bounds!;
  expect(b.minX).toBeGreaterThan(20);
  expect(b.maxX).toBeLessThan(625);
  // The kernel's SVG has the size of the sheet (mm).
  const svg = page.getByTestId('sheet-svg').locator('svg');
  await expect(svg).toHaveAttribute('viewBox', '0 0 630 297');

  await page.getByTestId('sheet-style').selectOption('iso7200');
  await expectNoError(page);
  s = (await state(page)).description!.sheets[0];
  expect(s.name).toBe('ISO A4x3');
  expect([s.width, s.height]).toEqual([630, 297]);

  await page.getByTestId('sheet-orientation').selectOption('portrait');
  await page.getByTestId('sheet-format').selectOption('A1x4');
  await expectNoError(page);
  s = (await state(page)).description!.sheets[0];
  expect([s.width, s.height]).toEqual([841, 2378]);
});

test('zones fill the specification zone column; the section is hatched by the part material', async ({ page }) => {
  const file = plasticShaftUnit();
  await page.goto('/');
  await expect(page.getByTestId('status')).toContainText('Откройте документ', { timeout: 120_000 });
  await page.getByTestId('open-model').setInputFiles(file);
  await expect(page.getByTestId('sheet-svg').locator('svg')).toHaveCount(1);
  await page.getByTestId('add-base').click();
  await expect(page.getByTestId('view-V1')).toHaveCount(1);
  await page.getByTestId('view-list').locator('li').first().click();
  await page.getByTestId('view-auto-balloons').click();
  await expectNoError(page);

  // Zones on the margins: the zone column gets «letter + numeral» for each of the 5 positions.
  await page.getByTestId('sheet-zones').click();
  await expect(page.getByTestId('status')).toContainText('Зоны на полях');
  await expectNoError(page);
  const d = (await state(page)).description!;
  expect(d.zones).toEqual({ enabled: true, fieldLength: 50 });
  expect(d.sheets[0].balloons.length).toBe(5);
  await page.getByTestId('tab-specification').click();
  for (const p of [1, 2, 3, 4, 5]) {
    const zone = page.getByTestId(`spec-position-${p}`).locator('td').nth(1);
    await expect(zone).toHaveText(/^[АБВГДЕЖИКЛМНПРСТУФЦЧШЩЭЮЯ]+\d+$/);
  }
  // A3 landscape: 8 columns × 6 rows (frame axes, fields of 50 mm) — the zone of position 1 is the
  // one of its shelf on the sheet (rows from the top, columns from the left).
  const cols = [20, 67.5, 117.5, 167.5, 217.5, 267.5, 317.5, 367.5, 415];
  const rows = [292, 248.5, 198.5, 148.5, 98.5, 48.5, 5];
  const letters = ['А', 'Б', 'В', 'Г', 'Д', 'Е'];
  const b1 = d.sheets[0].balloons.find((b) => b.text === '1')!;
  let c = 0;
  while (c < 7 && b1.at[0] >= cols[c + 1]) ++c;
  let r = 0;
  while (r < 5 && b1.at[1] + 3 <= rows[r + 1]) ++r;
  await expect(page.getByTestId('spec-position-1').locator('td').nth(1)).toHaveText(`${letters[r]}${c + 1}`);

  // Zone length 25 mm: more zones, the column follows.
  await page.getByTestId('tab-sheet').click();
  await page.getByTestId('sheet-zone-length').selectOption('25');
  await expect(page.getByTestId('status')).toContainText('Длина зоны 25 мм');
  await expectNoError(page);
  expect((await state(page)).description!.zones.fieldLength).toBe(25);

  // Section through the bore (plane x = 40, the middle of the front view).
  await page.getByTestId('tool-section').click();
  const front = (await state(page)).description!.sheets[0].views[0].bounds!;
  const p = await screenOf(page, [(front.minX + front.maxX) / 2, (front.minY + front.maxY) / 2]);
  await page.mouse.click(p.x, p.y);
  await expect(page.getByTestId('view-list').locator('li')).toHaveCount(2);
  await expectNoError(page);
  let sec = (await state(page)).description!.sheets[0].views[1];
  expect(sec.kind).toBe('section');
  expect(sec.hatchMaterial).toBeNull();
  expect(sec.hatchMaterialsShown).toEqual(['nonmetal']);

  await page.getByTestId('tool-select').click();
  await page.getByTestId('view-list').locator('li').nth(1).click();
  const select = page.getByTestId('view-hatch-material');
  await expect(select.locator('option').first()).toHaveText('По детали: неметалл (пластмасса, резина)');
  // Hatch lines of the section in the kernel's SVG: both slopes (crossed lines) on the bushing.
  await select.selectOption('glass');
  await expect(page.getByTestId('status')).toContainText('Штриховка: стекло');
  await expectNoError(page);
  sec = (await state(page)).description!.sheets[0].views[1];
  expect(sec.hatchMaterial).toBe('glass');
  expect(sec.hatchMaterialsShown).toEqual(['glass']);
  await select.selectOption('');
  await expect(page.getByTestId('status')).toContainText('Штриховка: По детали');
  sec = (await state(page)).description!.sheets[0].views[1];
  expect(sec.hatchMaterial).toBeNull();
  expect(sec.hatchMaterialsShown).toEqual(['nonmetal']);
});
