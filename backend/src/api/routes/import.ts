/**
 * BendCore Import Routes
 * REST API endpoints for geometry import, filtering, and recognition
 */

import { Router, Request, Response } from 'express';
import { ImportFilter, ImportedTube } from '../../core/import-filter';
import { GeometryRecognizer } from '../../core/geometry-recognition';
import { GeometryRounder } from '../../core/geometry-rounding';
import { Vector3, ApiResponse } from '../../models/types';

const router = Router();

/**
 * POST /api/import/filter
 * Filter and group imported geometry
 */
router.post('/filter', (req: Request, res: Response): void => {
  try {
    const { tubes } = req.body;

    if (!tubes || !Array.isArray(tubes) || tubes.length === 0) {
      res.status(400).json({
        success: false,
        error: 'Tubes array required and must not be empty',
        timestamp: new Date().toISOString()
      });
      return;
    }

    // Validate tube structure
    const validTubes: ImportedTube[] = tubes.map((t: Record<string, unknown>) => ({
      id: String(t.id || `tube_${Math.random()}`),
      centerline: t.centerline as Vector3[],
      diameter: Number(t.diameter || 0.5),
      material: t.material ? String(t.material) : undefined,
      profile: t.profile ? String(t.profile) : undefined,
      confidence: Number(t.confidence || 1.0)
    }));

    // Filter geometry
    const groups = ImportFilter.filterGeometry(validTubes);

    const response: ApiResponse<unknown> = {
      success: true,
      data: {
        total_tubes: validTubes.length,
        group_count: groups.length,
        groups: groups.map(g => ({
          id: g.id,
          grouping_rule: g.grouping_rule,
          tube_count: g.tubes.length,
          contact_point_count: g.contact_points.length
        }))
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
 * POST /api/import/recognize
 * Recognize tube features from geometry
 */
router.post('/recognize', (req: Request, res: Response): void => {
  try {
    const { centerline } = req.body;

    if (!centerline || !Array.isArray(centerline) || centerline.length < 2) {
      res.status(400).json({
        success: false,
        error: 'Centerline array required with at least 2 points',
        timestamp: new Date().toISOString()
      });
      return;
    }

    // Recognize geometry
    const recognition = GeometryRecognizer.recognizeTube(centerline as Vector3[]);

    const validation = GeometryRecognizer.validateRecognition(recognition);

    const response: ApiResponse<unknown> = {
      success: validation.valid,
      data: {
        diameter: recognition.diameter,
        diameter_confidence: (recognition.diameter_confidence * 100).toFixed(1),
        bend_angles: recognition.bend_angles.map(a => a.toFixed(1)),
        angle_confidence: (recognition.angle_confidence * 100).toFixed(1),
        straight_segments: recognition.straight_segments.map(s => s.toFixed(3)),
        segment_confidence: (recognition.segment_confidence * 100).toFixed(1),
        total_confidence: (recognition.total_confidence * 100).toFixed(1),
        profile_suggestion: recognition.profile_suggestion,
        warnings: recognition.warnings,
        errors: validation.errors
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
 * POST /api/import/round
 * Round geometry values to standard sizes
 */
router.post('/round', (req: Request, res: Response): void => {
  try {
    const { diameter, angle, length, wall_thickness } = req.body;

    const response: ApiResponse<unknown> = {
      success: true,
      data: {
        original_diameter: diameter,
        rounded_diameter: diameter ? GeometryRounder.roundDiameter(diameter) : undefined,
        original_angle: angle,
        rounded_angle: angle ? GeometryRounder.roundAngle(angle) : undefined,
        original_length: length,
        rounded_length: length ? GeometryRounder.roundLength(length) : undefined,
        original_wall_thickness: wall_thickness,
        rounded_wall_thickness: wall_thickness
          ? GeometryRounder.roundWallThickness(wall_thickness)
          : undefined,
        standard_diameters: GeometryRounder.getStandardDiameters()
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
 * POST /api/import/smooth
 * Smooth centerline geometry
 */
router.post('/smooth', (req: Request, res: Response): void => {
  try {
    const { centerline, subdivisions } = req.body;

    if (!centerline || !Array.isArray(centerline) || centerline.length < 4) {
      res.status(400).json({
        success: false,
        error: 'Centerline array required with at least 4 points',
        timestamp: new Date().toISOString()
      });
      return;
    }

    const smoothed = GeometryRecognizer.smoothCenterline(
      centerline as Vector3[],
      subdivisions || 2
    );

    const response: ApiResponse<unknown> = {
      success: true,
      data: {
        original_point_count: centerline.length,
        smoothed_point_count: smoothed.length,
        centerline: smoothed
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
 * GET /api/import/standards
 * Get standard values for rounding
 */
router.get('/standards', (req: Request, res: Response): void => {
  try {
    const response: ApiResponse<unknown> = {
      success: true,
      data: {
        standard_diameters: GeometryRounder.getStandardDiameters(),
        wall_thicknesses: [0.020, 0.035, 0.049, 0.065, 0.083, 0.109, 0.120],
        angle_tolerance: 0.1,
        length_tolerance: 0.0394 // 1mm in inches
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
