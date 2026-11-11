// End-to-end test of the assembly page (docs/ui/ASSEMBLY_UI.md) over bendcore_assembly_wasm.
//
// The user inserts a housing (80 × 60 × 20, bore Ø20, two holes Ø11) and a shaft Ø20 × 60 from
// part documents, makes the shaft concentric with the bore and its end face coincident with the
// housing top by clicking faces in the viewport, sees one degree of freedom, drags the shaft
// (it turns about the bore axis and stays on it — checked in the document state), puts bolt
// stacks into the holes from the fastener library and reads the BOM quantities.
//
// Faces are identified by their kernel geometry (window.bendcoreAssembly.faces → element());
// the click position is a visible triangle of that face (faceClientPoint), and the click goes
// through the page's own picking.
import { expect, test } from '@playwright/test';
import type { Page } from '@playwright/test';
import fs from 'fs';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const fixture = (name: string) => path.join(root, 'tests/assembly-fixtures', name);

interface Entity {
  type: string;
  point: number[];
  direction?: number[];
  radius?: number;
}

async function pageState(page: Page) {
  return page.evaluate(() => {
    const s = window.bendcoreAssembly!.getState();
    return {
      status: s.status,
      error: s.error,
      occurrences: s.state?.occurrences ?? [],
      mates: s.state?.mates ?? [],
      solve: s.state?.solve ?? null,
      slots: s.mateSlots.map((x) => (x ? x.ref : null)),
      hole: s.hole ? s.hole.ref : null,
      explode: s.explode,
      parts: s.parts.map((p) => ({ path: p.path, position: p.world.position })),
    };
  });
}

async function faceIndex(page: Page, occ: number, pred: string): Promise<number[]> {
  // pred is evaluated in the page against each face entity (e).
  return page.evaluate(
    ([o, p]) => {
      // eslint-disable-next-line no-new-func
      const f = new Function('e', `return ${p};`) as (e: Entity) => boolean;
      return window
        .bendcoreAssembly!.faces([o as number])
        .filter((x) => x.entity && f(x.entity as Entity))
        .map((x) => x.index);
    },
    [occ, pred] as [number, string],
  );
}

async function clickFace(page: Page, occ: number, face: number) {
  const pt = await page.evaluate(([o, f]) => window.bendcoreAssembly!.faceClientPoint([o], f), [occ, face]);
  expect(pt, `face ${face} of occurrence ${occ} is visible`).not.toBeNull();
  await page.mouse.click(pt!.x, pt!.y);
}

async function worldToClient(page: Page, p: number[]) {
  const c = await page.evaluate(([x, y, z]) => window.bendcoreAssembly!.worldToClient(x, y, z), p);
  return c!;
}

async function ready(page: Page) {
  await page.goto('/');
  await expect.poll(async () => (await pageState(page)).status, { timeout: 120_000 }).toBe('ready');
}

async function insertParts(page: Page) {
  await page.getByTestId('insert-part-input').setInputFiles(fixture('block.json'));
  await expect.poll(async () => (await pageState(page)).occurrences.length).toBe(1);
  await page.getByTestId('insert-part-input').setInputFiles(fixture('shaft.json'));
  await expect.poll(async () => (await pageState(page)).occurrences.length).toBe(2);
  const s = await pageState(page);
  const block = s.occurrences.find((o) => o.name === 'block')!;
  const shaft = s.occurrences.find((o) => o.name === 'shaft')!;
  expect(block.fixed).toBe(true);
  expect(shaft.dof).toBe(6);
  // Inserted beside the housing: 20 mm gap along +X.
  expect(shaft.placement.position[0]).toBeCloseTo(80 + 20 + 10, 6);
  return { block: block.id, shaft: shaft.id };
}

async function mateShaft(page: Page, ids: { block: number; shaft: number }) {
  const [shaftSide] = await faceIndex(page, ids.shaft, "e.type === 'cylinder' && Math.abs(e.radius - 10) < 1e-6");
  const [bore] = await faceIndex(page, ids.block, "e.type === 'cylinder' && Math.abs(e.radius - 10) < 1e-6");
  await clickFace(page, ids.shaft, shaftSide);
  await clickFace(page, ids.block, bore);
  let s = await pageState(page);
  expect(s.slots).toEqual([
    { path: [ids.shaft], kind: 'face', index: shaftSide },
    { path: [ids.block], kind: 'face', index: bore },
  ]);
  await expect(page.getByTestId('mate-type-concentric')).toBeChecked();
  await page.getByTestId('mate-add').click();
  await expect.poll(async () => (await pageState(page)).mates.length).toBe(1);
  s = await pageState(page);
  expect(s.error).toBeNull();
  expect(s.occurrences.find((o) => o.id === ids.shaft)!.dof).toBe(2);

  // Shaft top (plane facing +Z at the highest z) and housing top (z = 20).
  const top = await faceIndex(page, ids.shaft, "e.type === 'plane' && e.direction[2] > 0.999");
  const housingTop = await faceIndex(page, ids.block, "e.type === 'plane' && e.direction[2] > 0.999 && Math.abs(e.point[2] - 20) < 1e-6");
  expect(top).toHaveLength(1);
  expect(housingTop).toHaveLength(1);
  await clickFace(page, ids.shaft, top[0]);
  await clickFace(page, ids.block, housingTop[0]);
  await expect(page.getByTestId('mate-type-coincident')).toBeChecked();
  await page.getByTestId('mate-add').click();
  await expect.poll(async () => (await pageState(page)).mates.length).toBe(2);
  s = await pageState(page);
  expect(s.solve!.success).toBe(true);
  expect(s.solve!.totalDof).toBe(1);
  await expect(page.getByTestId('total-dof')).toHaveText('1 степень свободы');
  await expect(page.getByTestId(`dof-${ids.shaft}`)).toHaveText('1 СС');
  const shaft = s.occurrences.find((o) => o.id === ids.shaft)!;
  // On the bore axis (40, 30), top flush with the housing top: shaft z ∈ [−40, 20].
  expect(shaft.placement.position[0]).toBeCloseTo(40, 6);
  expect(shaft.placement.position[1]).toBeCloseTo(30, 6);
  expect(shaft.placement.position[2]).toBeCloseTo(-40, 6);
  return shaft;
}

const zAngle = (q: number[]) => 2 * Math.atan2(q[2], q[3]);

test('insert, mate by clicks, drag about the axis, fasteners and BOM', async ({ page }) => {
  await ready(page);
  const ids = await insertParts(page);
  const before = await mateShaft(page, ids);

  // Drag a point of the shaft top face (r = 8) a quarter turn round the axis.
  await page.getByTestId('mode-drag').check();
  const r = 8;
  const at = (t: number) => [40 + r * Math.cos(t), 30 + r * Math.sin(t), 20];
  const start = await worldToClient(page, at(0));
  await page.mouse.move(start.x, start.y);
  await page.mouse.down();
  for (let k = 1; k <= 12; ++k) {
    const c = await worldToClient(page, at(((Math.PI / 2) * k) / 12));
    await page.mouse.move(c.x, c.y, { steps: 2 });
  }
  const during = await page.evaluate(() => window.bendcoreAssembly!.getState().dragSolve);
  expect(during?.success).toBe(true);
  await page.mouse.up();
  await expect.poll(async () => (await pageState(page)).mates.length).toBe(2);
  await expect.poll(async () => page.evaluate(() => window.bendcoreAssembly!.getState().state?.history.undoLabel)).toBe(
    'Drag component',
  );
  const s = await pageState(page);
  const after = s.occurrences.find((o) => o.id === ids.shaft)!;
  // Concentricity and the end face are kept: same origin on the axis; only the angle changed.
  expect(after.placement.position[0]).toBeCloseTo(40, 6);
  expect(after.placement.position[1]).toBeCloseTo(30, 6);
  expect(after.placement.position[2]).toBeCloseTo(-40, 6);
  const q = after.placement.rotation;
  expect(Math.hypot(q[0], q[1])).toBeLessThan(1e-6); // rotation axis is Z
  let turned = zAngle(q) - zAngle(before.placement.rotation);
  turned = Math.atan2(Math.sin(turned), Math.cos(turned));
  expect(Math.abs(turned)).toBeGreaterThan(Math.PI / 4);
  expect(Math.abs(turned)).toBeLessThan(Math.PI * 0.75);
  expect(s.solve!.totalDof).toBe(1);

  // Fasteners: bolt М10×40 ГОСТ 7798 + washers ГОСТ 11371 + nut ГОСТ 5915 in both Ø11 holes.
  await page.getByTestId('mode-face').check();
  await page.getByTestId('tab-fasteners').click();
  const holes = await faceIndex(page, ids.block, "e.type === 'cylinder' && Math.abs(e.radius - 5.5) < 1e-6");
  expect(holes).toHaveLength(2);
  let clicked = false;
  for (const h of holes) {
    const pt = await page.evaluate(([o, f]) => window.bendcoreAssembly!.faceClientPoint([o], f), [ids.block, h]);
    if (!pt) continue;
    await page.mouse.click(pt.x, pt.y);
    clicked = true;
    break;
  }
  expect(clicked).toBe(true);
  await expect.poll(async () => (await pageState(page)).hole?.path).toEqual([ids.block]);
  await expect(page.getByTestId('fastener-description')).toContainText('Болт М10×40 ГОСТ 7798-70');
  await page.getByTestId('fastener-insert').click();
  await expect.poll(async () => (await pageState(page)).occurrences.length).toBe(10);
  const placed = await pageState(page);
  expect(placed.error).toBeNull();
  const bolts = placed.occurrences.filter((o) => o.name.startsWith('Болт'));
  expect(bolts.map((b) => [b.placement.position[0], b.placement.position[1], b.placement.position[2]]).sort()).toEqual(
    [
      [15, 30, 22],
      [65, 30, 22],
    ].map((p) => p.map((x) => expect.closeTo(x, 6))),
  );

  // Interference report: no interference (the bolts are Ø10 in Ø11 holes).
  await page.getByTestId('tab-interference').click();
  await page.getByTestId('interference-check').click();
  await expect(page.getByTestId('interference-summary')).toContainText('Пересечений: 0');

  // Density of the housing from the properties panel: its BOM mass is V·ρ.
  await page.getByTestId(`component-${ids.block}`).locator('.as-name').click();
  await page.getByTestId('tab-properties').click();
  await page.getByTestId('prop-density').fill('7850');
  await page.getByTestId('prop-apply').click();
  await expect
    .poll(async () => page.evaluate((id) => window.bendcoreAssembly!.getState().state!.definitions.find((d) => d.id === id)!.properties.density, placed.occurrences.find((o) => o.id === ids.block)!.definition))
    .toBe(7850);

  // BOM quantities.
  await page.getByTestId('tab-bom').click();
  const table = page.getByTestId('bom-table');
  await expect(table).toBeVisible();
  const rows = await table.locator('tbody tr').evaluateAll((trs) =>
    trs.map((tr) => [tr.querySelector('[data-testid=bom-name]')!.textContent, tr.querySelector('[data-testid=bom-quantity]')!.textContent]),
  );
  const quantities = Object.fromEntries(rows);
  expect(quantities).toEqual({
    block: '1',
    shaft: '1',
    'Болт М10×40 ГОСТ 7798-70': '2',
    'Шайба 10 ГОСТ 11371-78': '4',
    'Гайка М10 ГОСТ 5915-70': '2',
  });
  await expect(page.getByTestId('bom-total-parts')).toHaveText('10');
  // Housing 80 × 60 × 20 minus bore Ø20 and two holes Ø11, steel: 0.674 kg.
  const housingMass = ((80 * 60 * 20 - Math.PI * (100 + 2 * 30.25) * 20) * 1e-9 * 7850).toPrecision(3);
  await expect(table.locator('tbody tr').first().locator('td').nth(5)).toHaveText(housingMass);
  // A click on the empty background clears the selection.
  const box = (await page.getByTestId('assembly-viewport').boundingBox())!;
  await page.mouse.click(box.x + 10, box.y + 10);
  await expect.poll(async () => page.evaluate(() => window.bendcoreAssembly!.getState().selectedComponent)).toBeNull();

  if (process.env.BENDCORE_DOC_SCREENSHOT) {
    await page.screenshot({ path: process.env.BENDCORE_DOC_SCREENSHOT });
  }
});

test('undo/redo, exploded view slider, save and reopen', async ({ page }) => {
  await ready(page);
  const ids = await insertParts(page);
  await mateShaft(page, ids);

  // Ctrl+Z removes the coincident mate (2 DOF again), Ctrl+Y restores it.
  await page.locator('body').click({ position: { x: 5, y: 5 } });
  await page.keyboard.press('Control+z');
  await expect.poll(async () => (await pageState(page)).mates.length).toBe(1);
  await expect(page.getByTestId('total-dof')).toHaveText('2 степени свободы');
  await page.keyboard.press('Control+y');
  await expect.poll(async () => (await pageState(page)).mates.length).toBe(2);
  await expect(page.getByTestId('total-dof')).toHaveText('1 степень свободы');

  // Exploded view: the shaft up by 80 mm; the slider moves it, the document does not change.
  await page.getByTestId(`component-${ids.shaft}`).locator('.as-name').click();
  await page.getByTestId('tab-explode').click();
  await page.getByTestId('explode-direction').selectOption('+Z');
  await page.getByTestId('explode-distance').fill('80');
  await page.getByTestId('explode-add-step').click();
  await page.getByTestId('explode-create').click();
  await expect.poll(async () => (await pageState(page)).explode.view).toBe(0);
  await page.getByTestId('explode-slider').fill('0.5');
  await expect
    .poll(async () => (await pageState(page)).parts.find((p) => p.path[0] === ids.shaft)!.position[2])
    .toBeCloseTo(-40 + 40, 6);
  let s = await pageState(page);
  expect(s.occurrences.find((o) => o.id === ids.shaft)!.placement.position[2]).toBeCloseTo(-40, 6);
  await page.getByTestId('explode-slider').fill('1');
  await expect
    .poll(async () => (await pageState(page)).parts.find((p) => p.path[0] === ids.shaft)!.position[2])
    .toBeCloseTo(40, 6);

  // Save → new document → open the file: same components, mates and DOF.
  const [download] = await Promise.all([page.waitForEvent('download'), page.getByTestId('save').click()]);
  const file = await download.path();
  expect(file).toBeTruthy();
  await page.getByRole('button', { name: 'Новая' }).click();
  await expect.poll(async () => (await pageState(page)).occurrences.length).toBe(0);
  await page.getByTestId('open-input').setInputFiles({ name: 'unit.json', mimeType: 'application/json', buffer: fs.readFileSync(file!) });
  await expect.poll(async () => (await pageState(page)).occurrences.length).toBe(2);
  s = await pageState(page);
  expect(s.mates.map((m) => m.type)).toEqual(['concentric', 'coincident']);
  expect(s.solve!.totalDof).toBe(1);
  expect(s.occurrences.find((o) => o.name === 'shaft')!.placement.position[2]).toBeCloseTo(-40, 6);

  // A mate between faces of one component is refused by the panel (no allowed type).
  const blockId = s.occurrences.find((o) => o.name === 'block')!.id;
  await page.getByTestId('tab-mate').click();
  const [top] = await faceIndex(page, blockId, "e.type === 'plane' && e.direction[2] > 0.999");
  const [side] = await faceIndex(page, blockId, "e.type === 'plane' && e.direction[0] > 0.999");
  await clickFace(page, blockId, top);
  await clickFace(page, blockId, side);
  await expect(page.getByTestId('mate-slot-b')).toContainText('block');
  await expect(page.getByText('Элементы одного компонента')).toBeVisible();
  await expect(page.getByTestId('mate-add')).toBeDisabled();
});
