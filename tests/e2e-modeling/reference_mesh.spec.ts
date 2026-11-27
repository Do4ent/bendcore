// Reference mesh in the part (docs/ui/PART_MODELING_UI.md, "Справочная сетка"): "Insert STL
// mesh…" takes a binary STL of a 10 mm cube; the viewport shows it translucent; the panel shows 12
// triangles, size 10 × 10 × 10 and volume 1000; inches scale it to 254 mm; a distance between two
// corners of the top face picked on the mesh is 10·√2 (snapped vertices); the section at z = 5
// becomes a sketch; undo removes the sketch; the mesh is no body (no volume); a broken file fails.
import { expect, test, Page } from '@playwright/test';

interface MeshInfo {
  id: string;
  unit: string;
  properties: { triangles: number; size: number[]; volume: number | null };
}
interface ModelerState {
  status: string;
  error: { code: string; message: string } | null;
  features: { id: string; type: string; state: string }[];
  properties: { volume: number | null } | null;
  meshes: MeshInfo[];
  tools: { result: { value: number; unit: string } | null };
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

async function clickModelPoint(page: Page, x: number, y: number, z: number) {
  const p = await page.evaluate(([a, b, c]) => window.bendcorePartModeler!.worldToClient(a, b, c), [x, y, z]);
  await page.mouse.click(p.x, p.y);
}

test('STL mesh: insert, units, distance between mesh corners, section to sketch', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByTestId('modeling-status')).toHaveText('Ядро готово');
  await page.getByTestId('mesh-insert').setInputFiles({ name: 'cube.stl', mimeType: 'model/stl', buffer: cubeStl(10) });
  await expect.poll(async () => (await state(page)).meshes.length).toBe(1);
  let s = await state(page);
  expect(s.error).toBeNull();
  expect(s.meshes[0].properties.triangles).toBe(12);
  expect(s.meshes[0].properties.size).toEqual([10, 10, 10]);
  expect(s.meshes[0].properties.volume).toBeCloseTo(1000, 9);
  expect(s.properties?.volume ?? null).toBeNull(); // no body: the mesh is for reference
  await expect(page.getByTestId('kernel-viewport')).toHaveAttribute('data-reference-meshes', '1');
  await expect(page.getByTestId('mesh-volume')).toHaveAttribute('data-value', '1000');

  // Distance between the corners (0, 0, 10) and (10, 10, 10) picked on the mesh.
  await page.getByTestId('measure-toggle').click();
  await expect(page.getByTestId('measure-mesh-hint')).toBeVisible();
  await clickModelPoint(page, 0.06, 0.06, 10);
  await clickModelPoint(page, 9.94, 9.94, 10);
  await expect.poll(async () => (await state(page)).tools.result?.value ?? 0).toBeCloseTo(10 * Math.SQRT2, 9);
  await expect(page.getByTestId('measure-value')).toHaveText('14.142 mm');
  await expect(page.getByTestId('measure-label')).toHaveAttribute('data-label', '14.142 mm');
  await page.getByTestId('measure-toggle').click();

  // Inches: 254 mm; undo returns millimetres.
  await page.getByTestId('mesh-unit').selectOption('in');
  await expect.poll(async () => (await state(page)).meshes[0].properties.size[0]).toBeCloseTo(254, 6);
  await page.getByTestId('undo').click();
  await expect.poll(async () => (await state(page)).meshes[0].unit).toBe('mm');

  // Section at z = 5 as a sketch of construction lines; undo removes it.
  await page.getByTestId('mesh-section-offset').fill('5');
  await page.getByTestId('mesh-section-make').click();
  await expect.poll(async () => (await state(page)).features.filter((f) => f.type === 'Sketch').length).toBe(1);
  s = await state(page);
  expect(s.error).toBeNull();
  expect(s.features[0].state).not.toBe('error');
  await page.getByTestId('undo').click();
  await expect.poll(async () => (await state(page)).features.length).toBe(0);
  expect((await state(page)).meshes.length).toBe(1);

  // A broken file: a clear error, no mesh added.
  await page.getByTestId('mesh-insert').setInputFiles({ name: 'cut.stl', mimeType: 'model/stl', buffer: cubeStl(10).subarray(0, 300) });
  await expect(page.getByTestId('modeling-error')).toBeVisible();
  expect((await state(page)).meshes.length).toBe(1);
});
