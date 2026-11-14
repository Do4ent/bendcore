// Packs the kernel's edge polylines into one vertex buffer and a segment index buffer for
// THREE.LineSegments (a single draw call for all edges). The points are the kernel's own; only
// the connectivity of consecutive points of each polyline is written here.

export interface EdgePolylineData {
  edgeIndex: number;
  points: Float32Array;
}

export interface EdgeBuffers {
  /** xyz triples of all polylines, in order. */
  positions: Float32Array;
  /** Pairs of vertex indices: (k, k+1) for consecutive points of the same polyline. */
  indices: Uint32Array;
  /** Kernel edgeIndex of each segment (segment k = indices[2k], indices[2k+1]). */
  segmentEdges: Uint32Array;
}

export function packEdgePolylines(edges: readonly EdgePolylineData[]): EdgeBuffers {
  let pointCount = 0;
  let segmentCount = 0;
  for (const e of edges) {
    if (e.points.length % 3 !== 0) {
      throw new Error(`edge ${e.edgeIndex}: polyline length ${e.points.length} is not a multiple of 3`);
    }
    const n = e.points.length / 3;
    pointCount += n;
    if (n >= 2) segmentCount += n - 1;
  }
  const positions = new Float32Array(pointCount * 3);
  const indices = new Uint32Array(segmentCount * 2);
  const segmentEdges = new Uint32Array(segmentCount);
  let vertex = 0;
  let segment = 0;
  for (const e of edges) {
    positions.set(e.points, vertex * 3);
    const n = e.points.length / 3;
    for (let k = 0; k + 1 < n; ++k) {
      indices[segment * 2] = vertex + k;
      indices[segment * 2 + 1] = vertex + k + 1;
      segmentEdges[segment] = e.edgeIndex;
      ++segment;
    }
    vertex += n;
  }
  return { positions, indices, segmentEdges };
}
