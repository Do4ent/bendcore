# BendCore User Guide

**Complete CAD Workflow for Tube Analysis, Drawing Export, and Optimization**

---

## 📋 Table of Contents
1. [Getting Started](#getting-started)
2. [System Architecture](#system-architecture)
3. [Complete Workflow](#complete-workflow)
4. [API Reference](#api-reference)
5. [Advanced Features](#advanced-features)

---

## Getting Started

### Prerequisites
- **C++ Compiler:** GCC 13+ (Linux)
- **OpenCASCADE:** 7.6.3 (system packages)
- **Node.js:** 16+ with npm
- **CMake:** 3.20+

### Installation

```bash
# 1. Clone repository
git clone https://github.com/Do4ent/bendcore.git
cd bendcore

# 2. Install system dependencies
sudo apt install libeigen3-dev libtbb-dev \
  libocct-data-exchange-dev libocct-modeling-algorithms-dev \
  ninja-build cmake nodejs npm

# 3. Build native C++ backend
cmake --preset host -DBENDCORE_WITH_OCCT=ON
cmake --build --preset host

# 4. Build Node.js backend
cd backend && npm install && npm run build

# 5. Install frontend dependencies
cd ../ui && npm install
```

### Quick Start

```bash
# Terminal 1: Start C++ analysis server
./build/bendcore_server 8080

# Terminal 2: Start Node.js backend (port 3001)
cd backend && npm start

# Terminal 3: Start React frontend (dev server)
cd ui && npm start
# Open http://localhost:3000 in browser
```

---

## System Architecture

```
┌─────────────────────────────────────────────────┐
│         React UI (Frontend)                     │
│    - ImportDialog (STEP/IGES upload)            │
│    - Viewport3D (3D mesh display)               │
│    - FEA analysis results                       │
│    - Topology optimization panel                │
└──────────────────┬──────────────────────────────┘
                   │
                   ↓ HTTP/JSON
┌─────────────────────────────────────────────────┐
│    Node.js Backend (Express.js, port 3001)     │
│                                                 │
│  POST /api/import/analyze                       │
│  POST /api/import/export-drawing                │
│  POST /api/import/analyze-fea                   │
│  POST /api/import/topology-optimize             │
└──────────────────┬──────────────────────────────┘
                   │
                   ↓ HTTP
┌─────────────────────────────────────────────────┐
│   bendcore_server (C++, port 8080)              │
│                                                 │
│  POST /v1/analyze?format=step&deflection=0.1   │
│  POST /v1/fea?material=steel&pressure=10       │
│  POST /v1/topology?material=steel&stress=...   │
│  GET  /v1/health                               │
└─────────────────────────────────────────────────┘
         ↓
    ┌─────────────┐
    │ OCCT Engine │ (Tube recognition, mesh gen)
    └─────────────┘
```

---

## Complete Workflow

### Step 1: Import STEP/IGES File
```
1. Click "📦 Import STEP/IGES" in UI
2. Select .step, .stp, .iges, or .igs file
3. Import dialog shows:
   - File info (name, size, format)
   - Analysis results:
     ├─ Outer Diameter (OD)
     ├─ Wall Thickness
     ├─ Bend Angles & Radii
     ├─ Total Length
     └─ Mesh statistics (vertices, triangles)
4. 3D mesh auto-displays in Viewport3D
```

### Step 2: Export 2D Drawing (SVG)
```
1. Click "📋 Export Drawing" button
2. SVG file downloads with:
   - Technical drawing (side/end views)
   - Dimensions (OD, wall, length)
   - Bend specifications
   - Material properties
   - Legend
```

### Step 3: Run FEA Analysis
```
1. Click "⚙️ Analyze (FEA)" button
2. bendcore_server calculates:
   - Max stress (MPa) based on internal pressure
   - Max displacement (mm)
   - Element and node counts
3. Results displayed in dialog:
   ┌──────────────────────────┐
   │ Max Stress: 50.3 MPa     │
   │ Max Disp:   0.125 mm     │
   │ Elements:   1445         │
   │ Nodes:      324          │
   └──────────────────────────┘
```

### Step 4: Optimize Geometry (Topology)
```
1. Click "🎯 Optimize" button (after FEA)
2. Optimization calculates:
   - Current wall thickness vs optimal
   - Weight reduction potential
   - Safety factor compliance
3. Results show:
   ┌─────────────────────────────┐
   │ Current Wall:  2.0 mm       │
   │ Optimal Wall:  1.65 mm      │
   │ Weight Save:   18.5%        │
   │ Recommendation: Reduce wall │
   └─────────────────────────────┘
```

---

## API Reference

### bendcore_server Endpoints

#### GET /v1/health
Health check endpoint.

**Response:**
```json
{"ok":true,"service":"bendcore"}
```

#### POST /v1/analyze?format=step&deflection=0.1
Analyze STEP/IGES file.

**Request Body:** Binary STEP/IGES file content

**Response:**
```json
{
  "ok": true,
  "tube": {
    "od": 20.0,
    "wall": 2.0,
    "length": 242.83,
    "bends": [{"angle": 90, "radius": 40}],
    "confidence": 0.95
  },
  "mesh": {
    "vertices": [0, 0, 0, ...],
    "indices": [0, 1, 2, ...]
  }
}
```

#### POST /v1/fea?material=steel&pressure=10
Perform FEA stress analysis.

**Request Body:** JSON mesh
```json
{
  "vertices": [...],
  "indices": [...]
}
```

**Response:**
```json
{
  "ok": true,
  "fea": {
    "max_stress": 50000000,
    "max_displacement": 0.125,
    "element_count": 1445,
    "converged": true
  }
}
```

#### POST /v1/topology?material=steel&stress=50000000
Optimize geometry for minimal weight.

**Parameters:**
- `material`: steel|aluminum|titanium
- `stress`: Current max stress (Pa)
- `od`: Outer diameter (mm)
- `wall`: Wall thickness (mm)
- `length`: Tube length (mm)

**Response:**
```json
{
  "ok": true,
  "topology": {
    "original_wall": 2.0,
    "optimal_wall": 1.65,
    "weight_reduction_percent": 18.5,
    "safety_factor": 2.5,
    "recommendation": "Wall thickness reduced"
  }
}
```

---

## Advanced Features

### Custom Materials
Supported materials with automatic property loading:

| Material | E (GPa) | ν | ρ (kg/m³) | Notes |
|----------|---------|---|----------|-------|
| Steel    | 200     | 0.30 | 7850 | High strength, standard |
| Aluminum | 70      | 0.33 | 2700 | Light, lower stiffness |
| Titanium | 103     | 0.34 | 4506 | High strength-to-weight |

### Mesh Generation
- Automatic mesh from STEP geometry
- Controlled deflection (0.01-1.0 mm)
- Triangle mesh format for 3D display
- Tetrahedron elements for FEA analysis

### Caching
- Geometry analysis cached by file path + size + mtime
- LRU eviction policy (100 MB limit)
- Faster re-import of unchanged files

### Batch Processing
Future API for multiple file analysis:
```bash
# Planned for next release
POST /api/import/batch
  [
    {"file": "tube1.step", "material": "steel"},
    {"file": "tube2.step", "material": "aluminum"}
  ]
```

---

## Keyboard Shortcuts (3D Viewport)

| Key | Action |
|-----|--------|
| Left Mouse | Rotate |
| Right Mouse | Pan |
| Scroll | Zoom |
| S | Toggle snap constraints |
| Delete | Remove selected object |
| Ctrl+A | Select all |

---

## Troubleshooting

### bendcore_server won't start
```bash
# Check if port 8080 is already in use
lsof -i :8080

# Use different port
./build/bendcore_server 9090
```

### STEP file not recognized
```bash
# File must be valid STEP/IGES format
# Supported: .step, .stp, .iges, .igs
file yourfile.step
```

### FEA results look wrong
- Check pressure parameter (typical: 1-50 MPa)
- Verify material selection matches actual material
- Run health check: `curl http://localhost:8080/v1/health`

### 3D mesh not displaying
- Ensure Viewport3D component loaded
- Check browser WebGL support
- Clear browser cache and reload

---

## Performance Notes

| Operation | Time | Notes |
|-----------|------|-------|
| STEP parse | <100ms | Depends on file size |
| Mesh gen | <200ms | With deflection=0.1 |
| FEA estimate | <50ms | Simplified solver |
| Topology opt | <20ms | Weight calculation |
| SVG export | <30ms | Drawing generation |

---

## Next Steps

1. **Full FEA Solver** - Linear elastic tetrahedron element solver
2. **WASM Build** - Run in browser without backend
3. **Parametric Design** - CAD sketcher integration
4. **Advanced Optimization** - Genetic algorithm topology optimization

---

## Support

- **Issues:** https://github.com/Do4ent/bendcore/issues
- **Documentation:** https://github.com/Do4ent/bendcore/docs
- **Build Guide:** See IMPLEMENTATION_STATUS.md

---

**Version:** 1.0.0 (Stages 1-3 Complete)  
**Last Updated:** 2026-11-01
