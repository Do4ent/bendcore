/**
 * BendCore Import Routes
 * REST API endpoints for geometry import and analysis
 */

import { Router, Request, Response } from 'express';
import { ApiResponse } from '../../models/types';

const router = Router();

/**
 * POST /api/import/analyze
 * Analyze STEP/IGES file from bendcore_server, return tube data + viewport mesh
 */
router.post('/analyze', async (req: Request, res: Response): Promise<void> => {
  try {
    const { file_content, deflection } = req.body;

    if (!file_content) {
      res.status(400).json({
        success: false,
        error: 'file_content is required',
        timestamp: new Date().toISOString()
      });
      return;
    }

    // Forward to bendcore_server on localhost:8080
    const bendcorePort = process.env.BENDCORE_SERVER_PORT || '8080';
    const bendcoreUrl = `http://localhost:${bendcorePort}/v1/analyze?format=step&deflection=${deflection || 0.1}`;

    try {
      // Decode base64 from frontend to binary
      const binaryData = Buffer.from(file_content, 'base64');

      const analysisResponse = await fetch(bendcoreUrl, {
        method: 'POST',
        headers: { 'Content-Type': 'application/octet-stream' },
        body: binaryData
      });

      if (!analysisResponse.ok) {
        const errorText = await analysisResponse.text();
        res.status(analysisResponse.status).json({
          success: false,
          error: `bendcore_server error: ${errorText}`,
          timestamp: new Date().toISOString()
        });
        return;
      }

      const analysisData = (await analysisResponse.json()) as Record<string, any>;

      // Transform bendcore_server response to include ViewportMesh
      const transformedData = {
        ...analysisData,
        viewportMesh: (analysisData as any).mesh ? {
          id: `tube_${Date.now()}`,
          name: 'Imported Tube',
          mesh: (analysisData as any).mesh,
          color: 0x1f6fc5
        } : undefined
      };

      const response: ApiResponse<unknown> = {
        success: true,
        data: transformedData,
        timestamp: new Date().toISOString()
      };

      res.json(response);
    } catch (fetchErr) {
      const error = fetchErr instanceof Error ? fetchErr.message : 'Failed to connect to bendcore_server';
      res.status(503).json({
        success: false,
        error: `bendcore_server unavailable: ${error}`,
        timestamp: new Date().toISOString()
      });
    }
  } catch (err) {
    const error = err instanceof Error ? err.message : 'Unknown error';
    res.status(500).json({ success: false, error, timestamp: new Date().toISOString() });
  }
});

/**
 * POST /api/import
 * Combined import endpoint: filter segments, recognize geometry, match/suggest profiles, round
 */
router.post('/', (req: Request, res: Response): void => {
  try {
    const { segments, diameter, maxBendAngle, tolerance } = req.body;

    // Reject requests with no segments array (e.g. unsupported file types)
    if (segments === undefined || !Array.isArray(segments)) {
      res.status(400).json({ success: false, error: 'segments array is required', timestamp: new Date().toISOString() });
      return;
    }

    const segmentsArr: Array<{ start: number[]; end: number[] }> = segments;

    // Filter valid segments (non-degenerate)
    const valid_segments = segmentsArr.filter((seg) => {
      const s = seg.start;
      const e = seg.end;
      const dx = e[0] - s[0], dy = e[1] - s[1], dz = e[2] - s[2];
      return Math.sqrt(dx * dx + dy * dy + dz * dz) > 1e-9;
    });

    // Round segments if tolerance provided
    const rounded_segments = tolerance
      ? valid_segments.map((seg) => ({
          start: seg.start.map((v: number) => Math.round(v / tolerance) * tolerance),
          end: seg.end.map((v: number) => Math.round(v / tolerance) * tolerance),
        }))
      : valid_segments;

    // Recognize geometry type
    const recognized = valid_segments.length > 0 ? 'polyline' : 'empty';

    // Match profiles based on diameter
    const matching_profiles = diameter
      ? [{ id: 'default', name: 'Default Profile', diameter }]
      : [];

    // Suggest profiles based on diameter and maxBendAngle
    const suggested_profiles =
      diameter
        ? [{ id: 'suggested', name: 'Suggested Profile', diameter, maxBendAngle: maxBendAngle || 180 }]
        : [];

    const response: ApiResponse<unknown> = {
      success: true,
      data: {
        valid_segments,
        rounded_segments,
        recognized,
        matching_profiles,
        suggested_profiles,
      },
      timestamp: new Date().toISOString(),
    };

    res.json(response);
  } catch (err) {
    const error = err instanceof Error ? err.message : 'Unknown error';
    res.status(500).json({ success: false, error, timestamp: new Date().toISOString() });
  }
});

/**
 * POST /api/import/topology-optimize
 * Optimize tube geometry for minimal weight based on stress analysis
 */
router.post('/topology-optimize', async (req: Request, res: Response): Promise<void> => {
  try {
    const { stress, displacement, od, wall, length, material } = req.body;

    if (!stress || !od || !wall || !length) {
      res.status(400).json({
        success: false,
        error: 'stress, od, wall, and length are required',
        timestamp: new Date().toISOString()
      });
      return;
    }

    // Forward to bendcore_server on localhost:8080
    const bendcorePort = process.env.BENDCORE_SERVER_PORT || '8080';
    const bendcoreUrl = `http://localhost:${bendcorePort}/v1/topology?material=${encodeURIComponent(material || 'steel')}&stress=${stress}&displacement=${displacement || 0}&od=${od}&wall=${wall}&length=${length}&target=20`;

    try {
      const topoResponse = await fetch(bendcoreUrl, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({})
      });

      if (!topoResponse.ok) {
        const errorText = await topoResponse.text();
        res.status(topoResponse.status).json({
          success: false,
          error: `Topology optimization failed: ${errorText}`,
          timestamp: new Date().toISOString()
        });
        return;
      }

      const topoData = await topoResponse.json();

      const response: ApiResponse<unknown> = {
        success: true,
        data: topoData,
        timestamp: new Date().toISOString()
      };

      res.json(response);
    } catch (fetchErr) {
      const error = fetchErr instanceof Error ? fetchErr.message : 'Failed to connect to bendcore_server';
      res.status(503).json({
        success: false,
        error: `bendcore_server unavailable: ${error}`,
        timestamp: new Date().toISOString()
      });
    }
  } catch (err) {
    const error = err instanceof Error ? err.message : 'Unknown error';
    res.status(500).json({ success: false, error, timestamp: new Date().toISOString() });
  }
});

/**
 * POST /api/import/analyze-fea
 * Perform FEA analysis on mesh from tube analysis
 */
router.post('/analyze-fea', async (req: Request, res: Response): Promise<void> => {
  try {
    const { mesh, material, pressure } = req.body;

    if (!mesh) {
      res.status(400).json({
        success: false,
        error: 'mesh is required',
        timestamp: new Date().toISOString()
      });
      return;
    }

    // Forward to bendcore_server on localhost:8080
    const bendcorePort = process.env.BENDCORE_SERVER_PORT || '8080';
    const bendcoreUrl = `http://localhost:${bendcorePort}/v1/fea?material=${encodeURIComponent(material || 'steel')}&pressure=${pressure || 0}`;

    try {
      const feaResponse = await fetch(bendcoreUrl, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(mesh)
      });

      if (!feaResponse.ok) {
        const errorText = await feaResponse.text();
        res.status(feaResponse.status).json({
          success: false,
          error: `bendcore_server FEA failed: ${errorText}`,
          timestamp: new Date().toISOString()
        });
        return;
      }

      const feaData = await feaResponse.json();

      const response: ApiResponse<unknown> = {
        success: true,
        data: feaData,
        timestamp: new Date().toISOString()
      };

      res.json(response);
    } catch (fetchErr) {
      const error = fetchErr instanceof Error ? fetchErr.message : 'Failed to connect to bendcore_server';
      res.status(503).json({
        success: false,
        error: `bendcore_server unavailable: ${error}`,
        timestamp: new Date().toISOString()
      });
    }
  } catch (err) {
    const error = err instanceof Error ? err.message : 'Unknown error';
    res.status(500).json({ success: false, error, timestamp: new Date().toISOString() });
  }
});

/**
 * POST /api/import/export-drawing
 * Export 2D drawing (SVG) from tube analysis results
 */
router.post('/export-drawing', (req: Request, res: Response): void => {
  try {
    const { od, wall, length, bends } = req.body;

    if (!od || !wall || !length) {
      res.status(400).json({
        success: false,
        error: 'od, wall, and length are required',
        timestamp: new Date().toISOString()
      });
      return;
    }

    // Generate SVG drawing
    const svg = generateTubeDrawing({
      od: Number(od),
      wall: Number(wall),
      length: Number(length),
      bends: Array.isArray(bends) ? bends : []
    });

    res.setHeader('Content-Type', 'image/svg+xml');
    res.setHeader('Content-Disposition', 'attachment; filename="tube_drawing.svg"');
    res.send(svg);
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
 * Generate 2D SVG drawing of tube with bends
 */
function generateTubeDrawing(tube: {
  od: number;
  wall: number;
  length: number;
  bends: Array<{ angle: number; radius: number }>;
}): string {
  const scale = 0.5; // 0.5 pixels per mm
  const padding = 40;
  const width = tube.length * scale + padding * 2;
  const height = tube.od * scale + padding * 2;

  let svg = `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">
  <defs>
    <style>
      .tube-outline { fill: none; stroke: #1f6fc5; stroke-width: 2; }
      .tube-wall { fill: #e3f2fd; stroke: #1f6fc5; stroke-width: 1; }
      .dimension { font-size: 12px; fill: #333; }
      .dimension-line { stroke: #999; stroke-width: 0.5; }
      .center-line { stroke: #ccc; stroke-width: 0.5; stroke-dasharray: 2,2; }
    </style>
  </defs>

  <!-- Background -->
  <rect width="${width}" height="${height}" fill="white"/>

  <!-- Title -->
  <text x="${width / 2}" y="25" class="dimension" text-anchor="middle" font-weight="bold">
    Tube Drawing - OD ${tube.od}mm × Wall ${tube.wall}mm
  </text>

  <!-- Tube Centerline -->
  <line x1="${padding}" y1="${height / 2}" x2="${width - padding}" y2="${height / 2}" class="center-line"/>

  <!-- Outer Diameter -->
  <circle cx="${padding + 20}" cy="${height / 2}" r="${(tube.od / 2) * scale}" class="tube-outline"/>

  <!-- Inner Diameter -->
  <circle cx="${padding + 20}" cy="${height / 2}" r="${((tube.od / 2) - tube.wall) * scale}" class="tube-outline"/>

  <!-- Tube Body (simplified side view) -->
  <rect
    x="${padding}"
    y="${height / 2 - (tube.od / 2) * scale}"
    width="${tube.length * scale}"
    height="${tube.od * scale}"
    class="tube-wall"/>

  <!-- Wall Thickness Indicator -->
  <g class="dimension">
    <line x1="${padding - 5}" y1="${height / 2}" x2="${padding - 5}" y2="${height / 2 - (tube.od / 2) * scale}" class="dimension-line"/>
    <text x="${padding - 15}" y="${height / 2 - (tube.od / 4) * scale}" text-anchor="end">OD</text>

    <line x1="${padding + 5}" y1="${height / 2}" x2="${padding + 5}" y2="${height / 2 - ((tube.od / 2) - tube.wall) * scale}" class="dimension-line"/>
    <text x="${padding + 15}" y="${height / 2 - ((tube.od / 2) - tube.wall / 2) * scale}">Wall</text>
  </g>

  <!-- Length Dimension -->
  <line x1="${padding}" y1="${height - 10}" x2="${width - padding}" y2="${height - 10}" class="dimension-line"/>
  <text x="${width / 2}" y="${height - 2}" class="dimension" text-anchor="middle">
    Length: ${tube.length.toFixed(1)}mm
  </text>

  <!-- Bends Information -->
  <text x="${padding}" y="${height - 50}" class="dimension" font-weight="bold">Bends:</text>
  ${tube.bends
    .map(
      (bend, i) =>
        `<text x="${padding}" y="${height - 35 + i * 15}" class="dimension">
      ${i + 1}. ${bend.angle.toFixed(0)}° @ R${bend.radius.toFixed(1)}mm
    </text>`
    )
    .join('\n')}

  <!-- Legend -->
  <rect x="${width - 150}" y="${padding}" width="140" height="80" fill="#f9f9f9" stroke="#ddd" stroke-width="1"/>
  <text x="${width - 145}" y="${padding + 15}" class="dimension" font-weight="bold">Properties:</text>
  <text x="${width - 145}" y="${padding + 32}" class="dimension">OD: ${tube.od}mm</text>
  <text x="${width - 145}" y="${padding + 49}" class="dimension">Wall: ${tube.wall}mm</text>
  <text x="${width - 145}" y="${padding + 66}" class="dimension">ID: ${(tube.od - 2 * tube.wall).toFixed(1)}mm</text>
</svg>`;

  return svg;
}

export default router;
