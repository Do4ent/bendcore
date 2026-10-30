/**
 * BendCore Profile Manager Routes
 * REST API endpoints for profile management
 */

import { Router, Request, Response } from 'express';
import { ProfileManager } from '../../core/profile-manager';
import { TubeSpecification, ApiResponse } from '../../models/types';

const router = Router();
const profileManager = ProfileManager.getInstance();

/**
 * GET /api/profiles
 * List all available profiles
 */
router.get('/', (req: Request, res: Response): void => {
  try {
    const { category, manufacturer } = req.query;
    let profiles = profileManager.listProfiles();

    if (category) {
      profiles = profiles.filter(p => p.category === String(category));
    }
    if (manufacturer) {
      profiles = profiles.filter(p => p.manufacturer === String(manufacturer));
    }

    const data = profiles.map(p => ({
      id: p.id,
      name: p.name,
      category: p.category,
      manufacturer: p.manufacturer,
      type: p.type,
      description: p.description,
      diameters: p.diameters,
      diameter_count: p.diameters.length
    }));

    const response: ApiResponse<unknown> = {
      success: true,
      data,
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
 * GET /api/profiles/:id
 * Get profile details by ID
 */
router.get('/:id', (req: Request, res: Response): void => {
  try {
    const { id } = req.params;
    const profile = profileManager.getProfile(id);

    if (!profile) {
      res.status(404).json({
        success: false,
        error: `Profile not found: ${id}`,
        timestamp: new Date().toISOString()
      });
      return;
    }

    const response: ApiResponse<unknown> = {
      success: true,
      data: profile,
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
 * GET /api/profiles/:id/diameters
 * Get available diameters for a profile
 */
router.get('/:id/diameters', (req: Request, res: Response): void => {
  try {
    const { id } = req.params;
    const diameters = profileManager.getDiameters(id);

    if (diameters.length === 0 && !profileManager.profileExists(id)) {
      res.status(404).json({
        success: false,
        error: `Profile not found: ${id}`,
        timestamp: new Date().toISOString()
      });
      return;
    }

    const response: ApiResponse<unknown> = {
      success: true,
      data: {
        profile_id: id,
        count: diameters.length,
        diameters
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
 * GET /api/profiles/:profileId/diameters/:diameter
 * Get constraints for a specific diameter
 */
router.get('/:profileId/diameters/:diameter', (req: Request, res: Response): void => {
  try {
    const { profileId, diameter } = req.params;
    const diameterNum = parseFloat(diameter);

    if (isNaN(diameterNum)) {
      res.status(400).json({
        success: false,
        error: 'Invalid diameter value',
        timestamp: new Date().toISOString()
      });
      return;
    }

    const constraints = profileManager.getDiameterConstraints(profileId, diameterNum);

    if (!constraints) {
      res.status(404).json({
        success: false,
        error: `Diameter ${diameter}" not found for profile ${profileId}`,
        timestamp: new Date().toISOString()
      });
      return;
    }

    const response: ApiResponse<unknown> = {
      success: true,
      data: constraints,
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
 * POST /api/profiles
 * Create/validate a profile (stub - returns 400 for invalid data)
 */
router.post('/', (req: Request, res: Response): void => {
  const { id, name, category, diameters } = req.body || {};
  if (!id || !name || !category || !Array.isArray(diameters)) {
    res.status(400).json({
      success: false,
      error: 'Missing or invalid required fields: id, name, category, diameters (array)',
      timestamp: new Date().toISOString()
    });
    return;
  }
  res.status(201).json({ success: true, data: req.body, timestamp: new Date().toISOString() });
});

/**
 * POST /api/profiles/validate-spec
 * Validate a tube specification against profile constraints
 */
router.post('/validate-spec', (req: Request, res: Response): void => {
  try {
    const spec: TubeSpecification = req.body;

    // Validate request body
    if (!spec.profile_id || typeof spec.diameter !== 'number') {
      res.status(400).json({
        success: false,
        error: 'Missing required fields: profile_id, diameter',
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

export default router;
