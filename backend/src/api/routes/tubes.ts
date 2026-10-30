/**
 * BendCore Tubes Routes
 * REST API endpoints for tube building and analysis
 */

import { Router, Request, Response } from 'express';
import { TubeBuilder, PathSegment, BendingProfile as TubeProfile } from '../../core/tube-builder';
import { TubeSpecification, ApiResponse, Vector3 } from '../../models/types';
import { ProfileManager } from '../../core/profile-manager';

const router = Router();
const tubeBuilder = new TubeBuilder();
const profileManager = ProfileManager.getInstance();

/**
 * POST /api/tubes
 * Create a new tube (validates required fields)
 */
router.post('/', (req: Request, res: Response): void => {
  const { name, specification } = req.body;
  const errors: string[] = [];

  if (!name || typeof name !== 'string') {
    errors.push('name is required');
  }

  if (specification) {
    if (typeof specification.diameter === 'number' && specification.diameter <= 0) {
      errors.push('diameter must be positive');
    }
    if (specification.profile_id === '') {
      errors.push('profile_id must not be empty');
    }
  }

  if (errors.length > 0) {
    res.status(400).json({ success: false, errors, timestamp: new Date().toISOString() });
    return;
  }

  const id = `tube_${Date.now()}`;
  res.status(201).json({
    success: true,
    data: {
      id,
      name,
      specification: specification || {},
      geometry: specification ? { cylinders: [], tori: [] } : undefined,
      properties: specification ? { total_volume: 0, total_mass: 0, density: 2.7 } : undefined,
      valid: true,
    },
    timestamp: new Date().toISOString(),
  });
});

/**
 * POST /api/tubes/build
 * Build a tube from path, profile, and specification
 */
router.post('/build', (req: Request, res: Response): void => {
  try {
    const {
      path,
      profile_id,
      specification
    } = req.body;

    // Validate input
    if (!path || !Array.isArray(path) || path.length === 0) {
      res.status(400).json({
        success: false,
        error: 'Path array required and must not be empty',
        timestamp: new Date().toISOString()
      });
      return;
    }

    if (!profile_id) {
      res.status(400).json({
        success: false,
        error: 'Profile ID required',
        timestamp: new Date().toISOString()
      });
      return;
    }

    // Get profile
    const profile = profileManager.getProfile(profile_id);
    if (!profile) {
      res.status(404).json({
        success: false,
        error: `Profile not found: ${profile_id}`,
        timestamp: new Date().toISOString()
      });
      return;
    }

    // Get diameter constraints
    const diameterSpec = profileManager.getDiameterConstraints(
      profile_id,
      specification?.diameter || 0.5
    );

    if (!diameterSpec) {
      res.status(400).json({
        success: false,
        error: 'Invalid diameter for profile',
        timestamp: new Date().toISOString()
      });
      return;
    }

    // Prepare path segments
    const pathSegments: PathSegment[] = path.map((p: Record<string, unknown>) => ({
      start: p.start as Vector3,
      end: p.end as Vector3,
      bend_angle: Number(p.bend_angle || 90),
      segment_id: String(p.segment_id || `seg_${Math.random()}`),
      is_straight: Boolean(p.is_straight || false)
    }));

    // Prepare profile
    const tubeProfile: TubeProfile = {
      profile_id: profile.id,
      name: profile.name,
      radius: diameterSpec.R,
      min_bend_angle: diameterSpec.B_min,
      max_bend_angle: diameterSpec.B_max,
      min_straight: diameterSpec.S_min,
      min_degenerate: 0.5
    };

    // Prepare spec
    const tubeSpec: TubeSpecification = {
      standard: specification?.standard || 'ANSI/ASME',
      material: specification?.material || 'steel',
      type: specification?.type || 'tube',
      description: specification?.description,
      profile_id: profile_id,
      diameter: diameterSpec.OD,
      wall_thickness: specification?.wall_thickness || 0.065,
      first_segment_allowance: specification?.first_segment_allowance || 0.5,
      last_segment_allowance: specification?.last_segment_allowance || 0.5,
      calculate_allowance: specification?.calculate_allowance !== false
    };

    // Build tube
    const tubeBody = tubeBuilder.buildTube(pathSegments, tubeProfile, tubeSpec);

    const response: ApiResponse<unknown> = {
      success: true,
      data: tubeBody,
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
 * POST /api/tubes/validate-spec
 * Validate tube specification
 */
router.post('/validate-spec', (req: Request, res: Response): void => {
  try {
    const spec: TubeSpecification = req.body;

    if (!spec.profile_id || typeof spec.diameter !== 'number') {
      res.status(400).json({
        success: false,
        error: 'Missing required fields',
        timestamp: new Date().toISOString()
      });
      return;
    }

    const result = profileManager.validateTubeSpec(spec);

    const response: ApiResponse<unknown> = {
      success: result.valid,
      data: {
        valid: result.valid,
        errors: result.errors,
        warnings: result.warnings
      },
      timestamp: new Date().toISOString()
    };

    res.status(result.valid ? 200 : 400).json(response);
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
 * POST /api/tubes/properties
 * Calculate tube properties (volume, mass, etc.)
 */
router.post('/properties', (req: Request, res: Response): void => {
  try {
    const { straight_sections, bend_sections, density } = req.body;

    if (!straight_sections || !Array.isArray(straight_sections)) {
      res.status(400).json({
        success: false,
        error: 'straight_sections array required',
        timestamp: new Date().toISOString()
      });
      return;
    }

    // Calculate volume
    let volume = 0;

    // Cylinder volumes
    for (const cyl of straight_sections) {
      const innerArea = Math.PI * cyl.inner_radius * cyl.inner_radius;
      const outerArea = Math.PI * cyl.outer_radius * cyl.outer_radius;
      volume += (outerArea - innerArea) * cyl.length;
    }

    // Torus volumes
    if (bend_sections && Array.isArray(bend_sections)) {
      for (const tor of bend_sections) {
        const sweepAngle = tor.end_angle - tor.start_angle;
        const minorRadius = tor.minor_radius;
        const majorRadius = tor.major_radius;
        volume +=
          2 *
          Math.PI *
          minorRadius *
          minorRadius *
          majorRadius *
          (sweepAngle / (2 * Math.PI));
      }
    }

    const totalMass = volume * (density || 2.7);

    const response: ApiResponse<unknown> = {
      success: true,
      data: {
        total_volume: volume,
        total_mass: totalMass,
        density: density || 2.7
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
