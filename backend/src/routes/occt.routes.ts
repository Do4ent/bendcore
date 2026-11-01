/**
 * OCCT REST API Routes
 * Endpoints for geometry operations using OCCT binding
 */

import { Router, Request, Response } from 'express';
import fs from 'fs';
import path from 'path';
import {
  GeometryKernel,
  StepReader,
  TubeDetector,
  MeshGenerator,
  Shape,
  Mesh,
  VERSION
} from '../../native-binding/lib/index';

const router = Router();

/**
 * GET /api/occt/version
 * Get OCCT binding version info
 */
router.get('/version', (req: Request, res: Response) => {
  res.json({
    status: 'ok',
    versions: VERSION,
    timestamp: new Date().toISOString()
  });
});

/**
 * POST /api/occt/step/read
 * Read STEP file and extract geometry
 *
 * Request:
 * - filePath: string - Path to STEP file
 *
 * Response:
 * - geometry info (bounding box, topology, properties)
 */
router.post('/step/read', (req: Request, res: Response) => {
  try {
    const { filePath } = req.body;

    if (!filePath) {
      return res.status(400).json({ error: 'filePath required' });
    }

    // Security: Validate path
    if (!fs.existsSync(filePath)) {
      return res.status(404).json({ error: 'File not found' });
    }

    const geometry = StepReader.readFile(filePath);

    res.json({
      status: 'ok',
      geometry,
      timestamp: new Date().toISOString()
    });
  } catch (err: any) {
    res.status(500).json({
      error: 'Failed to read STEP file',
      message: err.message
    });
  }
});

/**
 * POST /api/occt/geometry/analyze
 * Analyze geometry and detect tubes
 *
 * Request:
 * - filePath: string - Path to geometry file
 *
 * Response:
 * - analysis results (shape type, tubes detected, properties)
 */
router.post('/geometry/analyze', (req: Request, res: Response) => {
  try {
    const { filePath } = req.body;

    if (!filePath) {
      return res.status(400).json({ error: 'filePath required' });
    }

    const geometry = StepReader.readFile(filePath);
    const analysis = StepReader.analyzeGeometry(geometry);
    const tubes = TubeDetector.detectAllTubes(geometry);

    res.json({
      status: 'ok',
      analysis,
      tubes,
      tubeCount: tubes.length,
      timestamp: new Date().toISOString()
    });
  } catch (err: any) {
    res.status(500).json({
      error: 'Failed to analyze geometry',
      message: err.message
    });
  }
});

/**
 * POST /api/occt/mesh/generate
 * Generate 3D mesh from geometry file
 *
 * Request:
 * - filePath: string - Path to geometry file
 * - deflection: number (optional) - Mesh quality (default: 0.1)
 *
 * Response:
 * - mesh data (vertices, indices, normals)
 */
router.post('/mesh/generate', (req: Request, res: Response) => {
  try {
    const { filePath, deflection = 0.1 } = req.body;

    if (!filePath) {
      return res.status(400).json({ error: 'filePath required' });
    }

    const geometry = StepReader.readFile(filePath);
    const mesh = MeshGenerator.generateMesh(geometry, deflection);

    res.json({
      status: 'ok',
      mesh,
      stats: {
        vertexCount: mesh.vertices.length / 3,
        triangleCount: mesh.indices.length / 3,
        normalCount: mesh.normals.length / 3
      },
      timestamp: new Date().toISOString()
    });
  } catch (err: any) {
    res.status(500).json({
      error: 'Failed to generate mesh',
      message: err.message
    });
  }
});

/**
 * POST /api/occt/shape/create/box
 * Create a box primitive
 *
 * Request:
 * - dx: number
 * - dy: number
 * - dz: number
 *
 * Response:
 * - shape object with volume
 */
router.post('/shape/create/box', (req: Request, res: Response) => {
  try {
    const { dx, dy, dz } = req.body;

    if (typeof dx !== 'number' || typeof dy !== 'number' || typeof dz !== 'number') {
      return res.status(400).json({ error: 'dx, dy, dz must be numbers' });
    }

    const box = GeometryKernel.makeBox(dx, dy, dz);

    res.json({
      status: 'ok',
      shape: box,
      timestamp: new Date().toISOString()
    });
  } catch (err: any) {
    res.status(500).json({
      error: 'Failed to create box',
      message: err.message
    });
  }
});

/**
 * POST /api/occt/shape/create/cylinder
 * Create a cylinder primitive
 *
 * Request:
 * - radius: number
 * - height: number
 *
 * Response:
 * - shape object with volume
 */
router.post('/shape/create/cylinder', (req: Request, res: Response) => {
  try {
    const { radius, height } = req.body;

    if (typeof radius !== 'number' || typeof height !== 'number') {
      return res.status(400).json({ error: 'radius and height must be numbers' });
    }

    const cylinder = GeometryKernel.makeCylinder(radius, height);

    res.json({
      status: 'ok',
      shape: cylinder,
      timestamp: new Date().toISOString()
    });
  } catch (err: any) {
    res.status(500).json({
      error: 'Failed to create cylinder',
      message: err.message
    });
  }
});

/**
 * POST /api/occt/shape/create/tube
 * Create a tube (cylinder with hole)
 *
 * Request:
 * - od: number - Outer diameter
 * - wall: number - Wall thickness
 * - height: number
 *
 * Response:
 * - shape object with volume
 */
router.post('/shape/create/tube', (req: Request, res: Response) => {
  try {
    const { od, wall, height } = req.body;

    if (typeof od !== 'number' || typeof wall !== 'number' || typeof height !== 'number') {
      return res.status(400).json({ error: 'od, wall, and height must be numbers' });
    }

    const tube = GeometryKernel.makeTube(od, wall, height);

    res.json({
      status: 'ok',
      shape: tube,
      timestamp: new Date().toISOString()
    });
  } catch (err: any) {
    res.status(500).json({
      error: 'Failed to create tube',
      message: err.message
    });
  }
});

/**
 * POST /api/occt/geometry/detect-tubes
 * Auto-detect tubes in a file
 *
 * Request:
 * - filePath: string
 *
 * Response:
 * - list of detected tubes with properties
 */
router.post('/geometry/detect-tubes', (req: Request, res: Response) => {
  try {
    const { filePath } = req.body;

    if (!filePath) {
      return res.status(400).json({ error: 'filePath required' });
    }

    const geometry = StepReader.readFile(filePath);
    const tubes = TubeDetector.detectAllTubes(geometry);

    res.json({
      status: 'ok',
      tubes,
      detectedCount: tubes.length,
      details: tubes.map(tube => ({
        od: tube.od,
        id: tube.id,
        wall: tube.wall,
        length: tube.length,
        confidence: `${(tube.confidence * 100).toFixed(0)}%`
      })),
      timestamp: new Date().toISOString()
    });
  } catch (err: any) {
    res.status(500).json({
      error: 'Failed to detect tubes',
      message: err.message
    });
  }
});

/**
 * POST /api/occt/intersection/check
 * Check if two shapes intersect
 *
 * Request:
 * - shape1: object
 * - shape2: object
 *
 * Response:
 * - intersection result with distance
 */
router.post('/intersection/check', (req: Request, res: Response) => {
  try {
    const { shape1, shape2 } = req.body;

    if (!shape1 || !shape2) {
      return res.status(400).json({ error: 'shape1 and shape2 required' });
    }

    const result = GeometryKernel.checkIntersection(shape1, shape2);

    res.json({
      status: 'ok',
      intersection: result,
      hasIntersection: result.hasIntersection,
      distance: result.distance,
      timestamp: new Date().toISOString()
    });
  } catch (err: any) {
    res.status(500).json({
      error: 'Failed to check intersection',
      message: err.message
    });
  }
});

/**
 * GET /api/occt/health
 * Health check endpoint
 */
router.get('/health', (req: Request, res: Response) => {
  res.json({
    status: 'healthy',
    occt: {
      version: VERSION.occt,
      binding: VERSION.binding,
      available: true
    },
    timestamp: new Date().toISOString()
  });
});

export default router;
