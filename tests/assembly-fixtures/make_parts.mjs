// Regenerates the part documents used by the assembly tests (docs/ui/ASSEMBLY_UI.md, "Тесты")
// with the part document module, the same way the part modeling page builds parts.
//
//   node tests/assembly-fixtures/make_parts.mjs --module <dir with bendcore_modeling.mjs>
//
// block.json  — housing 80 × 60 × 20 (z ∈ [0, 20]): bore Ø20 at (40, 30), two holes Ø11 at
//               (15, 30) and (65, 30), all through.
// shaft.json  — shaft Ø20 × 60 (z ∈ [0, 60]) on the Z axis.
// plate.json  — plate 100 × 60 × 20 with two holes Ø11 at (25, 30) and (75, 30).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const i = process.argv.indexOf('--module');
const moduleDir = i >= 0 ? path.resolve(process.argv[i + 1]) : path.resolve(here, '../../build-wasm/wasm_modeling');
const glue = await import(pathToFileURL(path.join(moduleDir, 'bendcore_modeling.mjs')).href);
const api = await import(pathToFileURL(path.join(moduleDir, 'bendcore_modeling_api.mjs')).href);
const modeling = await api.loadModeling(glue.default);

function part(file, profiles, height) {
  const doc = modeling.createDocument();
  try {
    doc.addFeature({ type: 'Sketch', id: 'Profile', options: { plane: 'XY' }, sketch: modeling.sketchTemplate({ profiles }) });
    doc.addFeature({ type: 'Extrude', id: 'Body', parameters: { distance: height }, links: { sketch: 'Profile' } });
    doc.rebuild();
    const bad = doc.features().filter((f) => f.state !== 'ok');
    if (bad.length) throw new Error(`${file}: ${JSON.stringify(bad)}`);
    const p = doc.properties('final');
    fs.writeFileSync(path.join(here, file), doc.save({ includeCache: false }));
    console.log(file, 'volume', p.volume, 'faces', p.counts.faces);
  } finally {
    doc.dispose();
  }
}

part('block.json', [
  { kind: 'rectangle', x: 0, y: 0, width: 80, height: 60 },
  { kind: 'circle', cx: 40, cy: 30, radius: 10 },
  { kind: 'circle', cx: 15, cy: 30, radius: 5.5 },
  { kind: 'circle', cx: 65, cy: 30, radius: 5.5 },
], 20);
part('shaft.json', [{ kind: 'circle', cx: 0, cy: 0, radius: 10 }], 60);
part('plate.json', [
  { kind: 'rectangle', x: 0, y: 0, width: 100, height: 60 },
  { kind: 'circle', cx: 25, cy: 30, radius: 5.5 },
  { kind: 'circle', cx: 75, cy: 30, radius: 5.5 },
], 20);
