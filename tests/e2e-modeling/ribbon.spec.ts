// Ribbon of the part and sketch editors (docs/ui/PART_MODELING_UI.md, "Лента"): tab switching,
// commands called from the ribbon (each a kernel call), unavailable commands while a panel is open,
// tooltip, "icons only" kept after a reload, collapsing, the sketch ribbon in place of the part
// ribbon. The second scenario takes the pictures of three commands for docs/ui/img/commands/ribbon
// (BENDCORE_RIBBON_SCREENSHOTS=<dir>; otherwise into the test output), 1440×900 at scale 2 like
// docs/ui/img/commands.
import { expect, test, Page } from '@playwright/test';
import path from 'path';

interface ModelerState {
  features: { id: string; type: string; state: string }[];
  properties: { volume: number | null } | null;
  panel: { type: string; picks: Record<string, string[]> } | null;
}

declare global {
  interface Window {
    bendcorePartModeler?: {
      getState(): ModelerState;
      idle(): Promise<void>;
      worldToClient(x: number, y: number, z: number): { x: number; y: number };
    };
  }
}

const state = (page: Page) =>
  page.evaluate(async () => {
    await window.bendcorePartModeler!.idle();
    return window.bendcorePartModeler!.getState();
  });

async function expectVolume(page: Page, expected: number) {
  await expect
    .poll(async () => {
      const v = (await state(page)).properties?.volume;
      return v === null || v === undefined ? Infinity : Math.abs(v - expected) / expected;
    })
    .toBeLessThan(1e-9);
}

async function sketchRectangle(page: Page, w: number, h: number) {
  await page.getByTestId('add-Sketch').click();
  await expect(page.getByTestId('feature-panel')).toHaveAttribute('data-type', 'Sketch');
  await page.getByTestId('sketch-width').fill(String(w));
  await page.getByTestId('sketch-height').fill(String(h));
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
}

async function extrude(page: Page, distance: number) {
  await page.getByTestId('add-Extrude').click();
  await page.getByTestId('param-distance').fill(String(distance));
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
}

const ribbon = (page: Page) => page.getByTestId('part-ribbon');

test.beforeEach(async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
});

test('part ribbon: tabs, commands, unavailable commands, tooltip, display mode, collapsing, sketch ribbon', async ({ page }) => {
  // Tabs of the part editor; the first one is "Эскиз".
  await expect(page.getByRole('tab')).toHaveText(['Эскиз', 'Операции', 'Массивы', 'Вспомогательная геометрия', 'Анализ']);
  await expect(ribbon(page)).toHaveAttribute('data-active-tab', 'sketch');
  await page.getByTestId('ribbon-tab-patterns').click();
  await expect(ribbon(page)).toHaveAttribute('data-active-tab', 'patterns');
  await expect(page.getByTestId('add-LinearPattern')).toBeInViewport();
  await expect(page.getByTestId('add-Extrude')).not.toBeInViewport();
  await expect(page.getByRole('group', { name: 'Массивы операций' })).toBeVisible();

  // Commands of the "Эскиз" and "Операции" tabs build a block: 60 × 40 × 20.
  await page.getByTestId('ribbon-tab-sketch').click();
  await sketchRectangle(page, 60, 40);
  await page.getByTestId('ribbon-tab-features').click();
  await expect(ribbon(page)).toHaveAttribute('data-active-tab', 'features');
  await page.getByTestId('add-Extrude').click();
  // While the panel is open the other feature commands are unavailable (shaded).
  await expect(page.getByTestId('add-Fillet')).toBeDisabled();
  await expect(page.getByTestId('add-Fillet')).toHaveClass(/bc-rb-disabled/);
  await page.getByTestId('param-distance').fill('20');
  await page.getByTestId('panel-ok').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);
  await expectVolume(page, 60 * 40 * 20);
  await expect(page.getByTestId('add-Fillet')).toBeEnabled();

  // "Вырез": the same kernel feature in cut mode; a pocket 10 deep through a circle Ø20 would need
  // a sketch on the top face, so here the panel opens with the mode set.
  await page.getByTestId('add-cut').click();
  await expect(page.getByTestId('feature-panel')).toHaveAttribute('data-type', 'Extrude');
  await expect(page.getByTestId('option-mode')).toHaveValue('cut');
  await page.getByTestId('panel-cancel').click();
  await expect(page.getByTestId('feature-panel')).toHaveCount(0);

  // A command of another tab brings its tab forward.
  await page.getByTestId('add-DatumPlane').click();
  await expect(ribbon(page)).toHaveAttribute('data-active-tab', 'reference');
  await expect(page.getByTestId('feature-panel')).toHaveAttribute('data-type', 'DatumPlane');
  await page.getByTestId('panel-cancel').click();

  // "Анализ": measuring from the ribbon switches the measurement tool of the viewport.
  await page.getByTestId('ribbon-tab-analysis').click();
  await page.getByTestId('ribbon-measure').click();
  await expect(page.getByTestId('ribbon-measure')).toHaveAttribute('aria-pressed', 'true');
  await expect(page.getByTestId('measure-toggle')).toHaveAttribute('aria-pressed', 'true');
  await page.getByTestId('ribbon-measure').click();
  await expect(page.getByTestId('measure-toggle')).toHaveAttribute('aria-pressed', 'false');

  // Tooltip: name, shortcut, description.
  await page.getByTestId('doc-save').hover();
  await expect(page.getByTestId('ribbon-tooltip')).toBeVisible();
  await expect(page.getByTestId('ribbon-tooltip')).toContainText('Сохранить');
  await expect(page.getByTestId('ribbon-tooltip')).toContainText('Клавиши: Ctrl+S');
  await expect(page.getByTestId('ribbon-tooltip')).toContainText('Сохранить документ в файл');
  await page.mouse.move(700, 600);
  await expect(page.getByTestId('ribbon-tooltip')).toBeHidden();

  // "Icons only" is kept after a reload; the command keeps its name.
  await page.getByTestId('ribbon-display').click();
  await expect(ribbon(page)).toHaveAttribute('data-display', 'icons');
  await page.reload();
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await expect(ribbon(page)).toHaveAttribute('data-display', 'icons');
  await expect(page.getByRole('button', { name: 'Выдавливание' })).toHaveAccessibleName('Выдавливание');
  await page.getByTestId('ribbon-display').click();
  await expect(ribbon(page)).toHaveAttribute('data-display', 'labels');

  // Collapsing hides the panels; a tab opens them over the page, a command closes them again.
  await page.getByTestId('ribbon-collapse').click();
  await expect(page.getByTestId('part-ribbon-body')).toBeHidden();
  await page.getByTestId('ribbon-tab-sketch').click();
  await expect(page.getByTestId('part-ribbon-body')).toBeVisible();
  await page.getByTestId('add-Sketch').click();
  await expect(page.getByTestId('part-ribbon-body')).toBeHidden();
  await expect(page.getByTestId('feature-panel')).toHaveAttribute('data-type', 'Sketch');
  await page.getByTestId('panel-cancel').click();
  await page.getByTestId('ribbon-collapse').click();
  await expect(page.getByTestId('part-ribbon-body')).toBeVisible();

  // The sketch editor shows its own ribbon in place of the part ribbon.
  await page.getByTestId('sketch-draw').click();
  await expect(page.getByTestId('sketch-ribbon')).toBeVisible();
  await expect(ribbon(page)).toBeHidden();
  await page.getByTestId('tool-circle').click();
  await expect(page.getByTestId('tool-circle')).toHaveAttribute('aria-pressed', 'true');
  await expect(page.getByTestId('sketch-undo')).toBeDisabled();
  await page.getByTestId('sketch-cancel').click();
  await expect(page.getByTestId('sketch-ribbon')).toHaveCount(0);
  await expect(ribbon(page)).toBeVisible();
});

test.describe('pictures of the commands', () => {
  test.use({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });

  test('extrude, fillet, threaded hole from the ribbon', async ({ page }, testInfo) => {
    const dir = process.env.BENDCORE_RIBBON_SCREENSHOTS;
    const shot = (name: string) => page.screenshot({ path: dir ? path.join(dir, name) : testInfo.outputPath(name) });
    const tip = page.getByTestId('ribbon-tooltip');

    // 1. Extrude: the panel of the new feature with its preview.
    await sketchRectangle(page, 60, 40);
    await page.getByTestId('add-Extrude').click();
    await page.getByTestId('param-distance').fill('20');
    await page.getByTestId('panel-preview').click();
    await expect(page.getByTestId('panel-state')).toHaveAttribute('data-state', 'ok');
    await expectVolume(page, 60 * 40 * 20);
    await page.getByTestId('add-Extrude').locator('..').hover();
    await expect(tip).toBeVisible();
    await shot('1_extrude.png');
    await page.getByTestId('panel-ok').click();
    await expect(page.getByTestId('feature-panel')).toHaveCount(0);

    // 2. Fillet R5 of the vertical edge facing the camera.
    await page.getByTestId('add-Fillet').click();
    await page.getByTestId('param-radius').fill('5');
    const p = await page.evaluate(() => window.bendcorePartModeler!.worldToClient(60, 0, 10));
    await page.mouse.click(p.x, p.y);
    await expect.poll(async () => (await state(page)).panel!.picks.edges.length).toBe(1);
    await page.getByTestId('panel-preview').click();
    await expect(page.getByTestId('panel-state')).toHaveAttribute('data-state', 'ok');
    await expectVolume(page, 60 * 40 * 20 - (1 - Math.PI / 4) * 25 * 20);
    await page.mouse.move(720, 880);
    await expect(tip).toBeHidden();
    await shot('2_fillet.png');
    await page.getByTestId('panel-cancel').click();
    await expect(page.getByTestId('feature-panel')).toHaveCount(0);

    // 3. Threaded hole M10 through the top face at (30, 20).
    await page.getByTestId('add-ThreadedHole').click();
    expect((await state(page)).panel!.type).toBe('ThreadedHole');
    const top = await page.evaluate(() => window.bendcorePartModeler!.worldToClient(15, 10, 20));
    await page.mouse.click(top.x, top.y);
    await expect.poll(async () => (await state(page)).panel!.picks.face.length).toBe(1);
    for (const [id, v] of [['param-nominal', 10], ['param-x', 30], ['param-y', 20], ['param-z', 20]] as const) {
      await page.getByTestId(id).fill(String(v));
    }
    await page.getByTestId('option-end').selectOption('through');
    await page.getByTestId('panel-preview').click();
    await expect(page.getByTestId('panel-state')).toHaveAttribute('data-state', 'ok');
    await page.mouse.move(720, 880);
    await shot('3_threaded_hole.png');
  });
});
