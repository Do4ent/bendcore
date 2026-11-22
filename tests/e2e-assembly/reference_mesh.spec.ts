// Reference mesh in the assembly (docs/ui/ASSEMBLY_UI.md, "Справочная сетка"): "Insert STL
// mesh…" adds a mesh component (not a part: no occurrence, no BOM row); the viewport shows it; a
// distance between two corners picked on the mesh is 10·√2; offset "0, 0, 50" moves it; undo
// returns it.
import { expect, test } from '@playwright/test';
import type { Page } from '@playwright/test';

function cubeStl(a: number): Buffer {
  const P = (x: number, y: number, z: number) => [x * a, y * a, z * a];
  const quads = [
    [[0, 0, 0], [0, 1, 0], [1, 1, 0], [1, 0, 0]],
    [[0, 0, 1], [1, 0, 1], [1, 1, 1], [0, 1, 1]],
    [[0, 0, 0], [1, 0, 0], [1, 0, 1], [0, 0, 1]],
    [[0, 1, 0], [0, 1, 1], [1, 1, 1], [1, 1, 0]],
    [[0, 0, 0], [0, 0, 1], [0, 1, 1], [0, 1, 0]],
    [[1, 0, 0], [1, 1, 0], [1, 1, 1], [1, 0, 1]],
  ];
  const tris: number[][][] = [];
  for (const q of quads) {
    const c = q.map((v) => P(v[0], v[1], v[2]));
    tris.push([c[0], c[1], c[2]], [c[0], c[2], c[3]]);
  }
  const buf = Buffer.alloc(84 + 50 * tris.length);
  buf.writeUInt32LE(tris.length, 80);
  tris.forEach((t, i) => t.forEach((v, k) => v.forEach((c, j) => buf.writeFloatLE(c, 84 + 50 * i + 12 + 12 * k + 4 * j))));
  return buf;
}

async function pageState(page: Page) {
  return page.evaluate(async () => {
    await window.bendcoreAssembly!.idle();
    const s = window.bendcoreAssembly!.getState();
    return {
      status: s.status,
      error: s.error,
      occurrences: s.state?.occurrences ?? [],
      meshes: s.state?.referenceMeshes ?? [],
      result: s.tools.result,
    };
  });
}

async function click(page: Page, p: number[]) {
  const c = await page.evaluate(([x, y, z]) => window.bendcoreAssembly!.worldToClient(x, y, z), p);
  await page.mouse.click(c!.x, c!.y);
}

test('STL mesh component: inserted, measured, moved, not a part', async ({ page }) => {
  await page.goto('/');
  await expect.poll(async () => (await pageState(page)).status, { timeout: 120_000 }).toBe('ready');
  await page.getByTestId('mesh-insert').setInputFiles({ name: 'scan.stl', mimeType: 'model/stl', buffer: cubeStl(10) });
  await expect.poll(async () => (await pageState(page)).meshes.length).toBe(1);
  let s = await pageState(page);
  expect(s.error).toBeNull();
  expect(s.occurrences).toEqual([]);
  expect(s.meshes[0].properties?.triangles).toBe(12);
  await expect(page.getByTestId('assembly-viewport')).toHaveAttribute('data-reference-meshes', '1');

  await page.getByTestId('measure-toggle').click();
  await click(page, [0.06, 0.06, 10]);
  await click(page, [9.94, 9.94, 10]);
  await expect.poll(async () => (await pageState(page)).result?.value ?? 0).toBeCloseTo(10 * Math.SQRT2, 9);
  await expect(page.getByTestId('measure-value')).toHaveText('14.142 mm');
  await page.getByTestId('measure-toggle').click();

  await page.getByTestId('mesh-translation').fill('0, 0, 50');
  await page.getByTestId('mesh-apply').click();
  await expect.poll(async () => (await pageState(page)).meshes[0].properties?.min[2] ?? 0).toBeCloseTo(50, 9);
  s = await pageState(page);
  expect(s.meshes[0].placement.position).toEqual([0, 0, 50]);
  await page.getByTestId('undo').click();
  await expect.poll(async () => (await pageState(page)).meshes[0].properties?.min[2] ?? 1).toBeCloseTo(0, 9);
});
