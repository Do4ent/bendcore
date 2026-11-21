// Datum geometry in the part editor (docs/ui/PART_MODELING_UI.md, "Вспомогательная геометрия"):
// glyphs drawn for built datum nodes and the slot order of clicks (three vertices of a plane).
import type { FeatureTypeSpec, FeatureView, TopoReference } from '../../../wasm/bendcore_modeling_api.mjs';
import { datumGlyphs } from '../datumGlyphs';
import { newFeatureForm, slotAfterPick } from '../featureForm';
import { TOOLBAR_TYPES, toolbarTypes } from '../labels';
import { translate } from '../../app/i18n';

function view(id: string, type: string, state: FeatureView['state'], datum?: FeatureView['datum']): FeatureView {
  return {
    index: 0,
    id,
    type,
    label: id,
    suppressed: false,
    state,
    messages: [],
    parameters: {},
    values: {},
    options: {},
    links: {},
    references: {},
    referenceReports: [],
    hasBody: false,
    datum,
  };
}

const planeSpec: FeatureTypeSpec = {
  type: 'DatumPlane',
  parameters: [],
  options: [{ name: 'method', allowed: ['offset', 'threePoints'] }],
  links: [],
  references: [
    { name: 'face', kind: 'face', multiple: false, optional: true },
    { name: 'face2', kind: 'face', multiple: false, optional: true },
    { name: 'vertex1', kind: 'vertex', multiple: false, optional: true },
    { name: 'vertex2', kind: 'vertex', multiple: false, optional: true },
    { name: 'vertex3', kind: 'vertex', multiple: false, optional: true },
    { name: 'edges', kind: 'edge', multiple: true, optional: true },
  ],
  hasSketch: false,
} as FeatureTypeSpec;

const ref = (name: string): TopoReference => ({ name, kind: 'vertex' }) as TopoReference;

describe('datum glyphs', () => {
  test('one glyph per built datum node; a coordinate system is drawn as its axes', () => {
    const glyphs = datumGlyphs([
      view('P', 'DatumPoint', 'ok', { point: [1, 2, 3] }),
      view('C', 'DatumCsys', 'ok', {
        planeOrigin: [0, 0, 0],
        planeNormal: [0, 0, 1],
        axisOrigin: [0, 0, 0],
        axisDirection: [0, 0, 1],
        point: [0, 0, 0],
        csys: { origin: [0, 0, 0], x: [1, 0, 0], y: [0, 1, 0], z: [0, 0, 1] },
      }),
      view('Pl', 'DatumPlane', 'warning', { planeOrigin: [0, 0, 5], planeNormal: [0, 0, 1] }),
      view('A', 'DatumAxis', 'ok', { axisOrigin: [0, 0, 0], axisDirection: [1, 0, 0] }),
      view('Bad', 'DatumPoint', 'error'),
      view('Off', 'DatumPoint', 'suppressed', { point: [0, 0, 0] }),
      view('S', 'Sketch', 'ok'),
    ]);
    expect(glyphs.map((g) => `${g.kind}:${g.id}`)).toEqual(['point:P', 'csys:C', 'plane:Pl', 'axis:A']);
    expect(glyphs[1]).toMatchObject({ x: [1, 0, 0], z: [0, 0, 1] });
  });
});

describe('picking order', () => {
  test('a pick moves to the next empty slot of the same kind', () => {
    let form = newFeatureForm(planeSpec, [], 0);
    expect(slotAfterPick(planeSpec, form, 'face')).toBe('face2');
    form = { ...form, references: { ...form.references, vertex1: [{ ref: ref('a'), index: 0 }] } };
    expect(slotAfterPick(planeSpec, form, 'vertex1')).toBe('vertex2');
    form = { ...form, references: { ...form.references, vertex2: [{ ref: ref('b'), index: 1 }] } };
    expect(slotAfterPick(planeSpec, form, 'vertex2')).toBe('vertex3');
    form = { ...form, references: { ...form.references, vertex3: [{ ref: ref('c'), index: 2 }] } };
    expect(slotAfterPick(planeSpec, form, 'vertex3')).toBe('vertex3');
    // An edited vertex 1 does not jump over the filled slots.
    expect(slotAfterPick(planeSpec, form, 'vertex1')).toBe('vertex1');
    // A multiple slot keeps collecting and is not entered by itself (body boolean: target, tools).
    expect(slotAfterPick(planeSpec, form, 'edges')).toBe('edges');
    const booleanSpec = {
      ...planeSpec,
      references: [
        { name: 'target', kind: 'face', multiple: false, optional: false },
        { name: 'tools', kind: 'face', multiple: true, optional: false },
      ],
    } as FeatureTypeSpec;
    expect(slotAfterPick(booleanSpec, newFeatureForm(booleanSpec, [], 0), 'target')).toBe('target');
  });
});

describe('toolbar and captions', () => {
  test('datum point and coordinate system are on the toolbar with captions in both languages', () => {
    expect(TOOLBAR_TYPES).toEqual(expect.arrayContaining(['DatumPoint', 'DatumCsys']));
    expect(toolbarTypes(['Sketch', 'DatumCsys', 'DatumPoint', 'DatumPlane'])).toEqual(['Sketch', 'DatumPlane', 'DatumPoint', 'DatumCsys']);
    expect(translate('ru', 'part.feature.DatumPoint')).toBe('Точка');
    expect(translate('en', 'part.feature.DatumCsys')).toBe('Coordinate system');
    expect(translate('ru', 'part.value.threePoints')).toBe('по трём точкам');
    expect(translate('en', 'part.value.tangent')).toBe('tangent to a cylinder');
  });
});
