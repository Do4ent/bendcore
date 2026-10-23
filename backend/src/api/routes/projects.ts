/**
 * BendCore Projects Routes
 * REST API endpoints for project management, CRUD, and persistence
 */

import { Router, Request, Response } from 'express';
import { Database } from '../../database/db';
import { ApiResponse } from '../../models/types';

const router = Router();

// Database instance (in production, this would be a singleton or dependency injection)
let db: Database | null = null;

/**
 * Initialize database for this route handler
 */
export function setDatabase(database: Database): void {
  db = database;
}

/**
 * POST /api/projects
 * Create a new project
 */
router.post('/', async (req: Request, res: Response): Promise<void> => {
  try {
    if (!db) {
      res.status(500).json({
        success: false,
        error: 'Database not initialized',
        timestamp: new Date().toISOString()
      });
      return;
    }

    const { name, description } = req.body;

    if (!name || typeof name !== 'string') {
      res.status(400).json({
        success: false,
        error: 'Project name required',
        timestamp: new Date().toISOString()
      });
      return;
    }

    const project = await db.createProject(name, description);

    const response: ApiResponse<unknown> = {
      success: true,
      data: project,
      timestamp: new Date().toISOString()
    };

    res.status(201).json(response);
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
 * GET /api/projects
 * List all projects
 */
router.get('/', async (req: Request, res: Response): Promise<void> => {
  try {
    if (!db) {
      res.status(500).json({
        success: false,
        error: 'Database not initialized',
        timestamp: new Date().toISOString()
      });
      return;
    }

    const projects = await db.listProjects();

    const response: ApiResponse<unknown> = {
      success: true,
      data: {
        count: projects.length,
        projects
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
 * GET /api/projects/:id
 * Get project details
 */
router.get('/:id', async (req: Request, res: Response): Promise<void> => {
  try {
    if (!db) {
      res.status(500).json({
        success: false,
        error: 'Database not initialized',
        timestamp: new Date().toISOString()
      });
      return;
    }

    const { id } = req.params;
    const project = await db.getProject(id);

    if (!project) {
      res.status(404).json({
        success: false,
        error: `Project not found: ${id}`,
        timestamp: new Date().toISOString()
      });
      return;
    }

    const response: ApiResponse<unknown> = {
      success: true,
      data: project,
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
 * PUT /api/projects/:id
 * Update project
 */
router.put('/:id', async (req: Request, res: Response): Promise<void> => {
  try {
    if (!db) {
      res.status(500).json({
        success: false,
        error: 'Database not initialized',
        timestamp: new Date().toISOString()
      });
      return;
    }

    const { id } = req.params;
    const { name, description } = req.body;

    const project = await db.updateProject(id, name, description);

    if (!project) {
      res.status(404).json({
        success: false,
        error: `Project not found: ${id}`,
        timestamp: new Date().toISOString()
      });
      return;
    }

    const response: ApiResponse<unknown> = {
      success: true,
      data: project,
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
 * DELETE /api/projects/:id
 * Delete project
 */
router.delete('/:id', async (req: Request, res: Response): Promise<void> => {
  try {
    if (!db) {
      res.status(500).json({
        success: false,
        error: 'Database not initialized',
        timestamp: new Date().toISOString()
      });
      return;
    }

    const { id } = req.params;
    const deleted = await db.deleteProject(id);

    if (!deleted) {
      res.status(404).json({
        success: false,
        error: `Project not found: ${id}`,
        timestamp: new Date().toISOString()
      });
      return;
    }

    const response: ApiResponse<unknown> = {
      success: true,
      data: { id, deleted: true },
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
 * POST /api/projects/:id/export
 * Export project as JSON
 */
router.post('/:id/export', async (req: Request, res: Response): Promise<void> => {
  try {
    if (!db) {
      res.status(500).json({
        success: false,
        error: 'Database not initialized',
        timestamp: new Date().toISOString()
      });
      return;
    }

    const { id } = req.params;
    const project = await db.getProject(id);

    if (!project) {
      res.status(404).json({
        success: false,
        error: `Project not found: ${id}`,
        timestamp: new Date().toISOString()
      });
      return;
    }

    const exportData = JSON.stringify(project, null, 2);

    res.setHeader('Content-Type', 'application/json');
    res.setHeader(
      'Content-Disposition',
      `attachment; filename="project_${id}.json"`
    );
    res.send(exportData);
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
 * POST /api/projects/import
 * Import project from JSON
 */
router.post('/import', async (req: Request, res: Response): Promise<void> => {
  try {
    if (!db) {
      res.status(500).json({
        success: false,
        error: 'Database not initialized',
        timestamp: new Date().toISOString()
      });
      return;
    }

    const { name, description, tubes } = req.body;

    if (!name) {
      res.status(400).json({
        success: false,
        error: 'Project name required',
        timestamp: new Date().toISOString()
      });
      return;
    }

    const project = await db.createProject(name, description);

    if (tubes && Array.isArray(tubes)) {
      for (const tube of tubes) {
        await db.createTube(project.id, tube.name, tube.specification);
      }
    }

    const updatedProject = await db.getProject(project.id);

    const response: ApiResponse<unknown> = {
      success: true,
      data: updatedProject,
      timestamp: new Date().toISOString()
    };

    res.status(201).json(response);
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
