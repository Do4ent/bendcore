// Node check of the part document properties through the WebAssembly modules
// (docs/modeling/PART_DOCUMENT.md, "Свойства документа"; docs/ui/PART_MODELING_UI.md): editing and
// undo, the file, STEP (PRODUCT id), the component name and BOM properties in the assembly, the
// pattern features the assembly sees, fine-pitch fasteners with a strength class, and the title
// block of a part drawing.
//
//   node tests/wasm/part_properties.test.mjs --modeling <dir> --assembly <dir> --drawing <dir>
//
// The assembly and drawing directories are optional (their checks are skipped without them).

import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { after, before, describe, test } from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, '../..');

function argument(name, fallback) {
  const i = process.argv.indexOf(`--${name}`);
  return i >= 0 && i + 1 < process.argv.length ? path.resolve(process.argv[i + 1]) : fallback;
}

const modelingDir = argument('modeling', argument('module', path.join(repo, 'build-wasm-all/wasm_modeling')));
const assemblyDir = argument('assembly', path.join(repo, 'build-wasm-all/wasm_assembly'));
const drawingDir = argument('drawing', path.join(repo, 'build-wasm-all/wasm_drawing'));

async function load(dir, glueName, apiName, loader) {
  const glue = await import(pathToFileURL(path.join(dir, glueName)).href);
  const api = await import(pathToFileURL(path.join(dir, apiName)).href);
  return api[loader](glue.default);
}

const PROPS = {
  designation: 'ABVG.715311.001',
  name: 'Плита опорная',
  developer: 'Иванов',
  organization: 'ООО «Ромашка»',
  note: 'Анодировать',
};

let modeling;
let assembly = null;
let drawing = null;

before(async () => {
  modeling = await load(modelingDir, 'bendcore_modeling.mjs', 'bendcore_modeling_api.mjs', 'loadModeling');
  if (fs.existsSync(path.join(assemblyDir, 'bendcore_assembly.mjs')))
    assembly = await load(assemblyDir, 'bendcore_assembly.mjs', 'bendcore_assembly_api.mjs', 'loadAssembly');
  if (fs.existsSync(path.join(drawingDir, 'bendcore_drawing.mjs'))) {
    drawing = await load(drawingDir, 'bendcore_drawing.mjs', 'bendcore_drawing_api.mjs', 'loadDrawingKernel');
    drawing.setFont(new Uint8Array(fs.readFileSync(path.join(drawingDir, 'DejaVuSans.ttf'))), 'DejaVuSans.ttf');
  }
});

after(() => {
  assert.equal(modeling.info().liveDocuments, 0, 'every part document is disposed');
});

/** Plate 100 × 60 × 10 of 6061-T6 with the document properties set. */
function plateText() {
  const doc = modeling.createDocument();
  try {
    doc.addFeature({ type: 'Box', id: 'Body', parameters: { length: '100', width: '60', height: '10' } });
    doc.rebuild();
    doc.setDocumentProperties({ ...PROPS, material: 'al_6061_t6' });
    return doc.save();
  } finally {
    doc.dispose();
  }
}

describe('part document properties', () => {
  test('edit, undo, file, old file', () => {
    const doc = modeling.createDocument();
    try {
      doc.addFeature({ type: 'Box', id: 'Body', parameters: { length: '100', width: '60', height: '10' } });
      doc.rebuild();
      let p = doc.documentProperties();
      assert.equal(p.designation, '');
      assert.equal(p.density, null);
      doc.setDocumentProperties(PROPS);
      p = doc.documentProperties();
      assert.equal(p.designation, PROPS.designation);
      assert.equal(p.organization, PROPS.organization);
      assert.equal(p.effective.name, PROPS.name);
      assert.equal(doc.state().canUndo, true);
      doc.undo();
      assert.equal(doc.documentProperties().name, '');
      doc.redo();
      assert.equal(doc.documentProperties().name, PROPS.name);
      assert.throws(() => doc.setDocumentProperties({ density: -1 }), /density/);
      assert.throws(() => doc.setDocumentProperties({ name: 'a\nb' }), /single line/);

      // Saved with the page material: both fields in the file; the reader takes them back.
      const text = doc.save({ material: { id: 'steel_s235', density: 7850 } });
      const json = JSON.parse(text);
      assert.equal(json.version, 3);
      assert.equal(json.properties.designation, PROPS.designation);
      assert.deepEqual(json.material, { id: 'steel_s235', density: 7850 });
      const loaded = modeling.loadDocument(text);
      try {
        const q = loaded.document.documentProperties();
        assert.equal(q.note, PROPS.note);
        assert.equal(q.material, 'steel_s235');
        assert.equal(q.density, 7850);
        assert.deepEqual(loaded.material, { id: 'steel_s235', density: 7850 });
      } finally {
        loaded.document.dispose();
      }
      // A file without properties (as written before them) opens with empty properties.
      delete json.properties;
      delete json.material;
      const old = modeling.loadDocument(JSON.stringify(json));
      try {
        assert.equal(old.document.documentProperties().designation, '');
        assert.equal(old.material, null);
      } finally {
        old.document.dispose();
      }

      // STEP: PRODUCT id is the designation.
      const step = new TextDecoder().decode(doc.exportFile('step', { name: 'plate' }));
      assert.match(step, /PRODUCT\('[^']*','ABVG\.715311\.001'|PRODUCT\('ABVG\.715311\.001'/);
    } finally {
      doc.dispose();
    }
  });

  test('a configuration suffix completes the document designation', () => {
    const doc = modeling.createDocument();
    try {
      doc.setVariable('W', '60');
      doc.addFeature({ type: 'Box', id: 'Body', parameters: { length: 'W', width: '40', height: '20' } });
      doc.rebuild();
      doc.setDocumentProperties({ designation: 'ABVG.715311.001', name: 'Брусок' });
      doc.setConfigurations({
        columns: [{ kind: 'variable', target: 'W' }],
        rows: [
          { designation: '-00', name: '', cells: ['60'] },
          { designation: '-01', name: 'Брусок длинный', cells: ['90'] },
        ],
        active: '-01',
      });
      doc.rebuild();
      const t = doc.configurations();
      assert.deepEqual(t.rows.map((r) => r.fullDesignation), ['ABVG.715311.001-00', 'ABVG.715311.001-01']);
      assert.equal(doc.documentProperties().effective.designation, 'ABVG.715311.001-01');
      assert.equal(doc.documentProperties().effective.name, 'Брусок длинный');
    } finally {
      doc.dispose();
    }
  });
});

describe('in the assembly', () => {
  test('component name, BOM row and mass from the part document', (t) => {
    if (!assembly) return t.skip('no assembly module');
    const doc = assembly.createDocument('Unit');
    try {
      const r = doc.insertPart({ name: 'plate.bcpart', format: 'part-json', data: plateText(), fixed: true });
      const s = doc.state();
      const def = s.definitions.find((d) => d.id === r.definition);
      assert.equal(def.name, PROPS.name);
      assert.equal(def.properties.designation, PROPS.designation);
      assert.equal(def.properties.material, '6061-T6');
      assert.equal(def.properties.density, 2700);
      assert.equal(s.occurrences.find((o) => o.id === r.occurrence).name, PROPS.name);
      const bom = doc.bom();
      assert.equal(bom.rows[0].designation, PROPS.designation);
      assert.equal(bom.rows[0].name, PROPS.name);
      // m = 100·60·10 mm³ · 1e-9 · 2700 kg/m³.
      assert.ok(Math.abs(bom.rows[0].unitMass - 100 * 60 * 10 * 1e-9 * 2700) < 1e-12);
    } finally {
      doc.dispose();
    }
  });

  test('fine pitch and strength class of a fastener', (t) => {
    if (!assembly) return t.skip('no assembly module');
    const bolt = assembly.describeFastener({ standard: 'gost-7798', size: 12, length: 60, pitch: 1.25, strengthClass: '8.8' });
    assert.equal(bolt.designation, 'Болт М12×1,25-6g×60.88 ГОСТ 7798-70');
    assert.equal(bolt.strengthClass, '8.8');
    assert.equal(bolt.material, 'Сталь, класс прочности 8.8 ГОСТ 1759.4-87');
    assert.equal(bolt.thread.fine, true);
    assert.ok(Math.abs(bolt.thread.pitch - 1.25) < 1e-15);
    const cat = assembly.fastenerCatalog().find((c) => c.id === 'gost-7798');
    assert.deepEqual(cat.finePitches['12'], [1.5, 1.25]);
    assert.ok(cat.strengthClasses['12'].includes('10.9'));
    assert.throws(() => assembly.describeFastener({ standard: 'gost-7798', size: 12, length: 60, pitch: 1 }), /fine pitch/);
  });
});

describe('in the drawing', () => {
  test('title block of a part drawing from the document properties', (t) => {
    if (!drawing) return t.skip('no drawing module');
    const d = drawing.createFromPart(plateText(), { path: 'plate.bcpart.json', properties: { name: 'plate' } });
    try {
      const title = d.describe().sheets[0].titleShown;
      assert.equal(title.designation, PROPS.designation);
      assert.equal(title.name, PROPS.name);  // the document wins over the name written in the drawing
      assert.equal(title.material, '6061-T6');
      assert.equal(title.organization, PROPS.organization);
      assert.equal(title.developer, PROPS.developer);
      // m = 60000 mm³ · 1e-9 · 2700 = 0,162 kg (3 digits below 1 kg).
      assert.equal(title.mass, '0,162');
    } finally {
      d.dispose();
    }
  });
});
