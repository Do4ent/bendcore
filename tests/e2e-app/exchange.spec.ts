// Data exchange in the application shell (docs/ui/DATA_EXCHANGE.md):
//   - the user builds a part, exports it to STEP (AP242) and to binary STL through "Export…";
//     the downloaded files are read back (STEP by the kernel module under node: volume and
//     faces; STL by a reader written here: volume and closure); the mass panel shows the mass;
//   - the user imports IGES files of the corpus: a part opens as a body without history with its
//     volume, an assembly opens in the assembly editor and exports to 3MF (valid ZIP, CRC).
//
//   npx playwright test -c web/app/playwright.config.ts exchange
import { expect, test } from '@playwright/test';
import type { Locator, Page } from '@playwright/test';
import fs from 'fs';
import path from 'path';
import { pathToFileURL } from 'url';
import zlib from 'zlib';

const root = path.resolve(__dirname, '../..');
const out = path.join(root, 'test-results/app');
const corpus = path.join(root, 'tests/corpus/generated');
const kernelDir = path.resolve(process.env.BENDCORE_KERNEL_WASM_DIR || path.join(root, 'build-wasm-all/wasm_kernel'));
const manifest = JSON.parse(fs.readFileSync(path.join(corpus, 'manifest.json'), 'utf8'));

interface ShellState {
  tabs: { id: string; kind: string; title: string }[];
  active: string;
  storage: string;
  notifications: { level: string; text: string }[];
}
interface ViewerState {
  model: { volume: number | null; counts: { solids: number; faces: number }; structure: { parts: number; warnings: string[] } } | null;
  error: { code: string; message: string } | null;
}

declare global {
  interface Window {
    bendcoreApp?: { getState(): ShellState; idle(): Promise<void> };
    bendcorePartModeler?: { getState(): { properties: { volume: number | null } | null } };
    bendcoreKernelViewer?: { getState(): ViewerState };
    bendcoreAssembly?: { getState(): { status: string; state: { occurrences: { id: number }[] } | null } };
  }
}

const shell = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcoreApp!.idle();
    return window.bendcoreApp!.getState();
  });
const viewer = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcoreApp!.idle();
    return window.bendcoreKernelViewer!.getState();
  });
const activePanel = (page: Page): Locator => page.locator('[data-tab-panel][data-active="true"]');

// The kernel module under node reads the downloaded STEP back (independent of the page).
const importEsm = new Function('u', 'return import(u)') as (u: string) => Promise<Record<string, unknown>>;
async function nodeKernel() {
  const glue = await importEsm(pathToFileURL(path.join(kernelDir, 'bendcore_kernel.mjs')).href);
  const api = await importEsm(pathToFileURL(path.join(kernelDir, 'bendcore_kernel_api.mjs')).href);
  return (api.loadKernel as (f: unknown) => Promise<{
    importModel(b: Uint8Array, f: string): {
      shape: { volumeProperties(): { volume: number }; counts(): { faces: number; solids: number }; dispose(): void };
      structure: { name: string };
    };
  }>)(glue.default);
}

function stlTriangles(buf: Buffer): number[][][] {
  const n = buf.readUInt32LE(80);
  expect(84 + 50 * n).toBe(buf.length);
  const tris: number[][][] = [];
  for (let i = 0; i < n; ++i) {
    const o = 84 + 50 * i + 12;
    tris.push([0, 1, 2].map((v) => [0, 1, 2].map((c) => buf.readFloatLE(o + 12 * v + 4 * c))));
  }
  return tris;
}

function signedVolume(tris: number[][][]): number {
  let v = 0;
  for (const [a, b, c] of tris) {
    v += a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0]);
  }
  return v / 6;
}

function openEdges(tris: number[][][]): number {
  const key = (p: number[]) => p.join(',');
  const directed = new Map<string, number>();
  for (const t of tris) for (let i = 0; i < 3; ++i) directed.set(`${key(t[i])}>${key(t[(i + 1) % 3])}`, 1 + (directed.get(`${key(t[i])}>${key(t[(i + 1) % 3])}`) ?? 0));
  let bad = 0;
  for (const [k, n] of directed) {
    const [a, b] = k.split('>');
    if (n !== 1 || directed.get(`${b}>${a}`) !== 1) bad += 1;
  }
  return bad;
}

async function exportVia(page: Page, setup: (dialog: Locator) => Promise<void>): Promise<{ name: string; bytes: Buffer }> {
  await page.getByTestId('shell-export').click();
  const dialog = page.getByTestId('export-dialog');
  await expect(dialog).toBeVisible();
  await setup(dialog);
  const [download] = await Promise.all([page.waitForEvent('download'), dialog.getByTestId('export-save').click()]);
  const file = path.join(out, download.suggestedFilename());
  await download.saveAs(file);
  await expect(dialog).toHaveCount(0);
  return { name: download.suggestedFilename(), bytes: fs.readFileSync(file) };
}

async function freshShell(page: Page) {
  fs.mkdirSync(out, { recursive: true });
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

test('part exported to STEP and STL, files read back; mass of the part', async ({ page }) => {
  await freshShell(page);
  const [W, D, H] = [60, 40, 20];
  await page.getByTestId('new-part').click();
  const P = activePanel(page);
  await expect(P.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await P.getByTestId('add-Sketch').click();
  await P.getByTestId('sketch-width').fill(String(W));
  await P.getByTestId('sketch-height').fill(String(D));
  await P.getByTestId('panel-ok').click();
  await expect(P.getByTestId('feature-panel')).toHaveCount(0);
  await P.getByTestId('add-Extrude').click();
  await P.getByTestId('param-distance').fill(String(H));
  await P.getByTestId('panel-ok').click();
  await expect(P.getByTestId('feature-panel')).toHaveCount(0);
  await expect
    .poll(async () => page.evaluate(async () => (await window.bendcoreApp!.idle(), window.bendcorePartModeler!.getState().properties?.volume)))
    .toBe(W * D * H);

  // STEP AP242: the kernel reads the file back with the same volume and six faces.
  const step = await exportVia(page, async (d) => {
    await d.getByTestId('export-format').selectOption('step');
    await d.getByTestId('export-schema').selectOption('AP242');
  });
  // The part is "деталь.bcpart.json": headless Chromium reports a non-ASCII download name as
  // "download", so the name is checked in the shell's notification of the saved file.
  expect((await shell(page)).notifications.map((n) => n.text).join('\n')).toContain('деталь.step');
  const text = step.bytes.toString('latin1');
  expect(text.startsWith('ISO-10303-21;')).toBe(true);
  expect(text).toContain('AP242_MANAGED_MODEL_BASED_3D_ENGINEERING');
  const kernel = await nodeKernel();
  const back = kernel.importModel(new Uint8Array(step.bytes), 'step');
  try {
    expect(Math.abs(back.shape.volumeProperties().volume - W * D * H) / (W * D * H)).toBeLessThan(1e-9);
    expect(back.shape.counts().faces).toBe(6);
  } finally {
    back.shape.dispose();
  }

  // Binary STL with deflection 0.1: planar faces, so the closed mesh has the exact volume.
  const stl = await exportVia(page, async (d) => {
    await d.getByTestId('export-format').selectOption('stl');
    await d.getByTestId('export-stl-format').selectOption('binary');
    await d.getByTestId('export-deflection').fill('0.1');
  });
  expect((await shell(page)).notifications.map((n) => n.text).join('\n')).toContain('деталь.stl');
  const tris = stlTriangles(stl.bytes);
  expect(tris.length).toBe(12);
  expect(Math.abs(signedVolume(tris) - W * D * H)).toBeLessThan(1e-6 * W * D * H);
  expect(openEdges(tris)).toBe(0);

  // Mass properties of the part tab (analysis module): steel 7850 kg/m³.
  await page.getByTestId('shell-mass').click();
  const mass = page.getByTestId('shell-mass-panel').getByTestId('mass-value');
  await expect.poll(async () => Number(await mass.getAttribute('data-value'))).toBeCloseTo(W * D * H * 1e-9 * 7850, 9);
  await expect(page.getByTestId('shell-mass-panel').getByTestId('mass-center')).toHaveAttribute('data-value', `${W / 2},${D / 2},${H / 2}`);
});

test('IGES of the corpus: a part without history with its volume, an assembly exported to 3MF', async ({ page }) => {
  await freshShell(page);

  // Trimmed-surface IGES: sewn into one solid, volume of the plate with the hole.
  const plate = manifest.models.find((m: { name: string }) => m.name === 'plate_120x80x10_hole_d20');
  await page.getByTestId('import-model').setInputFiles(path.join(corpus, 'plate_120x80x10_hole_d20_faces.igs'));
  await expect(page.getByTestId('doc-tab-viewer')).toHaveCount(1);
  await expect.poll(async () => (await viewer(page)).model?.volume ?? 0).toBeGreaterThan(0);
  const v = await viewer(page);
  expect(v.error).toBeNull();
  expect(Math.abs(v.model!.volume! - plate.volume) / plate.volume).toBeLessThan(1e-5);
  expect(v.model!.counts.solids).toBe(1);
  expect(v.model!.counts.faces).toBe(plate.faces);
  expect(v.model!.structure.parts).toBe(1);
  await expect(activePanel(page).getByTestId('model-volume')).toContainText(String(Math.round(plate.volume)).slice(0, 4));
  await expect(activePanel(page).getByTestId('kernel-notice')).toContainText('sewn');
  // Mass of the imported body at the default density.
  await expect
    .poll(async () => Number(await activePanel(page).getByTestId('mass-value').getAttribute('data-value')))
    .toBeCloseTo(plate.volume * 1e-9 * 7850, 6);

  // IGES assembly: four solids of three parts — opened in the assembly editor.
  await page.getByTestId('tab-start').click();
  await page.getByTestId('import-model').setInputFiles(path.join(corpus, 'assembly_bracket_pins.igs'));
  await expect(page.getByTestId('doc-tab-assembly')).toHaveCount(1);
  await expect
    .poll(async () =>
      page.evaluate(async () => {
        await window.bendcoreApp!.idle();
        return window.bendcoreAssembly?.getState().state?.occurrences.length ?? 0;
      }),
    )
    .toBe(4);
  await expect(activePanel(page).getByTestId('assembly-tree')).toContainText('PIN');
  expect((await shell(page)).notifications.map((n) => n.text).join('\n')).toContain('4');

  const pkg = await exportVia(page, async (d) => {
    await d.getByTestId('export-format').selectOption('3mf');
  });
  expect(pkg.name).toMatch(/\.3mf$/);
  // ZIP read here: every entry's CRC-32 checked with node's zlib.
  const buf = pkg.bytes;
  const eocd = buf.lastIndexOf(Buffer.from([0x50, 0x4b, 0x05, 0x06]));
  expect(eocd).toBeGreaterThan(0);
  let cd = buf.readUInt32LE(eocd + 16);
  const names: string[] = [];
  for (let i = 0; i < buf.readUInt16LE(eocd + 10); ++i) {
    const method = buf.readUInt16LE(cd + 10);
    const crc = buf.readUInt32LE(cd + 16);
    const size = buf.readUInt32LE(cd + 20);
    const nameLen = buf.readUInt16LE(cd + 28);
    const local = buf.readUInt32LE(cd + 42);
    const name = buf.subarray(cd + 46, cd + 46 + nameLen).toString('utf8');
    const at = local + 30 + buf.readUInt16LE(local + 26) + buf.readUInt16LE(local + 28);
    const packed = buf.subarray(at, at + size);
    const data = method === 0 ? packed : zlib.inflateRawSync(packed);
    expect(zlib.crc32(data) >>> 0).toBe(crc);
    if (name === '3D/3dmodel.model') expect((data.toString('utf8').match(/<object /g) ?? []).length).toBe(4);
    names.push(name);
    cd += 46 + nameLen + buf.readUInt16LE(cd + 30) + buf.readUInt16LE(cd + 32);
  }
  expect(names.sort()).toEqual(['3D/3dmodel.model', '[Content_Types].xml', '_rels/.rels']);
});
