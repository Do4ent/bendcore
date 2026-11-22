// Node check of assembly configurations in bendcore_assembly_wasm (docs/ui/ASSEMBLY_UI.md,
// "Исполнения сборки"; docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 8.2).
//
// Housing tests/assembly-fixtures/block.json (fixed) with a shaft shaft.json in its bore Ø20
// (concentric; the distance of the shaft end from the housing bottom is a column), a plate
// plate.json whose inclusion is a column and the configured block configured_block.json whose
// configuration is a column. Configurations: base РЦ.500.00.000 (plate included, block as
// saved, distance 0) and -01 (plate excluded, block -01, distance 15). Composition, BOM, the
// mass of the configured block (V·ρ by formula), DOF, positions after switching back and forth,
// CSV round trip, undo, composition of every configuration, file round trip, nested subassembly
// with a configuration.
//
//   node tests/wasm/assembly_design_table.test.mjs --module <dir with bendcore_assembly.mjs>
//
// Registered in ctest as wasm_assembly_design_table (cmake/wasm_assembly.cmake).

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

const moduleDir = argument('module', path.join(repo, 'build-wasm/wasm_assembly'));
const fixture = (name) => fs.readFileSync(path.join(repo, 'tests/assembly-fixtures', name), 'utf8');
const { PI } = Math;
// configured_block.json: W×D×H with a fillet R = 2 on four vertical edges and a hole r = 5.
const blockVolume = (w, d, h, hole) => w * d * h - (1 - PI / 4) * 4 * w - (hole ? PI * 25 * h : 0);
const kg = (mm3, rho) => mm3 * 1e-9 * rho;
const STEEL = 7850;
const ALU = 2700;
const near = (a, b, tol = 1e-6) => Math.abs(a - b) <= tol;

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

function faceWhere(doc, occurrenceId, pred, what) {
  const state = doc.state();
  const occ = state.occurrences.find((o) => o.id === occurrenceId);
  const def = state.definitions.find((d) => d.id === occ.definition);
  const found = [];
  for (let i = 0; i < def.faces; ++i) {
    const e = doc.element({ path: [occurrenceId], kind: 'face', index: i });
    if (e.entity && pred(e.entity)) found.push(i);
  }
  assert.equal(found.length, 1, `${what}: ${found.length} faces match`);
  return { path: [occurrenceId], kind: 'face', index: found[0] };
}
const isCylinder = (r) => (e) => e.type === 'cylinder' && near(e.radius, r);
const isPlane = (z, nz) => (e) => e.type === 'plane' && near(e.point[2], z) && near(e.direction[2], nz, 1e-9);

function build(doc) {
  const housing = doc.insertPart({ name: 'Корпус', format: 'part-json', data: fixture('block.json'), fixed: true });
  const shaft = doc.insertPart({ name: 'Вал', format: 'part-json', data: fixture('shaft.json'), placement: { position: [40, 30, 3] } });
  const plate = doc.insertPart({ name: 'Планка', format: 'part-json', data: fixture('plate.json'), placement: { position: [0, 100, 0] } });
  const block = doc.insertPart({ name: 'Брусок', format: 'part-json', data: fixture('configured_block.json'), placement: { position: [150, 0, 0] } });
  doc.addMate({
    type: 'concentric',
    a: faceWhere(doc, shaft.occurrence, isCylinder(10), 'shaft side'),
    b: faceWhere(doc, housing.occurrence, isCylinder(10), 'bore'),
  });
  const distance = doc.addMate({
    type: 'distance',
    name: 'Вылет',
    value: 0,
    alignment: 'aligned',
    a: faceWhere(doc, shaft.occurrence, isPlane(3, -1), 'shaft end'),
    b: faceWhere(doc, housing.occurrence, isPlane(0, -1), 'housing bottom'),
  });
  doc.setProperties(0, { designation: 'РЦ.500.00.000', name: 'Узел вала' });
  doc.setConfigurationTable({
    columns: [
      { kind: 'suppression', target: plate.occurrence },
      { kind: 'configuration', target: block.occurrence },
      { kind: 'mateValue', target: distance },
    ],
    rows: [
      { designation: 'РЦ.500.00.000', name: 'Узел вала', cells: ['0', '#', '0'] },
      { designation: '-01', name: 'Узел вала без планки', cells: ['1', '-01', '15'] },
    ],
    active: 'РЦ.500.00.000',
  });
  return { housing, shaft, plate, block, distance };
}

const placements = (doc) => Object.fromEntries(doc.state().occurrences.map((o) => [o.id, o.placement]));

test('configurations: composition, BOM, mass, DOF, switching back and forth, undo', () => {
  const doc = kernel.createDocument('Узел вала');
  try {
    const p = build(doc);
    let table = doc.configurationTable();
    assert.equal(table.active, 'РЦ.500.00.000');
    assert.deepEqual(table.columns.map((c) => c.title), ['suppress:Планка', 'configuration:Брусок', 'mate:Вылет']);
    assert.deepEqual(table.columns[1].options.map((o) => o.fullDesignation), ['РЦ.080.00.010', 'РЦ.080.00.010-01', 'РЦ.080.00.010-02']);
    assert.equal(table.columns[2].unit, 'mm');
    assert.equal(table.rows[1].fullDesignation, 'РЦ.500.00.000-01');
    let state = doc.state();
    assert.equal(state.solve.success, true);
    // Shaft: rotation; plate and block: free.
    assert.equal(state.solve.totalDof, 13);
    const base = placements(doc);
    assert.ok(near(base[p.shaft.occurrence].position[2], 0, 1e-7), 'shaft end on the housing bottom');

    const a = doc.activateConfiguration('-01');
    assert.equal(a.fullDesignation, 'РЦ.500.00.000-01');
    assert.deepEqual(a.excluded, [p.plate.occurrence]);
    assert.deepEqual(a.disabledMates, []);
    assert.equal(a.solve.success, true);
    assert.equal(a.solve.totalDof, 7);
    state = doc.state();
    assert.equal(state.configuration.active, '-01');
    assert.equal(state.configuration.designation, 'РЦ.500.00.000-01');
    assert.equal(state.occurrences.find((o) => o.id === p.plate.occurrence).suppressed, true);
    const v01 = placements(doc);
    // Signed distance: the shaft end lies on the side of the bottom's normal (−Z).
    assert.ok(near(v01[p.shaft.occurrence].position[2], -15, 1e-7), 'distance 15 of the configuration');
    let bom = doc.bom('flat');
    assert.equal(bom.rows.length, 3, 'housing, shaft, block -01: the plate is excluded');
    const blockRow = bom.rows.find((r) => r.designation === 'РЦ.080.00.010-01');
    assert.equal(blockRow.quantity, 1);
    assert.ok(near(blockRow.unitMass, kg(blockVolume(80, 50, 30, true), ALU), 1e-9));
    assert.equal(bom.totalParts, 3);

    doc.activateConfiguration('РЦ.500.00.000');
    assert.deepEqual(placements(doc), base, 'back to the base: the same positions');
    bom = doc.bom('flat');
    assert.equal(bom.rows.length, 4);
    assert.ok(near(bom.rows.find((r) => r.designation === 'РЦ.080.00.010').unitMass, kg(blockVolume(60, 40, 20, true), STEEL), 1e-9));
    doc.activateConfiguration('-01');
    assert.deepEqual(placements(doc), v01, 'and forth again');

    // Undo returns the composition and the positions.
    assert.equal(doc.state().history.undoLabel, 'Activate configuration');
    doc.undo();
    assert.equal(doc.state().configuration.active, 'РЦ.500.00.000');
    assert.deepEqual(placements(doc), base);
    doc.redo();
    assert.equal(doc.state().configuration.active, '-01');

    // Composition of every configuration (group specification).
    const all = doc.configurationBom();
    assert.deepEqual(all.configurations.map((c) => c.fullDesignation), ['РЦ.500.00.000', 'РЦ.500.00.000-01']);
    const q = (designation) => all.rows.find((r) => r.designation === designation)?.quantities;
    assert.deepEqual(q('РЦ.080.00.010'), [1, 0]);
    assert.deepEqual(q('РЦ.080.00.010-01'), [0, 1]);
    assert.deepEqual(all.configurations.map((c) => c.totalParts), [4, 3]);

    // CSV there and back.
    for (const decimal of ['.', ',']) {
      table = doc.configurationTable();
      const csv = doc.configurationsToCsv({ decimal });
      // The module writes the UTF-8 byte order mark; the UTF-8 decoding of the returned string
      // (TextDecoder) drops it, so the page adds it to the downloaded file.
      assert.equal(csv.replace(/^\uFEFF/, '').split('\r\n')[0], 'designation;name;suppress:Планка;configuration:Брусок;mate:Вылет');
      const back = doc.configurationsFromCsv(csv);
      assert.deepEqual(back.columns.map((c) => [c.kind, c.target]), table.columns.map((c) => [c.kind, c.target]));
      assert.deepEqual(back.rows.map((r) => [r.designation, r.name, r.cells]), table.rows.map((r) => [r.designation, r.name, r.cells]));
    }
    assert.throws(() => doc.configurationsFromCsv('designation;name;suppress:Нет\r\n'), (e) => e.code === 'ParseError' && /no occurrence named/.test(e.detail ?? e.message));
    assert.throws(() => doc.activateConfiguration('-09'), (e) => e.code === 'InvalidInput');

    // File: the table, the active configuration and the base values are kept.
    const text = doc.save();
    assert.match(text, /"version": 3/);
    const loaded = kernel.loadDocument(text);
    try {
      const t2 = loaded.document.configurationTable();
      assert.equal(t2.active, '-01');
      assert.deepEqual(t2.rows.map((r) => r.cells), doc.configurationTable().rows.map((r) => r.cells));
      assert.deepEqual(t2.base, doc.configurationTable().base);
      loaded.document.activateConfiguration('РЦ.500.00.000');
      assert.deepEqual(placements(loaded.document), base);
    } finally {
      loaded.document.dispose();
    }
  } finally {
    doc.dispose();
  }
});

test('nested subassembly with a configuration', () => {
  const unit = kernel.createDocument('Узел');
  let unitText;
  try {
    unit.setProperties(0, { designation: 'РЦ.600.00.000', name: 'Узел' });
    const housing = unit.insertPart({ name: 'Корпус', format: 'part-json', data: fixture('block.json'), fixed: true });
    const s1 = unit.insertPart({ name: 'Вал', format: 'part-json', data: fixture('shaft.json'), placement: { position: [0, 0, 40] } });
    const s2 = unit.insertOccurrence({ definition: unit.state().occurrences.find((o) => o.id === s1.occurrence).definition, placement: { position: [0, 50, 40] } });
    unit.setConfigurationTable({
      columns: [{ kind: 'suppression', target: s2 }],
      rows: [
        { designation: 'РЦ.600.00.000', name: 'Узел', cells: ['1'] },
        { designation: '-01', name: 'Узел с двумя валами', cells: ['0'] },
      ],
      active: 'РЦ.600.00.000',
    });
    assert.equal(housing.occurrence > 0, true);
    unitText = unit.save();
  } finally {
    unit.dispose();
  }
  kernel.registerSource('unit.json', unitText);
  const doc = kernel.createDocument('Стенд');
  try {
    const sub = doc.insertSubassembly({ source: 'unit.json', fixed: true });
    let state = doc.state();
    const def = state.definitions.find((d) => d.id === sub.definition);
    assert.equal(def.assemblySource, 'unit.json');
    assert.deepEqual(def.configurations.map((c) => c.fullDesignation), ['РЦ.600.00.000', 'РЦ.600.00.000-01']);
    const shafts = (bom) => bom.rows.filter((r) => r.kind === 'part').find((r) => r.name.startsWith('Вал'))?.quantity;
    assert.equal(shafts(doc.bom('flat')), 1);
    doc.setOccurrence(sub.occurrence, { configuration: '-01' });
    state = doc.state();
    const d01 = state.definitions.find((d) => d.id === state.occurrences.find((o) => o.id === sub.occurrence).definition);
    assert.equal(d01.source, 'unit.json#-01');
    assert.equal(d01.properties.designation, 'РЦ.600.00.000-01');
    assert.equal(shafts(doc.bom('flat')), 2);
    doc.undo();
    assert.equal(shafts(doc.bom('flat')), 1);
  } finally {
    doc.dispose();
  }
});
