// Node check of data exchange in the WebAssembly modules (docs/ui/DATA_EXCHANGE.md): STEP and
// IGES export -> import round trips, STL and 3MF files read back by readers written here (and,
// for 3MF, by node's zlib CRC-32 and Python's zipfile/xml.dom when python3 is present), mass
// properties against formulas, IGES import of the corpus.
//
//   node tests/wasm/exchange.test.mjs --kernel <dir> --modeling <dir> --assembly <dir> [--corpus <dir>]
//
// Expected values come from the dimensions (formulas below) and from
// tests/corpus/generated/manifest.json (computed by the generator from the dimensions).
// Registered in ctest as wasm_exchange (cmake/wasm_kernel.cmake).

import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { after, before, describe, test } from 'node:test';
import { fileURLToPath, pathToFileURL } from 'node:url';
import zlib from 'node:zlib';

const here = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(here, '../..');

function argument(name, fallback) {
  const i = process.argv.indexOf(`--${name}`);
  return i >= 0 && i + 1 < process.argv.length ? path.resolve(process.argv[i + 1]) : fallback;
}

const kernelDir = argument('kernel', path.join(repo, 'build-wasm-all/wasm_kernel'));
const modelingDir = argument('modeling', path.join(repo, 'build-wasm-all/wasm_modeling'));
const assemblyDir = argument('assembly', path.join(repo, 'build-wasm-all/wasm_assembly'));
const corpusDir = argument('corpus', path.join(repo, 'tests/corpus/generated'));
const manifest = JSON.parse(readFileSync(path.join(corpusDir, 'manifest.json'), 'utf8'));
const { PI } = Math;

function relClose(actual, expected, rel, what) {
  const err = Math.abs(actual - expected) / Math.abs(expected);
  assert.ok(err <= rel, `${what}: ${actual} vs ${expected} (relative error ${err.toExponential(2)} > ${rel})`);
}

// ------------------------------------------------------------------ STL reader (independent)

function readStl(bytes) {
  const buf = Buffer.from(bytes);
  const binaryCount = buf.length >= 84 ? buf.readUInt32LE(80) : -1;
  const triangles = [];
  if (binaryCount >= 0 && 84 + binaryCount * 50 === buf.length) {
    for (let i = 0; i < binaryCount; ++i) {
      const o = 84 + i * 50 + 12;
      const t = [];
      for (let v = 0; v < 3; ++v) t.push([0, 1, 2].map((c) => buf.readFloatLE(o + v * 12 + c * 4)));
      triangles.push(t);
    }
    return { binary: true, triangles };
  }
  const text = buf.toString('latin1');
  assert.match(text, /^solid /, 'ASCII STL starts with "solid"');
  assert.match(text.trimEnd(), /endsolid[^\n]*$/, 'ASCII STL ends with "endsolid"');
  const re = /outer loop\s+vertex\s+(\S+)\s+(\S+)\s+(\S+)\s+vertex\s+(\S+)\s+(\S+)\s+(\S+)\s+vertex\s+(\S+)\s+(\S+)\s+(\S+)\s+endloop/g;
  let m;
  while ((m = re.exec(text)) !== null) {
    const n = m.slice(1).map(Number);
    triangles.push([n.slice(0, 3), n.slice(3, 6), n.slice(6, 9)]);
  }
  assert.equal(triangles.length, (text.match(/facet normal/g) ?? []).length, 'every facet parsed');
  return { binary: false, triangles };
}

function soupVolume(triangles) {
  let v = 0;
  for (const [a, b, c] of triangles) {
    v += a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0]);
  }
  return v / 6;
}

// Closed and consistently oriented: every directed edge appears once and its reverse once.
function closureOf(triangles, keyOf) {
  const directed = new Map();
  for (const t of triangles) {
    for (let i = 0; i < 3; ++i) {
      const k = `${keyOf(t[i])}>${keyOf(t[(i + 1) % 3])}`;
      directed.set(k, (directed.get(k) ?? 0) + 1);
    }
  }
  let bad = 0;
  for (const [k, n] of directed) {
    const [a, b] = k.split('>');
    if (n !== 1 || directed.get(`${b}>${a}`) !== 1) ++bad;
  }
  return bad;
}

const floatKey = (p) => p.map((x) => Math.fround(x)).join(',');

// ------------------------------------------------------------------ ZIP reader (independent)

function readZip(bytes) {
  const buf = Buffer.from(bytes);
  let eocd = -1;
  for (let i = buf.length - 22; i >= Math.max(0, buf.length - 22 - 0xffff); --i) {
    if (buf.readUInt32LE(i) === 0x06054b50) {
      eocd = i;
      break;
    }
  }
  assert.ok(eocd >= 0, 'end of central directory');
  const count = buf.readUInt16LE(eocd + 10);
  let cd = buf.readUInt32LE(eocd + 16);
  const entries = [];
  for (let i = 0; i < count; ++i) {
    assert.equal(buf.readUInt32LE(cd), 0x02014b50, 'central directory signature');
    const method = buf.readUInt16LE(cd + 10);
    const crc = buf.readUInt32LE(cd + 16);
    const csize = buf.readUInt32LE(cd + 20);
    const usize = buf.readUInt32LE(cd + 24);
    const nameLen = buf.readUInt16LE(cd + 28);
    const extra = buf.readUInt16LE(cd + 30);
    const comment = buf.readUInt16LE(cd + 32);
    const local = buf.readUInt32LE(cd + 42);
    const name = buf.subarray(cd + 46, cd + 46 + nameLen).toString('utf8');
    assert.equal(buf.readUInt32LE(local), 0x04034b50, `local header of ${name}`);
    assert.equal(buf.readUInt32LE(local + 14), crc, `local and central CRC of ${name}`);
    const at = local + 30 + buf.readUInt16LE(local + 26) + buf.readUInt16LE(local + 28);
    const packed = buf.subarray(at, at + csize);
    const data = method === 0 ? packed : zlib.inflateRawSync(packed);
    assert.ok(method === 0 || method === 8, `method of ${name}`);
    assert.equal(data.length, usize, `size of ${name}`);
    assert.equal(zlib.crc32(data) >>> 0, crc, `CRC-32 of ${name} (node zlib)`);
    entries.push({ name, method, data });
    cd += 46 + nameLen + extra + comment;
  }
  return entries;
}

// ------------------------------------------------------------------ XML (well-formedness)

// Strict enough for the 3MF parts: declaration, elements, attributes in double quotes,
// predefined entities; throws on unbalanced tags, bad attribute syntax or stray '<' / '&'.
function parseXml(text) {
  const root = { name: '#document', attrs: {}, children: [] };
  const stack = [root];
  let i = 0;
  const decl = text.match(/^<\?xml[^?]*\?>\s*/);
  if (decl) i = decl[0].length;
  const entity = /&(amp|lt|gt|quot|apos|#[0-9]+|#x[0-9a-fA-F]+);/y;
  while (i < text.length) {
    if (text[i] === '<') {
      const close = text.indexOf('>', i);
      if (close < 0) throw new Error(`unterminated tag at ${i}`);
      const tag = text.slice(i + 1, close);
      if (tag.startsWith('/')) {
        const name = tag.slice(1).trim();
        const top = stack.pop();
        if (!top || top.name !== name) throw new Error(`</${name}> closes <${top?.name}> at ${i}`);
      } else {
        const selfClosing = tag.endsWith('/');
        const body = selfClosing ? tag.slice(0, -1) : tag;
        const m = body.match(/^([A-Za-z_][\w:.-]*)((?:\s+[A-Za-z_][\w:.-]*="[^"<]*")*)\s*$/);
        if (!m) throw new Error(`malformed tag <${tag}> at ${i}`);
        const attrs = {};
        for (const a of m[2].matchAll(/([A-Za-z_][\w:.-]*)="([^"]*)"/g)) {
          if (a[1] in attrs) throw new Error(`duplicate attribute ${a[1]}`);
          attrs[a[1]] = a[2].replace(/&quot;/g, '"').replace(/&apos;/g, "'").replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&amp;/g, '&');
        }
        const el = { name: m[1], attrs, children: [] };
        stack[stack.length - 1].children.push(el);
        if (!selfClosing) stack.push(el);
      }
      i = close + 1;
    } else {
      if (text[i] === '&') {
        entity.lastIndex = i;
        if (!entity.test(text)) throw new Error(`bad entity at ${i}`);
      }
      if (text[i] === '>') throw new Error(`stray '>' at ${i}`);
      ++i;
    }
  }
  if (stack.length !== 1) throw new Error(`unclosed <${stack[stack.length - 1].name}>`);
  if (root.children.length !== 1) throw new Error('XML must have exactly one root element');
  return root.children[0];
}

const childrenNamed = (el, name) => el.children.filter((c) => c.name === name);
const child = (el, name) => {
  const c = childrenNamed(el, name);
  assert.equal(c.length, 1, `<${el.name}> has one <${name}>`);
  return c[0];
};

// Python's zipfile (CRC check) and xml.dom.minidom as readers independent of this file.
function pythonCheck(bytes) {
  const probe = spawnSync('python3', ['--version']);
  if (probe.status !== 0) return null;
  const dir = mkdtempSync(path.join(os.tmpdir(), 'bendcore-3mf-'));
  try {
    const file = path.join(dir, 'model.3mf');
    writeFileSync(file, bytes);
    const script =
      'import sys, zipfile, xml.dom.minidom as d\n' +
      'z = zipfile.ZipFile(sys.argv[1])\n' +
      'assert z.testzip() is None\n' +
      'm = d.parseString(z.read("3D/3dmodel.model"))\n' +
      'print(len(m.getElementsByTagName("object")), len(m.getElementsByTagName("triangle")))\n';
    const r = spawnSync('python3', ['-I', '-c', script, file], { encoding: 'utf8' });
    assert.equal(r.status, 0, `python zipfile/minidom: ${r.stderr}`);
    const [objects, triangles] = r.stdout.trim().split(/\s+/).map(Number);
    return { objects, triangles };
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

// ------------------------------------------------------------------ modules

let kernel;
let modeling;
let assembly;

before(async () => {
  const kGlue = await import(pathToFileURL(path.join(kernelDir, 'bendcore_kernel.mjs')).href);
  const kApi = await import(pathToFileURL(path.join(kernelDir, 'bendcore_kernel_api.mjs')).href);
  kernel = await kApi.loadKernel(kGlue.default);
  const mGlue = await import(pathToFileURL(path.join(modelingDir, 'bendcore_modeling.mjs')).href);
  const mApi = await import(pathToFileURL(path.join(modelingDir, 'bendcore_modeling_api.mjs')).href);
  modeling = await mApi.loadModeling(mGlue.default);
  const aGlue = await import(pathToFileURL(path.join(assemblyDir, 'bendcore_assembly.mjs')).href);
  const aApi = await import(pathToFileURL(path.join(assemblyDir, 'bendcore_assembly_api.mjs')).href);
  assembly = await aApi.loadAssembly(aGlue.default);
});

after(() => {
  assert.equal(kernel.info().liveShapes, 0, 'every kernel shape is disposed');
  assert.equal(modeling.info().liveDocuments, 0, 'every part document is disposed');
  assert.equal(assembly.info().liveDocuments, 0, 'every assembly is disposed');
});

function withPart(features, body) {
  const doc = modeling.createDocument();
  try {
    for (const f of features) doc.addFeature(f);
    doc.rebuild();
    for (const f of doc.features()) assert.equal(f.state, 'ok', `${f.id}: ${f.messages.join('; ')}`);
    return body(doc);
  } finally {
    doc.dispose();
  }
}

function withImported(bytes, format, body) {
  const { shape, structure } = kernel.importModel(bytes, format);
  try {
    return body(shape, structure);
  } finally {
    shape.dispose();
  }
}

// Box 100x60x20 and a plate 120x80x10 with a Ø20 through hole (box minus cylinder).
const BOX = [{ type: 'Box', id: 'Box', parameters: { length: '100', width: '60', height: '20' } }];
const CYLINDER = [{ type: 'Cylinder', id: 'Cyl', parameters: { radius: '20', height: '50' } }];
const BOX_V = 100 * 60 * 20;
const CYL_V = PI * 20 * 20 * 50;
const CYL_S = 2 * PI * 20 * 20 + 2 * PI * 20 * 50;

describe('part export -> import round trip', () => {
  for (const schema of ['AP214', 'AP242']) {
    test(`STEP ${schema}: volume and faces`, () => {
      withPart(BOX, (doc) => {
        const bytes = doc.exportFile('step', { schema, name: 'Brick' });
        const text = Buffer.from(bytes).toString('latin1');
        assert.match(text, /^ISO-10303-21;/);
        assert.match(text, schema === 'AP214' ? /AUTOMOTIVE_DESIGN/ : /AP242_MANAGED_MODEL_BASED_3D_ENGINEERING/);
        assert.match(text, /PRODUCT\('Brick'/);
        withImported(bytes, 'step', (shape, structure) => {
          relClose(shape.volumeProperties().volume, BOX_V, 1e-9, 'volume');
          assert.equal(shape.counts().faces, 6);
          assert.equal(structure.assembly, false);
          assert.equal(structure.name, 'Brick');
        });
      });
    });
  }

  for (const igesMode of ['brep', 'faces']) {
    test(`IGES (${igesMode}): volume and faces`, () => {
      withPart(CYLINDER, (doc) => {
        const bytes = doc.exportFile('iges', { igesMode, name: 'Roller' });
        const text = Buffer.from(bytes).toString('latin1');
        assert.equal(text.split(/\r?\n/)[0].length, 80, 'fixed 80-column records');
        withImported(bytes, 'iges', (shape, structure) => {
          relClose(shape.volumeProperties().volume, CYL_V, 1e-5, 'volume');
          assert.equal(shape.counts().solids, 1);
          assert.equal(shape.counts().faces, 3);
          assert.equal(structure.assembly, false);
          if (igesMode === 'faces') assert.match(structure.warnings.join('\n'), /sewn into 1 solid/);
        });
      });
    });
  }

  test('STEP in metres and inches is read back in millimetres', () => {
    withPart(BOX, (doc) => {
      for (const [unit, mm] of [
        ['m', 1000],
        ['inch', 25.4],
      ]) {
        withImported(doc.exportFile('step', { unit }), 'step', (shape, structure) => {
          assert.equal(structure.fileUnitMm, mm);
          relClose(shape.volumeProperties().volume, BOX_V, 1e-9, `volume (${unit})`);
        });
      }
    });
  });
});

describe('STL', () => {
  const deflection = 0.05;
  for (const stlFormat of ['binary', 'ascii']) {
    test(`${stlFormat}: closed mesh, volume within the deflection bound`, () => {
      withPart(CYLINDER, (doc) => {
        const bytes = doc.exportFile('stl', { stlFormat, linearDeflection: deflection, name: 'Roller' });
        const stl = readStl(bytes);
        assert.equal(stl.binary, stlFormat === 'binary');
        assert.ok(stl.triangles.length > 100, `${stl.triangles.length} triangles`);
        const v = soupVolume(stl.triangles);
        // Inscribed mesh of a convex solid: 0 <= V - V_mesh <= S * deflection.
        assert.ok(v <= CYL_V * (1 + 1e-6), `mesh volume ${v} above ${CYL_V}`);
        assert.ok(CYL_V - v <= CYL_S * deflection, `mesh volume ${v} too small`);
        assert.equal(closureOf(stl.triangles, floatKey), 0, 'closed, consistently oriented');
      });
    });
  }

  test('finer deflection gives a closer volume', () => {
    withPart(CYLINDER, (doc) => {
      const coarse = soupVolume(readStl(doc.exportFile('stl', { linearDeflection: 0.5 })).triangles);
      const fine = soupVolume(readStl(doc.exportFile('stl', { linearDeflection: 0.01 })).triangles);
      assert.ok(CYL_V - fine < CYL_V - coarse, `${fine} closer to ${CYL_V} than ${coarse}`);
    });
  });
});

describe('3MF', () => {
  test('valid ZIP (CRC), XML parses, closed meshes in millimetres', () => {
    withPart(CYLINDER, (doc) => {
      const bytes = doc.exportFile('3mf', { name: 'Roller', linearDeflection: 0.05 });
      const entries = readZip(bytes);
      assert.deepEqual(entries.map((e) => e.name).sort(), ['3D/3dmodel.model', '[Content_Types].xml', '_rels/.rels']);
      const method = kernel.exchangeInfo().zipMethod === 'stored' ? 0 : 8;
      for (const e of entries) assert.equal(e.method, method, `method of ${e.name}`);
      const types = parseXml(entries.find((e) => e.name === '[Content_Types].xml').data.toString('utf8'));
      assert.equal(types.name, 'Types');
      const rels = parseXml(entries.find((e) => e.name === '_rels/.rels').data.toString('utf8'));
      assert.equal(child(rels, 'Relationship').attrs.Target, '/3D/3dmodel.model');
      const model = parseXml(entries.find((e) => e.name === '3D/3dmodel.model').data.toString('utf8'));
      assert.equal(model.attrs.unit, 'millimeter');
      const objects = childrenNamed(child(model, 'resources'), 'object');
      assert.equal(objects.length, 1);
      assert.equal(objects[0].attrs.name, 'Roller');
      const mesh = child(objects[0], 'mesh');
      const vertices = childrenNamed(child(mesh, 'vertices'), 'vertex').map((v) => [+v.attrs.x, +v.attrs.y, +v.attrs.z]);
      const triangles = childrenNamed(child(mesh, 'triangles'), 'triangle').map((t) => [
        vertices[+t.attrs.v1],
        vertices[+t.attrs.v2],
        vertices[+t.attrs.v3],
      ]);
      const indexKey = (p) => vertices.indexOf(p);
      assert.equal(closureOf(triangles, indexKey), 0, 'closed indexed mesh');
      const v = soupVolume(triangles);
      assert.ok(v <= CYL_V * (1 + 1e-9) && CYL_V - v <= CYL_S * 0.05, `3MF mesh volume ${v}`);
      const py = pythonCheck(bytes);
      if (py) {
        assert.equal(py.objects, 1);
        assert.equal(py.triangles, triangles.length);
      }
    });
  });

  test('a corrupted byte fails the CRC check', () => {
    withPart(BOX, (doc) => {
      const bytes = Buffer.from(doc.exportFile('3mf'));
      const at = bytes.indexOf('<model');
      assert.ok(at > 0, 'stored model XML is visible in the archive');
      bytes[at + 1] ^= 0x01;
      assert.throws(() => readZip(bytes), /CRC-32/);
    });
  });
});

describe('mass properties', () => {
  test('box 100x60x20 of steel: mass, centre, inertia against formulas', () => {
    withPart(BOX, (doc) => {
      const rho = 7850;
      const p = doc.massProperties(rho);
      const m = BOX_V * 1e-9 * rho;
      relClose(p.mass, m, 1e-12, 'mass');
      relClose(p.volume, BOX_V, 1e-12, 'volume');
      relClose(p.area, 2 * (100 * 60 + 60 * 20 + 20 * 100), 1e-12, 'area');
      for (const [i, c] of [50, 30, 10].entries()) assert.ok(Math.abs(p.centerOfMass[i] - c) < 1e-9, `centre ${i}`);
      relClose(p.inertiaAtCenter[0][0], (m * (60 * 60 + 20 * 20)) / 12, 1e-9, 'Ixx');
      relClose(p.inertiaAtCenter[1][1], (m * (100 * 100 + 20 * 20)) / 12, 1e-9, 'Iyy');
      relClose(p.inertiaAtCenter[2][2], (m * (100 * 100 + 60 * 60)) / 12, 1e-9, 'Izz');
      assert.ok(Math.abs(p.inertiaAtCenter[0][1]) < 1e-9 * m * 1e4, 'Ixy = 0');
      relClose(p.principalMoments[0], (m * (60 * 60 + 20 * 20)) / 12, 1e-9, 'smallest principal moment');
    });
  });

  test('imported shape: the same through the kernel module', () => {
    withImported(new Uint8Array(readFileSync(path.join(corpusDir, 'box_100x60x20.igs'))), 'iges', (shape) => {
      relClose(shape.massProperties(2700).mass, BOX_V * 1e-9 * 2700, 1e-9, 'mass');
    });
  });

  test('density out of range is InvalidInput', () => {
    withPart(BOX, (doc) => {
      assert.throws(() => doc.massProperties(0), (e) => e.code === 'InvalidInput');
      assert.throws(() => doc.massProperties(Number.NaN), (e) => e.code === 'InvalidInput');
    });
  });
});

describe('IGES corpus', () => {
  for (const entry of manifest.iges) {
    test(entry.file, () => {
      const model = manifest.models.find((m) => m.name === entry.model);
      withImported(new Uint8Array(readFileSync(path.join(corpusDir, entry.file))), 'iges', (shape, structure) => {
        relClose(shape.volumeProperties().volume, model.volume, model.volume_rel_tol * 10, 'volume');
        assert.equal(shape.counts().faces, model.faces);
        assert.equal(shape.counts().solids, model.solids);
        assert.equal(structure.assembly, Boolean(model.components));
        if (model.components) {
          const instances = model.components.reduce((n, c) => n + c.instances, 0);
          assert.equal(structure.parts, instances);
        }
      });
    });
  }

  test('not IGES is ParseError, unknown format InvalidInput', () => {
    assert.throws(() => kernel.importModel(new TextEncoder().encode('hello'), 'iges'), (e) => e.code === 'ParseError');
    assert.throws(() => kernel.importModel(new Uint8Array(4), 'dxf'), (e) => e.code === 'InvalidInput');
  });
});

describe('assembly exchange', () => {
  const pins = path.join(corpusDir, 'assembly_bracket_pins.igs');
  const model = manifest.models.find((m) => m.name === 'assembly_bracket_pins_ap214');

  function withAssembly(body) {
    const doc = assembly.importIges(readFileSync(pins, 'latin1'));
    try {
      return body(doc);
    } finally {
      doc.dispose();
    }
  }

  test('IGES import: occurrences, names, colours', () => {
    withAssembly((doc) => {
      const parts = doc.placements();
      assert.equal(parts.length, 4);
      const names = doc.state().definitions.map((d) => d.name).sort();
      for (const n of ['BASE_PLATE', 'PIN', 'SPACER']) assert.ok(names.some((x) => x.startsWith(n)), `${n} in ${names}`);
    });
  });

  test('3MF: one object per occurrence with its colour; volume of every mesh', () => {
    withAssembly((doc) => {
      const entries = readZip(doc.exportFile('3mf', { linearDeflection: 0.02 }));
      const xml = parseXml(entries.find((e) => e.name === '3D/3dmodel.model').data.toString('utf8'));
      const resources = child(xml, 'resources');
      const objects = childrenNamed(resources, 'object');
      assert.equal(objects.length, 4);
      assert.equal(childrenNamed(child(xml, 'build'), 'item').length, 4);
      const colours = childrenNamed(child(resources, 'basematerials'), 'base').map((b) => b.attrs.displaycolor).sort();
      assert.deepEqual([...new Set(colours)].sort(), ['#0000FFFF', '#00FF00FF', '#FF0000FF']);
      let total = 0;
      for (const o of objects) {
        const mesh = child(o, 'mesh');
        const vs = childrenNamed(child(mesh, 'vertices'), 'vertex').map((v) => [+v.attrs.x, +v.attrs.y, +v.attrs.z]);
        const ts = childrenNamed(child(mesh, 'triangles'), 'triangle').map((t) => [vs[+t.attrs.v1], vs[+t.attrs.v2], vs[+t.attrs.v3]]);
        assert.equal(closureOf(ts, (p) => vs.indexOf(p)), 0, `${o.attrs.name} closed`);
        total += soupVolume(ts);
      }
      assert.ok(total <= model.volume * (1 + 1e-9) && model.volume - total <= model.area * 0.02, `3MF volume ${total}`);
    });
  });

  test('STEP and IGES export -> import keep the total volume', () => {
    withAssembly((doc) => {
      for (const format of ['step', 'iges']) {
        withImported(doc.exportFile(format), format, (shape, structure) => {
          relClose(shape.volumeProperties().volume, model.volume, 1e-6, `${format} volume`);
          assert.equal(shape.counts().faces, model.faces);
          assert.equal(structure.parts, 4);
        });
      }
    });
  });

  test('mass properties: densities per part, Steiner sum', () => {
    withAssembly((doc) => {
      const none = doc.massProperties();
      assert.equal(none.mass, null);
      assert.equal(none.partsWithoutDensity.length, 4);
      relClose(none.volume, model.volume, 1e-9, 'volume without densities');
      for (const d of doc.state().definitions) if (d.kind === 'part') doc.setProperties(d.id, { density: 7850 });
      const all = doc.massProperties();
      assert.deepEqual(all.partsWithoutDensity, []);
      relClose(all.mass.mass, model.volume * 1e-9 * 7850, 1e-9, 'mass');
      // Centre of mass from the construction (generate_corpus make_assembly): base 80x40x10 at
      // the origin, pins r4 h30 at (20,20,10) and (60,20,10), spacer R6/r3 h15 at (40,20,10).
      const vb = 80 * 40 * 10;
      const vp = PI * 16 * 30;
      const vs = PI * (36 - 9) * 15;
      const z = (vb * 5 + 2 * vp * (10 + 15) + vs * (10 + 7.5)) / (vb + 2 * vp + vs);
      assert.ok(Math.abs(all.mass.centerOfMass[0] - 40) < 1e-6, 'x of the centre');
      assert.ok(Math.abs(all.mass.centerOfMass[1] - 20) < 1e-6, 'y of the centre');
      assert.ok(Math.abs(all.mass.centerOfMass[2] - z) < 1e-6, `z of the centre ${all.mass.centerOfMass[2]} vs ${z}`);
    });
  });
});
