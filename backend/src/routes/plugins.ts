/**
 * BendCore Plugin Marketplace API
 * REST endpoints for plugin discovery, installation, and management
 */

import express, { Request, Response } from 'express';

const router = express.Router();

// ============================================================================
// Types & Interfaces
// ============================================================================

interface PluginListing {
  id: string;
  name: string;
  version: string;
  author: string;
  description: string;
  category: string;
  published: boolean;
  downloads: number;
  rating: number;
  ratingCount: number;
  latestVersion: string;
  tags: string[];
  homepage?: string;
  repository?: string;
  license?: string;
  createdAt: Date;
  updatedAt: Date;
}

interface UserPluginInstall {
  userId: string;
  pluginId: string;
  version: string;
  installedAt: Date;
  enabled: boolean;
}

// In-memory storage for demo (replace with actual database)
const pluginMarketplace: Map<string, PluginListing> = new Map();
const userInstalls: UserPluginInstall[] = [];

// ============================================================================
// Marketplace Endpoints
// ============================================================================

/**
 * GET /api/plugins/marketplace
 * List marketplace plugins with filtering and sorting
 */
router.get('/marketplace', (req: Request, res: Response) => {
  try {
    const { category, search, sort = 'downloads', page = '1', limit = '20' } = req.query;

    let plugins = Array.from(pluginMarketplace.values()).filter((p) => p.published);

    // Filter by category
    if (category && category !== 'all') {
      plugins = plugins.filter((p) => p.category === category);
    }

    // Search by name or description
    if (search) {
      const searchLower = String(search).toLowerCase();
      plugins = plugins.filter(
        (p) =>
          p.name.toLowerCase().includes(searchLower) ||
          p.description.toLowerCase().includes(searchLower) ||
          p.tags.some((t) => t.toLowerCase().includes(searchLower))
      );
    }

    // Sort
    switch (sort) {
      case 'downloads':
        plugins.sort((a, b) => b.downloads - a.downloads);
        break;
      case 'rating':
        plugins.sort((a, b) => b.rating - a.rating);
        break;
      case 'newest':
        plugins.sort((a, b) => b.createdAt.getTime() - a.createdAt.getTime());
        break;
      case 'name':
        plugins.sort((a, b) => a.name.localeCompare(b.name));
        break;
    }

    // Pagination
    const pageNum = Math.max(1, parseInt(String(page)));
    const limitNum = Math.min(100, Math.max(1, parseInt(String(limit))));
    const offset = (pageNum - 1) * limitNum;
    const paginatedPlugins = plugins.slice(offset, offset + limitNum);

    return res.json({
      plugins: paginatedPlugins,
      total: plugins.length,
      page: pageNum,
      limit: limitNum,
      hasMore: offset + limitNum < plugins.length,
    });
  } catch (error) {
    return res.status(500).json({ error: error instanceof Error ? error.message : 'Unknown error' });
  }
});

/**
 * GET /api/plugins/categories
 * List available plugin categories
 */
router.get('/categories', (req: Request, res: Response) => {
  const categories = [
    { id: 'export', name: 'Export & File Formats', icon: '📤' },
    { id: 'integration', name: 'Integration', icon: '🔗' },
    { id: 'simulation', name: 'Simulation & Analysis', icon: '📊' },
    { id: 'manufacturing', name: 'Manufacturing', icon: '🏭' },
    { id: 'utilities', name: 'Utilities', icon: '🔧' },
    { id: 'rendering', name: 'Rendering & Visualization', icon: '🎨' },
    { id: 'collaboration', name: 'Collaboration', icon: '👥' },
    { id: 'ai', name: 'AI & Generative', icon: '🤖' },
  ];

  return res.json(categories);
});

/**
 * GET /api/plugins/:pluginId
 * Get detailed plugin information
 */
router.get('/:pluginId', (req: Request, res: Response) => {
  try {
    const plugin = pluginMarketplace.get(req.params.pluginId);

    if (!plugin || !plugin.published) {
      return res.status(404).json({ error: 'Plugin not found' });
    }

    return res.json({
      ...plugin,
      // In a real implementation, would fetch versions from database
      versions: [
        { version: plugin.latestVersion, releaseDate: new Date(), downloads: plugin.downloads },
      ],
    });
  } catch (error) {
    return res.status(500).json({ error: error instanceof Error ? error.message : 'Unknown error' });
  }
});

/**
 * POST /api/plugins/install
 * Install plugin for current user
 */
router.post('/install', (req: Request, res: Response) => {
  try {
    const { pluginId, version } = req.body;

    if (!pluginId) {
      return res.status(400).json({ error: 'pluginId is required' });
    }

    const plugin = pluginMarketplace.get(pluginId);
    if (!plugin || !plugin.published) {
      return res.status(404).json({ error: 'Plugin not found' });
    }

    // Simulate getting user from auth
    const userId = req.headers['x-user-id'] as string || 'demo-user';

    // Check if already installed
    const existing = userInstalls.find(
      (i) => i.userId === userId && i.pluginId === pluginId
    );
    if (existing) {
      return res.status(400).json({ error: 'Plugin already installed' });
    }

    // Record installation
    const install: UserPluginInstall = {
      userId,
      pluginId,
      version: version || plugin.latestVersion,
      installedAt: new Date(),
      enabled: true,
    };
    userInstalls.push(install);

    // Increment download count
    plugin.downloads++;

    return res.status(201).json({
      success: true,
      plugin,
      install,
    });
  } catch (error) {
    return res.status(500).json({ error: error instanceof Error ? error.message : 'Unknown error' });
  }
});

/**
 * POST /api/plugins/uninstall
 * Uninstall plugin
 */
router.post('/uninstall', (req: Request, res: Response) => {
  try {
    const { pluginId } = req.body;
    const userId = req.headers['x-user-id'] as string || 'demo-user';

    const index = userInstalls.findIndex((i) => i.userId === userId && i.pluginId === pluginId);

    if (index === -1) {
      return res.status(404).json({ error: 'Plugin not installed' });
    }

    userInstalls.splice(index, 1);

    return res.json({ success: true });
  } catch (error) {
    return res.status(500).json({ error: error instanceof Error ? error.message : 'Unknown error' });
  }
});

/**
 * GET /api/plugins/installed
 * Get user's installed plugins
 */
router.get('/user/installed', (req: Request, res: Response) => {
  try {
    const userId = req.headers['x-user-id'] as string || 'demo-user';

    const userPluginIds = userInstalls
      .filter((i) => i.userId === userId && i.enabled)
      .map((i) => i.pluginId);

    const plugins = userPluginIds
      .map((id) => pluginMarketplace.get(id))
      .filter((p) => p !== undefined) as PluginListing[];

    return res.json({
      plugins,
      count: plugins.length,
    });
  } catch (error) {
    return res.status(500).json({ error: error instanceof Error ? error.message : 'Unknown error' });
  }
});

/**
 * POST /api/plugins/publish
 * Publish a new plugin (requires authentication)
 */
router.post('/publish', (req: Request, res: Response) => {
  try {
    const { name, description, category, code, manifest, tags, homepage, repository, license } =
      req.body;

    const userId = req.headers['x-user-id'] as string || 'demo-user';

    if (!name || !description || !category || !manifest) {
      return res.status(400).json({ error: 'Missing required fields' });
    }

    const pluginId = `bendcore.${name.toLowerCase().replace(/\s+/g, '-')}`;

    // Check if already exists
    if (pluginMarketplace.has(pluginId)) {
      return res.status(400).json({ error: 'Plugin ID already exists' });
    }

    const plugin: PluginListing = {
      id: pluginId,
      name,
      version: '1.0.0',
      author: userId,
      description,
      category,
      published: false, // Requires moderation
      downloads: 0,
      rating: 0,
      ratingCount: 0,
      latestVersion: '1.0.0',
      tags: tags || [],
      homepage,
      repository,
      license,
      createdAt: new Date(),
      updatedAt: new Date(),
    };

    pluginMarketplace.set(pluginId, plugin);

    return res.status(201).json({
      success: true,
      plugin,
      message: 'Plugin submitted for review. Moderation may take 1-2 business days.',
    });
  } catch (error) {
    return res.status(500).json({ error: error instanceof Error ? error.message : 'Unknown error' });
  }
});

/**
 * POST /api/plugins/:pluginId/rate
 * Rate a plugin
 */
router.post('/:pluginId/rate', (req: Request, res: Response) => {
  try {
    const { rating } = req.body;

    if (rating < 1 || rating > 5) {
      return res.status(400).json({ error: 'Rating must be between 1 and 5' });
    }

    const plugin = pluginMarketplace.get(req.params.pluginId);
    if (!plugin) {
      return res.status(404).json({ error: 'Plugin not found' });
    }

    // Simple average (in real implementation, would track individual ratings)
    const totalRating = plugin.rating * plugin.ratingCount + rating;
    plugin.ratingCount++;
    plugin.rating = totalRating / plugin.ratingCount;

    return res.json({
      success: true,
      plugin: {
        id: plugin.id,
        rating: plugin.rating,
        ratingCount: plugin.ratingCount,
      },
    });
  } catch (error) {
    return res.status(500).json({ error: error instanceof Error ? error.message : 'Unknown error' });
  }
});

// ============================================================================
// Admin Endpoints
// ============================================================================

/**
 * POST /api/plugins/admin/approve
 * Approve plugin for marketplace (admin only)
 */
router.post('/admin/approve', (req: Request, res: Response) => {
  try {
    const { pluginId } = req.body;

    const plugin = pluginMarketplace.get(pluginId);
    if (!plugin) {
      return res.status(404).json({ error: 'Plugin not found' });
    }

    plugin.published = true;
    plugin.updatedAt = new Date();

    return res.json({
      success: true,
      plugin,
    });
  } catch (error) {
    return res.status(500).json({ error: error instanceof Error ? error.message : 'Unknown error' });
  }
});

/**
 * POST /api/plugins/admin/reject
 * Reject plugin from marketplace (admin only)
 */
router.post('/admin/reject', (req: Request, res: Response) => {
  try {
    const { pluginId, reason } = req.body;

    const plugin = pluginMarketplace.get(pluginId);
    if (!plugin) {
      return res.status(404).json({ error: 'Plugin not found' });
    }

    // In real implementation, would notify user and potentially delete
    pluginMarketplace.delete(pluginId);

    return res.json({
      success: true,
      message: `Plugin rejected${reason ? ': ' + reason : ''}`,
    });
  } catch (error) {
    return res.status(500).json({ error: error instanceof Error ? error.message : 'Unknown error' });
  }
});

// ============================================================================
// Initialize with sample plugins
// ============================================================================

function initializeSamplePlugins(): void {
  const samplePlugins: PluginListing[] = [
    {
      id: 'bendcore.dxf-export',
      name: 'DXF Export',
      version: '1.2.0',
      author: 'BendCore Team',
      description: 'Export projects to AutoCAD DXF format for manufacturing',
      category: 'export',
      published: true,
      downloads: 2450,
      rating: 4.8,
      ratingCount: 156,
      latestVersion: '1.2.0',
      tags: ['export', 'dxf', 'autocad'],
      homepage: 'https://bendcore.io',
      repository: 'https://github.com/bendcore/dxf-export',
      license: 'MIT',
      createdAt: new Date('2026-01-15'),
      updatedAt: new Date('2026-10-20'),
    },
    {
      id: 'bendcore.step-converter',
      name: 'STEP/IGES Converter',
      version: '1.1.0',
      author: 'BendCore Team',
      description: 'Convert between STEP and IGES CAD formats',
      category: 'export',
      published: true,
      downloads: 1890,
      rating: 4.6,
      ratingCount: 89,
      latestVersion: '1.1.0',
      tags: ['export', 'step', 'iges', 'cad'],
      homepage: 'https://bendcore.io',
      license: 'MIT',
      createdAt: new Date('2026-02-10'),
      updatedAt: new Date('2026-10-15'),
    },
    {
      id: 'bendcore.erp-connector',
      name: 'ERP Integration',
      version: '2.0.0',
      author: 'BendCore Team',
      description: 'Seamless integration with SAP, Oracle, and NetSuite',
      category: 'integration',
      published: true,
      downloads: 1200,
      rating: 4.9,
      ratingCount: 67,
      latestVersion: '2.0.0',
      tags: ['erp', 'sap', 'integration', 'oracle'],
      homepage: 'https://bendcore.io/plugins/erp',
      repository: 'https://github.com/bendcore/erp-connector',
      license: 'Proprietary',
      createdAt: new Date('2026-03-20'),
      updatedAt: new Date('2026-10-18'),
    },
    {
      id: 'bendcore.3d-pdf',
      name: '3D PDF Generator',
      version: '1.5.0',
      author: 'BendCore Team',
      description: 'Generate interactive 3D PDF documents for sharing and documentation',
      category: 'export',
      published: true,
      downloads: 1650,
      rating: 4.7,
      ratingCount: 112,
      latestVersion: '1.5.0',
      tags: ['pdf', '3d', 'documentation', 'export'],
      homepage: 'https://bendcore.io/plugins/3dpdf',
      license: 'MIT',
      createdAt: new Date('2026-04-05'),
      updatedAt: new Date('2026-10-19'),
    },
    {
      id: 'bendcore.bom-generator',
      name: 'BOM Generator',
      version: '1.0.0',
      author: 'BendCore Team',
      description: 'Generate bill of materials in Excel, CSV, and PDF formats',
      category: 'manufacturing',
      published: true,
      downloads: 1320,
      rating: 4.5,
      ratingCount: 78,
      latestVersion: '1.0.0',
      tags: ['bom', 'manufacturing', 'excel', 'csv'],
      homepage: 'https://bendcore.io/plugins/bom',
      license: 'MIT',
      createdAt: new Date('2026-05-12'),
      updatedAt: new Date('2026-10-17'),
    },
  ];

  samplePlugins.forEach((plugin) => pluginMarketplace.set(plugin.id, plugin));
}

// Initialize on module load
initializeSamplePlugins();

export default router;
