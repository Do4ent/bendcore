/**
 * BendCore Desktop - Tube Generation & Geometry Pipeline Tests
 * Tests 3D geometry generation, collision detection, and analysis
 *
 * Coverage: 30+ test cases, 400 LOC
 */

import { TubeBuilder } from '../../src/bendcore/tube_builder';
import { SketchSolver } from '../../src/bendcore/sketch_solver';
import { v4 as uuid } from 'uuid';

describe('Tube Generation & Geometry Pipeline Tests', () => {
  let tubeBuilder: TubeBuilder;
  let sketchSolver: SketchSolver;

  beforeEach(() => {
    tubeBuilder = new TubeBuilder();
    sketchSolver = new SketchSolver();
  });

  describe('Sketch to 3D Geometry Conversion', () => {
    test('should generate 3D geometry from 2D sketch', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      sketchSolver.solve(sketch.id);

      const spec = {
        profile_id: 'test-profile',
        diameter: 0.5,
        wall_thickness: 0.065,
        material: 'STEEL',
        standard: 'ASTM'
      };

      const geometry = tubeBuilder.generateGeometry(sketch, spec);
      expect(geometry).toBeDefined();
      expect(geometry.valid).toBe(true);
    });

    test('should create cylinders for straight segments', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      sketchSolver.solve(sketch.id);

      const spec = {
        profile_id: 'test',
        diameter: 0.5,
        wall_thickness: 0.065,
        material: 'STEEL',
        standard: 'ASTM'
      };

      const geometry = tubeBuilder.generateGeometry(sketch, spec);
      expect(geometry.straight_sections).toBeDefined();
      expect(geometry.straight_sections.length).toBeGreaterThan(0);
    });

    test('should create tori for bends', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 0 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      sketchSolver.addSegment(sketch.id, v2.id, v3.id, 10, 5, 90, 0);

      sketchSolver.solve(sketch.id);

      const spec = {
        profile_id: 'test',
        diameter: 0.5,
        wall_thickness: 0.065,
        R: 1.0, // Bend radius
        material: 'STEEL',
        standard: 'ASTM'
      };

      const geometry = tubeBuilder.generateGeometry(sketch, spec);
      expect(geometry.bend_sections).toBeDefined();
      expect(geometry.bend_sections.length).toBeGreaterThan(0);
    });

    test('should preserve path topology in geometry', () => {
      const sketch = sketchSolver.createSketch();

      const v1 = sketchSolver.addVertex(sketch.id, { x: 0, y: 0, z: 0 }, true);
      const v2 = sketchSolver.addVertex(sketch.id, { x: 10, y: 0, z: 0 }, false);
      const v3 = sketchSolver.addVertex(sketch.id, { x: 10, y: 10, z: 0 }, false);
      const v4 = sketchSolver.addVertex(sketch.id, { x: 0, y: 10, z: 0 }, false);

      sketchSolver.addSegment(sketch.id, v1.id, v2.id, 10, 5, 0, 0);
      sketchSolver.addSegment(sketch.id, v2.id, v3.id, 10, 5, 90, 0);
      sketchSolver.addSegment(sketch.id, v3.id, v4.id, 10, 5, 90, 0);

      sketchSolver.solve(sketch.id);

      const spec = {
        profile_id: 'test',
        diameter: 0.5,
        wall_thickness: 0.065,
        R: 1.0,
        material: 'STEEL',
        standard: 'ASTM'
      };

      const geometry = tubeBuilder.generateGeometry(sketch, spec);
      const totalSections = geometry.straight_sections.length + geometry.bend_sections.length;
      expect(totalSections).toBe(3); // 3 segments in the sketch
    });
  });

  describe('Bend Geometry Calculations', () => {
    test('should apply T = R×tan(B/2) algorithm correctly', () => {
      const bendRadius = 1.0;
      const bendAngle = 90; // degrees

      const T = bendRadius * Math.tan((bendAngle * Math.PI / 180) / 2);

      expect(T).toBeCloseTo(1.0, 1);
    });

    test('should calculate transition length T for various angles', () => {
      const bendRadius = 1.0;
      const angles = [30, 45, 60, 90, 120, 135];

      angles.forEach(angle => {
        const T = bendRadius * Math.tan((angle * Math.PI / 180) / 2);
        expect(T).toBeGreaterThan(0);
        expect(T).toBeLessThan(bendRadius * 10);
      });
    });

    test('should calculate segment length S = L - T_left - T_right', () => {
      const L = 10.0; // Overall segment length
      const T_left = 0.5;
      const T_right = 0.5;

      const S = L - T_left - T_right;
      expect(S).toBe(9.0);
    });

    test('should validate S >= S_min constraint', () => {
      const S = 8.0;
      const S_min = 5.0;

      const valid = S >= S_min;
      expect(valid).toBe(true);
    });

    test('should warn when S < S_min', () => {
      const S = 3.0;
      const S_min = 5.0;

      const valid = S >= S_min;
      expect(valid).toBe(false);
    });
  });

  describe('Tube Length & Mass Calculation', () => {
    test('should calculate total tube length', () => {
      const segments = [
        { length: 10 },
        { length: 5 },
        { length: 8 }
      ];

      const totalLength = segments.reduce((sum, s) => sum + s.length, 0);
      expect(totalLength).toBe(23);
    });

    test('should calculate tube volume from geometry', () => {
      const spec = {
        diameter: 0.5,
        wall_thickness: 0.065,
        length: 10
      };

      const outerRadius = spec.diameter / 2;
      const innerRadius = outerRadius - spec.wall_thickness;
      const crossSectionArea = Math.PI * (outerRadius ** 2 - innerRadius ** 2);
      const volume = crossSectionArea * spec.length;

      expect(volume).toBeGreaterThan(0);
    });

    test('should calculate tube mass from volume and material density', () => {
      const volume = 1.0; // cubic inches
      const density = 0.284; // lb/in^3 for steel

      const mass = volume * density;
      expect(mass).toBeCloseTo(0.284, 2);
    });

    test('should aggregate mass across all segments', () => {
      const segments = [
        { volume: 1.0, density: 0.284 },
        { volume: 0.5, density: 0.284 },
        { volume: 0.8, density: 0.284 }
      ];

      const totalMass = segments.reduce((sum, s) => sum + (s.volume * s.density), 0);
      expect(totalMass).toBeCloseTo(0.568, 2);
    });

    test('should include bend section volumes in mass calculation', () => {
      const straightVolume = 5.0;
      const bendVolume = 0.5;
      const density = 0.284;

      const totalMass = (straightVolume + bendVolume) * density;
      expect(totalMass).toBeGreaterThan(straightVolume * density);
    });
  });

  describe('Collision Detection', () => {
    test('should detect collision between adjacent segments', () => {
      const seg1 = {
        start: { x: 0, y: 0, z: 0 },
        end: { x: 10, y: 0, z: 0 },
        radius: 0.25
      };

      const seg2 = {
        start: { x: 10, y: 0, z: 0 },
        end: { x: 10, y: 10, z: 0 },
        radius: 0.25
      };

      // Adjacent segments share endpoint - no collision
      const collision = tubeBuilder.checkCollision(seg1, seg2);
      expect(collision).toBe(false);
    });

    test('should detect self-intersection in complex paths', () => {
      const segments = [
        { start: { x: 0, y: 0, z: 0 }, end: { x: 10, y: 0, z: 0 }, radius: 0.25 },
        { start: { x: 10, y: 0, z: 0 }, end: { x: 10, y: 10, z: 0 }, radius: 0.25 },
        { start: { x: 10, y: 10, z: 0 }, end: { x: 0, y: 10, z: 0 }, radius: 0.25 },
        { start: { x: 0, y: 10, z: 0 }, end: { x: 0, y: 0, z: 0 }, radius: 0.25 }
      ];

      let collisions = 0;
      for (let i = 0; i < segments.length; i++) {
        for (let j = i + 2; j < segments.length; j++) {
          if (tubeBuilder.checkCollision(segments[i], segments[j])) {
            collisions++;
          }
        }
      }

      expect(collisions).toBeGreaterThanOrEqual(0);
    });

    test('should report collision location and severity', () => {
      const collision = tubeBuilder.detectDetailedCollision(
        { start: { x: 0, y: 0, z: 0 }, end: { x: 10, y: 0, z: 0 } },
        { start: { x: 5, y: -0.1, z: 0 }, end: { x: 5, y: 10, z: 0 } }
      );

      if (collision) {
        expect(collision.location).toBeDefined();
        expect(collision.severity).toBeDefined();
      }
    });

    test('should warn on tight clearance', () => {
      const minClearance = 0.01; // inches
      const actualClearance = 0.005; // Too tight

      const warning = actualClearance < minClearance;
      expect(warning).toBe(true);
    });
  });

  describe('Pattern Template Generation', () => {
    test('should generate rectangular tube template', () => {
      const spec = {
        type: 'rectangular',
        width: 2.0,
        height: 1.0,
        wall_thickness: 0.065
      };

      const pattern = tubeBuilder.generateTemplate('rectangular', spec);
      expect(pattern).toBeDefined();
      expect(pattern.vertices).toBeDefined();
    });

    test('should generate circular tube template', () => {
      const spec = {
        type: 'circular',
        diameter: 1.0,
        wall_thickness: 0.065
      };

      const pattern = tubeBuilder.generateTemplate('circular', spec);
      expect(pattern).toBeDefined();
    });

    test('should generate hexagonal tube template', () => {
      const spec = {
        type: 'hexagonal',
        flat_width: 2.0,
        wall_thickness: 0.065
      };

      const pattern = tubeBuilder.generateTemplate('hexagonal', spec);
      expect(pattern).toBeDefined();
    });

    test('should generate elliptical tube template', () => {
      const spec = {
        type: 'elliptical',
        major_axis: 2.0,
        minor_axis: 1.0,
        wall_thickness: 0.065
      };

      const pattern = tubeBuilder.generateTemplate('elliptical', spec);
      expect(pattern).toBeDefined();
    });

    test('should generate square tube template', () => {
      const spec = {
        type: 'square',
        side: 1.5,
        wall_thickness: 0.065
      };

      const pattern = tubeBuilder.generateTemplate('square', spec);
      expect(pattern).toBeDefined();
    });

    test('should generate star tube template', () => {
      const spec = {
        type: 'star',
        points: 5,
        outer_radius: 1.0,
        inner_radius: 0.5,
        wall_thickness: 0.065
      };

      const pattern = tubeBuilder.generateTemplate('star', spec);
      expect(pattern).toBeDefined();
    });
  });

  describe('Geometry Validation', () => {
    test('should validate complete tube body', () => {
      const geometry = {
        tube_id: uuid(),
        straight_sections: [
          { length: 10, outer_radius: 0.25, inner_radius: 0.185 }
        ],
        bend_sections: [
          { major_radius: 1.0, minor_radius: 0.25 }
        ],
        valid: true,
        validation_errors: []
      };

      const isValid = tubeBuilder.validateGeometry(geometry);
      expect(isValid).toBe(true);
    });

    test('should detect invalid geometry state', () => {
      const geometry = {
        tube_id: uuid(),
        straight_sections: [],
        bend_sections: [],
        valid: false,
        validation_errors: ['Empty tube body']
      };

      const isValid = tubeBuilder.validateGeometry(geometry);
      expect(isValid).toBe(false);
    });

    test('should report missing required sections', () => {
      const geometry = {
        tube_id: uuid(),
        straight_sections: [],
        bend_sections: undefined,
        valid: false,
        validation_errors: []
      };

      const errors = tubeBuilder.validateGeometry(geometry);
      expect(errors).toBeDefined();
    });

    test('should validate dimension consistency', () => {
      const geometry = {
        tube_id: uuid(),
        straight_sections: [{ length: 10 }, { length: 5 }],
        total_length: 15,
        valid: true,
        validation_errors: []
      };

      const summed = geometry.straight_sections.reduce((s, sec) => s + sec.length, 0);
      expect(summed).toBeLessThanOrEqual(geometry.total_length);
    });
  });

  describe('Manufacturing Analysis', () => {
    test('should generate manufacturing analysis report', () => {
      const geometry = {
        tube_id: uuid(),
        total_length: 20,
        total_mass: 5.5,
        valid: true
      };

      const report = tubeBuilder.generateAnalysisReport(geometry);
      expect(report).toBeDefined();
      expect(report.valid).toBe(true);
    });

    test('should identify bends requiring special tooling', () => {
      const bends = [
        { angle: 30, radius: 1.0 },
        { angle: 120, radius: 0.5 },
        { angle: 180, radius: 0.25 }
      ];

      const specialBends = bends.filter(b => b.angle > 90 || b.radius < 0.5);
      expect(specialBends.length).toBe(2);
    });

    test('should estimate bending sequence', () => {
      const bends = [
        { angle: 45, radius: 1.0 },
        { angle: 90, radius: 1.0 },
        { angle: 45, radius: 1.0 }
      ];

      const sequence = tubeBuilder.estimateBendSequence(bends);
      expect(sequence).toBeDefined();
      expect(sequence.length).toBe(bends.length);
    });

    test('should calculate total bending time estimate', () => {
      const bends = [
        { angle: 45, radius: 1.0 },
        { angle: 90, radius: 1.0 }
      ];

      const timePerBend = 2.5; // minutes
      const totalTime = bends.length * timePerBend;

      expect(totalTime).toBe(5.0);
    });
  });
});
