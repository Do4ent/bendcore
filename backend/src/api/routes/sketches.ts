/**
 * BendCore Sketches Routes
 * REST API endpoints for sketch solving and validation
 */

import { Router, Request, Response } from 'express';
import { SketchSolver } from '../../core/sketch-solver';
import { Vertex, Segment, ConstraintDef, ApiResponse } from '../../models/types';

const router = Router();

/**
 * POST /api/sketches/solve
 * Solve constraints using Newton-Raphson
 */
router.post('/solve', (req: Request, res: Response): void => {
  try {
    const {
      sketch_id,
      vertices,
      segments,
      constraints,
      profile_r,
      profile_s_min,
      profile_b_min,
      profile_b_max,
      max_iterations,
      tolerance
    } = req.body;

    // Validate input
    if (
      !sketch_id ||
      !vertices ||
      !Array.isArray(vertices) ||
      !segments ||
      !Array.isArray(segments) ||
      !constraints ||
      !Array.isArray(constraints)
    ) {
      res.status(400).json({
        success: false,
        error: 'Missing required fields: sketch_id, vertices, segments, constraints',
        timestamp: new Date().toISOString()
      });
      return;
    }

    // Initialize solver
    const solver = new SketchSolver();
    solver.initialize(
      sketch_id,
      vertices as Vertex[],
      segments as Segment[],
      constraints as ConstraintDef[],
      profile_r || 1.0,
      profile_s_min || 0.25,
      profile_b_min || 5,
      profile_b_max || 179
    );

    // Solve
    const result = solver.solve(max_iterations || 100, tolerance || 1e-6);

    const response: ApiResponse<unknown> = {
      success: result.converged,
      data: {
        converged: result.converged,
        iterations: result.iterations,
        residual: result.residual,
        conflicts: result.conflicts,
        over_constrained: result.over_constrained,
        vertices: result.vertices,
        constraints: result.constraints
      },
      timestamp: new Date().toISOString()
    };

    res.json(response);
  } catch (err) {
    const error = err instanceof Error ? err.message : 'Unknown error';
    res.status(500).json({
      success: false,
      error,
      timestamp: new Date().toISOString()
    });
  }
});

/**
 * POST /api/sketches/validate
 * Validate sketch path against profile constraints
 */
router.post('/validate', (req: Request, res: Response): void => {
  try {
    const {
      vertices,
      segments,
      constraints,
      profile_r,
      profile_s_min,
      profile_b_min,
      profile_b_max
    } = req.body;

    if (!vertices || !segments) {
      res.status(400).json({
        success: false,
        error: 'Missing required fields: vertices, segments',
        timestamp: new Date().toISOString()
      });
      return;
    }

    // Initialize solver
    const solver = new SketchSolver();
    solver.initialize(
      'validation-sketch',
      vertices as Vertex[],
      segments as Segment[],
      constraints as ConstraintDef[],
      profile_r || 1.0,
      profile_s_min || 0.25,
      profile_b_min || 5,
      profile_b_max || 179
    );

    // Validate
    const validation = solver.validatePath();

    const response: ApiResponse<unknown> = {
      success: validation.valid,
      data: {
        valid: validation.valid,
        errors: validation.errors,
        warnings: validation.warnings
      },
      timestamp: new Date().toISOString()
    };

    res.status(validation.valid ? 200 : 400).json(response);
  } catch (err) {
    const error = err instanceof Error ? err.message : 'Unknown error';
    res.status(500).json({
      success: false,
      error,
      timestamp: new Date().toISOString()
    });
  }
});

/**
 * POST /api/sketches/dimension-edit
 * Edit sketch dimension and re-solve
 */
router.post('/dimension-edit', (req: Request, res: Response): void => {
  try {
    const {
      sketch_id,
      vertices,
      segments,
      constraints,
      segment_id,
      new_length,
      profile_r,
      profile_s_min,
      profile_b_min,
      profile_b_max
    } = req.body;

    if (!sketch_id || !segment_id || typeof new_length !== 'number') {
      res.status(400).json({
        success: false,
        error: 'Missing required fields: sketch_id, segment_id, new_length',
        timestamp: new Date().toISOString()
      });
      return;
    }

    // Update segment dimension
    const updatedSegments = (segments as Segment[]).map(seg => {
      if (seg.id === segment_id) {
        return { ...seg, dimension_L: new_length };
      }
      return seg;
    });

    // Initialize solver with updated segments
    const solver = new SketchSolver();
    solver.initialize(
      sketch_id,
      vertices as Vertex[],
      updatedSegments,
      constraints as ConstraintDef[],
      profile_r || 1.0,
      profile_s_min || 0.25,
      profile_b_min || 5,
      profile_b_max || 179
    );

    // Solve
    const result = solver.solve(100, 1e-6);

    const response: ApiResponse<unknown> = {
      success: result.converged,
      data: {
        converged: result.converged,
        iterations: result.iterations,
        residual: result.residual,
        conflicts: result.conflicts,
        over_constrained: result.over_constrained,
        segments: updatedSegments,
        vertices: result.vertices,
        constraints: result.constraints
      },
      timestamp: new Date().toISOString()
    };

    res.json(response);
  } catch (err) {
    const error = err instanceof Error ? err.message : 'Unknown error';
    res.status(500).json({
      success: false,
      error,
      timestamp: new Date().toISOString()
    });
  }
});

export default router;
