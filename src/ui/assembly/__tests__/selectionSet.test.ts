import { combineRefs, placedBox, refKey, topLevelIds } from '../selectionSet';

const a = { path: [1], index: 0 };
const b = { path: [1], index: 3 };
const c = { path: [2, 5], index: 0 };

describe('assembly selection sets', () => {
  test('replace, add and remove', () => {
    expect(combineRefs([a], [b, b], 'replace').map(refKey)).toEqual(['1:3']);
    expect(combineRefs([a], [b, a], 'add').map(refKey)).toEqual(['1:0', '1:3']);
    expect(combineRefs([a, b, c], [b], 'remove').map(refKey)).toEqual(['1:0', '2/5:0']);
  });

  test('the same index in different occurrences are different elements', () => {
    expect(refKey({ path: [1], index: 0 })).not.toBe(refKey({ path: [2], index: 0 }));
  });

  test('components are top-level occurrences', () => {
    expect(topLevelIds([[1], [1, 4], [2, 5], []])).toEqual([1, 2]);
  });
});

describe('placed box', () => {
  // unit cube corners moved by 50 along X (column-major matrix) and the same cube turned 90 deg about Z
  const cube = [0, 0, 0, 1, 1, 1];
  const move = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 50, 0, 0, 1];
  const turn = [0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
  test('world coordinates of every part', () => {
    expect(placedBox([{ positions: cube, matrix: move }])).toEqual({ min: [50, 0, 0], max: [51, 1, 1] });
    expect(placedBox([{ positions: cube, matrix: turn }])).toEqual({ min: [-1, 0, 0], max: [0, 1, 1] });
    expect(placedBox([{ positions: cube, matrix: move }, { positions: cube, matrix: turn }])).toEqual({ min: [-1, 0, 0], max: [51, 1, 1] });
  });
  test('no vertices, no box', () => {
    expect(placedBox([])).toBeNull();
    expect(placedBox([{ positions: [], matrix: move }])).toBeNull();
  });
});
