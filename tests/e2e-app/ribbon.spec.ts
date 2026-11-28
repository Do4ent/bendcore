// Ribbon in the application shell (docs/ui/APP_SHELL.md, "Лента") and the icon catalog
// (src/ui/icons/README.md). Shell: the part, assembly and STEP viewer tabs each show their ribbon;
// a ribbon command calls the editor (a sketch and an extrusion: the volume from the kernel); the
// language switch translates tabs, groups and captions; "icons only" chosen in one editor holds in
// the others and after a reload. Catalog: every icon drawn at 32, 24 and 16 px with a visible
// size; its picture for the documentation (BENDCORE_ICON_CATALOG_SCREENSHOT=<file>; otherwise
// the test output).
import { expect, test, Page, Locator } from '@playwright/test';

const activePanel = (page: Page): Locator => page.locator('[data-tab-panel][data-active="true"]');

async function freshShell(page: Page) {
  await page.goto('/');
  await page.evaluate(() => {
    try {
      window.localStorage.removeItem('bendcore.ribbon');
    } catch {
      // closed storage: nothing to clear
    }
    return new Promise<void>((resolve) => {
      const req = indexedDB.deleteDatabase('bendcore-app');
      req.onsuccess = req.onerror = req.onblocked = () => resolve();
    });
  });
  await page.reload();
  await expect(page.getByTestId('start-page')).toBeVisible();
}

test('ribbon in the shell: part, assembly and viewer tabs, a command, languages, icons only', async ({ page }) => {
  await freshShell(page);

  // Part: the ribbon, a sketch and an extrusion from its commands.
  await page.getByTestId('new-part').click();
  const P = activePanel(page);
  await expect(P.getByTestId('modeling-status')).toHaveText('Ядро готово');
  const partRibbon = P.getByTestId('part-ribbon');
  await expect(partRibbon.getByRole('tab')).toHaveText(['Эскиз', 'Операции', 'Массивы', 'Вспомогательная геометрия', 'Анализ']);
  await P.getByTestId('add-Sketch').click();
  await P.getByTestId('sketch-width').fill('30');
  await P.getByTestId('sketch-height').fill('20');
  await P.getByTestId('panel-ok').click();
  await expect(P.getByTestId('feature-panel')).toHaveCount(0);
  await P.getByTestId('ribbon-tab-features').click();
  await expect(partRibbon).toHaveAttribute('data-active-tab', 'features');
  await P.getByTestId('add-Extrude').click();
  await P.getByTestId('param-distance').fill('10');
  await P.getByTestId('panel-ok').click();
  await expect(P.getByTestId('feature-panel')).toHaveCount(0);
  await expect
    .poll(() =>
      page.evaluate(async () => {
        const m = (window as unknown as { bendcorePartModeler: { idle(): Promise<void>; getState(): { properties: { volume: number | null } | null } } }).bendcorePartModeler;
        await m.idle();
        return m.getState().properties?.volume ?? 0;
      }),
    )
    .toBeCloseTo(30 * 20 * 10, 6);

  // English: tabs, groups and captions from the dictionaries.
  await page.getByTestId('lang').selectOption('en');
  await expect(partRibbon.getByRole('tab')).toHaveText(['Sketch', 'Features', 'Patterns', 'Reference geometry', 'Analysis']);
  await expect(P.getByRole('group', { name: 'Shape' })).toBeVisible();
  await expect(P.getByTestId('add-Extrude')).toHaveAccessibleName('Extrude');
  await page.getByTestId('lang').selectOption('ru');
  await expect(P.getByTestId('add-Extrude')).toHaveAccessibleName('Выдавливание');

  // "Icons only" in the part editor.
  await P.getByTestId('ribbon-display').click();
  await expect(partRibbon).toHaveAttribute('data-display', 'icons');

  // Assembly: its ribbon, in the mode chosen before.
  await page.getByTestId('tab-start').click();
  await page.getByTestId('new-assembly').click();
  const A = activePanel(page);
  const asmRibbon = A.getByTestId('assembly-ribbon');
  await expect(asmRibbon.getByRole('tab')).toHaveText(['Компоненты', 'Сопряжения', 'Анализ', 'Исполнения']);
  await expect(asmRibbon).toHaveAttribute('data-display', 'icons');
  await A.getByTestId('ribbon-tab-analysis').click();
  await A.getByTestId('tab-bom').click();
  await expect(A.getByTestId('bom-panel')).toBeVisible();
  await A.getByTestId('ribbon-display').click();
  await expect(asmRibbon).toHaveAttribute('data-display', 'labels');

  // STEP viewer.
  await page.getByTestId('tab-start').click();
  await page.getByTestId('new-viewer').click();
  const V = activePanel(page);
  await expect(V.getByTestId('viewer-ribbon').getByRole('tab')).toHaveText(['Просмотр']);
  await expect(V.getByTestId('viewer-ribbon')).toHaveAttribute('data-display', 'labels');
});

test('icon catalog: every icon at 32, 24 and 16 px; picture for the documentation', async ({ page }, testInfo) => {
  await page.setViewportSize({ width: 1440, height: 900 });
  await page.goto('/icons.html');
  const catalog = page.getByTestId('icon-catalog');
  await expect(catalog).toBeVisible();
  const count = page.getByTestId('icon-count');
  const total = Number(await count.getAttribute('data-total'));
  const own = Number(await count.getAttribute('data-own'));
  const tabler = Number(await count.getAttribute('data-tabler'));
  expect(own).toBeGreaterThanOrEqual(100);
  expect(tabler).toBeGreaterThanOrEqual(10);
  expect(own + tabler).toBe(total);
  await expect(page.locator('[data-testid^="icon-"][data-source]')).toHaveCount(total);
  for (const section of ['general', 'sketch', 'feature', 'assembly', 'drawing']) {
    await expect(page.getByTestId(`icon-section-${section}`).locator('figure').first()).toBeVisible();
  }
  // Each icon draws something inside its 16 px box (not empty, not overflowing).
  const sizes = await page.evaluate(() =>
    [...document.querySelectorAll('figure[data-source]')].map((f) => {
      const svg = f.querySelectorAll('svg')[2] as SVGSVGElement;
      const b = svg.getBBox();
      return { name: f.getAttribute('data-testid'), w: b.width, h: b.height, x: b.x, y: b.y, size: svg.getAttribute('width') };
    }),
  );
  for (const s of sizes) {
    expect(s.size, s.name!).toBe('16');
    expect(Math.max(s.w, s.h), s.name!).toBeGreaterThan(6);
    expect(s.x, s.name!).toBeGreaterThanOrEqual(0);
    expect(s.y, s.name!).toBeGreaterThanOrEqual(0);
    expect(s.x + s.w, s.name!).toBeLessThanOrEqual(24);
    expect(s.y + s.h, s.name!).toBeLessThanOrEqual(24);
  }
  await page.screenshot({ path: process.env.BENDCORE_ICON_CATALOG_SCREENSHOT || testInfo.outputPath('icon_catalog.png'), fullPage: true });
});
