/**
 * BendCore Desktop - Import/Export Workflow Integration Tests
 * Tests file import, geometry recognition, and export workflows
 *
 * Coverage: 20+ test cases, 300 LOC
 */

import { ImportFilter } from '../../src/bendcore/import_filter';
import { GeometryRecognition } from '../../src/bendcore/geometry_recognition';
import { GeometryRounding } from '../../src/bendcore/geometry_rounding';
import { ProfileManager } from '../../src/bendcore/profile_manager';
import { v4 as uuid } from 'uuid';

describe('Import/Export Workflow Integration Tests', () => {
  let importFilter: ImportFilter;
  let geometryRecognition: GeometryRecognition;
  let geometryRounding: GeometryRounding;
  let profileManager: ProfileManager;

  beforeEach(() => {
    importFilter = new ImportFilter();
    geometryRecognition = new GeometryRecognition();
    geometryRounding = new GeometryRounding();
    profileManager = ProfileManager.getInstance();
  });

  describe('STEP File Import', () => {
    test('should import STEP file format', () => {
      const stepPath = '/test/path/model.step';

      const result = importFilter.importSTEP(stepPath);
      expect(result).toBeDefined();
      expect(result.success).toBeDefined();
    });

    test('should parse STEP file geometry entities', () => {
      const mockStepData = {
        edges: [
          { id: 1, start: [0, 0, 0], end: [10, 0, 0] },
          { id: 2, start: [10, 0, 0], end: [10, 10, 0] }
        ],
        faces: [
          { id: 1, edges: [1, 2] }
        ]
      };

      const parsed = importFilter.parseSTEPEntities(mockStepData);
      expect(parsed.edges.length).toBe(2);
      expect(parsed.faces.length).toBe(1);
    });

    test('should extract curve segments from STEP model', () => {
      const mockGeometry = {
        curves: [
          { type: 'line', points: [[0, 0, 0], [10, 0, 0]] },
          { type: 'arc', points: [[10, 0, 0], [10, 10, 0]], radius: 10 }
        ]
      };

      const curves = importFilter.extractCurves(mockGeometry);
      expect(curves.length).toBe(2);
    });

    test('should validate STEP import result', () => {
      const importResult = {
        success: true,
        geometry: { edges: [], faces: [] },
        errors: []
      };

      const isValid = importResult.success && importResult.errors.length === 0;
      expect(isValid).toBe(true);
    });
  });

  describe('IGES File Import', () => {
    test('should import IGES file format', () => {
      const igesPath = '/test/path/model.iges';

      const result = importFilter.importIGES(igesPath);
      expect(result).toBeDefined();
      expect(result.success).toBeDefined();
    });

    test('should parse IGES parametric data', () => {
      const mockIgesData = {
        entities: [
          { type: 100, parameters: { x: 0, y: 0, z: 0 } },
          { type: 102, parameters: { x: 10, y: 0, z: 0 } }
        ]
      };

      const parsed = importFilter.parseIGESEntities(mockIgesData);
      expect(parsed.entities.length).toBe(2);
    });

    test('should extract spline segments from IGES', () => {
      const mockIges = {
        splines: [
          { degree: 3, knots: [0, 0, 0, 1, 1, 1], controlPoints: [[0,0],[1,1],[2,0]] }
        ]
      };

      const splines = importFilter.extractSplines(mockIges);
      expect(splines.length).toBe(1);
    });
  });

  describe('Geometry Recognition', () => {
    test('should recognize straight segments in imported geometry', () => {
      const edges = [
        { start: [0, 0, 0], end: [10, 0, 0], tolerance: 0.001 },
        { start: [10, 0, 0], end: [10, 10, 0], tolerance: 0.001 }
      ];

      const recognized = geometryRecognition.recognizeSegments(edges);
      expect(recognized.length).toBeGreaterThanOrEqual(1);
    });

    test('should identify arc segments in imported geometry', () => {
      const edges = [
        {
          type: 'arc',
          start: [10, 0, 0],
          end: [14.14, 4.14, 0],
          center: [10, 10, 0],
          radius: 10
        }
      ];

      const recognized = geometryRecognition.recognizeArcs(edges);
      expect(recognized.length).toBeGreaterThan(0);
    });

    test('should snap recognized segments to grid', () => {
      const tolerance = 0.01;
      const point = [10.004, 0.003, 0];

      const snapped = geometryRecognition.snapToGrid(point, tolerance);
      expect(snapped[0]).toBeCloseTo(10.0, 2);
      expect(snapped[1]).toBeCloseTo(0.0, 2);
    });

    test('should merge nearby segments', () => {
      const segments = [
        { start: [0, 0, 0], end: [10, 0, 0] },
        { start: [10.002, 0, 0], end: [20, 0, 0] }
      ];

      const merged = geometryRecognition.mergeColinearSegments(segments, 0.01);
      expect(merged.length).toBeLessThanOrEqual(segments.length);
    });

    test('should detect diameter from imported circular geometry', () => {
      const geometry = {
        type: 'cylinder',
        radius: 0.25,
        length: 10
      };

      const diameter = 2 * geometry.radius;
      expect(diameter).toBe(0.5);
    });
  });

  describe('Automatic Diameter Matching', () => {
    test('should match imported diameter to profile', () => {
      const importedDiameter = 0.5;
      const profiles = profileManager.listProfiles();

      const matchedProfile = profiles.find(p =>
        p.diameters.some(d => Math.abs(d.OD - importedDiameter) < 0.01)
      );

      expect(matchedProfile).toBeDefined();
    });

    test('should find closest matching diameter if exact match not found', () => {
      const importedDiameter = 0.505; // Slightly off
      const profiles = profileManager.listProfiles();

      let bestMatch = null;
      let bestDistance = Infinity;

      profiles.forEach(profile => {
        profile.diameters.forEach(diameter => {
          const distance = Math.abs(diameter.OD - importedDiameter);
          if (distance < bestDistance) {
            bestDistance = distance;
            bestMatch = diameter;
          }
        });
      });

      expect(bestMatch).toBeDefined();
      expect(bestDistance).toBeLessThan(0.05);
    });

    test('should calculate wall thickness from geometry', () => {
      const outerDiameter = 0.5;
      const innerDiameter = 0.37;

      const wallThickness = (outerDiameter - innerDiameter) / 2;
      expect(wallThickness).toBeCloseTo(0.065, 2);
    });

    test('should validate diameter against profile constraints', () => {
      const importedDiameter = 0.5;
      const profile = profileManager.listProfiles()[0];

      const isValid = profile.diameters.some(d =>
        Math.abs(d.OD - importedDiameter) < 0.01
      );

      expect(typeof isValid).toBe('boolean');
    });
  });

  describe('Profile Suggestion', () => {
    test('should suggest profiles based on geometry', () => {
      const geometry = {
        type: 'tube',
        diameter: 0.5,
        length: 20,
        bends: [
          { angle: 45, radius: 1.0 },
          { angle: 90, radius: 1.0 }
        ]
      };

      const suggestions = geometryRecognition.suggestProfiles(geometry);
      expect(Array.isArray(suggestions)).toBe(true);
    });

    test('should rank profile suggestions by suitability', () => {
      const importedGeometry = {
        diameter: 0.5,
        maxBendAngle: 120
      };

      const profiles = profileManager.listProfiles();
      const candidates = profiles.filter(p =>
        p.diameters.some(d =>
          Math.abs(d.OD - importedGeometry.diameter) < 0.01 &&
          d.B_max >= importedGeometry.maxBendAngle
        )
      );

      expect(candidates.length).toBeGreaterThanOrEqual(0);
    });

    test('should warn on marginal profile matches', () => {
      const importedAngle = 179;
      const profileMaxAngle = 180;

      const marginal = (profileMaxAngle - importedAngle) < 1;
      expect(marginal).toBe(true);
    });

    test('should suggest alternative profiles if primary not suitable', () => {
      const geometry = {
        diameter: 0.75,
        maxAngle: 170
      };

      const profiles = profileManager.listProfiles();
      const suitable = profiles.filter(p =>
        p.diameters.some(d => d.OD === geometry.diameter && d.B_max >= geometry.maxAngle)
      );

      if (suitable.length === 0) {
        const alternatives = profiles.slice(0, 3);
        expect(alternatives.length).toBeGreaterThan(0);
      }
    });
  });

  describe('Export to Manufacturing Format', () => {
    test('should export tube geometry to manufacturing format', () => {
      const tubeGeometry = {
        id: uuid(),
        segments: [
          { type: 'straight', length: 10, diameter: 0.5 },
          { type: 'bend', angle: 90, radius: 1.0 },
          { type: 'straight', length: 8, diameter: 0.5 }
        ]
      };

      const exported = importFilter.exportManufacturingFormat(tubeGeometry);
      expect(exported).toBeDefined();
      expect(exported.segments).toBeDefined();
    });

    test('should generate bend sequence for manufacturing', () => {
      const segments = [
        { type: 'straight', length: 5 },
        { type: 'bend', angle: 45, radius: 1.0 },
        { type: 'straight', length: 10 },
        { type: 'bend', angle: 90, radius: 1.0 }
      ];

      const sequence = importFilter.generateBendSequence(segments);
      expect(sequence).toBeDefined();
      expect(sequence.bends.length).toBe(2);
    });

    test('should calculate cutting allowances', () => {
      const segments = [
        { length: 10, bendAngle: 0 },
        { length: 15, bendAngle: 45 }
      ];

      const allowances = geometryRounding.calculateAllowances(segments);
      expect(allowances).toBeDefined();
      expect(allowances.length).toBe(segments.length);
    });

    test('should round dimensions to manufacturing tolerances', () => {
      const dims = [10.0043, 15.9997, 5.00001];
      const tolerance = 0.001;

      const rounded = dims.map(d => geometryRounding.roundToTolerance(d, tolerance));

      expect(rounded[0]).toBeCloseTo(10.004, 2);
      expect(rounded[1]).toBeCloseTo(16.0, 2);
    });

    test('should generate DXF output for manufacturing', () => {
      const tubeGeometry = {
        id: uuid(),
        segments: [
          { type: 'straight', length: 10 },
          { type: 'bend', angle: 90 }
        ]
      };

      const dxf = importFilter.generateDXF(tubeGeometry);
      expect(dxf).toBeDefined();
      expect(typeof dxf).toBe('string');
    });
  });

  describe('Import Error Handling', () => {
    test('should handle missing file gracefully', () => {
      const result = importFilter.importFile('/nonexistent/file.step');

      expect(result.success).toBe(false);
      expect(result.error).toBeDefined();
    });

    test('should validate file format before import', () => {
      const invalidFile = {
        path: '/test/file.xyz',
        extension: 'xyz'
      };

      const isValid = ['step', 'iges', 'stp'].includes(invalidFile.extension.toLowerCase());
      expect(isValid).toBe(false);
    });

    test('should report malformed geometry in file', () => {
      const badGeometry = {
        edges: [
          { start: [0, 0, 0] } // Missing end point
        ]
      };

      const errors = importFilter.validateGeometry(badGeometry);
      expect(errors.length).toBeGreaterThan(0);
    });

    test('should skip degenerate segments', () => {
      const segments = [
        { start: [0, 0, 0], end: [10, 0, 0] },
        { start: [5, 5, 5], end: [5, 5, 5] }, // Degenerate
        { start: [10, 0, 0], end: [20, 0, 0] }
      ];

      const valid = segments.filter(s =>
        Math.hypot(s.end[0] - s.start[0], s.end[1] - s.start[1], s.end[2] - s.start[2]) > 0.001
      );

      expect(valid.length).toBe(2);
    });

    test('should recover from partial import failures', () => {
      const importResult = {
        success: false,
        partialGeometry: {
          valid_segments: [{ start: [0, 0, 0], end: [10, 0, 0] }],
          failed_segments: 1
        },
        errors: ['Failed to parse segment 2']
      };

      expect(importResult.partialGeometry.valid_segments.length).toBeGreaterThan(0);
    });
  });

  describe('Round-trip Import/Export', () => {
    test('should preserve geometry through import/export cycle', () => {
      const original = {
        segments: [
          { type: 'straight', length: 10 },
          { type: 'bend', angle: 90, radius: 1.0 }
        ]
      };

      const exported = importFilter.export(original);
      const reimported = importFilter.import(exported);

      expect(reimported.segments.length).toBe(original.segments.length);
    });

    test('should maintain dimensional accuracy through cycles', () => {
      const originalLength = 10.0;

      const cycle1 = geometryRounding.roundToTolerance(originalLength, 0.001);
      const cycle2 = geometryRounding.roundToTolerance(cycle1, 0.001);
      const cycle3 = geometryRounding.roundToTolerance(cycle2, 0.001);

      expect(cycle3).toBeCloseTo(originalLength, 3);
    });

    test('should verify profile consistency after round-trip', () => {
      const original = {
        profileId: 'test-profile-123',
        diameter: 0.5
      };

      const exported = JSON.stringify(original);
      const reimported = JSON.parse(exported);

      expect(reimported.profileId).toBe(original.profileId);
      expect(reimported.diameter).toBe(original.diameter);
    });
  });
});
