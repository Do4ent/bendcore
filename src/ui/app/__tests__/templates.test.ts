// Document templates of the application shell (docs/ui/APP_SHELL.md, "Шаблоны"): the built-in
// templates, the template file (reading with checks, writing), settings taken from documents,
// storage in IndexedDB (a small in-memory stand-in of the database) and the kernel calls that apply
// the settings (the module loaders are replaced by recording stand-ins).

import { en, ru } from '../i18n';
import type { MessageKey } from '../i18n';
import { AppStore } from '../storage';
import {
  BUILTIN_TEMPLATES,
  assemblyMaterialFromState,
  componentsFromParts,
  defaultComponentProperties,
  drawingSettingsFromDescription,
  newTemplateId,
  parseTemplateFile,
  partSettingsFromFile,
  serializeTemplate,
  templateFileName,
  templatesOfKind,
} from '../templates';
import type { AnyTemplate, DocumentTemplate, DrawingTemplateSettings } from '../templates';
import { buildAssembly, buildDrawing, buildPart, settingsOfDocument } from '../templateService';
import { unpackDrawingFile } from '../../drawing/drawingFile';

const calls: string[] = [];
const log = (name: string, ...args: unknown[]) => calls.push(args.length ? `${name} ${JSON.stringify(args)}` : name);

/** Stand-in of a kernel document/drawing: every call is recorded, results are fixed texts. */
const fakeDoc = (save: string) => ({
  setVariable: async (...a: unknown[]) => log('setVariable', ...a),
  addFeature: async (...a: unknown[]) => log('addFeature', ...a),
  save: async (...a: unknown[]) => (log('save', ...a), save),
  saveModel: async () => '{"format":"bendcore-part","m":1}',
  setSettings: async (...a: unknown[]) => log('setSettings', ...a),
  insertPart: async (...a: unknown[]) => (log('insertPart', ...a), { definition: calls.length, occurrence: 1 }),
  describe: async () => ({
    projection: 'thirdAngle',
    sheetStyle: 'iso7200',
    textFont: 'truetype',
    sheets: [{ format: 'A3', orientation: 'landscape', title: { organization: 'Org', developer: { surname: 'Dev' }, department: '', language: 'en' } }],
  }),
  state: async () => ({ definitions: [{ kind: 'part', properties: { material: 'Alu', density: 2700 } }] }),
  dispose: async () => log('dispose'),
});

jest.mock('../../modeling/modelingLoader', () => ({
  defaultModelingBaseUrl: () => 'x/',
  loadModelingFrom: async () => ({
    dispose: () => log('module dispose'),
    root: { createDocument: async () => fakeDoc('PARTJSON') },
  }),
}));
jest.mock('../../assembly/assemblyLoader', () => ({
  defaultAssemblyBaseUrl: () => 'x/',
  loadAssemblyFrom: async () => ({
    dispose: () => log('module dispose'),
    root: { createDocument: async (name: string) => (log('createDocument', name), fakeDoc('ASMJSON')), loadDocument: async () => ({ document: fakeDoc('') }) },
  }),
}));
jest.mock('../../drawing/drawingLoader', () => ({
  defaultDrawingBaseUrl: () => 'x/',
  loadDrawingFrom: async () => ({
    dispose: () => log('module dispose'),
    root: {
      createFromPart: async (...a: unknown[]) => (log('createFromPart', ...a), fakeDoc('{"format":"bendcore-drawing","d":1}')),
      createFromAssembly: async (...a: unknown[]) => (log('createFromAssembly', ...a), fakeDoc('{"format":"bendcore-drawing","d":1}')),
      load: async () => ({ drawing: fakeDoc('') }),
    },
  }),
}));

beforeEach(() => {
  calls.length = 0;
});

const byId = (id: string) => BUILTIN_TEMPLATES.find((x) => x.id === id) as AnyTemplate;

describe('built-in templates', () => {
  test('the set of the task: part ESKD mm, drawings ESKD A3 / A4, ISO A3 first and third angle, assembly', () => {
    expect(BUILTIN_TEMPLATES.map((x) => `${x.kind}:${x.id}`)).toEqual([
      'part:builtin:part-eskd-mm',
      'drawing:builtin:drawing-eskd-a3',
      'drawing:builtin:drawing-eskd-a4',
      'drawing:builtin:drawing-iso-a3-first',
      'drawing:builtin:drawing-iso-a3-third',
      'assembly:builtin:assembly',
    ]);
    expect(new Set(BUILTIN_TEMPLATES.map((x) => x.id)).size).toBe(BUILTIN_TEMPLATES.length);
  });

  test('names and hints exist in both dictionaries', () => {
    for (const x of BUILTIN_TEMPLATES) {
      for (const key of [x.nameKey, x.descriptionKey] as (MessageKey | undefined)[]) {
        expect(key).toBeDefined();
        expect(ru[key as MessageKey]).toBeTruthy();
        expect(en[key as MessageKey]).toBeTruthy();
      }
    }
    expect(ru['template.builtin.partEskd']).toBe('Деталь ЕСКД, мм');
    expect(ru['template.builtin.drawingIsoA3Third']).toBe('Чертёж ISO A3 (третий угол)');
  });

  test('ISO A3 third angle: ISO 7200 sheet, method A; ESKD A4 is portrait with the ESKD style', () => {
    expect((byId('builtin:drawing-iso-a3-third') as DocumentTemplate<'drawing'>).settings).toMatchObject({
      format: 'A3',
      orientation: 'landscape',
      style: 'iso7200',
      projection: 'thirdAngle',
    });
    expect((byId('builtin:drawing-iso-a3-first') as DocumentTemplate<'drawing'>).settings.projection).toBe('firstAngle');
    expect((byId('builtin:drawing-eskd-a4') as DocumentTemplate<'drawing'>).settings).toMatchObject({ format: 'A4', orientation: 'portrait', style: 'eskd', textFont: 'gost2304' });
  });

  test('every built-in template passes the reader of the template file', () => {
    for (const x of BUILTIN_TEMPLATES) {
      const r = parseTemplateFile(serializeTemplate(x, 'name'), { id: 'user:1', now: 5 });
      expect(r.ok).toBe(true);
      if (r.ok) expect(r.template.settings).toEqual(x.settings);
    }
  });

  test('templatesOfKind: built-in first, user templates by name', () => {
    const mk = (name: string, at: number): AnyTemplate => ({ ...(byId('builtin:assembly') as DocumentTemplate<'assembly'>), id: name, name, builtin: false, createdAt: at });
    const list = templatesOfKind(BUILTIN_TEMPLATES, [mk('b', 1), mk('a', 2)], 'assembly');
    expect(list.map((x) => x.id)).toEqual(['builtin:assembly', 'a', 'b']);
  });
});

describe('template file', () => {
  const user: DocumentTemplate<'part'> = {
    format: 'bendcore-template',
    version: 1,
    id: 'user:x',
    kind: 'part',
    name: 'Кронштейн',
    builtin: false,
    createdAt: 1,
    settings: {
      units: 'mm',
      material: { id: 'alu_6061', density: 2700 },
      variables: [{ name: 'T', expression: '4' }, { name: 'Ширина', expression: 'T*10' }],
      planes: [{ label: 'Mid', base: 'XZ', offset: 'T/2' }],
    },
  };

  test('export and import give the same settings, the import gets a new id and is not built-in', () => {
    const r = parseTemplateFile(serializeTemplate(user), { id: 'user:new', now: 99 });
    expect(r).toEqual({ ok: true, template: { ...user, id: 'user:new', createdAt: 99 } });
  });

  test.each([
    ['not json', '{', /not JSON/],
    ['wrong format', JSON.stringify({ format: 'other' }), /format must be/],
    ['future version', JSON.stringify({ ...user, version: 2 }), /unsupported template version 2/],
    ['unknown kind', JSON.stringify({ ...user, kind: 'sketch' }), /kind must be/],
    ['no name', JSON.stringify({ ...user, name: '  ' }), /no name/],
    ['units', JSON.stringify({ ...user, settings: { ...user.settings, units: 'inch' } }), /only "mm"/],
    ['duplicate variable', JSON.stringify({ ...user, settings: { ...user.settings, variables: [{ name: 'T', expression: '1' }, { name: 'T', expression: '2' }] } }), /listed twice/],
    ['bad identifier', JSON.stringify({ ...user, settings: { ...user.settings, variables: [{ name: '1x', expression: '1' }] } }), /not an identifier/],
    ['bad plane', JSON.stringify({ ...user, settings: { ...user.settings, planes: [{ label: 'a', base: 'AB', offset: '0' }] } }), /base plane/],
    ['bad density', JSON.stringify({ ...user, settings: { ...user.settings, material: { id: 'x', density: -1 } } }), /positive density/],
    ['drawing projection', JSON.stringify({ format: 'bendcore-template', version: 1, kind: 'drawing', name: 'n', settings: { format: 'A3', orientation: 'landscape', style: 'eskd', projection: 'x', textFont: 'gost2304' } }), /projection/],
    ['drawing format', JSON.stringify({ format: 'bendcore-template', version: 1, kind: 'drawing', name: 'n', settings: { format: 'B5', orientation: 'landscape', style: 'eskd', projection: 'firstAngle', textFont: 'gost2304' } }), /format must be/],
  ])('rejects %s', (_what, text, pattern) => {
    const r = parseTemplateFile(text, { id: 'u', now: 0 });
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.error).toMatch(pattern);
  });

  test('a drawing template keeps the title fields and ignores unknown ones', () => {
    const text = JSON.stringify({
      format: 'bendcore-template',
      version: 1,
      kind: 'drawing',
      name: 'ISO',
      extra: 1,
      settings: { format: 'A3', orientation: 'landscape', style: 'iso7200', projection: 'thirdAngle', textFont: 'truetype', title: { organization: 'Org', developer: 'Dev' } },
    });
    const r = parseTemplateFile(text, { id: 'u', now: 0 });
    expect(r.ok && r.template.settings).toEqual({
      format: 'A3',
      orientation: 'landscape',
      style: 'iso7200',
      projection: 'thirdAngle',
      textFont: 'truetype',
      title: { organization: 'Org', developer: 'Dev', department: '', language: '' },
    });
  });

  test('file names are Latin; a Cyrillic name falls back to the id', () => {
    expect(templateFileName('Bracket 2', 'user:a')).toBe('Bracket-2.bctemplate.json');
    expect(templateFileName('Кронштейн', 'user:abc123')).toMatch(/^template-[A-Za-z0-9]+\.bctemplate\.json$/);
  });

  test('new ids are unique', () => {
    expect(new Set(Array.from({ length: 50 }, () => newTemplateId(1))).size).toBe(50);
  });
});

describe('settings of documents', () => {
  test('part: material, variables and offset planes; planes tied to geometry are left out', () => {
    const text = JSON.stringify({
      format: 'bendcore-part',
      version: 3,
      material: { id: 'steel_45', density: 7826 },
      variables: [{ name: 'L', expression: '40' }],
      features: [
        { id: 'DatumPlane001', label: 'Mid', type: 'DatumPlane', links: {}, options: { method: 'offset', base: 'XZ' }, parameters: { offset: 'L/2' } },
        { id: 'DatumPlane002', label: 'Face', type: 'DatumPlane', links: {}, options: { method: 'offset', base: 'face' }, parameters: { offset: '1' } },
        { id: 'DatumPlane003', label: 'Linked', type: 'DatumPlane', links: { plane: 'DatumPlane001' }, options: { method: 'offset', base: 'XY' }, parameters: { offset: '1' } },
        { id: 'Extrude001', type: 'Extrude', links: {}, options: {}, parameters: {} },
      ],
    });
    expect(partSettingsFromFile(text)).toEqual({
      units: 'mm',
      material: { id: 'steel_45', density: 7826 },
      variables: [{ name: 'L', expression: '40' }],
      planes: [{ label: 'Mid', base: 'XZ', offset: 'L/2' }],
    });
    expect(partSettingsFromFile('{"format":"bendcore-assembly"}')).toMatch(/not a bendcore-part/);
    expect(partSettingsFromFile('x')).toMatch(/not JSON/);
  });

  test('drawing: format, orientation, style, projection, font and the constant title fields', () => {
    expect(
      drawingSettingsFromDescription({
        projection: 'thirdAngle',
        sheetStyle: 'iso7200',
        textFont: 'truetype',
        sheets: [{ format: 'A3', orientation: 'landscape', title: { organization: 'Org', developer: { surname: 'Dev' }, department: 'D', language: 'en' } }],
      }),
    ).toEqual({
      format: 'A3',
      orientation: 'landscape',
      style: 'iso7200',
      projection: 'thirdAngle',
      textFont: 'truetype',
      title: { organization: 'Org', developer: 'Dev', department: 'D', language: 'en' },
    });
    expect(drawingSettingsFromDescription({ projection: 'firstAngle', sheetStyle: 'eskd', textFont: 'gost2304', sheets: [] })).toBe('the drawing has no sheet');
  });

  test('assembly: the most common material of the part definitions, one entry per part source', () => {
    const def = (material: string, density: number | null, kind: 'part' | 'assembly' = 'part') => ({ kind, properties: { material, density } });
    expect(assemblyMaterialFromState({ definitions: [def('A', 1), def('B', 2), def('B', 2), def('C', null), def('Z', 9, 'assembly')] })).toEqual({ name: 'B', density: 2 });
    expect(assemblyMaterialFromState({ definitions: [def('C', null)] })).toBeNull();
    expect(
      componentsFromParts([
        { definition: 3, name: 'b.json', text: '{}' },
        { definition: 1, name: 'a.json', text: '{"x":1}' },
        { definition: -1, name: 'library.json', text: '{}' },
        { definition: 4, name: 'a.json', text: '{"x":2}' },
      ]),
    ).toEqual([
      { name: 'a.json', text: '{"x":1}' },
      { name: 'b.json', text: '{}' },
      { name: 'a.json', text: '{"x":2}' },
    ]);
  });

  test('default material of a component: only for parts without a material of their own', () => {
    const material = { name: 'Steel', density: 7850 };
    expect(defaultComponentProperties('{"format":"bendcore-part"}', material)).toEqual({ material: 'Steel', density: 7850 });
    expect(defaultComponentProperties('{"material":{"id":"a","density":2700}}', material)).toBeNull();
    expect(defaultComponentProperties('{}', null)).toBeNull();
  });
});

/** The parts of IndexedDB the store uses: transactions on object stores keyed by `id`. */
function fakeDb(): IDBDatabase {
  const stores = new Map<string, Map<string, unknown>>();
  const table = (n: string) => stores.get(n) ?? stores.set(n, new Map()).get(n)!;
  const done = (tx: { oncomplete?: () => void }) => queueMicrotask(() => tx.oncomplete?.());
  return {
    transaction(name: string) {
      const tx: { oncomplete?: () => void; objectStore(n: string): unknown } = {
        objectStore: () => ({
          put: (v: { id: string }) => (table(name).set(v.id, JSON.parse(JSON.stringify(v))), done(tx)),
          delete: (k: string) => (table(name).delete(k), done(tx)),
          clear: () => (table(name).clear(), done(tx)),
          getAll: () => {
            const req: { result?: unknown; onsuccess?: () => void } = {};
            queueMicrotask(() => {
              req.result = [...table(name).values()];
              req.onsuccess?.();
            });
            return req;
          },
        }),
      };
      return tx;
    },
  } as unknown as IDBDatabase;
}

describe('storage of user templates', () => {
  const mk = (id: string, createdAt: number): AnyTemplate => ({ format: 'bendcore-template', version: 1, kind: 'part', settings: (byId('builtin:part-eskd-mm') as DocumentTemplate<'part'>).settings, id, name: id, builtin: false, createdAt });

  test('put, list in creation order, delete; the built-in ones are never stored', async () => {
    const store = new AppStore(fakeDb());
    expect(await store.templates()).toEqual([]);
    await store.putTemplate(mk('user:b', 2));
    await store.putTemplate(mk('user:a', 1));
    expect((await store.templates()).map((x) => x.id)).toEqual(['user:a', 'user:b']);
    await store.putTemplate({ ...mk('user:a', 1), name: 'renamed' });
    expect((await store.templates()).map((x) => x.name)).toEqual(['renamed', 'user:b']);
    await store.removeTemplate('user:a');
    expect((await store.templates()).map((x) => x.id)).toEqual(['user:b']);
  });

  test('a stored template reads back through the template file', async () => {
    const store = new AppStore(fakeDb());
    const t = mk('user:c', 3);
    await store.putTemplate(t);
    const [back] = await store.templates();
    expect(parseTemplateFile(serializeTemplate(back), { id: back.id, now: back.createdAt })).toEqual({ ok: true, template: t });
  });
});

describe('applying the settings with the kernel', () => {
  test('part: variables first, then planes, then the material in the file; both workers stop', async () => {
    const template = byId('builtin:part-eskd-mm') as DocumentTemplate<'part'>;
    const file = await buildPart(
      { ...template.settings, variables: [{ name: 'L', expression: '40' }], planes: [{ label: 'Mid', base: 'XZ', offset: 'L/2' }] },
      'part.bcpart.json',
    );
    expect(file).toEqual({ kind: 'part', name: 'part.bcpart.json', text: 'PARTJSON' });
    expect(calls).toEqual([
      'setVariable ["L","40"]',
      'addFeature [{"type":"DatumPlane","label":"Mid","options":{"method":"offset","base":"XZ"},"parameters":{"offset":"L/2"}}]',
      'save [{"material":{"id":"steel_45","density":7826}}]',
      'dispose',
      'module dispose',
    ]);
  });

  test('drawing: ISO A3 third angle goes to the sheet options and the settings; the file holds drawing and model', async () => {
    const template = byId('builtin:drawing-iso-a3-third') as DocumentTemplate<'drawing'>;
    const settings: DrawingTemplateSettings = { ...template.settings, title: { organization: 'Org', developer: 'Dev', department: '', language: '' } };
    const file = await buildDrawing(settings, { kind: 'part', name: 'bracket.bcpart.json', text: 'PART' });
    expect(calls[0]).toBe(
      'createFromPart ["PART",{"path":"bracket.bcpart.json","sheet":{"format":"A3","orientation":"landscape","title":{"organization":"Org","department":"","language":"","developer":{"surname":"Dev"}}},"properties":{"name":"bracket"}}]',
    );
    expect(calls[1]).toBe('setSettings [{"sheetStyle":"iso7200","projection":"thirdAngle","textFont":"truetype"}]');
    expect(file.name).toBe('bracket.bcdrw.json');
    const r = unpackDrawingFile(file.text);
    expect(r.ok && r.content).toMatchObject({ drawing: '{"format":"bendcore-drawing","d":1}', model: '{"format":"bendcore-part","m":1}', modelName: 'bracket.bcpart.json' });
  });

  test('drawing of an assembly uses the assembly call', async () => {
    const template = byId('builtin:drawing-eskd-a4') as DocumentTemplate<'drawing'>;
    await buildDrawing(template.settings, { kind: 'assembly', name: 'unit.bcasm.json', text: 'ASM' });
    expect(calls[0]).toMatch(/^createFromAssembly \["ASM",\{"path":"unit.bcasm.json","sheet":\{"format":"A4","orientation":"portrait"/);
  });

  test('assembly: components are inserted (the first one fixed), the default material only for parts without one', async () => {
    const file = await buildAssembly(
      { material: { name: 'Steel', density: 7850 }, components: [{ name: 'a.bcpart.json', text: '{"material":{"id":"x","density":1}}' }, { name: 'b.bcpart.json', text: '{}' }] },
      'Assembly',
      'assembly.bcasm.json',
    );
    expect(file.kind).toBe('assembly');
    expect(file.parts?.map((p) => p.name)).toEqual(['a.bcpart.json', 'b.bcpart.json']);
    expect(calls[0]).toBe('createDocument ["Assembly"]');
    expect(calls[1]).toBe('insertPart [{"name":"a","format":"part-json","data":"{\\"material\\":{\\"id\\":\\"x\\",\\"density\\":1}}","fixed":true,"beside":false}]');
    expect(calls[2]).toBe('insertPart [{"name":"b","format":"part-json","data":"{}","fixed":false,"beside":true,"properties":{"material":"Steel","density":7850}}]');
  });

  test('settings of an open drawing, assembly and part for "Save as template"', async () => {
    const drawing = await settingsOfDocument({ kind: 'drawing', name: 'd.bcdrw.json', text: JSON.stringify({ format: 'bendcore-drawing-file', version: 1, modelName: 'm', drawing: '{"format":"bendcore-drawing"}', model: '{"format":"bendcore-part"}' }) });
    expect(drawing).toMatchObject({ kind: 'drawing', settings: { projection: 'thirdAngle', style: 'iso7200', title: { organization: 'Org', developer: 'Dev' } } });
    const asm = await settingsOfDocument({ kind: 'assembly', name: 'a', text: '{}', parts: [{ definition: 0, name: 'p.json', text: '{}' }] });
    expect(asm).toEqual({ kind: 'assembly', settings: { material: { name: 'Alu', density: 2700 }, components: [{ name: 'p.json', text: '{}' }] } });
    expect(await settingsOfDocument({ kind: 'part', name: 'p', text: '{"format":"bendcore-part","variables":[],"features":[]}' })).toEqual({
      kind: 'part',
      settings: { units: 'mm', material: null, variables: [], planes: [] },
    });
    expect(typeof (await settingsOfDocument({ kind: 'part', name: 'p', text: 'nope' }))).toBe('string');
  });
});
