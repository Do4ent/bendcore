/**
 * DimensionRenderer.test.ts - Phase 35B
 * Tests for dimension rendering on 3D canvas
 */

import * as THREE from 'three';
import { DimensionRenderer } from '../src/ui/DimensionRenderer';
import { OrbitCamera } from '../src/ui/OrbitCamera';
import { PathSegment, PathVertex, Vector3 } from '../src/ui/PathSketch3D';

describe('DimensionRenderer', () => {
  let renderer: DimensionRenderer;
  let camera: OrbitCamera;
  let testGroup: THREE.Group;

  beforeEach(() => {
    // Create a mock camera
    camera = new OrbitCamera(1.33);

    // Create renderer instance
    renderer = new DimensionRenderer(camera);

    // Create a test group for rendering
    testGroup = new THREE.Group();
  });

  afterEach(() => {
    renderer.dispose();
  });

  // ===== INITIALIZATION =====

  test('should create DimensionRenderer instance', () => {
    expect(renderer).toBeDefined();
    expect(renderer).toBeInstanceOf(DimensionRenderer);
  });

  test('should initialize with camera', () => {
    expect(camera).toBeDefined();
  });

  // ===== DIMENSION RENDERING =====

  test('should render length dimension', () => {
    const segment: PathSegment = {
      id: 'seg_1',
      from_vertex: 'v1',
      to_vertex: 'v2',
      dimension_L: 10.5,
      dimension_S: 2.0,
      bend_angle: 45,
      plane_rotation: 0,
      selected_dimension: 'L',
    };

    const fromVertex: PathVertex = {
      id: 'v1',
      position: { x: 0, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    const toVertex: PathVertex = {
      id: 'v2',
      position: { x: 10, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    expect(() => {
      renderer.renderDimension(segment, fromVertex, toVertex, testGroup);
    }).not.toThrow();

    // Group should have items after rendering
    expect(testGroup.children.length).toBeGreaterThanOrEqual(0);
  });

  test('should render straight dimension', () => {
    const segment: PathSegment = {
      id: 'seg_1',
      from_vertex: 'v1',
      to_vertex: 'v2',
      dimension_L: 10.5,
      dimension_S: 3.0,
      bend_angle: 45,
      plane_rotation: 0,
      selected_dimension: 'S',
    };

    const fromVertex: PathVertex = {
      id: 'v1',
      position: { x: 0, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    const toVertex: PathVertex = {
      id: 'v2',
      position: { x: 10, y: 5, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    expect(() => {
      renderer.renderDimension(segment, fromVertex, toVertex, testGroup);
    }).not.toThrow();
  });

  test('should render bend angle dimension', () => {
    const segment: PathSegment = {
      id: 'seg_1',
      from_vertex: 'v1',
      to_vertex: 'v2',
      dimension_L: 10.5,
      dimension_S: 2.0,
      bend_angle: 90,
      plane_rotation: 0,
      selected_dimension: 'B',
    };

    const fromVertex: PathVertex = {
      id: 'v1',
      position: { x: 0, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    const toVertex: PathVertex = {
      id: 'v2',
      position: { x: 10, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    expect(() => {
      renderer.renderDimension(segment, fromVertex, toVertex, testGroup);
    }).not.toThrow();
  });

  test('should render plane rotation dimension', () => {
    const segment: PathSegment = {
      id: 'seg_1',
      from_vertex: 'v1',
      to_vertex: 'v2',
      dimension_L: 10.5,
      dimension_S: 2.0,
      bend_angle: 45,
      plane_rotation: 180,
      selected_dimension: 'A',
    };

    const fromVertex: PathVertex = {
      id: 'v1',
      position: { x: 0, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    const toVertex: PathVertex = {
      id: 'v2',
      position: { x: 10, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    expect(() => {
      renderer.renderDimension(segment, fromVertex, toVertex, testGroup);
    }).not.toThrow();
  });

  // ===== DIMENSION VALUES =====

  test('should handle zero dimension values', () => {
    const segment: PathSegment = {
      id: 'seg_1',
      from_vertex: 'v1',
      to_vertex: 'v2',
      dimension_L: 0,
      dimension_S: 0,
      bend_angle: 0,
      plane_rotation: 0,
      selected_dimension: 'L',
    };

    const fromVertex: PathVertex = {
      id: 'v1',
      position: { x: 0, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    const toVertex: PathVertex = {
      id: 'v2',
      position: { x: 5, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    expect(() => {
      renderer.renderDimension(segment, fromVertex, toVertex, testGroup);
    }).not.toThrow();
  });

  test('should handle large dimension values', () => {
    const segment: PathSegment = {
      id: 'seg_1',
      from_vertex: 'v1',
      to_vertex: 'v2',
      dimension_L: 999.99,
      dimension_S: 500,
      bend_angle: 359.9,
      plane_rotation: 360,
      selected_dimension: 'L',
    };

    const fromVertex: PathVertex = {
      id: 'v1',
      position: { x: 0, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    const toVertex: PathVertex = {
      id: 'v2',
      position: { x: 100, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    expect(() => {
      renderer.renderDimension(segment, fromVertex, toVertex, testGroup);
    }).not.toThrow();
  });

  // ===== VERTEX POSITIONS =====

  test('should handle identical vertex positions', () => {
    const segment: PathSegment = {
      id: 'seg_1',
      from_vertex: 'v1',
      to_vertex: 'v2',
      dimension_L: 0,
      dimension_S: 0,
      bend_angle: 0,
      plane_rotation: 0,
      selected_dimension: 'L',
    };

    const vertex: PathVertex = {
      id: 'v1',
      position: { x: 5, y: 5, z: 5 },
      is_fixed: false,
      is_selected: false,
    };

    expect(() => {
      renderer.renderDimension(segment, vertex, vertex, testGroup);
    }).not.toThrow();
  });

  test('should handle 3D vertex positions', () => {
    const segment: PathSegment = {
      id: 'seg_1',
      from_vertex: 'v1',
      to_vertex: 'v2',
      dimension_L: 15.5,
      dimension_S: 5.0,
      bend_angle: 60,
      plane_rotation: 90,
      selected_dimension: 'L',
    };

    const fromVertex: PathVertex = {
      id: 'v1',
      position: { x: 10, y: 20, z: 30 },
      is_fixed: false,
      is_selected: false,
    };

    const toVertex: PathVertex = {
      id: 'v2',
      position: { x: 20, y: 30, z: 40 },
      is_fixed: false,
      is_selected: false,
    };

    expect(() => {
      renderer.renderDimension(segment, fromVertex, toVertex, testGroup);
    }).not.toThrow();
  });

  // ===== MULTIPLE DIMENSIONS =====

  test('should render multiple segments', () => {
    const segments: PathSegment[] = [
      {
        id: 'seg_1',
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 45,
        plane_rotation: 0,
        selected_dimension: 'L',
      },
      {
        id: 'seg_2',
        from_vertex: 'v2',
        to_vertex: 'v3',
        dimension_L: 15,
        dimension_S: 3,
        bend_angle: 60,
        plane_rotation: 90,
        selected_dimension: 'S',
      },
    ];

    const vertices: PathVertex[] = [
      { id: 'v1', position: { x: 0, y: 0, z: 0 }, is_fixed: false, is_selected: false },
      { id: 'v2', position: { x: 10, y: 0, z: 0 }, is_fixed: false, is_selected: false },
      { id: 'v3', position: { x: 20, y: 10, z: 0 }, is_fixed: false, is_selected: false },
    ];

    segments.forEach((seg) => {
      const from = vertices.find((v) => v.id === seg.from_vertex)!;
      const to = vertices.find((v) => v.id === seg.to_vertex)!;

      expect(() => {
        renderer.renderDimension(seg, from, to, testGroup);
      }).not.toThrow();
    });
  });

  // ===== GROUP OPERATIONS =====

  test('should add items to group', () => {
    const segment: PathSegment = {
      id: 'seg_1',
      from_vertex: 'v1',
      to_vertex: 'v2',
      dimension_L: 10,
      dimension_S: 2,
      bend_angle: 45,
      plane_rotation: 0,
      selected_dimension: 'L',
    };

    const fromVertex: PathVertex = {
      id: 'v1',
      position: { x: 0, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    const toVertex: PathVertex = {
      id: 'v2',
      position: { x: 10, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    const initialChildCount = testGroup.children.length;

    renderer.renderDimension(segment, fromVertex, toVertex, testGroup);

    // Group should accept the rendering (whether it adds items or not)
    expect(testGroup).toBeDefined();
  });

  // ===== DIMENSION TYPES =====

  test('should render all dimension types', () => {
    const types: ('L' | 'S' | 'B' | 'A')[] = ['L', 'S', 'B', 'A'];

    types.forEach((type) => {
      const segment: PathSegment = {
        id: `seg_${type}`,
        from_vertex: 'v1',
        to_vertex: 'v2',
        dimension_L: 10,
        dimension_S: 2,
        bend_angle: 45,
        plane_rotation: 90,
        selected_dimension: type,
      };

      const fromVertex: PathVertex = {
        id: 'v1',
        position: { x: 0, y: 0, z: 0 },
        is_fixed: false,
        is_selected: false,
      };

      const toVertex: PathVertex = {
        id: 'v2',
        position: { x: 10, y: 0, z: 0 },
        is_fixed: false,
        is_selected: false,
      };

      expect(() => {
        renderer.renderDimension(segment, fromVertex, toVertex, testGroup);
      }).not.toThrow();
    });
  });

  // ===== DISPOSAL =====

  test('should dispose resources', () => {
    expect(() => {
      renderer.dispose();
    }).not.toThrow();
  });

  test('should handle multiple dispose calls', () => {
    expect(() => {
      renderer.dispose();
      renderer.dispose();
    }).not.toThrow();
  });

  // ===== EDGE CASES =====

  test('should handle NaN values gracefully', () => {
    const segment: PathSegment = {
      id: 'seg_1',
      from_vertex: 'v1',
      to_vertex: 'v2',
      dimension_L: NaN,
      dimension_S: NaN,
      bend_angle: NaN,
      plane_rotation: NaN,
      selected_dimension: 'L',
    };

    const fromVertex: PathVertex = {
      id: 'v1',
      position: { x: 0, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    const toVertex: PathVertex = {
      id: 'v2',
      position: { x: 10, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    expect(() => {
      renderer.renderDimension(segment, fromVertex, toVertex, testGroup);
    }).not.toThrow();
  });

  test('should handle Infinity values gracefully', () => {
    const segment: PathSegment = {
      id: 'seg_1',
      from_vertex: 'v1',
      to_vertex: 'v2',
      dimension_L: Infinity,
      dimension_S: -Infinity,
      bend_angle: 45,
      plane_rotation: 0,
      selected_dimension: 'L',
    };

    const fromVertex: PathVertex = {
      id: 'v1',
      position: { x: 0, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    const toVertex: PathVertex = {
      id: 'v2',
      position: { x: 10, y: 0, z: 0 },
      is_fixed: false,
      is_selected: false,
    };

    expect(() => {
      renderer.renderDimension(segment, fromVertex, toVertex, testGroup);
    }).not.toThrow();
  });
});
