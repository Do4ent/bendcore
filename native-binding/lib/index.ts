/**
 * BendCore OCCT Node.js Binding - TypeScript Interface
 * High-level TypeScript API for OCCT geometry kernel
 */

// Load native binding
let occtNative: any;
try {
  occtNative = require('../build/Release/occt_binding');
} catch (err) {
  console.warn('OCCT binding not compiled yet. Using mock implementation.');
  occtNative = {
    version: '0.1.0-alpha',
    occtVersion: '7.7.0',
    Shape: class Shape {
      constructor(type: string) {}
      getShapeType() { return 'SOLID'; }
      getVolume() { return 0; }
      getSurfaceArea() { return 0; }
      toJSON() { return {}; }
    },
    readStepFile: (path: string) => ({
      filePath: path,
      format: 'STEP',
      status: 'success',
      boundingBox: { minX: 0, minY: 0, minZ: 0, maxX: 100, maxY: 100, maxZ: 100 },
      topology: { faceCount: 6, edgeCount: 12, vertexCount: 8 },
      properties: { volume: 1000000, surfaceArea: 60000 }
    }),
    analyzeGeometry: (shape: any) => ({
      shapeType: 'SOLID',
      isValid: true,
      tubeInfo: {
        diameter: 50,
        wall: 2.5,
        length: 200,
        isTube: true,
        confidence: 0.95
      }
    }),
    kernel: {
      makeBox: (dx: number, dy: number, dz: number) => ({
        type: 'Box',
        dx, dy, dz,
        volume: dx * dy * dz
      }),
      makeCylinder: (radius: number, height: number) => ({
        type: 'Cylinder',
        radius,
        height,
        volume: Math.PI * radius * radius * height
      }),
      makeTube: (od: number, wall: number, height: number) => ({
        type: 'Tube',
        od,
        wall,
        id: od - 2 * wall,
        height,
        volume: Math.PI * ((od * od - (od - 2 * wall) * (od - 2 * wall)) / 4) * height
      }),
      checkIntersection: (shape1: any, shape2: any) => ({
        hasIntersection: false,
        distance: 5.0
      }),
      getBoundingBox: (shape: any) => ({
        minX: 0, minY: 0, minZ: 0,
        maxX: 100, maxY: 100, maxZ: 100
      })
    }
  };
}

// Shape class
export class Shape {
  private native: any;

  constructor(type?: string) {
    if (occtNative.Shape) {
      this.native = new occtNative.Shape(type || 'SOLID');
    }
  }

  getShapeType(): string {
    return this.native?.getShapeType?.() || 'SOLID';
  }

  getVolume(): number {
    return this.native?.getVolume?.() || 0;
  }

  getSurfaceArea(): number {
    return this.native?.getSurfaceArea?.() || 0;
  }

  toJSON(): any {
    return this.native?.toJSON?.() || {};
  }
}

// Geometry kernel
export class GeometryKernel {
  static makeBox(dx: number, dy: number, dz: number): any {
    return occtNative.kernel.makeBox(dx, dy, dz);
  }

  static makeCylinder(radius: number, height: number): any {
    return occtNative.kernel.makeCylinder(radius, height);
  }

  static makeTube(od: number, wall: number, height: number): any {
    return occtNative.kernel.makeTube(od, wall, height);
  }

  static checkIntersection(shape1: any, shape2: any): any {
    return occtNative.kernel.checkIntersection(shape1, shape2);
  }

  static getBoundingBox(shape: any): any {
    return occtNative.kernel.getBoundingBox(shape);
  }
}

// STEP file reader
export class StepReader {
  static readFile(filePath: string): any {
    return occtNative.readStepFile(filePath);
  }

  static analyzeGeometry(geometry: any): any {
    return occtNative.analyzeGeometry(geometry);
  }
}

// Tube detection
export interface TubeInfo {
  od: number;
  id: number;
  wall: number;
  length: number;
  isTube: boolean;
  confidence: number;
  bendAngles?: number[];
  bendRadii?: number[];
}

export class TubeDetector {
  /**
   * Detect if shape is a tube and extract properties
   */
  static detectTube(shape: any): TubeInfo | null {
    const bbox = GeometryKernel.getBoundingBox(shape);
    const volume = shape.getVolume?.();
    const analysis = StepReader.analyzeGeometry(shape);

    if (analysis.tubeInfo?.isTube) {
      return {
        od: analysis.tubeInfo.diameter,
        id: analysis.tubeInfo.diameter - 2 * analysis.tubeInfo.wall,
        wall: analysis.tubeInfo.wall,
        length: analysis.tubeInfo.length || (bbox.maxZ - bbox.minZ),
        isTube: true,
        confidence: analysis.tubeInfo.confidence,
        bendAngles: [],
        bendRadii: []
      };
    }

    return null;
  }

  /**
   * Detect all tubes in assembly
   */
  static detectAllTubes(geometry: any): TubeInfo[] {
    const tubes: TubeInfo[] = [];

    if (geometry.topology?.faceCount > 0) {
      const tube = this.detectTube(geometry);
      if (tube) {
        tubes.push(tube);
      }
    }

    return tubes;
  }
}

// Mesh generation
export interface Mesh {
  vertices: number[];
  indices: number[];
  normals: number[];
}

export class MeshGenerator {
  /**
   * Generate mesh from shape for 3D visualization
   */
  static generateMesh(shape: any, deflection: number = 0.1): Mesh {
    // This will be implemented when OCCT is integrated
    // For now, return mock mesh
    return {
      vertices: [
        0, 0, 0,
        100, 0, 0,
        100, 100, 0,
        0, 100, 0,
        0, 0, 100,
        100, 0, 100,
        100, 100, 100,
        0, 100, 100
      ],
      indices: [
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1,
        2, 6, 7, 2, 7, 3,
        0, 3, 7, 0, 7, 4,
        1, 5, 6, 1, 6, 2
      ],
      normals: Array(24).fill(0).map((_, i) => i % 3 === 0 ? 0.57735 : (i % 3 === 1 ? 0.57735 : 0.57735))
    };
  }
}

// Export native for advanced usage
export { occtNative };

// Version info
export const VERSION = {
  binding: occtNative.version,
  occt: occtNative.occtVersion,
  api: '0.1.0-alpha'
};

export default {
  Shape,
  GeometryKernel,
  StepReader,
  TubeDetector,
  MeshGenerator,
  VERSION
};
