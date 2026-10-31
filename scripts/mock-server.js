#!/usr/bin/env node
/**
 * Mock BendCore Server for UI Testing
 * Provides /v1/tube and /v1/analyze endpoints with realistic test data
 *
 * Usage: node scripts/mock-server.js
 * Then test ImportDialog against http://localhost:8080
 */

const http = require('http');
const url = require('url');

const PORT = 8080;

// Generate simple mesh for a tube
function generateTubeMesh(od, wall) {
  const radius = od / 2;
  const innerRadius = (od - 2 * wall) / 2;
  const length = 100;

  const vertices = [];
  const indices = [];

  // Simplified tube: cylinder rings
  const segments = 16;

  for (let i = 0; i <= 1; i++) {
    const z = i * length;

    // Outer ring
    for (let j = 0; j < segments; j++) {
      const angle = (j / segments) * Math.PI * 2;
      vertices.push(
        radius * Math.cos(angle), // x
        radius * Math.sin(angle), // y
        z                          // z
      );
    }
  }

  // Generate triangles for outer surface
  for (let i = 0; i < 1; i++) {
    for (let j = 0; j < segments; j++) {
      const v0 = i * segments + j;
      const v1 = i * segments + ((j + 1) % segments);
      const v2 = (i + 1) * segments + j;
      const v3 = (i + 1) * segments + ((j + 1) % segments);

      // Two triangles per quad
      indices.push(v0, v1, v2);
      indices.push(v1, v3, v2);
    }
  }

  return { vertices, indices };
}

// Handle /v1/tube endpoint
function handleTube(body) {
  const text = body.toString('utf-8');
  const params = new URLSearchParams(text);

  const od = parseFloat(params.get('od')) || 16;
  const wall = parseFloat(params.get('wall')) || 1;
  const format = params.get('format') || 'csv';

  const mesh = generateTubeMesh(od, wall);

  return JSON.stringify({
    ok: true,
    od,
    wall,
    volume: 1234.5,
    valid: true,
    axis_length: 100,
    ports: [
      {
        name: 'Port1',
        pos: [0, 0, 0],
        direction: [1, 0, 0],
        od,
      },
      {
        name: 'Port2',
        pos: [100, 0, 0],
        direction: [1, 0, 0],
        od,
      },
    ],
    mesh,
  });
}

// Handle /v1/analyze endpoint
function handleAnalyze(body) {
  // Simple mock: detect standard tubing sizes
  const mesh = generateTubeMesh(16, 1.5);

  return JSON.stringify({
    ok: true,
    tube: {
      od: 16.0,
      wall: 1.5,
      inner_diameter: 13.0,
      length: 145.3,
      bends: [
        { angle: 45, radius: 25 },
        { angle: 90, radius: 20 },
        { angle: 45, radius: 25 },
      ],
      shape_type: 'compound_tube',
      confidence: 85,
      notes: 'Mock analysis - not real OCCT processing',
      valid: true,
    },
    mesh,
  });
}

// HTTP server
const server = http.createServer((req, res) => {
  const urlObj = url.parse(req.url, true);
  const path = urlObj.pathname;

  console.log(`[${new Date().toISOString()}] ${req.method} ${path}`);

  // CORS
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Content-Type', 'application/json');

  if (req.method === 'OPTIONS') {
    res.writeHead(200);
    res.end();
    return;
  }

  // Health check
  if (path === '/v1/health') {
    res.writeHead(200);
    res.end(JSON.stringify({ ok: true }));
    return;
  }

  // Collect body
  let body = Buffer.alloc(0);
  req.on('data', (chunk) => {
    body = Buffer.concat([body, chunk]);
  });

  req.on('end', () => {
    try {
      let response;

      if (path === '/v1/tube') {
        response = handleTube(body);
      } else if (path === '/v1/analyze') {
        response = handleAnalyze(body);
      } else {
        res.writeHead(404);
        res.end(JSON.stringify({ ok: false, error: 'Not found' }));
        return;
      }

      res.writeHead(200);
      res.end(response);
    } catch (err) {
      console.error('Error:', err);
      res.writeHead(500);
      res.end(JSON.stringify({ ok: false, error: err.message }));
    }
  });
});

server.listen(PORT, () => {
  console.log(`🔧 Mock BendCore Server listening on http://localhost:${PORT}`);
  console.log(`📍 Available endpoints:`);
  console.log(`   GET  /v1/health`);
  console.log(`   POST /v1/tube?format=csv|json&od=16&wall=1`);
  console.log(`   POST /v1/analyze?format=step&deflection=0.1`);
  console.log(``);
  console.log(`💡 Use for UI testing when backend is unavailable`);
  console.log(`⚠️  Replace with real backend when Stage 2 complete`);
});

process.on('SIGINT', () => {
  console.log('\n⏹️  Mock server stopped');
  process.exit(0);
});
