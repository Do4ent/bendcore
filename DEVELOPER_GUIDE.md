# BendCore Developer Guide

**Version:** 1.0.0-beta.1  
**Last Updated:** November 1, 2026  
**Target Audience:** Developers contributing to Phase 2+  

---

## 📚 Table of Contents

1. [Getting Started](#getting-started)
2. [Project Structure](#project-structure)
3. [Development Setup](#development-setup)
4. [Architecture Overview](#architecture-overview)
5. [Code Standards](#code-standards)
6. [Testing Guide](#testing-guide)
7. [Troubleshooting](#troubleshooting)
8. [Contributing](#contributing)

---

## 🚀 Getting Started

### Prerequisites

- **Windows 10/11** x64
- **Node.js** 18.x or 20.x LTS
- **OCCT 7.7.0** (pre-compiled via vcpkg)
- **Visual Studio 2022** Community or Pro
- **CMake** 3.28+
- **Git** 2.40+

### Quick Setup

```bash
# 1. Clone repository
git clone https://github.com/Do4ent/bendcore.git
cd bendcore

# 2. Install dependencies
npm install

# 3. Compile OCCT (if not already done)
vcpkg install opencascade:x64-windows

# 4. Build Node.js binding
cd native-binding
npm run build
cd ..

# 5. Run tests
npm test

# 6. Start development
npm run dev      # Frontend on :3000
npm start        # Backend on :3001
```

---

## 📁 Project Structure

```
bendcore/
├── native-binding/          # C++ OCCT binding
│   ├── binding.gyp          # Build configuration
│   ├── src/
│   │   ├── addon.cc         # Module initialization
│   │   ├── shape_wrapper.cc # Shape class wrapper
│   │   ├── geometry_kernel.cc
│   │   ├── step_parser.cc
│   │   ├── mesh_generator.cc
│   │   └── shape_operations.cc
│   ├── lib/
│   │   └── index.ts         # TypeScript interface
│   ├── tests/
│   │   ├── occt.test.ts
│   │   ├── step-reader.test.ts
│   │   ├── mesh-generation.test.ts
│   │   ├── e2e-pipeline.test.ts
│   │   └── integration.test.ts
│   └── package.json
│
├── backend/                 # Express.js backend
│   ├── src/
│   │   ├── index.ts         # Server entry point
│   │   └── routes/
│   │       ├── occt.routes.ts
│   │       ├── mesh.routes.ts
│   │       └── ...
│   └── package.json
│
├── src/                     # React frontend
│   ├── ui/
│   │   ├── Viewport3D.tsx
│   │   ├── ImportDialog.tsx
│   │   ├── tubeService.ts
│   │   └── ...
│   ├── index.tsx
│   └── ...
│
├── docs/                    # Documentation
│   ├── WINDOWS_CI_CD_PIPELINE.md
│   ├── CMAKE_WINDOWS_GUIDE.md
│   ├── WEEK_5_STEP_PARSER_PLAN.md
│   ├── WEEK_6_MESH_GENERATION_PLAN.md
│   ├── WEEK_7_8_INTEGRATION_COMPLETION.md
│   ├── RELEASE_CHECKLIST_V1_0_BETA.md
│   ├── PHASE_2_ROADMAP.md
│   └── ...
│
├── package.json             # Root package
├── tsconfig.json            # TypeScript config
├── jest.config.js           # Jest test config
└── README.md                # Project README
```

---

## 🔧 Development Setup

### Environment Variables

Create `.env` file:

```bash
# Backend
NODE_ENV=development
OCCT_DEBUG=0
LOG_LEVEL=debug

# Frontend
REACT_APP_API_URL=http://localhost:3001
REACT_APP_DEBUG=true
```

### Build Commands

```bash
# Install all dependencies
npm install

# Build C++ binding
cd native-binding && npm run build && cd ..

# Build TypeScript
npm run build:backend

# Build React frontend
npm run build:frontend

# Production build (optimized)
npm run build:prod

# Clean all builds
npm run clean
```

### Development Commands

```bash
# Run backend only
npm start

# Run frontend dev server
npm run dev

# Run full stack
npm run dev:all

# Watch C++ changes
cd native-binding && npm run watch

# Watch backend changes
npm run watch:backend
```

---

## 🏗️ Architecture Overview

### Layer Stack

```
┌─────────────────────────────────────┐
│      React Frontend (3000)          │
│  - ImportDialog, Viewport3D         │
│  - WebGL rendering (Three.js)       │
└─────────────────────────────────────┘
              ↓ HTTP
┌─────────────────────────────────────┐
│     Express.js Backend (3001)       │
│  - REST API endpoints (/api/*)      │
│  - Request validation & routing     │
└─────────────────────────────────────┘
              ↓ Function calls
┌─────────────────────────────────────┐
│   TypeScript OCCT Interface         │
│  - Shape, GeometryKernel, etc.      │
│  - Mock fallbacks for development   │
└─────────────────────────────────────┘
              ↓ NAPI
┌─────────────────────────────────────┐
│  C++ Node.js Binding (native-binding)
│  - addon.cc, shape_wrapper.cc       │
│  - OCCT wrapper functions           │
└─────────────────────────────────────┘
              ↓ Direct calls
┌─────────────────────────────────────┐
│    OCCT 7.7.0 C++ Kernel            │
│  - Geometry operations              │
│  - Mesh generation                  │
│  - CAD file parsing                 │
└─────────────────────────────────────┘
```

### Data Flow

```
User imports STEP file
    ↓
Frontend sends file to /api/occt/step/read
    ↓
Backend calls StepReader.readFile()
    ↓
TypeScript interface calls C++ binding
    ↓
NAPI invokes STEPCAFControl_Reader
    ↓
OCCT reads STEP and returns geometry
    ↓
Backend calls TubeDetector and MeshGenerator
    ↓
Results converted to JSON and sent to frontend
    ↓
Frontend renders 3D mesh using Three.js
    ↓
User sees 3D model in Viewport3D
```

---

## 📝 Code Standards

### TypeScript

```typescript
// File: native-binding/lib/index.ts

// ✅ DO: Type everything strictly
export class Shape {
  constructor(type: string) { }
  getVolume(): number { }
  toJSON(): Record<string, unknown> { }
}

// ❌ DON'T: Use 'any'
export class Shape {
  getVolume(): any { }  // Bad!
}

// ✅ DO: Export all public types
export interface MeshData {
  vertices: number[];
  indices: number[];
  normals: number[];
}

// ✅ DO: Document with JSDoc
/**
 * Generate triangle mesh from shape
 * @param shape - OCCT shape to mesh
 * @param deflection - Mesh quality (0.001-10.0)
 * @returns Triangle mesh with normals
 */
export function generateMesh(shape: Shape, deflection?: number): MeshData
```

### C++

```cpp
// File: native-binding/src/addon.cc

// ✅ DO: Use descriptive names
Napi::Value MakeBox(const Napi::CallbackInfo& info);
Napi::Value GetVolume(const Napi::CallbackInfo& info);

// ✅ DO: Validate inputs
if (info.Length() < 3) {
  throw Napi::TypeError::New(env, "Expected 3 arguments");
}

// ✅ DO: Handle OCCT exceptions
try {
  BRepPrimAPI_MakeBox boxMaker(dx, dy, dz);
  return boxMaker.Shape();
} catch (Standard_Failure& e) {
  throw Napi::Error::New(env, std::string(e.GetMessageString()));
}

// ❌ DON'T: Ignore errors
BRepPrimAPI_MakeBox boxMaker(dx, dy, dz);  // Silent failure!
```

### REST API

```typescript
// ✅ DO: Validate and document
router.post('/generate', (req, res) => {
  const { shapeType, params, deflection } = req.body;
  
  // Validate
  if (!shapeType) {
    return res.status(400).json({
      ok: false,
      error: 'shapeType is required'
    });
  }
  
  // Process
  const mesh = MeshGenerator.generateMesh(shape);
  
  // Return consistent format
  res.json({
    ok: true,
    mesh: { ... },
    performance: { ... }
  });
});
```

### Testing

```typescript
// ✅ DO: Test behavior, not implementation
test('should calculate box volume correctly', () => {
  const box = GeometryKernel.makeBox(10, 20, 30);
  expect(box.getVolume()).toBe(6000);
});

// ✅ DO: Test error cases
test('should handle invalid input', () => {
  expect(() => {
    GeometryKernel.makeBox(-1, 0, 0);
  }).toThrow();
});

// ❌ DON'T: Test implementation details
test('should call BRepPrimAPI_MakeBox', () => {
  // Don't test that OCCT was called
});
```

---

## 🧪 Testing Guide

### Running Tests

```bash
# All tests
npm test

# Specific suite
npm test -- shape.test.ts

# With coverage
npm test -- --coverage

# Watch mode
npm test -- --watch
```

### Writing Tests

```typescript
describe('GeometryKernel - Box Creation', () => {
  test('should create box with correct dimensions', () => {
    const box = GeometryKernel.makeBox(100, 200, 300);
    
    expect(box).toBeDefined();
    expect(box.getVolume()).toBe(6000000);
    expect(box.getShapeType()).toBe('SOLID');
  });

  test('should handle edge case: zero volume', () => {
    const box = GeometryKernel.makeBox(0, 0, 0);
    expect(box.getVolume()).toBe(0);
  });

  test('should handle edge case: very large dimensions', () => {
    const box = GeometryKernel.makeBox(1e6, 1e6, 1e6);
    expect(box.getVolume()).toBeGreaterThan(0);
  });
});
```

---

## 🐛 Troubleshooting

### Build Issues

#### Issue: "OCCT not found"

**Solution:**
```bash
# Install OCCT via vcpkg
vcpkg install opencascade:x64-windows

# Verify installation
vcpkg list | grep opencascade
```

#### Issue: "npm run build fails in native-binding"

**Solution:**
```bash
# Clean and rebuild
cd native-binding
npm run clean
npm install
npm run build

# Check for errors
npm run build 2>&1 | tail -50
```

### Runtime Issues

#### Issue: "Shape is undefined" in frontend

**Solution:**
1. Check that backend is running (`npm start`)
2. Check network tab in DevTools
3. Verify API URL in .env
4. Check backend logs for errors

#### Issue: "Memory usage growing"

**Solution:**
1. Check for unclosed file handles
2. Look for circular references
3. Use `--inspect` to profile:
   ```bash
   node --inspect=9229 dist/index.js
   # Open chrome://inspect in Chrome
   ```

### Performance Issues

#### Issue: Slow STEP parsing

**Solution:**
1. Check file size (>100MB problematic)
2. Profile with timers:
   ```typescript
   const start = Date.now();
   const result = StepReader.readFile(path);
   console.log(`Took ${Date.now() - start}ms`);
   ```
3. Consider streaming parser for large files

---

## 🤝 Contributing

### Pull Request Process

1. **Fork & Branch**
   ```bash
   git checkout -b feature/your-feature
   ```

2. **Make Changes**
   - Follow code standards
   - Write tests
   - Update documentation

3. **Test Everything**
   ```bash
   npm run lint
   npm run typecheck
   npm test
   ```

4. **Commit & Push**
   ```bash
   git commit -m "feat: your feature description"
   git push origin feature/your-feature
   ```

5. **Create PR**
   - Fill out PR template
   - Link related issues
   - Request review

### Code Review Checklist

- [ ] Code follows standards
- [ ] Tests added/updated
- [ ] Documentation updated
- [ ] No breaking changes
- [ ] Performance considered
- [ ] Error cases handled

---

## 📚 Additional Resources

### Documentation

- [OCCT Documentation](https://dev.opencascade.org/)
- [Node.js NAPI](https://nodejs.org/api/n-api.html)
- [Express.js Guide](https://expressjs.com/)
- [Three.js Documentation](https://threejs.org/docs/)

### Key Files to Review

- `native-binding/lib/index.ts` - TypeScript interface
- `backend/src/routes/occt.routes.ts` - REST API
- `native-binding/tests/integration.test.ts` - Integration tests
- `docs/WEEK_7_8_INTEGRATION_COMPLETION.md` - Integration details

### Getting Help

1. Check documentation first
2. Search existing issues
3. Ask in project discussions
4. Create new issue with details

---

## 📝 Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0-beta.1 | Nov 1, 2026 | Initial Phase 1 release |
| 1.1.0-beta.1 | Feb 15, 2027 | Phase 2 (planned) |
| 1.0.0 | Mar 31, 2027 | First GA release (planned) |

---

**Last Updated:** November 1, 2026  
**Maintained By:** BendCore Team  
**License:** MIT  

