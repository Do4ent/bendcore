// Node check of bendcore_drawing_wasm: the drawing document through the JS API
// (docs/ui/DRAWING_UI.md).
//
// The model is reference part 1 of docs/modeling/REFERENCE_PARTS.md, the bracket
// (tests/wasm/fixtures/bracket.part.json: L-profile A = 60 along X, B = 50 along Z, wall T = 8,
// width Wd = 30 along Y, inner fillet r = 4, hole d = 8 at x = hx = 40). The edges for dimensions
// are chosen the way the UI does it: from the 2D edges of sheetPicture() by their sheet geometry.
// Dimension values are compared with the model parameters, view sizes with the part size times
// the scale; then a parameter change and update, annotations, view moves, export (PDF header and
// pdfinfo, DXF group structure and texts, ezdxf when available), save/load and errors.
//
//   node tests/wasm/drawing_document.test.mjs --module <dir with bendcore_drawing.mjs and DejaVuSans.ttf>
//
// Registered in ctest as wasm_drawing_document (cmake/wasm_drawing.cmake).

import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { after, before, describe, test } from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, '../..');

function argument(name, fallback) {
  const i = process.argv.indexOf(`--${name}`);
  return i >= 0 && i + 1 < process.argv.length ? path.resolve(process.argv[i + 1]) : fallback;
}

const moduleDir = argument('module', path.join(repo, 'build-wasm-drawing/wasm_drawing'));
const fontPath = argument('font', path.join(moduleDir, 'DejaVuSans.ttf'));
const partJson = fs.readFileSync(path.join(here, 'fixtures/bracket.part.json'), 'utf8');
const P = { A: 60, B: 50, T: 8, Wd: 30, r: 4, d: 8, hx: 40 };
const PROPS = { designation: 'РД.10.001', name: 'Кронштейн', material: 'Сталь 20', density: 7850 };
const EPS = 1e-6;

let api;
let kernel;

before(async () => {
  const glue = await import(pathToFileURL(path.join(moduleDir, 'bendcore_drawing.mjs')).href);
  api = await import(pathToFileURL(path.join(moduleDir, 'bendcore_drawing_api.mjs')).href);
  kernel = await api.loadDrawingKernel(glue.default);
});

after(() => {
  assert.equal(kernel.info().liveDrawings, 0, 'every drawing created by the tests is disposed');
});

const view = (pic, id) => pic.views.find((v) => v.id === id);
const centre = (b) => [(b.minX + b.maxX) / 2, (b.minY + b.maxY) / 2];
const isVertical = (e) => e.type === 'line' && Math.abs(e.points[0][0] - e.points[1][0]) < EPS;
const isHorizontal = (e) => e.type === 'line' && Math.abs(e.points[0][1] - e.points[1][1]) < EPS;
const lineLength = (e) => Math.hypot(e.points[1][0] - e.points[0][0], e.points[1][1] - e.points[0][1]);

// The visible straight edge with an extreme coordinate (min/max x of verticals, y of horizontals).
function extremeLine(v, pred, coord, pick) {
  const lines = v.edges.filter((e) => e.visible && e.edgeIndex !== null && pred(e));
  assert.ok(lines.length > 0, `${v.id}: no lines`);
  const value = (e) => e.points[0][coord];
  const best = pick(...lines.map(value));
  const found = lines.filter((e) => Math.abs(value(e) - best) < EPS);
  // Several collinear pieces: the longest.
  return found.sort((a, b) => lineLength(b) - lineLength(a))[0];
}

function dimensionById(drawing, sheet, id) {
  const d = drawing.describe().sheets.find((s) => s.id === sheet).dimensions.find((x) => x.id === id);
  assert.ok(d, `dimension ${id}`);
  return d;
}

function expectKernelError(fn, code, what) {
  assert.throws(fn, (e) => e instanceof api.KernelError && e.code === code, what);
}

// DXF group pairs: code lines are integers, the file ends with "0 EOF".
function dxfPairs(text) {
  const lines = text.split(/\r?\n/);
  if (lines[lines.length - 1] === '') lines.pop();
  assert.equal(lines.length % 2, 0, 'DXF: even number of lines');
  const pairs = [];
  for (let i = 0; i < lines.length; i += 2) {
    const code = Number(lines[i].trim());
    assert.ok(Number.isInteger(code), `DXF: group code at line ${i + 1}: '${lines[i]}'`);
    pairs.push([code, lines[i + 1]]);
  }
  return pairs;
}
const decodeDxf = (s) => s.replace(/\\U\+([0-9A-Fa-f]{4})/g, (_, h) => String.fromCharCode(parseInt(h, 16)));
function dxfTexts(pairs) {
  const out = [];
  for (let i = 0; i < pairs.length; ++i) {
    if (pairs[i][0] === 0 && pairs[i][1] === 'TEXT') {
      for (let j = i + 1; j < pairs.length && pairs[j][0] !== 0; ++j) if (pairs[j][0] === 1) out.push(decodeDxf(pairs[j][1]));
    }
  }
  return out;
}
function dxfHeader(pairs, name) {
  const i = pairs.findIndex(([c, v]) => c === 9 && v === name);
  assert.ok(i >= 0, `DXF header ${name}`);
  const values = [];
  for (let j = i + 1; j < pairs.length && pairs[j][0] !== 9 && pairs[j][0] !== 0; ++j) values.push(Number(pairs[j][1]));
  return values;
}

const which = (tool) => spawnSync('sh', ['-c', `command -v ${tool}`], { encoding: 'utf8' }).status === 0;

describe('bendcore_drawing_wasm', () => {
  test('a font is required and checked', () => {
    expectKernelError(() => kernel.createFromPart(partJson), 'InvalidInput', 'no font yet');
    expectKernelError(() => kernel.setFont(new Uint8Array([1, 2, 3, 4])), 'ParseError', 'not a TrueType font');
    const f = kernel.setFont(new Uint8Array(fs.readFileSync(fontPath)), path.basename(fontPath));
    assert.equal(f.postScriptName, 'DejaVuSans');
    assert.equal(kernel.info().font.postScriptName, 'DejaVuSans');
  });

  test('drawing of the bracket: views, dimensions = model parameters, update, export', () => {
    const drawing = kernel.createFromPart(partJson, { properties: PROPS, path: 'bracket.part.json' });
    try {
      // New drawing: one A3 landscape sheet with the title block only.
      let desc = drawing.describe();
      assert.equal(desc.sheets.length, 1);
      assert.equal(desc.sheets[0].name, 'А3');
      assert.equal(desc.sheets[0].width, 420);
      assert.equal(desc.sheets[0].height, 297);
      assert.equal(desc.sheets[0].views.length, 0);
      assert.equal(desc.sheets[0].titleShown.designation, PROPS.designation);
      assert.ok(drawing.sheetPicture('S1').svg.includes('Кронштейн'), 'title block text in the SVG');

      // Front, top, left (GOST 2.305, first angle) and isometric.
      const F = drawing.addView('S1', { kind: 'base', standard: 'front' });
      const T = drawing.addView('S1', { kind: 'projected', standard: 'top' });
      const L = drawing.addView('S1', { kind: 'projected', standard: 'left' });
      const I = drawing.addView('S1', { kind: 'axonometric', axonometry: 'isometric' });
      desc = drawing.describe();
      const sheet = desc.sheets[0];
      assert.deepEqual(sheet.views.map((v) => v.kind), ['base', 'projected', 'projected', 'axonometric']);
      assert.equal(sheet.views[1].side, 'below', 'first angle: the top view stands below the front view');
      assert.equal(sheet.views[2].side, 'rightOf', 'first angle: the left view stands right of the front view');
      const scale = sheet.computedScale.ratio[0] / sheet.computedScale.ratio[1];
      assert.equal(sheet.titleShown.scale, sheet.computedScale.label);

      let pic = drawing.sheetPicture('S1');
      const fb = view(pic, F).bounds, tb = view(pic, T).bounds, lb = view(pic, L).bounds;
      // View sizes: front A × B, top A × Wd, left Wd × B (times the sheet scale).
      assert.ok(Math.abs(fb.maxX - fb.minX - P.A * scale) < 1e-6 && Math.abs(fb.maxY - fb.minY - P.B * scale) < 1e-6, 'front size');
      assert.ok(Math.abs(tb.maxY - tb.minY - P.Wd * scale) < 1e-6, 'top view depth = Wd');
      assert.ok(Math.abs(lb.maxX - lb.minX - P.Wd * scale) < 1e-6, 'left view width = Wd');
      // Projection relation: top under the front (same x), left beside it (same y), no overlap.
      assert.ok(Math.abs(fb.minX - tb.minX) < 1e-6 && Math.abs(fb.maxX - tb.maxX) < 1e-6, 'top aligned with front');
      assert.ok(tb.maxY < fb.minY, 'top below front');
      assert.ok(Math.abs(fb.minY - lb.minY) < 1e-6 && Math.abs(fb.maxY - lb.maxY) < 1e-6, 'left aligned with front');
      assert.ok(lb.minX > fb.maxX, 'left right of front');
      assert.ok(view(pic, I).edges.length > view(pic, F).edges.length, 'isometric shows more edges');
      for (const v of pic.views) {
        assert.ok(v.bounds.minX > 20 && v.bounds.maxX < 415 && v.bounds.minY > 5 && v.bounds.maxY < 292, `${v.id} inside the frame`);
      }

      // Dimensions by picked edges: A between the outer verticals of the front view, B between
      // the bottom and the top lines, d on the hole circle of the top view, r on the fillet arc.
      const front = view(pic, F);
      const left = extremeLine(front, isVertical, 0, Math.min);
      const right = extremeLine(front, isVertical, 0, Math.max);
      const dA = drawing.addDimension('S1', F, [left.index, right.index]);
      const bottom = extremeLine(front, isHorizontal, 1, Math.min);
      const top = extremeLine(front, isHorizontal, 1, Math.max);
      const dB = drawing.addDimension('S1', F, [bottom.index, top.index]);
      const hole = view(pic, T).edges.find((e) => e.type === 'circle' && e.visible);
      assert.ok(hole, 'hole circle in the top view');
      assert.ok(Math.abs(hole.radius - (P.d / 2) * scale) < 1e-6);
      const dd = drawing.addDimension('S1', T, [hole.index]);
      const arc = front.edges.find((e) => e.type === 'arc' && e.visible);
      assert.ok(arc, 'fillet arc in the front view');
      const dr = drawing.addDimension('S1', F, [arc.index]);
      const dAng = drawing.addDimension('S1', F, [left.index, bottom.index]);
      const values = Object.fromEntries([dA, dB, dd, dr, dAng].map((id) => [id, dimensionById(drawing, 'S1', id)]));
      assert.equal(values[dA].type, 'horizontal');
      assert.ok(Math.abs(values[dA].value - P.A) < 1e-9, `A = ${values[dA].value}`);
      assert.equal(values[dA].text, '60');
      assert.equal(values[dB].type, 'vertical');
      assert.ok(Math.abs(values[dB].value - P.B) < 1e-9, `B = ${values[dB].value}`);
      assert.equal(values[dd].type, 'diameter');
      assert.ok(Math.abs(values[dd].value - P.d) < 1e-9);
      assert.equal(values[dd].text, '⌀8');
      assert.equal(values[dr].type, 'radius');
      assert.ok(Math.abs(values[dr].value - P.r) < 1e-9);
      assert.equal(values[dr].text, 'R4');
      assert.equal(values[dAng].type, 'angle');
      assert.ok(Math.abs(values[dAng].value - 90) < 1e-9);
      // Dimension placed outside the view: the A line is above the front view.
      assert.ok(values[dA].labelPosition[1] > fb.maxY, 'A above the front view');
      // Tolerance class: ISO 286 H7 on ⌀8.
      drawing.setDimension('S1', dd, { tolerance: 'H7' });
      assert.equal(dimensionById(drawing, 'S1', dd).text, '⌀8H7');

      // Annotations on edges: roughness, flatness, datum + parallelism, requirements, title.
      drawing.addRoughness('S1', F, left.index, { value: '3,2' });
      drawing.addDatum('S1', F, bottom.index, 'А');
      const topOfFlange = extremeLine(front, isHorizontal, 1, Math.max);
      drawing.addTolerance('S1', F, topOfFlange.index, { characteristic: 'parallelism', tolerance: '0,02', datums: ['А'] });
      drawing.addTolerance('S1', F, left.index, { characteristic: 'flatness', tolerance: '0,05', at: 0.3 });
      drawing.setSheet('S1', {
        technicalRequirements: ['Острые кромки притупить.', 'Покрытие Хим. Окс. прм.'],
        roughnessRemainder: { value: '6,3' },
        title: { organization: 'bendcore', developer: { surname: 'Иванов', date: '11.26' } },
      });
      desc = drawing.describe();
      assert.deepEqual(desc.sheets[0].roughness.map((r) => r.state), ['ok']);
      assert.deepEqual(desc.sheets[0].tolerances.map((r) => r.state), ['ok', 'ok']);
      assert.deepEqual(desc.sheets[0].datums.map((r) => r.state), ['ok']);
      // A location tolerance needs a datum: rejected, the drawing is unchanged.
      expectKernelError(
        () => drawing.addTolerance('S1', F, right.index, { characteristic: 'perpendicularity', tolerance: '0,1' }),
        'InvalidInput',
        'perpendicularity without datum',
      );
      assert.equal(drawing.describe().sheets[0].tolerances.length, 2);
      // A hole circle is not a straight line: no roughness on it.
      expectKernelError(() => drawing.addRoughness('S1', T, hole.index, { value: '1,6' }), 'InvalidInput', 'roughness on a circle');

      // Model change: A = 80, d = 10 -> the drawing reports what will change, update recomputes.
      const stale = drawing.setVariable('A', 80);
      assert.equal(stale.modelChanged, true);
      assert.ok(stale.staleViews.includes(`S1/${F}`));
      drawing.setVariable('d', 10);
      assert.equal(dimensionById(drawing, 'S1', dA).value, P.A, 'the drawing is not updated until asked');
      const report = drawing.updateFromModel();
      assert.equal(report.modelChanged, true);
      assert.equal(report.revision, 2);
      assert.equal(report.viewsComputed, 4);
      assert.deepEqual(report.dangling, []);
      assert.ok(report.stale.includes(`S1/${dA}: 60 → 80`), report.stale.join('; '));
      assert.ok(report.stale.includes(`S1/${dd}: 8 → 10`), report.stale.join('; '));
      assert.ok(!report.stale.some((s) => s.startsWith(`S1/${dB}:`)), 'B did not change');
      assert.ok(Math.abs(dimensionById(drawing, 'S1', dA).value - 80) < 1e-9);
      assert.equal(dimensionById(drawing, 'S1', dd).text, '⌀10H7');
      assert.equal(drawing.checkModel().modelChanged, false);
      // Update again: nothing changed, views from the cache.
      const again = drawing.updateFromModel();
      assert.equal(again.modelChanged, false);
      assert.deepEqual(again.stale, []);
      assert.equal(again.viewsComputed, 0);
      assert.equal(again.viewsFromCache, 4);

      // Moving views: a projected view moves the whole group (relation kept); the isometric moves alone.
      pic = drawing.sheetPicture('S1');
      const before = Object.fromEntries(pic.views.map((v) => [v.id, centre(v.bounds)]));
      const moved = drawing.moveView('S1', T, before[T][0] - 10, before[T][1] + 5);
      assert.equal(moved.group, true);
      pic = drawing.sheetPicture('S1');
      for (const id of [F, T, L]) {
        const c = centre(view(pic, id).bounds);
        assert.ok(Math.abs(c[0] - (before[id][0] - 10)) < 1e-6 && Math.abs(c[1] - (before[id][1] + 5)) < 1e-6, `${id} moved with the group`);
      }
      assert.deepEqual(centre(view(pic, I).bounds), before[I], 'the isometric stays');
      drawing.moveView('S1', I, 330, 200);
      pic = drawing.sheetPicture('S1');
      const ic = centre(view(pic, I).bounds);
      assert.ok(Math.abs(ic[0] - 330) < 1e-6 && Math.abs(ic[1] - 200) < 1e-6, 'isometric centre at (330, 200)');
      // Off the sheet: the layout refuses nothing, but the dimension follows its view.
      assert.ok(Math.abs(dimensionById(drawing, 'S1', dA).value - 80) < 1e-9);

      // Section and detail element views.
      const S = drawing.addView('S1', { kind: 'section', side: 'leftOf', planePoint: [P.hx, 0, 0] });
      const D = drawing.addView('S1', { kind: 'detail', parent: F, detailCenter: [10, 10], detailRadius: 8 });
      desc = drawing.describe();
      const sv = desc.sheets[0].views.find((v) => v.id === S);
      const dv = desc.sheets[0].views.find((v) => v.id === D);
      assert.equal(sv.label, 'А–А');
      assert.match(dv.label, /^Б \(\d+(,\d+)?:\d+\)$/);
      // Detail scale: 2× the sheet where it fits its area (GOST 2.302 series), never below the sheet.
      assert.equal(dv.label, `Б (${dv.computedScale.label})`);
      assert.ok(dv.computedScale.ratio[0] / dv.computedScale.ratio[1] >= scale - 1e-12);
      // Its size is the circle of the detail (diameter 16) times its scale.
      const k = dv.computedScale.ratio[0] / dv.computedScale.ratio[1];
      assert.ok(Math.abs(dv.bounds.maxX - dv.bounds.minX - 16 * k) < 1e-6, 'detail bounds = its circle');
      // Removing the front view is refused (it is the base); removing the section removes only it.
      assert.deepEqual(drawing.removeView('S1', S), [S]);

      // Export: PDF, DXF, SVG.
      const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'bendcore-drawing-'));
      const pdf = drawing.exportPdf();
      assert.ok(pdf instanceof Uint8Array);
      assert.equal(Buffer.from(pdf.slice(0, 5)).toString('latin1'), '%PDF-');
      assert.ok(Buffer.from(pdf.slice(-6)).toString('latin1').includes('%%EOF'));
      fs.writeFileSync(path.join(tmp, 'bracket.pdf'), pdf);
      if (which('pdfinfo')) {
        const info = spawnSync('pdfinfo', [path.join(tmp, 'bracket.pdf')], { encoding: 'utf8' });
        assert.equal(info.status, 0, info.stderr);
        assert.match(info.stdout, /Pages:\s+1/);
        assert.match(info.stdout, /\(A3\)/);
      }
      if (which('pdftotext')) {
        const t = spawnSync('pdftotext', [path.join(tmp, 'bracket.pdf'), '-'], { encoding: 'utf8' });
        assert.equal(t.status, 0, t.stderr);
        for (const s of ['РД.10.001', 'Кронштейн', 'Сталь 20', 'Иванов', '80', 'Острые кромки притупить.']) {
          assert.ok(t.stdout.includes(s), `pdftotext: '${s}'`);
        }
      }
      const dxf = Buffer.from(drawing.exportSheet('S1', 'dxf')).toString('latin1');
      const pairs = dxfPairs(dxf);
      assert.deepEqual(pairs[pairs.length - 1], [0, 'EOF']);
      assert.ok(pairs.some(([c, v]) => c === 1 && v === 'AC1015'), 'AutoCAD R2000');
      assert.deepEqual(dxfHeader(pairs, '$LIMMAX'), [420, 297]);
      assert.deepEqual(dxfHeader(pairs, '$INSUNITS'), [4]);
      const texts = dxfTexts(pairs);
      for (const s of ['80', '50', '⌀10H7', 'R4', '90°', 'РД.10.001', '0,02', '0,05', 'А']) assert.ok(texts.includes(s), `DXF text '${s}'`);
      fs.writeFileSync(path.join(tmp, 'bracket.dxf'), dxf, 'latin1');
      if (process.env.BENDCORE_PYTHON_EZDXF) {
        const script =
          'import sys, ezdxf\ndoc = ezdxf.readfile(sys.argv[1])\na = doc.audit()\n' +
          "t = [ezdxf.decode_dxf_unicode(e.dxf.text) for e in doc.modelspace().query('TEXT')]\n" +
          "print(doc.dxfversion, len(a.errors), doc.header.get('$INSUNITS'), int('80' in t))\n";
        const r = spawnSync(process.env.BENDCORE_PYTHON_EZDXF, ['-c', script, path.join(tmp, 'bracket.dxf')], { encoding: 'utf8' });
        assert.equal(r.status, 0, r.stderr);
        assert.equal(r.stdout.trim(), 'AC1015 0 4 1');
      }
      const svg = Buffer.from(drawing.exportSheet('S1', 'svg')).toString('utf8');
      assert.ok(svg.startsWith('<?xml') && svg.includes('viewBox="0 0 420 297"'));
      fs.rmSync(tmp, { recursive: true, force: true });

      // Save and load: the same definitions and values; the model JSON carries A = 80.
      const saved = drawing.save();
      const savedModel = drawing.saveModel();
      const { drawing: copy, report: loadReport } = kernel.load(saved, savedModel);
      try {
        assert.equal(loadReport.modelChanged, false);
        assert.deepEqual(loadReport.stale, []);
        assert.equal(copy.save(), saved);
        const a = copy.describe().sheets[0];
        const b = drawing.describe().sheets[0];
        assert.deepEqual(a.dimensions.map((x) => [x.id, x.text]), b.dimensions.map((x) => [x.id, x.text]));
        assert.equal(copy.variables().find((v) => v.name === 'A').value, 80);
        // The original model (A = 60) against the saved drawing: one stale dimension per change.
        const { drawing: old, report: oldReport } = kernel.load(saved, partJson);
        try {
          assert.equal(oldReport.modelChanged, true);
          assert.ok(oldReport.stale.includes(`S1/${dA}: 80 → 60`), oldReport.stale.join('; '));
        } finally {
          old.dispose();
        }
      } finally {
        copy.dispose();
      }
    } finally {
      drawing.dispose();
    }
  });

  test('a parameter the part cannot be built with is refused; a moved hole keeps its dimension', () => {
    const drawing = kernel.createFromPart(partJson, { properties: PROPS });
    try {
      const F = drawing.addView('S1', { kind: 'base' });
      const T = drawing.addView('S1', { kind: 'projected', standard: 'top' });
      const hole = drawing.sheetPicture('S1').views.find((v) => v.id === T).edges.find((e) => e.type === 'circle');
      const dd = drawing.addDimension('S1', T, [hole.index]);
      // A hole of zero diameter cannot be built: the variable is restored, the part keeps its state.
      expectKernelError(() => drawing.setVariable('d', 0), 'OperationFailed', 'part does not rebuild');
      assert.equal(drawing.variables().find((v) => v.name === 'd').value, 8);
      assert.equal(drawing.checkModel().modelChanged, false);
      // hx moves the hole: the diameter follows the hole (reference resolved, not dangling).
      drawing.setVariable('hx', 30);
      const rep = drawing.updateFromModel();
      assert.deepEqual(rep.dangling, []);
      assert.equal(dimensionById(drawing, 'S1', dd).value, 8);
      assert.ok(rep.viewsComputed >= 2);
      assert.equal(drawing.describe().sheets[0].views[0].id, F);
    } finally {
      drawing.dispose();
    }
  });

  test('errors: bad arguments, unknown ids, part without specification, disposed drawing', () => {
    const drawing = kernel.createFromPart(partJson, { properties: PROPS, sheet: { format: 'A4', orientation: 'portrait' } });
    const desc = drawing.describe();
    assert.equal(desc.sheets[0].name, 'А4');
    expectKernelError(() => drawing.addView('S1', { kind: 'projected', standard: 'top' }), 'InvalidInput', 'no base view yet');
    const F = drawing.addView('S1', { kind: 'base' });
    expectKernelError(() => drawing.addView('S1', { kind: 'base' }), 'InvalidInput', 'second base view');
    expectKernelError(() => drawing.addView('S9', { kind: 'base' }), 'InvalidInput', 'unknown sheet');
    expectKernelError(() => drawing.addView('S1', { kind: 'nonsense' }), 'InvalidInput', 'unknown kind');
    expectKernelError(() => drawing.setSheet('S1', { scale: [1, 3] }), 'InvalidInput', 'scale not in GOST 2.302');
    expectKernelError(() => drawing.setSheet('S1', { format: 'A4', orientation: 'landscape', multiplier: 2 }), 'InvalidInput', 'A4x2');
    expectKernelError(() => drawing.addDimension('S1', F, [9999]), 'InvalidInput', 'no such edge');
    expectKernelError(() => drawing.addDimension('S1', F, []), 'InvalidInput', 'no picks');
    const front = drawing.sheetPicture('S1').views[0];
    const line = front.edges.find((e) => e.type === 'line' && e.visible && e.edgeIndex !== null);
    expectKernelError(() => drawing.addDimension('S1', F, [line.index]), 'InvalidInput', 'one line is not a dimension');
    expectKernelError(() => drawing.addDimension('S1', F, [line.index, line.index]), 'InvalidInput', 'same edge twice');
    expectKernelError(() => drawing.removeAnnotation('S1', 'nope'), 'InvalidInput', 'unknown annotation');
    expectKernelError(() => drawing.removeSheet('S1'), 'InvalidInput', 'last sheet');
    const S2 = drawing.addSheet({ format: 'A3', orientation: 'landscape' });
    assert.equal(drawing.describe().sheets[1].titleShown.sheet, 2);
    assert.equal(drawing.describe().sheets[0].titleShown.sheets, 2);
    drawing.removeSheet(S2);
    expectKernelError(() => drawing.specification(), 'Unsupported', 'a part has no specification');
    expectKernelError(() => drawing.setVariable('nope', 1), 'InvalidInput', 'unknown variable');
    expectKernelError(() => drawing.exportSheet('S1', 'png'), 'InvalidInput', 'unknown export format');
    expectKernelError(() => kernel.createFromPart('{"format": "bendcore-part"'), 'ParseError', 'broken part JSON');
    expectKernelError(() => kernel.load('{}', partJson), 'ParseError', 'not a drawing');
    drawing.dispose();
    drawing.dispose();
    expectKernelError(() => drawing.describe(), 'Disposed', 'disposed');
  });

  // Positions ГОСТ 2.109 on the drawing of the shaft unit (tests/assembly-fixtures/shaft_unit.mjs,
  // made with the assembly module): two parts and three kinds of standard items — five positions.
  const assemblyDir = argument('assembly-module', path.join(moduleDir, '../wasm_assembly'));
  const haveAssembly = fs.existsSync(path.join(assemblyDir, 'bendcore_assembly.mjs'));
  test(
    'positions: automatic by the specification, manual with a leader, both follow the specification',
    { skip: haveAssembly ? false : `no bendcore_assembly.mjs in ${assemblyDir} (--assembly-module)` },
    async () => {
      const glue = await import(pathToFileURL(path.join(assemblyDir, 'bendcore_assembly.mjs')).href);
      const asmApi = await import(pathToFileURL(path.join(assemblyDir, 'bendcore_assembly_api.mjs')).href);
      const { buildShaftUnit } = await import(pathToFileURL(path.join(repo, 'tests/assembly-fixtures/shaft_unit.mjs')).href);
      const unit = buildShaftUnit(await asmApi.loadAssembly(glue.default));
      const drawing = kernel.createFromAssembly(unit, { path: 'shaft_unit.bcasm.json' });
      try {
        // Specification by construction (ГОСТ 2.106: documentation, parts, standard items).
        const spec = drawing.specification();
        const items = spec.sections.flatMap((s) => s.items.filter((i) => i.position > 0).map((i) => [i.position, i.name, i.quantity]));
        assert.deepEqual(items, [
          [1, 'Втулка', 1],
          [2, 'Вал', 1],
          [3, 'Болт М10×40 ГОСТ 7798-70', 2],
          [4, 'Гайка М10 ГОСТ 5915-70', 2],
          [5, 'Шайба 10 ГОСТ 11371-78', 4],
        ]);
        const V = drawing.addView('S1', { kind: 'base', standard: 'front' });
        drawing.setView('S1', V, { autoBalloons: true });
        let sheet = drawing.describe().sheets[0];
        // Every position of the specification once, the number of its definition.
        assert.deepEqual(sheet.balloons.map((b) => Number(b.text)).sort(), [1, 2, 3, 4, 5]);
        assert.ok(sheet.balloons.every((b) => b.id === null));
        const numberOf = new Map(sheet.balloons.map((b) => [b.definition, b.text]));
        assert.equal(numberOf.size, 5);

        // Manual position on the top line of the shaft (the highest horizontal visible line of the
        // front view: the shaft is 60 tall, the bushing 20, bolt heads below 30).
        const v = view(drawing.sheetPicture('S1'), V);
        const top = extremeLine(v, isHorizontal, 1, Math.max);
        const shelf = [top.points[0][0] + 60, top.points[0][1] + 25];
        const P = drawing.addBalloon('S1', V, top.index, { at: shelf });
        sheet = drawing.describe().sheets[0];
        assert.equal(sheet.positions.length, 1);
        const p = sheet.positions[0];
        assert.equal(p.id, P);
        assert.equal(p.state, 'ok');
        assert.equal(p.text, '2', 'the shaft is position 2');
        assert.equal(p.text, numberOf.get(p.definition), 'the same number as the automatic position of that part');
        assert.deepEqual(p.shelf, shelf);
        // The leader point is on the picked line.
        assert.ok(Math.abs(p.target[1] - top.points[0][1]) < 1e-6);
        const [x0, x1] = [Math.min(top.points[0][0], top.points[1][0]), Math.max(top.points[0][0], top.points[1][0])];
        assert.ok(p.target[0] >= x0 - 1e-6 && p.target[0] <= x1 + 1e-6);
        // The automatic positions leave out the part that has a manual one.
        assert.deepEqual(sheet.balloons.filter((b) => b.id === null).map((b) => Number(b.text)).sort(), [1, 3, 4, 5]);
        assert.deepEqual(sheet.balloons.filter((b) => b.id === P).map((b) => b.text), ['2']);
        // The sheet shows the number on the shelf (the kernel's SVG).
        assert.ok(drawing.exportSheet('S1', 'svg').length > 0);

        // Shelf moved, then saved and loaded: the position and its number stay.
        drawing.setBalloon('S1', P, { at: [shelf[0] + 10, shelf[1]] });
        assert.deepEqual(drawing.describe().sheets[0].positions[0].shelf, [shelf[0] + 10, shelf[1]]);
        drawing.setBalloon('S1', P, { at: null });
        const def = drawing.describe().sheets[0].positions[0];
        assert.equal(def.at, null);
        assert.deepEqual(def.shelf, [def.target[0] + 12, def.target[1] + 8]);
        const loaded = kernel.load(drawing.save(), drawing.saveModel());
        try {
          const lp = loaded.drawing.describe().sheets[0].positions;
          assert.deepEqual(lp.map((x) => [x.id, x.text, x.state]), [[P, '2', 'ok']]);
        } finally {
          loaded.drawing.dispose();
        }

        // Errors: silhouette lines are not edges of a part; unknown ids.
        const silhouette = v.edges.find((e) => e.edgeIndex === null);
        if (silhouette) expectKernelError(() => drawing.addBalloon('S1', V, silhouette.index), 'InvalidInput', 'silhouette');
        expectKernelError(() => drawing.addBalloon('S1', V, 99999), 'InvalidInput', 'no such edge');
        expectKernelError(() => drawing.setBalloon('S1', 'nope', { at: null }), 'InvalidInput', 'unknown position');

        // Removed: the automatic position of the shaft comes back.
        drawing.removeAnnotation('S1', P);
        sheet = drawing.describe().sheets[0];
        assert.equal(sheet.positions.length, 0);
        assert.deepEqual(sheet.balloons.map((b) => Number(b.text)).sort(), [1, 2, 3, 4, 5]);
      } finally {
        drawing.dispose();
      }

      // A part drawing has no specification: no positions.
      const part = kernel.createFromPart(partJson, { properties: PROPS });
      try {
        const F = part.addView('S1', { kind: 'base' });
        const line = part.sheetPicture('S1').views[0].edges.find((e) => e.visible && e.edgeIndex !== null);
        expectKernelError(() => part.addBalloon('S1', F, line.index), 'InvalidInput', 'positions need an assembly');
      } finally {
        part.dispose();
      }
    },
  );
});

// Local section (ГОСТ 2.305), undo/redo of the drawing edits, chain and baseline dimensions,
// dragging of a dimension (docs/ui/DRAWING_UI.md). Geometry of the bracket: the hole Ø8 at x = 40
// goes through the plate z ∈ [0, 8]; in the front view its walls x = 36 and 44 are hidden lines.
describe('local section, undo/redo, chain and baseline, drag', () => {
  const toSheet = (v, p) => [v.anchor[0] + v.factor * p[0], v.anchor[1] + v.factor * p[1]];
  // Length (model mm) of visible / hidden vertical edges of the view at view x = x0.
  const verticalAt = (v, x0, visible) => {
    const xs = v.anchor[0] + v.factor * x0;
    let len = 0;
    for (const e of v.edges) {
      if (e.visible !== visible || !isVertical(e) || Math.abs(e.points[0][0] - xs) > 1e-4) continue;
      len += Math.abs(e.points[1][1] - e.points[0][1]) / v.factor;
    }
    return len;
  };

  test('local section around the hole; undo and redo restore the document', () => {
    const drawing = kernel.createFromPart(partJson, { properties: PROPS });
    try {
      const F = drawing.addView('S1', { kind: 'base', standard: 'front' });
      drawing.addView('S1', { kind: 'projected', standard: 'top' });
      let v = view(drawing.sheetPicture('S1'), F);
      assert.ok(verticalAt(v, 36, false) > 7.9, 'hole wall hidden before the section');
      assert.equal(verticalAt(v, 36, true), 0);
      const before = drawing.save();
      const h0 = drawing.describe().history;
      assert.equal(h0.canUndo, true);
      assert.equal(h0.undoDepth, 2);

      // Region x ∈ [33, 47], z ∈ [1, 7]; depth 15 = half the width Wd: the plane y = 0 through the axis.
      const region = [[33, 1], [47, 1], [47, 7], [33, 7]].map((p) => toSheet(v, p));
      const r = drawing.addLocalSection('S1', F, { points: region, depth: P.Wd / 2 });
      assert.equal(r.index, 0);
      v = view(drawing.sheetPicture('S1'), F);
      // Inside the region the walls of the hole are visible (behind the plane), outside — hidden.
      assert.ok(Math.abs(verticalAt(v, 36, true) - 6) < 1e-3, `visible wall ${verticalAt(v, 36, true)}`);
      assert.ok(Math.abs(verticalAt(v, 44, true) - 6) < 1e-3);
      const desc = drawing.describe();
      const ls = desc.sheets[0].views.find((x) => x.id === F).localSections;
      assert.equal(ls.length, 1);
      assert.ok(Math.abs(ls[0].points[0][0] - 33) < 1e-9 && Math.abs(ls[0].points[0][1] - 1) < 1e-9, 'view coordinates');
      assert.equal(ls[0].depth, 15);
      assert.equal(desc.history.undoLabel, 'addLocalSection');
      // DXF: hatching of the section figure.
      const pairs = dxfPairs(new TextDecoder().decode(drawing.exportSheet('S1', 'dxf')));
      assert.ok(pairs.some(([c, val]) => c === 8 && val === 'HATCH'), 'hatch lines');
      const after = drawing.save();
      assert.notEqual(after, before);

      // Undo: the document is the one before the edit; redo: after it.
      const u = drawing.undo();
      assert.equal(u.label, 'addLocalSection');
      assert.equal(drawing.save(), before);
      assert.equal(verticalAt(view(drawing.sheetPicture('S1'), F), 36, true), 0);
      assert.equal(drawing.describe().history.canRedo, true);
      drawing.redo();
      assert.equal(drawing.save(), after);
      // Three undos: back to the empty sheet; nothing more to undo.
      drawing.undo();
      drawing.undo();
      drawing.undo();
      assert.equal(drawing.describe().sheets[0].views.length, 0);
      expectKernelError(() => drawing.undo(), 'InvalidInput', 'nothing to undo');
      drawing.redo();
      drawing.redo();
      drawing.redo();
      assert.equal(drawing.save(), after);
      // A failed edit is not recorded; a new edit clears redo.
      const depth = drawing.describe().history.undoDepth;
      expectKernelError(() => drawing.addLocalSection('S1', F, { points: region, depth: 0 }), 'InvalidInput', 'depth 0');
      expectKernelError(
        () => drawing.addLocalSection('S1', F, { points: [region[0], region[2], region[1], region[3]], depth: 5 }),
        'InvalidInput',
        'self-intersecting outline',
      );
      assert.equal(drawing.describe().history.undoDepth, depth);
      drawing.undo();
      assert.equal(drawing.describe().history.canRedo, true);
      drawing.setSheet('S1', { technicalRequirements: ['Острые кромки притупить.'] });
      assert.equal(drawing.describe().history.canRedo, false);
      expectKernelError(() => drawing.removeLocalSection('S1', F, 0), 'InvalidInput', 'no local section after undo');
    } finally {
      drawing.dispose();
    }
  });

  test('chain and baseline dimensions; a dragged dimension follows the pointer', () => {
    const drawing = kernel.createFromPart(partJson, { properties: PROPS });
    try {
      const F = drawing.addView('S1', { kind: 'base', standard: 'front' });
      const v = view(drawing.sheetPicture('S1'), F);
      // Vertical lines x = 0 (back of the wall), x = T (inner face of the wall), x = A (end of the plate).
      const left = extremeLine(v, isVertical, 0, Math.min);
      const right = extremeLine(v, isVertical, 0, Math.max);
      const xT = v.anchor[0] + v.factor * P.T;
      const inner = v.edges.find((e) => e.visible && e.edgeIndex !== null && isVertical(e) && Math.abs(e.points[0][0] - xT) < 1e-6);
      assert.ok(inner, 'inner wall line');
      const chain = drawing.addDimensionGroup('S1', F, [left.index, inner.index, right.index], { mode: 'chain' });
      assert.equal(chain.length, 2);
      const values = (ids) => ids.map((id) => dimensionById(drawing, 'S1', id).value).sort((a, b) => a - b);
      assert.deepEqual(values(chain).map((x) => Math.round(x * 1e6) / 1e6), [P.T, P.A - P.T]);
      // Chain: one dimension line (same label height).
      const y0 = chain.map((id) => dimensionById(drawing, 'S1', id).labelPosition[1]);
      assert.ok(Math.abs(y0[0] - y0[1]) < 1e-6, 'chain on one line');
      const base = drawing.addDimensionGroup('S1', F, [left.index, inner.index, right.index], { mode: 'baseline', spacing: 8 });
      assert.deepEqual(values(base).map((x) => Math.round(x * 1e6) / 1e6), [P.T, P.A]);
      const yb = base.map((id) => dimensionById(drawing, 'S1', id).labelPosition[1]);
      assert.ok(Math.abs(Math.abs(yb[0] - yb[1]) - 8) < 1e-6, `baseline spacing ${yb}`);
      expectKernelError(() => drawing.addDimensionGroup('S1', F, [left.index], { mode: 'chain' }), 'InvalidInput', 'one pick');

      // Drag: the text lands at the pointer; the value does not change; undo puts it back.
      const id = chain[0];
      const d0 = dimensionById(drawing, 'S1', id);
      const target = [d0.labelPosition[0] + 6, d0.labelPosition[1] + 9];
      drawing.moveDimension('S1', id, target[0], target[1]);
      const d1 = dimensionById(drawing, 'S1', id);
      assert.ok(Math.hypot(d1.labelPosition[0] - target[0], d1.labelPosition[1] - target[1]) < 1e-6, `label ${d1.labelPosition}`);
      assert.equal(d1.value, d0.value);
      assert.equal(drawing.describe().history.undoLabel, 'moveDimension');
      drawing.undo();
      const d2 = dimensionById(drawing, 'S1', id);
      assert.ok(Math.hypot(d2.labelPosition[0] - d0.labelPosition[0], d2.labelPosition[1] - d0.labelPosition[1]) < 1e-9);
      expectKernelError(() => drawing.moveDimension('S1', 'nope', 1, 1), 'InvalidInput', 'unknown dimension');
    } finally {
      drawing.dispose();
    }
  });
});
