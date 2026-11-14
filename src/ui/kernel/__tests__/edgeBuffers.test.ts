import { packEdgePolylines } from '../edgeBuffers';

describe('packEdgePolylines', () => {
  it('concatenates points and links consecutive points of each polyline only', () => {
    const edges = [
      { edgeIndex: 4, points: new Float32Array([0, 0, 0, 1, 0, 0, 2, 0, 0]) },
      { edgeIndex: 7, points: new Float32Array([5, 5, 5, 5, 6, 5]) },
    ];
    const b = packEdgePolylines(edges);
    expect(Array.from(b.positions)).toEqual([0, 0, 0, 1, 0, 0, 2, 0, 0, 5, 5, 5, 5, 6, 5]);
    // Polyline 1: vertices 0-1, 1-2; polyline 2: vertices 3-4. No segment 2-3 across edges.
    expect(Array.from(b.indices)).toEqual([0, 1, 1, 2, 3, 4]);
    expect(Array.from(b.segmentEdges)).toEqual([4, 4, 7]);
  });

  it('keeps single-point polylines as vertices without segments', () => {
    const b = packEdgePolylines([{ edgeIndex: 0, points: new Float32Array([1, 2, 3]) }]);
    expect(b.positions.length).toBe(3);
    expect(b.indices.length).toBe(0);
  });

  it('rejects a polyline whose length is not a multiple of 3', () => {
    expect(() => packEdgePolylines([{ edgeIndex: 2, points: new Float32Array([1, 2]) }])).toThrow(/edge 2/);
  });

  it('returns empty buffers for no edges', () => {
    const b = packEdgePolylines([]);
    expect(b.positions.length).toBe(0);
    expect(b.indices.length).toBe(0);
  });
});
