// End-to-end scenarios of the application shell (docs/ui/APP_SHELL.md):
//   - part (sketch → extrude → fillet) → save → drawing of it → PDF; assembly of two parts,
//     "open component part";
//   - a draft survives a page reload and is restored;
//   - a long HLR in the drawing worker does not block the page, and can be cancelled.
//
//   npx playwright test -c web/app/playwright.config.ts
import { expect, test } from '@playwright/test';
import type { Locator, Page } from '@playwright/test';
import { spawnSync } from 'child_process';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const out = path.join(root, 'test-results/app');
const modelingDir = path.resolve(process.env.BENDCORE_MODELING_WASM_DIR || path.join(root, 'build-wasm-all/wasm_modeling'));

interface ShellState {
  tabs: { id: string; kind: string; title: string; dirty: boolean }[];
  active: string;
  draftSavedAt: number | null;
  storage: string;
  offeredDrafts: { id: string; kind: string; name: string }[];
  recent: { id: string; kind: string; name: string }[];
  lang: string;
}
interface ModelerState {
  status: string;
  error: { code: string; message: string } | null;
  features: { id: string; type: string; state: string }[];
  properties: { volume: number | null } | null;
  panel: { picks: Record<string, string[]> } | null;
}

declare global {
  interface Window {
    bendcoreApp?: { getState(): ShellState; idle(): Promise<void> };
    bendcorePartModeler?: { getState(): ModelerState; worldToClient(x: number, y: number, z: number): { x: number; y: number } };
    bendcoreAssembly?: { getState(): { status: string; state: { occurrences: { id: number; name: string }[] } | null } };
  }
}

// Kernel calls run in workers: idle() waits for every module worker of the page.
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

/** The shown document tab. */
const activePanel = (page: Page): Locator => page.locator('[data-tab-panel][data-active="true"]');

const filletLoss = (r: number, L: number) => (1 - Math.PI / 4) * r * r * L;

async function expectVolume(page: Page, expected: number) {
  await expect
    .poll(async () => {
      const v = (await modeler(page)).properties?.volume;
      return v === null || v === undefined ? Infinity : Math.abs(v - expected) / expected;
    })
    .toBeLessThan(1e-9);
}

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

/** New part tab: rectangle sketch W×D, extrusion H. */
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
  await expectVolume(page, W * D * H);
  return P;
}

async function saveWithCtrlS(page: Page): Promise<string> {
  const [download] = await Promise.all([page.waitForEvent('download'), page.keyboard.press('Control+s')]);
  const file = path.join(out, download.suggestedFilename());
  await download.saveAs(file);
  return fs.readFileSync(file, 'utf8');
}

test('part with a fillet, its drawing in PDF, an assembly of two parts', async ({ page }) => {
  fs.mkdirSync(out, { recursive: true });
  await freshShell(page);
  const W = 60;
  const D = 40;
  const H = 20;
  const r = 5;
  const P = await buildBlock(page, W, D, H);

  // Fillet of the vertical edge x = W, y = 0, picked in the viewport.
  await P.getByTestId('add-Fillet').click();
  await P.getByTestId('param-radius').fill(String(r));
  const at = await page.evaluate(() => window.bendcorePartModeler!.worldToClient(60, 0, 10));
  await page.mouse.click(at.x, at.y);
  await expect.poll(async () => (await modeler(page)).panel?.picks.edges.length ?? 0).toBe(1);
  await P.getByTestId('panel-ok').click();
  await expect(P.getByTestId('feature-panel')).toHaveCount(0);
  await expectVolume(page, W * D * H - filletLoss(r, H));
  await expect(page.getByTestId('doc-tab-part').getByTestId('tab-dirty')).toHaveCount(1);

  // Ctrl+S saves the part file: the tab is clean and the file is in the recent list.
  await page.locator('.app-status').click();
  const partJson = await saveWithCtrlS(page);
  expect(JSON.parse(partJson).format).toBe('bendcore-part');
  await expect(page.getByTestId('doc-tab-part').getByTestId('tab-dirty')).toHaveCount(0);
  await expect.poll(async () => (await shell(page)).recent.map((x) => x.kind)).toContain('part');

  // Part → create drawing: a drawing tab of this part; base view and PDF.
  await P.getByTestId('doc-create-drawing').click();
  const Dr = activePanel(page);
  await expect(page.getByTestId('doc-tab-drawing')).toHaveCount(1);
  await expect(Dr.getByTestId('add-base')).toBeEnabled();
  await Dr.getByTestId('add-base').click();
  await expect(Dr.getByTestId('view-V1')).toHaveCount(1);
  await Dr.getByTestId('add-top').click();
  await expect(Dr.getByTestId('view-list').locator('li')).toHaveCount(2);
  const [pdf] = await Promise.all([page.waitForEvent('download'), Dr.getByTestId('export-pdf').click()]);
  const pdfFile = path.join(out, pdf.suggestedFilename());
  await pdf.saveAs(pdfFile);
  expect(fs.readFileSync(pdfFile).subarray(0, 5).toString('latin1')).toBe('%PDF-');

  // The drawing saves as one .bcdrw.json with its model; the start page opens it in a new tab.
  const [drw] = await Promise.all([page.waitForEvent('download'), Dr.getByTestId('drawing-save').click()]);
  expect(drw.suggestedFilename()).toMatch(/\.bcdrw\.json$/);
  const drwFile = path.join(out, drw.suggestedFilename());
  await drw.saveAs(drwFile);
  const drwJson = JSON.parse(fs.readFileSync(drwFile, 'utf8'));
  expect(drwJson.format).toBe('bendcore-drawing-file');
  expect(JSON.parse(drwJson.model).format).toBe('bendcore-part');
  await page.getByTestId('tab-start').click();
  await page.getByTestId('open-files').setInputFiles(drwFile);
  await expect(page.getByTestId('doc-tab-drawing')).toHaveCount(2);
  await expect(activePanel(page).getByTestId('view-list').locator('li')).toHaveCount(2);
  await expect(activePanel(page).getByTestId('view-V1')).toHaveCount(1);

  // Second part, then an assembly of both from the shell's part library.
  const P2 = await buildBlock(page, 30, 30, 10);
  await expect(P2.getByTestId('doc-save')).toBeEnabled();
  await page.getByTestId('tab-start').click();
  await page.getByTestId('new-assembly').click();
  const A = activePanel(page);
  await expect(A.getByTestId('library-part')).toBeVisible();
  const options = await A.getByTestId('library-part').locator('option').allTextContents();
  expect(options.length).toBeGreaterThanOrEqual(2);
  await A.getByTestId('library-part').selectOption({ index: 1 });
  await A.getByTestId('library-insert').click();
  await A.getByTestId('library-part').selectOption({ index: 2 });
  await A.getByTestId('library-insert').click();
  await expect
    .poll(async () =>
      page.evaluate(async () => {
        await window.bendcoreApp!.idle();
        return window.bendcoreAssembly!.getState().state?.occurrences.length ?? 0;
      }),
    )
    .toBe(2);
  await expect(A.getByTestId('assembly-tree').locator('[data-testid^="component-"]')).toHaveCount(2);

  // Assembly → open component part: a part tab with that part document.
  const partTabs = await page.getByTestId('doc-tab-part').count();
  await A.getByTestId('assembly-tree').locator('[data-testid^="component-"] .as-name').first().click();
  await A.getByTestId('open-component-part').click();
  await expect(page.getByTestId('doc-tab-part')).toHaveCount(partTabs + 1);
  const opened = activePanel(page);
  await expect(opened.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await expect.poll(async () => (await modeler(page)).features.length).toBeGreaterThanOrEqual(2);

  // The status bar names the shown document; the language switch changes the shell strings.
  await expect(page.getByTestId('app-status-doc')).toContainText('Деталь');
  await page.getByTestId('lang').selectOption('en');
  await expect(page.getByTestId('tab-start')).toHaveText('Start');
  await expect(page.getByTestId('app-status-doc')).toContainText('Part');
  // ...and the panels of the editors.
  await expect(opened.getByTestId('modeling-status')).toHaveText('Kernel ready');
  await expect(opened.getByTestId('add-Extrude')).toHaveText('Extrude');
  await page.getByTestId('lang').selectOption('ru');
  await expect(opened.getByTestId('add-Extrude')).toHaveText('Выдавливание');

  // Documentation screenshot of the shell with the drawing tab.
  await page.getByTestId('doc-tab-drawing').first().click();
  await page.screenshot({ path: process.env.BENDCORE_DOC_SCREENSHOT || path.join(out, 'app_shell.png') });
});

test('a draft is restored after the page is reloaded', async ({ page }) => {
  await freshShell(page);
  await buildBlock(page, 50, 30, 10);
  // Autosave writes the draft of the modified document within AUTOSAVE_MS.
  await expect(page.getByTestId('draft-saved')).toBeVisible();
  await page.reload();
  await expect(page.getByTestId('drafts-offer')).toBeVisible();
  const offered = (await shell(page)).offeredDrafts;
  expect(offered).toHaveLength(1);
  expect(offered[0].kind).toBe('part');
  await page.getByTestId('drafts-restore').click();
  const P = activePanel(page);
  await expect(P.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await expectVolume(page, 50 * 30 * 10);
  expect((await modeler(page)).features.map((f) => f.type)).toEqual(['Sketch', 'Extrude']);
  // Still a modified document: the restored draft is not a saved file.
  await expect(page.getByTestId('doc-tab-part').getByTestId('tab-dirty')).toHaveCount(1);
  // Discarding: after a reload the draft is offered again (the tab still holds it); discard removes it.
  await page.reload();
  await expect(page.getByTestId('drafts-offer')).toBeVisible();
  await page.getByTestId('drafts-discard').click();
  // The offer disappears once the deletion is committed.
  await expect(page.getByTestId('drafts-offer')).toHaveCount(0);
  await page.reload();
  await expect(page.getByTestId('start-page')).toBeVisible();
  await expect.poll(async () => (await shell(page)).storage).toBe('ready');
  await expect(page.getByTestId('drafts-offer')).toHaveCount(0);
});

/** Part with N×N through holes made by the modeling module in node (heavy for HLR). */
function heavyPart(n: number): string {
  const file = path.join(out, `plate_${n}x${n}_holes.bcpart.json`);
  if (fs.existsSync(file)) return file;
  const script = `
    const dir = ${JSON.stringify(modelingDir)};
    const { loadModeling } = await import(dir + '/bendcore_modeling_api.mjs');
    const m = await loadModeling((await import(dir + '/bendcore_modeling.mjs')).default);
    const doc = m.createDocument();
    const profiles = [{ kind: 'rectangle', x: 0, y: 0, width: ${n * 16 + 4}, height: ${n * 16 + 4} }];
    for (let i = 0; i < ${n}; ++i) for (let j = 0; j < ${n}; ++j) profiles.push({ kind: 'circle', cx: 10 + i * 16, cy: 10 + j * 16, radius: 4 });
    doc.addFeature({ type: 'Sketch', id: 'Plate', options: { plane: 'XY' }, sketch: m.sketchTemplate({ profiles }) });
    doc.addFeature({ type: 'Extrude', id: 'Body', parameters: { distance: 10 }, links: { sketch: 'Plate' } });
    const r = doc.rebuild();
    if (r.failed.length) throw new Error('rebuild failed: ' + JSON.stringify(r.failed));
    process.stdout.write(doc.save({ includeCache: true }));
  `;
  const run = spawnSync(process.execPath, ['--input-type=module', '-e', script], { maxBuffer: 1 << 30, encoding: 'utf8' });
  if (run.status !== 0) throw new Error(run.stderr);
  fs.writeFileSync(file, run.stdout);
  return file;
}

test('a long HLR does not block the page and can be cancelled', async ({ page }) => {
  fs.mkdirSync(out, { recursive: true });
  const file = heavyPart(16);
  await freshShell(page);
  await page.getByTestId('open-files').setInputFiles(file);
  const P = activePanel(page);
  await expect(P.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await expect.poll(async () => (await modeler(page)).features.length).toBe(2);
  await P.getByTestId('doc-create-drawing').click();
  const Dr = activePanel(page);
  await expect(Dr.getByTestId('add-base')).toBeEnabled({ timeout: 120_000 });

  // The base view: HLR of 256 holes runs in the drawing worker.
  await Dr.getByTestId('add-base').click();
  await expect(Dr.getByTestId('busy-indicator')).toBeVisible();
  await expect(Dr.getByTestId('busy-indicator')).toHaveAttribute('data-operation', 'hlr');
  // While it runs the page answers: the language switch re-renders the shell at once.
  const t0 = Date.now();
  await page.getByTestId('lang').selectOption('en');
  await expect(page.getByTestId('tab-start')).toHaveText('Start');
  const answered = Date.now() - t0;
  await expect(Dr.getByTestId('busy-indicator')).toBeVisible();
  expect(answered).toBeLessThan(1000);
  await page.getByTestId('lang').selectOption('ru');
  await expect(Dr.getByTestId('view-V1')).toHaveCount(1, { timeout: 120_000 });
  await expect(Dr.getByTestId('busy-indicator')).toHaveCount(0);

  // A second view, cancelled: the worker restarts, the drawing comes back with the base view.
  await Dr.getByTestId('add-isometric').click();
  await expect(Dr.getByTestId('busy-cancel')).toBeVisible();
  await Dr.getByTestId('busy-cancel').click();
  await expect(Dr.getByTestId('drawing-notice')).toBeVisible({ timeout: 120_000 });
  await expect(Dr.getByTestId('view-list').locator('li')).toHaveCount(1, { timeout: 120_000 });
  await expect(Dr.getByTestId('view-V1')).toHaveCount(1);
});

// Assembly document whose embedded binary BRep has one byte changed (offset 4384: 0 → 235),
// found by mutating the BRep of tests/assembly-fixtures/block.json
// (tests/regression/fuzz/wasm/README.md). Native OCCT rejects it with an error; in WASM the read
// runs out of linear memory: a WebAssembly trap ("memory access out of bounds").
const trapFile = path.join(root, 'tests/regression/fuzz/wasm/assembly_brep_trap.bcasm.json');
const assemblyFixture = (name: string) => path.join(root, 'tests/assembly-fixtures', name);

const draftsInStore = (page: Page) =>
  page.evaluate(
    () =>
      new Promise<{ id: string; kind: string; savedAt: number }[]>((resolve, reject) => {
        const req = indexedDB.open('bendcore-app');
        req.onerror = () => reject(req.error);
        req.onsuccess = () => {
          const all = req.result.transaction('drafts', 'readonly').objectStore('drafts').getAll();
          all.onsuccess = () => {
            resolve((all.result as { id: string; file: { kind: string }; savedAt: number }[]).map((d) => ({ id: d.id, kind: d.file.kind, savedAt: d.savedAt })));
            req.result.close();
          };
          all.onerror = () => reject(all.error);
        };
      }),
  );

const occurrences = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcoreApp!.idle();
    return window.bendcoreAssembly!.getState().state?.occurrences.length ?? -1;
  });

test('a module fault saves a draft and the module restarts from it', async ({ page }) => {
  await freshShell(page);
  await page.getByTestId('new-assembly').click();
  const A = activePanel(page);
  await expect.poll(() => page.evaluate(() => window.bendcoreAssembly?.getState().status), { timeout: 120_000 }).toBe('ready');
  await A.getByTestId('insert-part-input').setInputFiles(assemblyFixture('block.json'));
  await expect.poll(() => occurrences(page)).toBe(1);
  await expect(page.getByTestId('doc-tab-assembly').getByTestId('tab-dirty')).toHaveCount(1);

  // The damaged file, opened in the same editor: the call traps inside the assembly module.
  await A.getByTestId('open-input').setInputFiles(trapFile);
  const card = A.getByTestId('fault-card');
  await expect(card).toBeVisible();
  await expect(card).toHaveAttribute('data-code', 'ModuleFault');
  await expect(card).toHaveAttribute('data-module', 'assembly');
  await expect(card).toContainText('memory access out of bounds');
  await expect(card).toContainText('может быть повреждено');
  await expect(A.getByTestId('fault-draft')).toContainText('сохранено как черновик');
  const s = await shell(page);
  const tab = s.tabs.find((x) => x.kind === 'assembly')!;
  expect(tab.fault).toMatchObject({ module: 'assembly', code: 'ModuleFault' });
  // The draft is in IndexedDB: the assembly with one component, the last completed change.
  const drafts = await draftsInStore(page);
  expect(drafts.map((d) => [d.id, d.kind])).toEqual([[tab.id, 'assembly']]);
  // The failed module refuses further calls: no kernel call is left pending.
  await page.evaluate(() => window.bendcoreApp!.idle());

  // Restart: a new worker, the document restored from the draft, still unsaved.
  await A.getByTestId('fault-restart').click();
  await expect(A.getByTestId('fault-card')).toHaveCount(0);
  await expect.poll(() => occurrences(page)).toBe(1);
  await expect(A.getByTestId('assembly-tree').locator('[data-testid^="component-"]')).toHaveCount(1);
  const after = (await shell(page)).tabs.find((x) => x.id === tab.id)!;
  expect(after.fault).toBeNull();
  expect(after.generation).toBe(1);
  await expect(page.getByTestId('doc-tab-assembly').getByTestId('tab-dirty')).toHaveCount(1);
  // The restarted module works: a second part goes in.
  await A.getByTestId('insert-part-input').setInputFiles(assemblyFixture('shaft.json'));
  await expect.poll(() => occurrences(page)).toBe(2);
});
