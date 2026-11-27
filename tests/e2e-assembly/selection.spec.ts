// Selection filters, box selection and the section in the assembly page (docs/ui/ASSEMBLY_UI.md,
// "Фильтры выбора, выбор рамкой и сечение"). The housing (80 x 60 x 20, bore D20 at (40, 30), two
// holes D11) and the shaft (D20 x 60) are inserted side by side. Expected counts come from the
// kernel's own element records (faces() -> entity type), the section area from the formula
// written out here.
import { expect, test } from '@playwright/test';
import type { Page } from '@playwright/test';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const fixture = (name: string) => path.join(root, 'tests/assembly-fixtures', name);

interface Ref {
  path: number[];
  index: number;
}

async function pageState(page: Page) {
  return page.evaluate(async () => {
    await window.bendcoreAssembly!.idle();
    const s = window.bendcoreAssembly!.getState();
    return {
      status: s.status,
      error: s.error,
      occurrences: s.state?.occurrences ?? [],
      elements: s.elements,
      tools: s.tools,
    };
  });
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
  return { block: s.occurrences.find((o) => o.name === 'block')!.id, shaft: s.occurrences.find((o) => o.name === 'shaft')!.id };
}

/** Number of faces of the occurrence whose kernel entity has this type. */
async function facesOfType(page: Page, occ: number, type: string): Promise<number> {
  return page.evaluate(
    async ([o, t]) => (await window.bendcoreAssembly!.faces([o as number])).filter((f) => f.entity && f.entity.type === t).length,
    [occ, type] as [number, string],
  );
}

const client = async (page: Page, p: number[]) => (await page.evaluate(([x, y, z]) => window.bendcoreAssembly!.worldToClient(x, y, z), p))!;

type Rect = { x0: number; y0: number; x1: number; y1: number };

async function rectAround(page: Page, min: number[], max: number[], margin = 14): Promise<Rect> {
  const xs: number[] = [];
  const ys: number[] = [];
  for (const x of [min[0], max[0]]) for (const y of [min[1], max[1]]) for (const z of [min[2], max[2]]) {
    const p = await client(page, [x, y, z]);
    xs.push(p.x);
    ys.push(p.y);
  }
  return { x0: Math.min(...xs) - margin, y0: Math.min(...ys) - margin, x1: Math.max(...xs) + margin, y1: Math.max(...ys) + margin };
}

async function drag(page: Page, r: Rect, direction: 'inside' | 'cross', modifier?: 'Shift' | 'Control') {
  const [ax, bx] = direction === 'inside' ? [r.x0, r.x1] : [r.x1, r.x0];
  if (modifier) await page.keyboard.down(modifier);
  await page.mouse.move(ax, r.y0);
  await page.mouse.down();
  await page.mouse.move((ax + bx) / 2, (r.y0 + r.y1) / 2, { steps: 4 });
  await page.mouse.move(bx, r.y1, { steps: 4 });
  await expect(page.getByTestId('box-rect')).toHaveAttribute('data-direction', direction);
  await page.mouse.up();
  if (modifier) await page.keyboard.up(modifier);
  await expect(page.getByTestId('box-rect')).toHaveCount(0);
}

test('box selection of faces by type, of components, with Shift and Ctrl', async ({ page }) => {
  await ready(page);
  const ids = await insertParts(page);
  const planes = (await facesOfType(page, ids.block, 'plane')) + (await facesOfType(page, ids.shaft, 'plane'));
  const cylinders = (await facesOfType(page, ids.block, 'cylinder')) + (await facesOfType(page, ids.shaft, 'cylinder'));
  expect(planes).toBe(8);
  expect(cylinders).toBeGreaterThanOrEqual(4);

  // Both components in view (the view was framed on the housing alone): zoom out until the
  // bounding box of the assembly lies inside the canvas.
  const canvas = (await page.getByTestId('assembly-viewport').boundingBox())!;
  await page.mouse.move(canvas.x + canvas.width / 2, canvas.y + canvas.height / 2);
  await page.mouse.wheel(0, 500);
  await expect
    .poll(async () => {
      const r = await rectAround(page, [0, -10, 0], [120, 60, 60], 20);
      return r.x0 > canvas.x && r.x1 < canvas.x + canvas.width && r.y0 > canvas.y && r.y1 < canvas.y + canvas.height;
    })
    .toBe(true);
  // The housing 0..80 x 0..60 x 0..20, the shaft D20 x 60 from x = 100 on.
  const whole = await rectAround(page, [0, -10, 0], [120, 60, 60], 12);
  await page.getByTestId('sel-box').click();
  const faces = async () => (await pageState(page)).elements.faces as Ref[];

  // Faces, plane filter: left to right takes every plane of both parts entirely inside.
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await faces()).length).toBe(planes + cylinders);
  await page.getByTestId('sel-filter-Plane').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await faces()).length).toBe(planes);
  expect(new Set((await faces()).map((r) => r.path[0])).size).toBe(2);
  await page.getByTestId('sel-filter-Plane').click();
  await page.getByTestId('sel-filter-Cylinder').click();
  await drag(page, whole, 'inside');
  await expect.poll(async () => (await faces()).length).toBe(cylinders);

  // Ctrl removes the cylinders again, Shift adds the planes.
  await drag(page, whole, 'inside', 'Control');
  await expect.poll(async () => (await faces()).length).toBe(0);
  await page.getByTestId('sel-filter-Cylinder').click();
  await page.getByTestId('sel-filter-Plane').click();
  await drag(page, whole, 'inside', 'Shift');
  await expect.poll(async () => (await faces()).length).toBe(planes);
  // The kernel's total area of the planes: housing outer faces + shaft end faces.
  const housingPlanes = 2 * (80 * 60 + 80 * 20 + 60 * 20) - 0;
  await expect
    .poll(async () => (await pageState(page)).elements.total)
    .toBeCloseTo(housingPlanes - 2 * (Math.PI * 100 + 2 * Math.PI * 5.5 * 5.5) + 2 * Math.PI * 100, 2);

  // Components: left to right around the housing alone takes it; around both takes both.
  await page.getByTestId('mode-component').check();
  const housingOnly = await rectAround(page, [0, 0, 0], [80, 60, 20], 6);
  await drag(page, housingOnly, 'inside');
  await expect.poll(async () => (await pageState(page)).elements.components).toEqual([ids.block]);
  await drag(page, whole, 'inside', 'Shift');
  await expect.poll(async () => [...(await pageState(page)).elements.components].sort()).toEqual([ids.block, ids.shaft].sort());
  await drag(page, housingOnly, 'inside', 'Control');
  await expect.poll(async () => (await pageState(page)).elements.components).toEqual([ids.shaft]);
});

test('hover highlights only allowed faces; a refused face is not picked', async ({ page }) => {
  await ready(page);
  const ids = await insertParts(page);
  void ids;
  const viewport = page.getByTestId('assembly-viewport');
  const top = await client(page, [20, 45, 20]);
  await page.mouse.move(top.x, top.y);
  await expect(viewport).toHaveAttribute('data-hover', /^face:/);
  await page.getByTestId('sel-filter-Cylinder').click();
  await page.mouse.move(top.x + 1, top.y + 1);
  await expect(viewport).toHaveAttribute('data-hover', '');
  await page.mouse.click(top.x, top.y);
  expect((await pageState(page)).elements.faces).toEqual([]);
  await page.getByTestId('sel-filter-Cylinder').click();
  await page.mouse.move(top.x, top.y);
  await expect(viewport).toHaveAttribute('data-hover', /^face:/);
});

// Section of the assembly (docs/ui/ASSEMBLY_UI.md): the kernel cuts every part by the plane; the
// area is the sum over the parts. Housing: 80 x 60 minus the bore D20 and two holes D11 at any
// height inside 0..20; shaft: a disc D20 for any height inside 0..60.
test('section of the assembly by a Z plane shows the area by formula', async ({ page }) => {
  await ready(page);
  await insertParts(page);
  const bore = Math.PI * 10 * 10;
  const hole = Math.PI * 5.5 * 5.5;
  const housing = 80 * 60 - bore - 2 * hole;

  await page.getByTestId('section-toggle').click();
  await page.getByTestId('section-axis').selectOption('Z');
  // The assembly spans z 0..60: slider 100/1000 is z = 6 (housing and shaft are both cut).
  await page.getByTestId('section-slider').fill('100');
  await expect(page.getByTestId('section-position')).toHaveAttribute('data-value', '6');
  await expect.poll(async () => (await pageState(page)).tools.sectionArea, { timeout: 30_000 }).toBeCloseTo(housing + bore, 3);
  await expect(page.getByTestId('section-area')).toHaveAttribute('data-value', /./);
  await expect(page.getByTestId('assembly-viewport')).toHaveAttribute('data-clipped', 'true');
  await expect.poll(async () => Number(await page.getByTestId('assembly-viewport').getAttribute('data-section-lines'))).toBeGreaterThanOrEqual(2);

  // Above the housing (z = 36): only the shaft is cut.
  await page.getByTestId('section-slider').fill('600');
  await expect(page.getByTestId('section-position')).toHaveAttribute('data-value', '36');
  await expect.poll(async () => (await pageState(page)).tools.sectionArea, { timeout: 30_000 }).toBeCloseTo(bore, 3);

  // A plane along X through the housing bore at x = 40: 60 x 20 minus the bore section 20 x 20.
  await page.getByTestId('section-axis').selectOption('X');
  await expect.poll(async () => (await pageState(page)).tools.section.axis).toBe('X');
  // The assembly spans x 0..120: 1000/3 -> x = 40 (the slider moves in steps of 1/1000).
  await page.getByTestId('section-slider').fill('333');
  const x = Number(await page.getByTestId('section-position').getAttribute('data-value'));
  expect(x).toBeCloseTo(39.96, 2);
  // At x = 39.96 the plane cuts the bore chord: 2 sqrt(100 - 0.04^2) = 19.9999...; area = 60 x 20 - 20 x chord.
  const chord = 2 * Math.sqrt(100 - (x - 40) * (x - 40));
  await expect.poll(async () => (await pageState(page)).tools.sectionArea, { timeout: 30_000 }).toBeCloseTo(60 * 20 - chord * 20, 2);

  // Removing the section restores the whole model.
  await page.getByTestId('section-toggle').click();
  await expect(page.getByTestId('assembly-viewport')).toHaveAttribute('data-clipped', 'false');
});
