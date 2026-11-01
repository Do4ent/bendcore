/**
 * Mesh Generation REST API Routes
 * Week 6 - 3D Triangle Mesh Generation from OCCT Shapes
 *
 * POST /api/mesh/generate - Generate triangle mesh from shape
 * GET /api/mesh/stats - Mesh generation statistics
 */

import { Router, Request, Response } from 'express';
import { MeshGenerator, GeometryKernel } from '../../native-binding/lib/index';

const router = Router();

/**
 * POST /api/mesh/generate
 * Generate triangle mesh from geometry
 *
 * Request:
 * {
 *   shapeType: "box" | "cylinder" | "tube" | "step_file"
 *   params: {
 *     // For box
 *     dx?: number, dy?: number, dz?: number
 *     // For cylinder
 *     radius?: number, height?: number
 *     // For tube
 *     od?: number, wall?: number, height?: number
 *     // For STEP
 *     filePath?: string
 *   }
 *   deflection?: number  // Mesh quality (default 0.1)
 * }
 *
 * Response:
 * {
 *   ok: true,
 *   mesh: {
 *     vertices: Float32Array (x,y,z triplets)
 *     indices: Uint32Array (triangle references)
 *     normals: Float32Array (vertex normals)
 *     vertexCount: number
 *     triangleCount: number
 *     boundingBox: [minX, minY, minZ, maxX, maxY, maxZ]
 *   }
 *   performance: {
 *     generationTime: number (ms)
 *     normalTime: number (ms)
 *     totalTime: number (ms)
 *   }
 * }
 */
router.post('/generate', (req: Request, res: Response) => {
  try {
    const {
      shapeType,
      params = {},
      deflection = 0.1,
    } = req.body;

    // Validate input
    if (!shapeType || typeof shapeType !== 'string') {
      return res.status(400).json({
        ok: false,
        error: 'shapeType is required',
      });
    }

    const startTime = Date.now();
    let shape: any;

    // Create shape based on type
    switch (shapeType.toLowerCase()) {
      case 'box':
        const { dx = 100, dy = 100, dz = 100 } = params;
        shape = GeometryKernel.makeBox(dx, dy, dz);
        break;

      case 'cylinder':
        const { radius = 50, height = 200 } = params;
        shape = GeometryKernel.makeCylinder(radius, height);
        break;

      case 'tube':
        const { od = 50, wall = 2.5, height: h = 200 } = params;
        shape = GeometryKernel.makeTube(od, wall, h);
        break;

      default:
        return res.status(400).json({
          ok: false,
          error: `Unknown shape type: ${shapeType}`,
        });
    }

    // Validate deflection
    const clampedDeflection = Math.max(0.001, Math.min(10.0, deflection));

    // Generate mesh
    const generationStart = Date.now();
    const mesh = MeshGenerator.generateMesh(shape, clampedDeflection);
    const generationTime = Date.now() - generationStart;

    const totalTime = Date.now() - startTime;

    res.json({
      ok: true,
      mesh: {
        vertices: mesh.vertices,
        indices: mesh.indices,
        normals: mesh.normals || [],
        vertexCount: mesh.vertexCount,
        triangleCount: mesh.triangleCount,
        boundingBox: mesh.boundingBox,
      },
      performance: {
        generationTime,
        normalTime: 0, // Included in generation
        totalTime,
        deflection: clampedDeflection,
      },
    });
  } catch (error) {
    res.status(500).json({
      ok: false,
      error: error instanceof Error ? error.message : 'Mesh generation failed',
    });
  }
});

/**
 * POST /api/mesh/from-step
 * Generate mesh from STEP file geometry
 *
 * Request:
 * {
 *   filePath: string
 *   deflection?: number
 * }
 */
router.post('/from-step', (req: Request, res: Response) => {
  try {
    const { filePath, deflection = 0.1 } = req.body;

    if (!filePath) {
      return res.status(400).json({
        ok: false,
        error: 'filePath is required',
      });
    }

    const startTime = Date.now();

    // When binding compiled: read STEP and generate mesh
    // For now: mock response
    const mockMesh = {
      vertices: new Float32Array([
        0, 0, 0, 100, 0, 0, 0, 100, 0,
        0, 0, 0, 0, 100, 0, 0, 0, 100,
      ]),
      indices: new Uint32Array([0, 1, 2, 3, 4, 5]),
      normals: new Float32Array([
        0, 0, 1, 0, 0, 1, 0, 0, 1,
        0, 0, 1, 0, 0, 1, 0, 0, 1,
      ]),
      vertexCount: 6,
      triangleCount: 2,
      boundingBox: [0, 0, 0, 100, 100, 100],
    };

    const totalTime = Date.now() - startTime;

    res.json({
      ok: true,
      mesh: mockMesh,
      performance: {
        readTime: totalTime / 2,
        generationTime: totalTime / 2,
        totalTime,
      },
      file: {
        path: filePath,
        format: 'STEP',
      },
    });
  } catch (error) {
    res.status(500).json({
      ok: false,
      error: error instanceof Error ? error.message : 'STEP mesh generation failed',
    });
  }
});

/**
 * GET /api/mesh/stats
 * Get mesh generation statistics and performance metrics
 */
router.get('/stats', (req: Request, res: Response) => {
  res.json({
    ok: true,
    stats: {
      defaultDeflection: 0.1,
      deflectionRange: [0.001, 10.0],
      supportedShapes: ['box', 'cylinder', 'tube'],
      performanceTargets: {
        box: '<5ms',
        cylinder: '<50ms',
        tube: '<100ms',
        assembly: '<500ms',
      },
      webglLimits: {
        maxVertices: 16777215, // 2^24 - 1 for 32-bit indices
        maxTriangles: 5242880, // ~16M vertices / 3
        recommendedTriangles: 500000,
      },
    },
  });
});

/**
 * POST /api/mesh/decimation
 * Reduce triangle count while preserving shape
 *
 * Request:
 * {
 *   mesh: { vertices, indices, normals }
 *   targetRatio: 0.5  // Keep 50% of triangles
 * }
 */
router.post('/decimation', (req: Request, res: Response) => {
  try {
    const { mesh, targetRatio = 0.5 } = req.body;

    if (!mesh || !mesh.vertices || !mesh.indices) {
      return res.status(400).json({
        ok: false,
        error: 'mesh with vertices and indices is required',
      });
    }

    // Validate ratio
    const ratio = Math.max(0.01, Math.min(1.0, targetRatio));

    // Mock decimation (real implementation would use mesh simplification algorithm)
    const targetTriangles = Math.floor(mesh.indices.length / 3 * ratio);
    const targetIndices = Math.floor(targetTriangles * 3);

    const decimatedMesh = {
      vertices: mesh.vertices.slice(0, Math.floor(mesh.vertices.length * ratio)),
      indices: mesh.indices.slice(0, targetIndices),
      normals: mesh.normals ? mesh.normals.slice(0, Math.floor(mesh.normals.length * ratio)) : [],
      vertexCount: Math.floor(mesh.vertices.length / 3 * ratio),
      triangleCount: targetTriangles,
      boundingBox: mesh.boundingBox,
    };

    res.json({
      ok: true,
      mesh: decimatedMesh,
      decimation: {
        originalTriangles: mesh.indices.length / 3,
        decimatedTriangles: targetTriangles,
        ratio: ratio,
        reduction: ((1 - ratio) * 100).toFixed(1) + '%',
      },
    });
  } catch (error) {
    res.status(500).json({
      ok: false,
      error: error instanceof Error ? error.message : 'Decimation failed',
    });
  }
});

/**
 * POST /api/mesh/validate
 * Validate mesh integrity
 *
 * Request:
 * {
 *   vertices: number[]
 *   indices: number[]
 * }
 *
 * Response:
 * {
 *   valid: boolean
 *   errors: string[]
 *   warnings: string[]
 * }
 */
router.post('/validate', (req: Request, res: Response) => {
  try {
    const { vertices, indices } = req.body;
    const errors: string[] = [];
    const warnings: string[] = [];

    // Validate vertices
    if (!vertices || vertices.length === 0) {
      errors.push('vertices array is empty');
    } else if (vertices.length % 3 !== 0) {
      errors.push('vertices length must be divisible by 3 (x,y,z triplets)');
    }

    // Validate indices
    if (!indices || indices.length === 0) {
      errors.push('indices array is empty');
    } else if (indices.length % 3 !== 0) {
      errors.push('indices length must be divisible by 3 (triangle triplets)');
    }

    // Check index validity
    if (vertices && indices) {
      const maxIndex = Math.max(...indices);
      const vertexCount = vertices.length / 3;

      if (maxIndex >= vertexCount) {
        errors.push(
          `index ${maxIndex} exceeds vertex count ${vertexCount}`
        );
      }

      // Check for isolated vertices
      const usedIndices = new Set(indices);
      if (usedIndices.size < vertexCount) {
        warnings.push(
          `${vertexCount - usedIndices.size} isolated vertices (not referenced by any triangle)`
        );
      }

      // Check for degenerate triangles
      let degenerateCount = 0;
      for (let i = 0; i < indices.length; i += 3) {
        const i1 = indices[i] * 3;
        const i2 = indices[i + 1] * 3;
        const i3 = indices[i + 2] * 3;

        const v1 = vertices.slice(i1, i1 + 3);
        const v2 = vertices.slice(i2, i2 + 3);
        const v3 = vertices.slice(i3, i3 + 3);

        const e1 = [v2[0] - v1[0], v2[1] - v1[1], v2[2] - v1[2]];
        const e2 = [v3[0] - v1[0], v3[1] - v1[1], v3[2] - v1[2]];

        // Cross product magnitude
        const cross = [
          e1[1] * e2[2] - e1[2] * e2[1],
          e1[2] * e2[0] - e1[0] * e2[2],
          e1[0] * e2[1] - e1[1] * e2[0],
        ];

        const area = Math.sqrt(cross[0] ** 2 + cross[1] ** 2 + cross[2] ** 2);
        if (area < 1e-10) {
          degenerateCount++;
        }
      }

      if (degenerateCount > 0) {
        warnings.push(`${degenerateCount} degenerate triangles (zero area)`);
      }
    }

    res.json({
      ok: true,
      valid: errors.length === 0,
      errors,
      warnings,
      stats: {
        vertexCount: vertices?.length ? vertices.length / 3 : 0,
        triangleCount: indices?.length ? indices.length / 3 : 0,
      },
    });
  } catch (error) {
    res.status(500).json({
      ok: false,
      error: error instanceof Error ? error.message : 'Validation failed',
    });
  }
});

export default router;
