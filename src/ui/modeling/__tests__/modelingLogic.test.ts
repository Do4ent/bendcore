import type { FeatureTypeSpec, FeatureView, SketchData } from '../../../wasm/bendcore_modeling_api.mjs';
import {
  eligibleLinks,
  existingFeatureForm,
  missingInputs,
  newFeatureForm,
  toFeatureEdits,
  toFeatureSpec,
  togglePick,
} from '../featureForm';
import { applySketchDimensions, sketchDimensions, templateProfiles, DEFAULT_TEMPLATE } from '../sketchDimensions';
import { treeActions } from '../treeActions';

// Rectangle as built by SketchBuilder::rectangle (fixed corner, H/V sides, two distances).
const rectangle: SketchData = {
  points: [
    { x: '0', y: '0', fixed: true },
    { x: '(0) + 1', y: '0', fixed: false },
    { x: '(0) + 1', y: '(0) + 1', fixed: false },
    { x: '0', y: '(0) + 1', fixed: false },
  ],
  geometry: [0, 1, 2, 3].map((i) => ({
    type: 'line' as const,
    start: i,
    end: (i + 1) % 4,
    center: -1,
    radius: '1',
    counterClockwise: true,
    construction: false,
    projection: -1,
  })),
  constraints: [
    { type: 'horizontal', a: 0, b: 1, c: -1, value: '0' },
    { type: 'vertical', a: 1, b: 2, c: -1, value: '0' },
    { type: 'horizontal', a: 2, b: 3, c: -1, value: '0' },
    { type: 'vertical', a: 3, b: 0, c: -1, value: '0' },
    { type: 'distance', a: 0, b: 1, c: -1, value: 'W' },
    { type: 'distance', a: 1, b: 2, c: -1, value: '40' },
  ],
  regions: [],
};

function view(partial: Partial<FeatureView> & { id: string; type: string; index: number }): FeatureView {
  return {
    label: partial.id,
    suppressed: false,
    state: 'ok',
    messages: [],
    parameters: {},
    values: {},
    options: {},
    links: {},
    references: {},
    referenceReports: [],
    hasBody: true,
    ...partial,
  };
}

const extrudeSpec: FeatureTypeSpec = {
  type: 'Extrude',
  parameters: [
    { name: 'distance', defaultExpression: '10' },
    { name: 'distance2', defaultExpression: '0' },
  ],
  references: [{ name: 'limit', kind: 'face', multiple: false, optional: true }],
  options: [{ name: 'mode', allowed: ['boss', 'cut'] }],
  links: [{ name: 'sketch', types: ['Sketch'], optional: false }],
  hasSketch: false,
};

const filletSpec: FeatureTypeSpec = {
  type: 'Fillet',
  parameters: [{ name: 'radius', defaultExpression: '1' }],
  references: [{ name: 'edges', kind: 'edge', multiple: true, optional: false }],
  options: [],
  links: [],
  hasSketch: false,
};

const tree = [
  view({ id: 'S1', type: 'Sketch', index: 0, hasBody: false }),
  view({ id: 'Body', type: 'Extrude', index: 1 }),
  view({ id: 'S2', type: 'Sketch', index: 2, hasBody: false }),
  view({ id: 'Round', type: 'Fillet', index: 3 }),
];

const ref = (name: string) => ({
  name,
  kind: 'edge' as const,
  signature: {
    valid: false,
    kind: 'edge' as const,
    geometry: '',
    center: [0, 0, 0] as [number, number, number],
    hasDirection: false,
    direction: [0, 0, 0] as [number, number, number],
    radius: 0,
    size: 0,
  },
});

describe('sketch dimensions', () => {
  it('lists fixed point coordinates and dimension constraints with readable labels', () => {
    const dims = sketchDimensions(rectangle);
    expect(dims.map((d) => [d.key, d.label, d.expression])).toEqual([
      ['p0.x', 'Точка 1: X', '0'],
      ['p0.y', 'Точка 1: Y', '0'],
      ['c4', 'Длина по X (т.1–т.2)', 'W'],
      ['c5', 'Длина по Y (т.2–т.3)', '40'],
    ]);
  });

  it('applies edits to a copy, keeping the structure', () => {
    const next = applySketchDimensions(rectangle, { c4: '80', 'p0.y': '5' });
    expect(next.constraints[4].value).toBe('80');
    expect(next.points[0].y).toBe('5');
    expect(rectangle.constraints[4].value).toBe('W');
    expect(next.geometry).toEqual(rectangle.geometry);
    expect(() => applySketchDimensions(rectangle, { c0: '1' })).toThrow(/no dimension/);
    expect(() => applySketchDimensions(rectangle, { 'p1.x': '1' })).toThrow(/no fixed point/);
    expect(() => applySketchDimensions(rectangle, { q: '1' })).toThrow(/unknown/);
  });

  it('turns the template form into profile specs; polygon lines are "x; y"', () => {
    expect(templateProfiles(DEFAULT_TEMPLATE)).toEqual({
      profiles: [{ kind: 'rectangle', x: '0', y: '0', width: '60', height: '40' }],
    });
    const poly = templateProfiles({ ...DEFAULT_TEMPLATE, kind: 'polygon', polygon: '0; 0\nmin(L, 50); 0\n\n0; 30' });
    expect(poly.profiles[0]).toEqual({ kind: 'polygon', points: [['0', '0'], ['min(L, 50)', '0'], ['0', '30']] });
    expect(() => templateProfiles({ ...DEFAULT_TEMPLATE, kind: 'polygon', polygon: '0 0\n1;1\n2;2' })).toThrow(/строка 1/);
    expect(() => templateProfiles({ ...DEFAULT_TEMPLATE, kind: 'polygon', polygon: '0;0\n1;1' })).toThrow(/3 вершин/);
  });
});

describe('feature form', () => {
  it('fills catalog defaults and links the latest eligible feature before the position', () => {
    const form = newFeatureForm(extrudeSpec, tree, 4);
    expect(form.parameters).toEqual({ distance: '10', distance2: '0' });
    expect(form.options).toEqual({ mode: 'boss' });
    expect(form.links).toEqual({ sketch: 'S2' });
    expect(newFeatureForm(extrudeSpec, tree, 2).links).toEqual({ sketch: 'S1' });
    expect(eligibleLinks(extrudeSpec.links[0], tree, 4).map((f) => f.id)).toEqual(['S1', 'S2']);
    // A slot accepting any type defaults to the latest body feature, not a sketch.
    const pattern = { ...filletSpec, links: [{ name: 'feature', types: [], optional: false }] };
    expect(newFeatureForm(pattern, tree, 3).links).toEqual({ feature: 'Body' });
  });

  it('reports missing required inputs and converts to spec and edits', () => {
    let form = newFeatureForm(filletSpec, tree, 4);
    expect(missingInputs(filletSpec, form, (s) => s)).toEqual(['не выбрано: edges']);
    form = { ...form, references: { edges: togglePick(form.references.edges, { ref: ref('E1'), index: 3 }, true) } };
    expect(missingInputs(filletSpec, form, (s) => s)).toEqual([]);
    expect(toFeatureSpec(form)).toEqual({
      type: 'Fillet',
      parameters: { radius: '1' },
      options: {},
      links: {},
      references: { edges: [ref('E1')] },
    });
    expect(toFeatureEdits({ ...form, label: ' R ' }).label).toBe('R');
  });

  it('toggles picks: multiple slots accumulate, single slots replace, repeated picks remove', () => {
    let picks = togglePick([], { ref: ref('A'), index: 1 }, true);
    picks = togglePick(picks, { ref: ref('B'), index: 2 }, true);
    expect(picks.map((p) => p.ref.name)).toEqual(['A', 'B']);
    expect(togglePick(picks, { ref: ref('A'), index: 1 }, true).map((p) => p.ref.name)).toEqual(['B']);
    expect(togglePick(picks, { ref: ref('C'), index: 5 }, false).map((p) => p.ref.name)).toEqual(['C']);
  });

  it('edit form of a sketch carries its dimensions', () => {
    const sketchSpec: FeatureTypeSpec = { ...extrudeSpec, type: 'Sketch', links: [], hasSketch: true, references: [] };
    const form = existingFeatureForm(sketchSpec, view({ id: 'S1', type: 'Sketch', index: 0, sketch: rectangle }));
    expect(form.dimensions).toEqual({ 'p0.x': '0', 'p0.y': '0', c4: 'W', c5: '40' });
    expect(form.template).toBeNull();
  });
});

describe('tree actions', () => {
  it('offers moves within the tree, rollback after the node, suppress toggle', () => {
    expect(treeActions(tree[0], 4, null)).toEqual(['edit', ...(tree[0].type === 'Sketch' ? ['editSketch'] : []), 'suppress', 'remove', 'moveDown', 'rollbackHere']);
    expect(treeActions(tree[3], 4, null)).toEqual(['edit', 'suppress', 'remove', 'moveUp']);
    expect(treeActions(tree[1], 4, 2)).toEqual(['edit', 'suppress', 'remove', 'moveUp', 'moveDown']);
    // Rolled-back nodes cannot be edited; a suppressed node offers "unsuppress".
    expect(treeActions({ ...tree[2], suppressed: true }, 4, 2)).toEqual(['unsuppress', 'remove', 'moveUp', 'moveDown', 'rollbackHere']);
  });
});
