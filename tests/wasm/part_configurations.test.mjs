// Node check of the configuration table of bendcore_modeling_wasm (docs/ui/PART_MODELING_UI.md,
// "Таблица параметров"; docs/modeling/PART_DOCUMENT.md).
//
// Block W×D×H with a fillet R = 2 on the top front edge and a through hole r = 5 in the middle of
// the top face; three configurations (different W/D/H, the hole suppressed in "-02"). Volume of
// each configuration against V = W·D·H − (1 − π/4)·R²·W − π·r²·H; switching back gives the same
// volume; the fillet reference survives; CSV and file round trips; explicit errors; undo.
//
//   node tests/wasm/part_configurations.test.mjs --module <dir with bendcore_modeling.mjs>
//
// Registered in ctest as wasm_part_configurations (cmake/wasm_modeling.cmake).

import assert from 'node:assert/strict';
import path from 'node:path';
import { after, before, test } from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, '../..');

function argument(name, fallback) {
  const i = process.argv.indexOf(`--${name}`);
  return i >= 0 && i + 1 < process.argv.length ? path.resolve(process.argv[i + 1]) : fallback;
}

const moduleDir = argument('module', path.join(repo, 'build-wasm-all/wasm_modeling'));
const MESH = { linearDeflection: 0.05, angularDeflection: 0.2 };
const { PI } = Math;
const R = 2;
const r = 5;
const blockVolume = (w, d, h, hole) => w * d * h - (1 - PI / 4) * R * R * w - (hole ? PI * r * r * h : 0);
const near = (a, b, tol = 1e-3) => Math.abs(a - b) <= tol;

let api;
let modeling;

before(async () => {
  const glue = await import(pathToFileURL(path.join(moduleDir, 'bendcore_modeling.mjs')).href);
  api = await import(pathToFileURL(path.join(moduleDir, 'bendcore_modeling_api.mjs')).href);
  modeling = await api.loadModeling(glue.default);
});

after(() => {
  assert.equal(modeling.info().liveDocuments, 0, 'every document created by the tests is disposed');
});

function edgeWhere(mesh, pred, what) {
  const found = mesh.edges.filter((e) => {
    for (let i = 0; i < e.points.length; i += 3) if (!pred(e.points[i], e.points[i + 1], e.points[i + 2])) return false;
    return true;
  });
  assert.equal(found.length, 1, `${what}: ${found.length} edges match`);
  return found[0].edgeIndex;
}

function faceWhere(mesh, pred, what) {
  const found = [];
  for (let s = 0; s < mesh.faceSpans.length; s += 3) {
    const first = mesh.faceSpans[s + 1];
    const count = mesh.faceSpans[s + 2];
    let all = true;
    for (let k = first; k < first + count && all; ++k) {
      const v = mesh.indices[k] * 3;
      all = pred(mesh.positions[v], mesh.positions[v + 1], mesh.positions[v + 2]);
    }
    if (all) found.push(mesh.faceSpans[s]);
  }
  assert.equal(found.length, 1, `${what}: ${found.length} faces match`);
  return found[0];
}

function rebuildOk(doc) {
  const report = doc.rebuild();
  const bad = doc.features().filter((f) => f.state !== 'ok' && f.state !== 'suppressed');
  assert.deepEqual(bad.map((f) => `${f.id}: ${f.state} ${f.messages.join('; ')}`), [], 'all features ok');
  return report;
}

const volume = (doc) => doc.properties('final').volume;

function buildBlock() {
  const doc = modeling.createDocument();
  doc.setVariable('W', '60');
  doc.setVariable('D', '40');
  doc.setVariable('H', '20');
  doc.addFeature({
    type: 'Sketch',
    id: 'Outline',
    options: { plane: 'XY' },
    sketch: modeling.sketchTemplate({ profiles: [{ kind: 'rectangle', x: 0, y: 0, width: 'W', height: 'D' }] }),
  });
  doc.addFeature({ type: 'Extrude', id: 'Body', parameters: { distance: 'H' }, links: { sketch: 'Outline' } });
  rebuildOk(doc);
  let mesh = doc.tessellate('final', MESH);
  const edge = doc.makeReference('final', 'edge', edgeWhere(mesh, (x, y, z) => near(y, 0) && near(z, 20), 'top front edge'));
  doc.addFeature({ type: 'Fillet', id: 'Round', parameters: { radius: R }, references: { edges: [edge] } });
  rebuildOk(doc);
  mesh = doc.tessellate('final', MESH);
  const top = doc.makeReference('final', 'face', faceWhere(mesh, (x, y, z) => near(z, 20), 'top'));
  doc.addFeature({
    type: 'Sketch',
    id: 'HoleSketch',
    options: { plane: 'face' },
    parameters: { originX: 0, originY: 0, originZ: 'H' },
    references: { face: [top] },
    sketch: modeling.sketchTemplate({ profiles: [{ kind: 'circle', cx: 'W / 2', cy: 'D / 2', radius: r }] }),
  });
  doc.addFeature({
    type: 'Extrude',
    id: 'Hole',
    options: { mode: 'cut', reversed: 'true', extent: 'throughAll' },
    links: { sketch: 'HoleSketch' },
  });
  rebuildOk(doc);
  return doc;
}

const TABLE = {
  columns: [
    { kind: 'variable', target: 'W' },
    { kind: 'variable', target: 'D' },
    { kind: 'variable', target: 'H' },
    { kind: 'suppression', target: 'Hole' },
    { kind: 'material', target: '' },
  ],
  rows: [
    { designation: 'РЦ.080.00.010', name: 'Брусок', cells: ['60', '40', '20', '0', 'steel_s235'] },
    { designation: '-01', name: 'Брусок большой', cells: ['80', '50', '30', '0', 'al_6061_t6'] },
    { designation: '-02', name: 'Брусок без отверстия', cells: ['50', '30', '15', '1', ''] },
  ],
  active: 'РЦ.080.00.010',
};

test('volume of every configuration, switching back and forth, fillet reference', () => {
  const doc = buildBlock();
  try {
    assert.ok(Math.abs(volume(doc) - blockVolume(60, 40, 20, true)) < 1e-6 * blockVolume(60, 40, 20, true));
    doc.setConfigurations(TABLE);
    const t = doc.configurations();
    assert.equal(t.rows[1].fullDesignation, 'РЦ.080.00.010-01');
    assert.equal(t.columns[3].title, 'suppress:Hole');
    assert.equal(t.material, 'steel_s235');
    const cases = [
      ['-01', 80, 50, 30, true],
      ['-02', 50, 30, 15, false],
      ['РЦ.080.00.010', 60, 40, 20, true],
    ];
    let v0 = null;
    for (let round = 0; round < 2; ++round) {
      for (const [d, w, dd, h, hole] of cases) {
        doc.activateConfiguration(d);
        rebuildOk(doc);
        const v = volume(doc);
        const expected = blockVolume(w, dd, h, hole);
        assert.ok(Math.abs(v - expected) <= 1e-6 * expected, `${d}: ${v} vs ${expected}`);
        assert.equal(doc.feature('Hole').state, hole ? 'ok' : 'suppressed');
        assert.equal(doc.feature('Round').state, 'ok');
        if (d === 'РЦ.080.00.010') {
          if (v0 === null) v0 = v;
          else assert.equal(v, v0, 'switching back gives the same volume bit for bit');
        }
      }
    }
    const vars = Object.fromEntries(doc.variables().map((x) => [x.name, x.value]));
    assert.deepEqual(vars, { W: 60, D: 40, H: 20 });
    // Undo of the activation brings "-02" back.
    doc.undo();
    assert.equal(doc.configurations().active, '-02');
    assert.ok(Math.abs(volume(doc) - blockVolume(50, 30, 15, false)) < 1e-3);
  } finally {
    doc.dispose();
  }
});

test('explicit errors and the check of every configuration', () => {
  const doc = buildBlock();
  try {
    const bad = structuredClone(TABLE);
    bad.columns[0].target = 'Q';
    assert.throws(() => doc.setConfigurations(bad), (e) => e.code === 'InvalidInput' && /unknown variable 'Q'/.test(e.detail));
    const bad2 = structuredClone(TABLE);
    bad2.rows[2].cells[3] = 'maybe';
    assert.throws(() => doc.setConfigurations(bad2), (e) => /invalid value 'maybe'/.test(e.detail));
    assert.equal(doc.configurations().rows.length, 0, 'refused table changes nothing');
    doc.setConfigurations(TABLE);
    assert.throws(() => doc.setConfigurationCell(1, 4, 'unobtainium'), (e) => /unknown material/.test(e.detail));
    assert.throws(() => doc.activateConfiguration('-09'), (e) => /no configuration '-09'/.test(e.detail));
    doc.setConfigurationCell(2, 2, '-5');
    const checks = doc.checkConfigurations();
    assert.deepEqual(checks.map((c) => c.ok), [true, true, false]);
    assert.ok(checks[2].problems.some((p) => p.includes("feature 'Body'")), checks[2].problems.join('\n'));
    assert.ok(Math.abs(checks[1].volume - blockVolume(80, 50, 30, true)) < 1e-3);
    doc.addConfigurationColumn({ kind: 'suppression', target: 'Round' });
    assert.equal(doc.configurations().rows[0].cells[5], '0');
  } finally {
    doc.dispose();
  }
});

test('CSV and file round trips without loss; old files without a table', () => {
  const doc = buildBlock();
  try {
    const plain = doc.save();
    assert.equal(JSON.parse(plain).version, 3);
    const t = structuredClone(TABLE);
    t.rows[1].cells[0] = '80.5';
    t.rows[1].name = 'Брусок; "большой"';
    doc.setConfigurations(t);
    const before = doc.configurations();
    for (const decimal of ['.', ',']) {
      const csv = doc.exportConfigurationsCsv({ decimal });
      assert.ok(csv.includes(decimal === ',' ? ';80,5;' : ';80.5;'), csv);
      doc.setConfigurations({ columns: [], rows: [], active: '' });
      doc.importConfigurationsCsv(csv);
      const back = doc.configurations();
      assert.deepEqual(back.columns, before.columns);
      assert.deepEqual(back.rows, before.rows);
    }
    doc.activateConfiguration('-01');
    rebuildOk(doc);
    const json = doc.save({ material: { id: 'steel_s235', density: 7850 } });
    assert.equal(JSON.parse(json).version, 4);
    const loaded = modeling.loadDocument(json);
    try {
      const lt = loaded.document.configurations();
      assert.equal(lt.active, '-01');
      assert.deepEqual(lt.rows, doc.configurations().rows);
      assert.ok(Math.abs(volume(loaded.document) - blockVolume(80.5, 50, 30, true)) < 1e-3);
    } finally {
      loaded.document.dispose();
    }
    const old = modeling.loadDocument(plain);
    try {
      assert.deepEqual(old.document.configurations().rows, []);
    } finally {
      old.document.dispose();
    }
    assert.throws(() => doc.importConfigurationsCsv('designation;name;W\nA\n'), (e) => e.code === 'ParseError' && /line 2/.test(e.detail));
  } finally {
    doc.dispose();
  }
});
