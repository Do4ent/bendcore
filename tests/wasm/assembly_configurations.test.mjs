// Node check of component configurations in bendcore_assembly_wasm (docs/ui/ASSEMBLY_UI.md,
// "Исполнение компонента"; docs/assembly/ASSEMBLY_DOCUMENT.md, "Исполнения деталей").
//
// Part document tests/assembly-fixtures/configured_block.json: block W×D×H with a fillet R = 2 and
// a through hole r = 5, configurations РЦ.080.00.010 (60×40×20, steel S235), -01 (80×50×30,
// 6061-T6), -02 (50×30×15, no hole, no material). Two components of different configurations:
// BOM rows, quantities and masses V·ρ against the formula; switching back reuses the
// definition; undo; save/load keeps the configuration sources.
//
//   node tests/wasm/assembly_configurations.test.mjs --module <dir with bendcore_assembly.mjs>
//
// Registered in ctest as wasm_assembly_configurations (cmake/wasm_assembly.cmake).

import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { after, before, test } from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, '../..');

function argument(name, fallback) {
  const i = process.argv.indexOf(`--${name}`);
  return i >= 0 && i + 1 < process.argv.length ? path.resolve(process.argv[i + 1]) : fallback;
}

const moduleDir = argument('module', path.join(repo, 'build-wasm-all/wasm_assembly'));
const blockJson = fs.readFileSync(path.join(repo, 'tests/assembly-fixtures/configured_block.json'), 'utf8');
const { PI } = Math;
const blockVolume = (w, d, h, hole) => w * d * h - (1 - PI / 4) * 4 * w - (hole ? PI * 25 * h : 0);
const kg = (mm3, rho) => mm3 * 1e-9 * rho;
const STEEL = 7850;
const ALU = 2700;

let api;
let kernel;

before(async () => {
  const glue = await import(pathToFileURL(path.join(moduleDir, 'bendcore_assembly.mjs')).href);
  api = await import(pathToFileURL(path.join(moduleDir, 'bendcore_assembly_api.mjs')).href);
  kernel = await api.loadAssembly(glue.default);
});

after(() => {
  assert.equal(kernel.info().liveDocuments, 0, 'every document created by the tests is disposed');
});

const close = (a, b, rel = 1e-6) => Math.abs(a - b) <= rel * Math.abs(b);
const rowOf = (bom, designation) => bom.rows.find((r) => r.designation === designation);

test('two components of different configurations: BOM and mass by configuration', () => {
  const doc = kernel.createDocument('Стойка');
  try {
    const first = doc.insertPart({ name: 'Брусок', format: 'part-json', data: blockJson, fixed: true });
    let state = doc.state();
    const def = state.definitions.find((d) => d.id === first.definition);
    assert.equal(def.partSource, 'Брусок.part.json');
    assert.equal(def.configuration, '');
    assert.deepEqual(def.configurations.map((c) => c.fullDesignation), ['РЦ.080.00.010', 'РЦ.080.00.010-01', 'РЦ.080.00.010-02']);
    assert.equal(def.properties.designation, 'РЦ.080.00.010');
    assert.equal(def.properties.density, STEEL);

    const second = doc.insertOccurrence({ definition: first.definition, placement: { position: [0, 120, 0] } });
    doc.setOccurrence(second, { configuration: '-01' });
    state = doc.state();
    const occ = state.occurrences.find((o) => o.id === second);
    const def01 = state.definitions.find((d) => d.id === occ.definition);
    assert.notEqual(def01.id, first.definition);
    assert.equal(def01.configuration, '-01');
    assert.equal(def01.source, 'Брусок.part.json#-01');
    assert.equal(def01.properties.designation, 'РЦ.080.00.010-01');
    assert.equal(def01.properties.material, '6061-T6');

    let bom = doc.bom('flat');
    assert.equal(bom.rows.length, 2);
    const r0 = rowOf(bom, 'РЦ.080.00.010');
    const r1 = rowOf(bom, 'РЦ.080.00.010-01');
    assert.equal(r0.quantity, 1);
    assert.equal(r1.quantity, 1);
    assert.ok(close(r0.unitVolume, blockVolume(60, 40, 20, true)));
    assert.ok(close(r1.unitVolume, blockVolume(80, 50, 30, true)));
    assert.ok(close(r0.unitMass, kg(blockVolume(60, 40, 20, true), STEEL)));
    assert.ok(close(r1.unitMass, kg(blockVolume(80, 50, 30, true), ALU)));
    assert.ok(close(bom.totalMass, kg(blockVolume(60, 40, 20, true), STEEL) + kg(blockVolume(80, 50, 30, true), ALU)));

    // "-02" without material keeps the material of the part as saved; back to '' reuses the base.
    doc.setOccurrence(second, { configuration: '-02' });
    bom = doc.bom('flat');
    assert.ok(close(rowOf(bom, 'РЦ.080.00.010-02').unitMass, kg(blockVolume(50, 30, 15, false), STEEL)));
    doc.setOccurrence(second, { configuration: '' });
    assert.equal(doc.state().occurrences.find((o) => o.id === second).definition, first.definition);
    assert.equal(doc.bom('flat').rows[0].quantity, 2);
    doc.undo();
    assert.equal(rowOf(doc.bom('flat'), 'РЦ.080.00.010-02').quantity, 1);

    assert.throws(() => doc.setOccurrence(second, { configuration: '-09' }), (e) => /no configuration '-09'/.test(e.detail));

    // Save and load: the configuration source is kept (part sources are registered in the module).
    doc.setOccurrence(second, { configuration: '-01' });
    const json = doc.save();
    const loaded = kernel.loadDocument(json);
    try {
      for (const s of loaded.report.sources) assert.ok(['Loaded', 'Embedded'].includes(s.status), `${s.source}: ${s.status} ${s.message}`);
      const lb = loaded.document.bom('flat');
      assert.equal(rowOf(lb, 'РЦ.080.00.010-01').quantity, 1);
      assert.ok(close(lb.totalMass, doc.bom('flat').totalMass));
    } finally {
      loaded.document.dispose();
    }
  } finally {
    doc.dispose();
  }
});

test('insert a configuration directly; registerPartSource lists the configurations', () => {
  const listed = kernel.registerPartSource('block.json', blockJson);
  assert.deepEqual(listed.configurations.map((c) => c.designation), ['РЦ.080.00.010', '-01', '-02']);
  const doc = kernel.createDocument('A');
  try {
    const p = doc.insertPart({ name: 'Брусок', format: 'part-json', data: blockJson, source: 'block.json', configuration: '-02' });
    const def = doc.state().definitions.find((d) => d.id === p.definition);
    assert.equal(def.properties.designation, 'РЦ.080.00.010-02');
    assert.ok(close(doc.bom('flat').rows[0].unitVolume, blockVolume(50, 30, 15, false)));
    assert.throws(
      () => doc.insertPart({ name: 'X', format: 'part-json', data: blockJson, configuration: '-07' }),
      (e) => /no configuration '-07'/.test(e.detail),
    );
  } finally {
    doc.dispose();
  }
});
