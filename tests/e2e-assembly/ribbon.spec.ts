// Ribbon of the assembly editor (docs/ui/ASSEMBLY_UI.md, "Лента"): parts inserted from the
// "Компоненты" tab, a concentric mate whose type is chosen on the "Сопряжения" tab, mate types the
// picked pair does not allow shaded, the BOM opened from "Анализ". At 1440×900, scale 2 (like
// docs/ui/img/commands) the scenario takes the picture of the mate command for
// docs/ui/img/commands/ribbon (BENDCORE_RIBBON_SCREENSHOTS=<dir>; otherwise into the test output).
import { expect, test } from '@playwright/test';
import type { Page } from '@playwright/test';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const fixture = (name: string) => path.join(root, 'tests/assembly-fixtures', name);

interface Entity {
  type: string;
  point: number[];
  direction?: number[];
  radius?: number;
}
interface AssemblyApi {
  idle(): Promise<void>;
  getState(): { status: string; state: { occurrences: { id: number; name: string }[]; mates: unknown[] } | null };
  faces(path: number[]): Promise<{ index: number; entity?: Entity }[]>;
  faceClientPoint(path: number[], face: number): { x: number; y: number } | null;
}
const api = (w: Window) => (w as unknown as { bendcoreAssembly: AssemblyApi }).bendcoreAssembly;

async function snapshot(page: Page) {
  return page.evaluate(async () => {
    const a = (window as unknown as { bendcoreAssembly: AssemblyApi }).bendcoreAssembly;
    await a.idle();
    const s = a.getState();
    return { status: s.status, occurrences: s.state?.occurrences ?? [], mates: s.state?.mates.length ?? 0 };
  });
}

async function face(page: Page, occ: number, pred: string): Promise<number> {
  const found = await page.evaluate(
    async ([o, p]) => {
      // eslint-disable-next-line no-new-func
      const f = new Function('e', `return ${p};`) as (e: Entity) => boolean;
      const a = (window as unknown as { bendcoreAssembly: AssemblyApi }).bendcoreAssembly;
      return (await a.faces([o as number])).filter((x) => x.entity && f(x.entity)).map((x) => x.index);
    },
    [occ, pred] as [number, string],
  );
  expect(found.length).toBeGreaterThan(0);
  return found[0];
}

async function clickFace(page: Page, occ: number, index: number) {
  const pt = await page.evaluate(([o, f]) => (window as unknown as { bendcoreAssembly: AssemblyApi }).bendcoreAssembly.faceClientPoint([o], f), [occ, index]);
  expect(pt).not.toBeNull();
  await page.mouse.click(pt!.x, pt!.y);
}

test.use({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });

test('assembly ribbon: insert, mate type from the ribbon, shaded types, BOM; picture of the mate command', async ({ page }, testInfo) => {
  void api;
  await page.goto('/');
  await expect.poll(async () => (await snapshot(page)).status, { timeout: 120_000 }).toBe('ready');
  const ribbon = page.getByTestId('assembly-ribbon');
  await expect(page.getByRole('tab')).toHaveText(['Компоненты', 'Сопряжения', 'Анализ', 'Исполнения']);
  await expect(ribbon).toHaveAttribute('data-active-tab', 'components');

  // "Компоненты / Вставка": the housing and the shaft.
  await page.getByTestId('insert-part-input').setInputFiles(fixture('block.json'));
  await expect.poll(async () => (await snapshot(page)).occurrences.length).toBe(1);
  await page.getByTestId('insert-part-input').setInputFiles(fixture('shaft.json'));
  await expect.poll(async () => (await snapshot(page)).occurrences.length).toBe(2);
  const occ = (await snapshot(page)).occurrences;
  const block = occ.find((o) => o.name === 'block')!.id;
  const shaft = occ.find((o) => o.name === 'shaft')!.id;

  // "Сопряжения": without a picked pair every type is offered.
  await page.getByTestId('ribbon-tab-mates').click();
  await expect(ribbon).toHaveAttribute('data-active-tab', 'mates');
  await expect(page.getByTestId('ribbon-mate-concentric')).toBeEnabled();
  await expect(page.getByTestId('ribbon-mate-coincident')).toBeEnabled();

  // Shaft side and bore: "Соосность" from the ribbon, then "Добавить сопряжение".
  await clickFace(page, shaft, await face(page, shaft, "e.type === 'cylinder' && Math.abs(e.radius - 10) < 1e-6"));
  await clickFace(page, block, await face(page, block, "e.type === 'cylinder' && Math.abs(e.radius - 10) < 1e-6"));
  await page.getByTestId('ribbon-mate-concentric').click();
  await expect(page.getByTestId('mate-type-concentric')).toBeChecked();
  await page.getByTestId('mate-add').click();
  await expect.poll(async () => (await snapshot(page)).mates).toBe(1);

  // Two planes: concentric and tangent are shaded, coincident chosen from the ribbon.
  await clickFace(page, shaft, await face(page, shaft, "e.type === 'plane' && e.direction[2] > 0.999"));
  await clickFace(page, block, await face(page, block, "e.type === 'plane' && e.direction[2] > 0.999 && Math.abs(e.point[2] - 20) < 1e-6"));
  await expect(page.getByTestId('ribbon-mate-concentric')).toBeDisabled();
  await expect(page.getByTestId('ribbon-mate-tangent')).toBeDisabled();
  await page.getByTestId('ribbon-mate-distance').click();
  await expect(page.getByTestId('mate-type-distance')).toBeChecked();
  await page.getByTestId('ribbon-mate-coincident').click();
  await expect(page.getByTestId('mate-type-coincident')).toBeChecked();
  await page.mouse.move(720, 600);
  await page.getByTestId('ribbon-mate-coincident').locator('..').hover();
  await expect(page.getByTestId('ribbon-tooltip')).toContainText('Сопряжение «Совпадение» для выбранной пары элементов');
  await page.mouse.move(720, 880);
  await expect(page.getByTestId('ribbon-tooltip')).toBeHidden();
  const dir = process.env.BENDCORE_RIBBON_SCREENSHOTS;
  await page.screenshot({ path: dir ? path.join(dir, '4_mate.png') : testInfo.outputPath('4_mate.png') });
  await page.getByTestId('mate-add').click();
  await expect.poll(async () => (await snapshot(page)).mates).toBe(2);
  await expect(page.getByTestId('solve-status')).toHaveText('сопряжения выполнены');

  // "Решить" from the ribbon keeps the solution.
  await page.getByTestId('solve').click();
  await expect(page.getByTestId('solve-status')).toHaveText('сопряжения выполнены');

  // "Анализ / BOM": the panel with both parts.
  await page.getByTestId('ribbon-tab-analysis').click();
  await expect(ribbon).toHaveAttribute('data-active-tab', 'analysis');
  await page.getByTestId('tab-bom').click();
  await expect(page.getByTestId('tab-bom')).toHaveAttribute('aria-pressed', 'true');
  await expect(page.getByTestId('bom-panel')).toBeVisible();
  await expect(page.getByTestId('bom-name')).toHaveCount(2);

  // A panel command of another tab brings its tab forward.
  await page.getByTestId('tab-configurations').click();
  await expect(ribbon).toHaveAttribute('data-active-tab', 'configurations');
});
