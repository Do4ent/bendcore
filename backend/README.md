# BendCore Web Backend v1.0.0

Professional Node.js + TypeScript REST API for BendCore CAD - a comprehensive 3D tube bending design and manufacturing system.

## Overview

BendCore Backend translates the complete C++ logic from BendCore Desktop (Phases 35A-36) into a production-ready Express.js REST API with TypeScript for type safety and reliability.

### Key Features

- **Profile Management**: Load and validate bending profiles against component constraints
- **Constraint Solver**: Newton-Raphson solver for path sketch constraints (7 constraint types)
- **Tube Builder**: 3D geometry generation with hollow cylinders and torus sections
- **Geometry Operations**: AABB collision, distance calculations, bounding boxes
- **Import/Export**: Geometry filtering, recognition, and rounding to standard sizes
- **Project Persistence**: SQLite database for project storage and CRUD operations
- **Type-Safe**: Full TypeScript implementation with strict type checking

## Installation

### Prerequisites

- Node.js >= 16.0.0
- npm >= 8.0.0

### Setup

```bash
cd backend
npm install
npm run build
```

### Configuration

Create `.env` file:

```bash
cp .env.example .env
```

Environment variables:

```env
PORT=3001
NODE_ENV=development
DATABASE_URL=bendcore.db
CORS_ORIGIN=http://localhost:3000
LOG_LEVEL=debug
ENABLE_AUTH=false
```

## Running

### Development

```bash
npm run dev
```

Server starts on `http://localhost:3001`

### Production

```bash
npm run build
npm start
```

## API Documentation

### Health Check

```http
GET /api/health
```

Response:
```json
{
  "status": "ok",
  "timestamp": "2026-10-23T...",
  "version": "1.0.0",
  "environment": "development"
}
```

### Profiles

#### List Profiles
```http
GET /api/profiles
```

#### Get Profile
```http
GET /api/profiles/{profile_id}
```

#### Get Profile Diameters
```http
GET /api/profiles/{profile_id}/diameters
```

#### Get Diameter Constraints
```http
GET /api/profiles/{profile_id}/diameters/{diameter}
```

#### Validate Tube Specification
```http
POST /api/profiles/validate-spec
Content-Type: application/json

{
  "standard": "ANSI/ASME",
  "material": "steel-1020",
  "type": "tube",
  "profile_id": "manual-half-inch",
  "diameter": 0.5,
  "wall_thickness": 0.049,
  "first_segment_allowance": 0.5,
  "last_segment_allowance": 0.5,
  "calculate_allowance": true
}
```

### Tubes

#### Build Tube
```http
POST /api/tubes/build
Content-Type: application/json

{
  "path": [
    {
      "start": {"x": 0, "y": 0, "z": 0},
      "end": {"x": 10, "y": 0, "z": 0},
      "bend_angle": 0,
      "is_straight": true
    }
  ],
  "profile_id": "manual-half-inch",
  "specification": {
    "standard": "ANSI/ASME",
    "material": "steel",
    "diameter": 0.5,
    "wall_thickness": 0.049
  }
}
```

#### Validate Tube Specification
```http
POST /api/tubes/validate-spec
Content-Type: application/json

{
  "profile_id": "manual-half-inch",
  "diameter": 0.5,
  "wall_thickness": 0.049
}
```

#### Calculate Properties
```http
POST /api/tubes/properties
Content-Type: application/json

{
  "straight_sections": [...],
  "bend_sections": [...],
  "density": 2.7
}
```

### Sketches

#### Solve Constraints
```http
POST /api/sketches/solve
Content-Type: application/json

{
  "sketch_id": "sketch_1",
  "vertices": [...],
  "segments": [...],
  "constraints": [...],
  "profile_r": 1.0,
  "profile_s_min": 0.25,
  "profile_b_min": 5,
  "profile_b_max": 179
}
```

#### Validate Path
```http
POST /api/sketches/validate
Content-Type: application/json

{
  "vertices": [...],
  "segments": [...],
  "constraints": [...],
  "profile_r": 1.0,
  "profile_s_min": 0.25,
  "profile_b_min": 5,
  "profile_b_max": 179
}
```

#### Edit Dimension
```http
POST /api/sketches/dimension-edit
Content-Type: application/json

{
  "sketch_id": "sketch_1",
  "vertices": [...],
  "segments": [...],
  "constraints": [...],
  "segment_id": "seg_1",
  "new_length": 5.0
}
```

### Import/Export

#### Filter Geometry
```http
POST /api/import/filter
Content-Type: application/json

{
  "tubes": [
    {
      "id": "tube_1",
      "centerline": [...],
      "diameter": 0.5,
      "material": "steel",
      "confidence": 0.95
    }
  ]
}
```

#### Recognize Geometry
```http
POST /api/import/recognize
Content-Type: application/json

{
  "centerline": [
    {"x": 0, "y": 0, "z": 0},
    {"x": 10, "y": 0, "z": 0}
  ]
}
```

#### Round Geometry
```http
POST /api/import/round
Content-Type: application/json

{
  "diameter": 0.501,
  "angle": 45.1,
  "length": 10.2,
  "wall_thickness": 0.066
}
```

#### Get Standard Values
```http
GET /api/import/standards
```

#### Smooth Centerline
```http
POST /api/import/smooth
Content-Type: application/json

{
  "centerline": [...],
  "subdivisions": 2
}
```

### Projects

#### Create Project
```http
POST /api/projects
Content-Type: application/json

{
  "name": "My Project",
  "description": "Project description"
}
```

#### List Projects
```http
GET /api/projects
```

#### Get Project
```http
GET /api/projects/{project_id}
```

#### Update Project
```http
PUT /api/projects/{project_id}
Content-Type: application/json

{
  "name": "Updated Name",
  "description": "Updated description"
}
```

#### Delete Project
```http
DELETE /api/projects/{project_id}
```

#### Export Project
```http
POST /api/projects/{project_id}/export
```

Returns JSON export file.

#### Import Project
```http
POST /api/projects/import
Content-Type: application/json

{
  "name": "Imported Project",
  "description": "...",
  "tubes": [...]
}
```

## Testing

### Run Tests

```bash
npm test
```

### Run Tests with Coverage

```bash
npm run test:coverage
```

Test coverage target: **>85%** for all modules

### Test Files

- `tests/profile-manager.test.ts` - Profile loading and validation
- `tests/vector.test.ts` - 3D vector operations
- `tests/geometry-ops.test.ts` - Geometric calculations
- `tests/geometry-rounding.test.ts` - Standard size rounding

## Architecture

### Directory Structure

```
backend/
├── src/
│   ├── api/
│   │   ├── routes/           # Express route handlers
│   │   │   ├── profiles.ts
│   │   │   ├── tubes.ts
│   │   │   ├── sketches.ts
│   │   │   ├── import.ts
│   │   │   └── projects.ts
│   │   └── middleware/       # Express middleware
│   ├── core/                 # Core algorithms
│   │   ├── profile-manager.ts
│   │   ├── sketch-solver.ts
│   │   ├── tube-builder.ts
│   │   ├── geometry-ops.ts
│   │   ├── import-filter.ts
│   │   ├── geometry-recognition.ts
│   │   └── geometry-rounding.ts
│   ├── models/
│   │   └── types.ts          # TypeScript type definitions
│   ├── database/
│   │   └── db.ts             # SQLite database abstraction
│   ├── utils/
│   │   └── vector.ts         # Vector math utilities
│   └── index.ts              # Application entry point
├── tests/
│   ├── profile-manager.test.ts
│   ├── vector.test.ts
│   ├── geometry-ops.test.ts
│   └── geometry-rounding.test.ts
├── package.json
├── tsconfig.json
├── jest.config.js
└── README.md
```

### Core Modules

#### ProfileManager
Singleton pattern manages bending profiles loaded from `data/bending_profiles.json`. Validates tube specifications against profile constraints.

#### SketchSolver
Newton-Raphson constraint solver for 2D/3D path sketches. Supports:
- COINCIDENT, PARALLEL, PERPENDICULAR, EQUALITY, FIXED, DISTANCE, ANGLE constraints
- Convergence tolerance: 1e-6
- Max iterations: 100
- Conflict and over-constraint detection

#### TubeBuilder
Core geometry generation algorithm:
```
T = R × tan(B/2)        // Tangent offset
S = L - T_left - T_right // Straight segment
arc = R × B              // Arc length
```

Creates hollow cylinders for straight sections and hollow tori for bend sections.

#### GeometryOps
Geometric calculations:
- Bounding box (AABB) calculation
- Collision detection
- Volume and surface area
- Distance calculations
- Center of mass computation

#### ImportFilter
Groups imported tubes by:
- Diameter (tolerance: 0.05")
- Material
- Contact proximity (AABB collision)
- Isolated components

#### GeometryRecognizer
Extracts features from centerline geometry:
- Bend angle detection
- Straight segment extraction
- Diameter estimation
- Profile suggestion
- Centerline smoothing (Catmull-Rom)

#### GeometryRounder
Snaps geometry to standard values:
- 18 standard diameters (0.1875" - 3.0")
- 7 standard wall thicknesses
- Angle precision: 0.1°
- Length precision: 1mm

## Data Models

### Bending Profile
```typescript
{
  id: string;
  name: string;
  category: string;
  manufacturer: string;
  type: string;
  description: string;
  capacity_max_OD?: number;
  capacity_max_wall?: number;
  diameters: ProfileDiameter[];
}
```

### Tube Specification
```typescript
{
  standard: string;
  material: string;
  type: string;
  profile_id: string;
  diameter: number;
  wall_thickness: number;
  first_segment_allowance: number;
  last_segment_allowance: number;
  calculate_allowance: boolean;
}
```

### Project
```typescript
{
  id: string;
  name: string;
  description?: string;
  created_at: string;
  updated_at: string;
  tubes: Tube[];
  metadata?: Record<string, unknown>;
}
```

## Development

### Build

```bash
npm run build
```

Outputs TypeScript compiled JavaScript to `dist/` directory.

### Linting

```bash
npm run lint
```

Checks code with ESLint according to ``.eslintrc.json`.

### Code Format

```bash
npm run format
```

Formats code with Prettier.

## Dependencies

### Production
- **express**: Web framework
- **cors**: Cross-origin resource sharing
- **dotenv**: Environment variable management
- **uuid**: Unique ID generation
- **sqlite3**: Database driver

### Development
- **typescript**: Type-safe JavaScript
- **ts-node**: TypeScript execution for Node.js
- **jest**: Testing framework
- **ts-jest**: Jest support for TypeScript
- **eslint**: Code quality
- **prettier**: Code formatter

## Performance

- Query optimization with AABB collision detection
- Efficient constraint solver with numerical stability
- Memory-efficient geometry generation
- Connection pooling for database operations

## Security

- Type-safe TypeScript prevents runtime errors
- Input validation on all API endpoints
- Graceful error handling
- CORS configuration
- No SQL injection (parameterized queries)

## Deployment

### Docker

```dockerfile
FROM node:18-alpine
WORKDIR /app
COPY backend /app
RUN npm install
RUN npm run build
EXPOSE 3001
CMD ["npm", "start"]
```

### Environment Setup

Production `.env`:
```env
PORT=3001
NODE_ENV=production
DATABASE_URL=/var/lib/bendcore/bendcore.db
CORS_ORIGIN=https://bendcore.example.com
LOG_LEVEL=warn
ENABLE_AUTH=true
```

## Troubleshooting

### Failed to load profiles

Ensure `data/bending_profiles.json` exists relative to execution directory:
```bash
ls -la ../data/bending_profiles.json
```

### Port already in use

Change PORT in `.env`:
```env
PORT=3002
```

### Database errors

Ensure write permissions for database file:
```bash
chmod 755 bendcore.db
```

## License

MIT

## Author

Do4ent (BendCore Team)

## Support

For issues and feature requests, see [BendCore GitHub](https://github.com/Do4ent/bendcore)
