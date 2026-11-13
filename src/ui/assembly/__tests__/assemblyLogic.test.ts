import { bomTableRows, clampProgress, flattenOccurrences, insertionDirection, rayPlane } from '../assemblyMath';
import { allowedMateTypes, mateAccepts, nextMateSlots, suggestMateType } from '../mateRules';
import type { PickedElement } from '../mateRules';
import { dofText, massText } from '../labels';
import { translate } from '../../app/i18n';
import type { BomRow, OccurrenceInfo } from '../../../wasm/bendcore_assembly_api.mjs';

const pick = (occ: number, index: number, type: 'plane' | 'cylinder' | 'axis' | 'point' | 'sphere'): PickedElement => ({
  ref: { path: [occ], kind: 'face', index },
  geometry: type,
  entity: { type, point: [0, 0, 0], direction: [0, 0, 1], radius: 1 },
  label: `${occ}:${index}`,
});

describe('mate rules', () => {
  it('follows the entity pairs of the solver', () => {
    expect(mateAccepts('concentric', 'cylinder', 'axis')).toBe(true);
    expect(mateAccepts('concentric', 'cylinder', 'plane')).toBe(false);
    expect(mateAccepts('coincident', 'plane', 'plane')).toBe(true);
    expect(mateAccepts('coincident', 'plane', 'cylinder')).toBe(false);
    expect(mateAccepts('coincident', 'point', 'cylinder')).toBe(true);
    expect(mateAccepts('distance', 'plane', 'point')).toBe(true);
    expect(mateAccepts('distance', 'axis', 'axis')).toBe(false);
    expect(mateAccepts('tangent', 'cylinder', 'plane')).toBe(true);
    expect(mateAccepts('tangent', 'sphere', 'plane')).toBe(true);
    expect(mateAccepts('tangent', 'sphere', 'cylinder')).toBe(false);
    expect(mateAccepts('angle', 'plane', 'axis')).toBe(true);
    expect(mateAccepts('perpendicular', 'point', 'axis')).toBe(false);
  });

  it('allows nothing within one component and suggests the natural type', () => {
    expect(allowedMateTypes(pick(1, 0, 'plane'), pick(1, 2, 'plane'))).toEqual([]);
    expect(suggestMateType(pick(1, 0, 'cylinder'), pick(2, 3, 'cylinder'))).toBe('concentric');
    expect(suggestMateType(pick(1, 0, 'plane'), pick(2, 3, 'plane'))).toBe('coincident');
    expect(suggestMateType(pick(1, 0, 'plane'), pick(2, 3, 'cylinder'))).toBe('tangent');
    const allowed = allowedMateTypes(pick(1, 0, 'plane'), pick(2, 3, 'plane'));
    expect(allowed).toEqual(['coincident', 'distance', 'angle', 'parallel', 'perpendicular', 'rigid']);
    const noEntity = { ...pick(2, 1, 'plane'), entity: null };
    expect(allowedMateTypes(pick(1, 0, 'plane'), noEntity)).toEqual(['rigid']);
  });

  it('fills, replaces and clears the two slots', () => {
    const a = pick(1, 0, 'plane');
    const b = pick(2, 0, 'plane');
    const c = pick(3, 0, 'plane');
    expect(nextMateSlots([null, null], a)).toEqual([a, null]);
    expect(nextMateSlots([a, null], b)).toEqual([a, b]);
    expect(nextMateSlots([a, b], c)).toEqual([a, c]);
    expect(nextMateSlots([a, b], a)).toEqual([b, null]);
    expect(nextMateSlots([a, b], b)).toEqual([a, null]);
  });
});

describe('assembly math', () => {
  it('intersects rays with planes', () => {
    expect(rayPlane([0, 0, 10], [0, 0, -1], [0, 0, 2], [0, 0, 1])).toEqual([0, 0, 2]);
    expect(rayPlane([0, 0, 10], [1, 0, 0], [0, 0, 2], [0, 0, 1])).toBeNull();
    expect(rayPlane([0, 0, 10], [0, 0, 1], [0, 0, 2], [0, 0, 1])).toBeNull();
  });

  it('points the insertion away from the viewer', () => {
    expect(insertionDirection([0, 0, 1], [0.3, -0.4, -0.8])).toEqual([-0, -0, -1]);
    expect(insertionDirection([0, 0, -1], [0.3, -0.4, -0.8])).toEqual([0, 0, -1]);
  });

  it('flattens the occurrence tree and the BOM', () => {
    const o = (id: number, children?: OccurrenceInfo[]) => ({ id, children }) as unknown as OccurrenceInfo;
    const flat = flattenOccurrences([o(1, [o(3), o(4)]), o(2)]);
    expect(flat.map((x) => [x.path, x.depth])).toEqual([
      [[1], 0],
      [[1, 3], 1],
      [[1, 4], 1],
      [[2], 0],
    ]);
    const row = (positionPath: string, children: BomRow[] = []) => ({ positionPath, children }) as unknown as BomRow;
    expect(bomTableRows([row('1', [row('1.1'), row('1.2')]), row('2')]).map((r) => r.positionPath)).toEqual([
      '1',
      '1.1',
      '1.2',
      '2',
    ]);
    expect(clampProgress(3.5, 2)).toBe(2);
    expect(clampProgress(-1, 2)).toBe(0);
    expect(clampProgress(Number.NaN, 2)).toBe(0);
  });
});

describe('labels', () => {
  it('declines the DOF count', () => {
    expect(dofText(1)).toBe('1 степень свободы');
    expect(dofText(2)).toBe('2 степени свободы');
    expect(dofText(5)).toBe('5 степеней свободы');
    expect(dofText(11)).toBe('11 степеней свободы');
    expect(dofText(21)).toBe('21 степень свободы');
    expect(dofText(0)).toBe('0 степеней свободы');
    const en = (k: Parameters<typeof translate>[1], p?: Record<string, string | number>) => translate('en', k, p);
    expect(dofText(1, en, 'en')).toBe('1 degree of freedom');
    expect(dofText(21, en, 'en')).toBe('21 degrees of freedom');
  });

  it('formats masses', () => {
    expect(massText(null)).toBe('—');
    expect(massText(1.23456)).toBe('1.235');
    expect(massText(0.0040781)).toBe('0.00408');
  });
});
