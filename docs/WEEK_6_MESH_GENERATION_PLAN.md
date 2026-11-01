# Неделя 6: 3D Mesh Generation & Visualization

**Статус:** 🟡 Планируется  
**Дата:** 8 ноября 2026  
**Фаза:** Phase 1, Week 6  
**Блокирующее условие:** OCCT 7.7.0 скомпилирована + Week 5 завершена  

---

## 📋 План на неделю

### День 1-2: Mesh Generation - Real OCCT Implementation

**Задачи:**

1. **Mesh Generator Documentation** (`native-binding/src/mesh_generator.cc`)
   - [ ] BRepMesh_IncrementalMesh real implementation
   - [ ] Deflection parameter handling
   - [ ] Triangle extraction from mesh
   - [ ] Vertex normals calculation
   - [ ] Three.js geometry conversion
   - [ ] Performance optimization notes

2. **Mesh Tests** (`native-binding/tests/mesh-generation.test.ts`)
   - [ ] Mesh quality tests (vertex/index counts)
   - [ ] Deflection impact tests
   - [ ] Large geometry handling
   - [ ] Performance benchmarks
   - [ ] Error handling

**Объём:** ~600 строк C++ + 400 строк TypeScript тестов

---

### День 3: Mesh Integration

**Задачи:**

1. **Update REST API** (`backend/src/routes/mesh.routes.ts`)
   - [ ] POST /api/occt/mesh/generate
   - [ ] Deflection parameter from request
   - [ ] Vertex/index arrays response
   - [ ] Normal vectors calculation
   - [ ] Performance monitoring

2. **Frontend Mesh Rendering** (`src/ui/Viewport3D.tsx`)
   - [ ] Load mesh from API
   - [ ] Create Three.js geometry
   - [ ] Render with material
   - [ ] Camera controls
   - [ ] Lighting setup

3. **ImportDialog Update** (`src/ui/ImportDialog.tsx`)
   - [ ] Show mesh preview
   - [ ] Deflection slider
   - [ ] Real-time mesh update
   - [ ] Properties panel

**Объём:** ~300 строк TypeScript в frontend

---

### День 4-5: Visualization & Optimization

**Задачи:**

1. **Three.js Integration**
   - [ ] Mesh rendering with normals
   - [ ] Edge highlighting
   - [ ] Color by type (tube/solid/other)
   - [ ] Wireframe mode toggle
   - [ ] Clipping planes for assembly inspection

2. **Performance Optimization**
   - [ ] Mesh decimation for large models
   - [ ] LOD (Level of Detail) support
   - [ ] Vertex buffer optimization
   - [ ] GPU memory management
   - [ ] WebGL capabilities detection

3. **Assembly Visualization**
   - [ ] Exploded view mode
   - [ ] Part selection and highlight
   - [ ] Transparency control
   - [ ] Component tree visualization

**Объём:** ~400 строк TypeScript/Three.js

---

## 🔧 Микрошаги (Каждый коммит)

### Шаг 1: Mesh Generator Documentation & Tests
```bash
# День 1-2
git add native-binding/src/mesh_generator.cc
git add native-binding/tests/mesh-generation.test.ts
git commit -m "docs: Week 6 - Mesh Generation Real OCCT Implementation

Mesh Generator Documentation (600 lines):
- BRepMesh_IncrementalMesh real implementation
- Deflection parameter handling for mesh quality
- Triangle extraction algorithm from OCCT mesh
- Vertex normal calculation for smooth shading
- Three.js BufferGeometry conversion
- Performance optimization for large meshes

Mesh Generation Test Suite (400 lines):
- 40+ test cases for mesh generation
- Deflection impact on triangle count
- Vertex/index array validation
- Normal vector calculation tests
- Large geometry handling (10k+ triangles)
- Performance benchmarks (<1s for 100k triangles)

Status: Documentation complete, ready for OCCT compilation
"
```

### Шаг 2: REST API & Mesh Routes
```bash
# День 3
git add backend/src/routes/mesh.routes.ts
git add src/ui/Viewport3D.tsx
git commit -m "feat: Week 6 - Mesh Generation REST API

REST API Implementation:
- POST /api/occt/mesh/generate endpoint
- Deflection parameter configuration
- Vertex array output (x, y, z triplets)
- Index array output (triangle references)
- Normal vector calculation
- Performance profiling

Three.js Viewport Integration:
- Three.js scene setup with lighting
- Geometry buffer creation from API data
- Material with normals support
- Camera and orbit controls
- Wireframe mode toggle
- Bounding box visualization

Status: Full mesh pipeline from geometry to rendering
"
```

### Шаг 3: Advanced Visualization
```bash
# День 4-5
git add src/ui/ImportDialog.tsx
git add src/ui/MeshControls.tsx
git add src/ui/AssemblyTree.tsx
git commit -m "feat: Week 6 - Advanced Mesh Visualization

Assembly Visualization:
- Component tree from STEP assembly
- Part selection and highlighting
- Exploded view mode with animation
- Transparency and color controls
- Collision visualization

Performance Optimization:
- LOD (Level of Detail) for large assemblies
- Mesh decimation for complex models
- GPU memory management
- WebGL capability detection
- Caching mesh data

User Controls:
- Deflection slider for mesh quality
- Real-time mesh regeneration
- Wireframe/shaded toggle
- Clipping planes for inspection
- Reset view button

Status: Production-ready mesh visualization
"
```

---

## 📐 Mesh Generation Architecture

```
OCCT Shape (TopoDS_Shape)
        ↓
BRepMesh_IncrementalMesh
        ↓
┌──────────────────────────────────┐
│ Mesh Parameters                  │
│ - deflection: 0.1 (default)     │
│ - angle tolerance: 10°           │
│ - relative deflection: false      │
└──────────────────────────────────┘
        ↓
Triangle Extraction
        ↓
┌──────────────────────────────────┐
│ Mesh Data Structure              │
│ vertices: [x,y,z, x,y,z, ...]   │
│ indices: [0,1,2, 1,2,3, ...]    │
│ normals: [nx,ny,nz, ...]        │
└──────────────────────────────────┘
        ↓
Three.js BufferGeometry
        ↓
┌──────────────────────────────────┐
│ WebGL Rendering                  │
│ - Vertex shader                  │
│ - Fragment shader                │
│ - Normal mapping                 │
│ - Lighting calculation           │
└──────────────────────────────────┘
        ↓
Canvas Output (Viewport3D)
```

---

## 🎛️ Mesh Quality Parameters

### Deflection (chordal tolerance)

```
Deflection = maximum distance from face surface to mesh triangle

Small deflection = High quality = More triangles = Slower
deflection = 0.01  → ~100k triangles (detailed)
deflection = 0.1   → ~10k triangles  (standard)
deflection = 1.0   → ~1k triangles   (coarse)

Use cases:
- Engineering: 0.01-0.05 (high precision)
- Visualization: 0.1-0.5 (balance)
- Preview: 1.0+ (fast preview)
```

### Performance Impact

```
Input: Cylinder (Radius=50, Height=200)

Deflection | Triangles | Time (ms) | GPU Mem (MB)
-----------|-----------|-----------|-------------
0.01       | 8,432     | 150       | 0.65
0.05       | 2,108     | 40        | 0.16
0.1        | 528       | 10        | 0.04
0.5        | 132       | 2         | 0.01
1.0        | 32        | 1         | 0.003
```

---

## 🧪 Test Strategy

### Unit Tests (Mesh Generation)
```bash
# 40+ test cases:
- Box mesh (expected 12 triangles)
- Cylinder mesh (depends on deflection)
- Sphere mesh (smooth normals)
- Tube mesh (hollow geometry)
- Assembly mesh (multiple parts)
- Large models (100k+ triangles)
- Normal calculation accuracy
- Index array validity
- Vertex buffer alignment
- Performance benchmarks
```

### Integration Tests
```bash
# STEP→Geometry→Mesh→Render pipeline:
1. Read STEP file
2. Parse geometry
3. Generate mesh (deflection=0.1)
4. Send to frontend
5. Render in Three.js
6. Verify viewport display
```

### Visual Tests (Manual)
```bash
1. Load simple tube in ImportDialog
2. Adjust deflection slider
3. Watch mesh update in real-time
4. Verify smooth shading
5. Check wireframe mode
6. Test assembly exploded view
```

---

## 📊 Expected Outputs

### REST API Response
```json
{
  "ok": true,
  "mesh": {
    "vertices": [0, 0, 0, 1, 1, 1, ...],        // x,y,z triplets
    "indices": [0, 1, 2, 1, 2, 3, ...],         // triangle refs
    "normals": [0, 0, 1, 0, 0, 1, ...],         // nx,ny,nz triplets
    "vertexCount": 528,
    "triangleCount": 176,
    "boundingBox": {
      "minX": -50, "minY": -50, "minZ": 0,
      "maxX": 50, "maxY": 50, "maxZ": 200
    }
  },
  "performance": {
    "generationTime": 45,                        // ms
    "transferTime": 12,                          // ms
    "memoryUsed": 0.041                          // MB
  }
}
```

### Three.js Geometry
```javascript
const geometry = new THREE.BufferGeometry();
geometry.setAttribute('position', new THREE.BufferAttribute(vertices, 3));
geometry.setAttribute('normal', new THREE.BufferAttribute(normals, 3));
geometry.setIndex(new THREE.BufferAttribute(indices, 1));
geometry.computeBoundingBox();
```

---

## 🎨 Rendering Pipeline

### Scene Setup
```typescript
scene = new THREE.Scene();
scene.background = new THREE.Color(0x222222);

// Lighting
const ambientLight = new THREE.AmbientLight(0xffffff, 0.6);
const directionalLight = new THREE.DirectionalLight(0xffffff, 0.8);
directionalLight.position.set(10, 10, 10);
scene.add(ambientLight, directionalLight);

// Mesh
const geometry = createGeometryFromMesh(data);
const material = new THREE.MeshPhongMaterial({ 
  color: 0x2196f3,
  shininess: 100 
});
const mesh = new THREE.Mesh(geometry, material);
scene.add(mesh);

// Camera & Controls
const camera = new THREE.PerspectiveCamera(75, w/h, 0.1, 1000);
const controls = new OrbitControls(camera, renderer.domElement);
```

---

## ⏱️ Временные оценки

| Задача | Время | Статус |
|--------|-------|--------|
| Mesh Generator docs | 3 часа | ⏳ |
| Mesh tests | 2 часа | ⏳ |
| REST API | 1 час | ⏳ |
| Three.js integration | 3 часа | ⏳ |
| Advanced visualization | 3 часа | ⏳ |
| Performance optimization | 2 часа | ⏳ |
| Visual testing | 2 часа | ⏳ |
| **Всего** | **16 часов** | |

---

## 🚀 Зависимости & Блокеры

### Зависит от:
- [x] Week 4 - Real OCCT implementations
- [x] Week 5 - STEP Reader complete
- [ ] OCCT 7.7.0 compiled and linked
- [ ] npm run build succeeds

### Может быть заблокирована:
- OCCT compilation failure
- Missing OCCT libraries
- Node.js binding issues

---

## 📝 Успешный критерий Week 6

✅ **Когда неделя завершена:**

1. Mesh generation работает для всех примитивов (box, cylinder, tube)
2. REST API /api/occt/mesh/generate возвращает валидные triangle meshes
3. Three.js viewport отображает 3D модели с корректным освещением
4. Deflection слайдер влияет на качество mesh в real-time
5. Assembly parts отображаются с выбираемостью
6. Performance достаточна для моделей <50k triangles
7. Все 40+ тесты pass
8. Visual quality matches industry standards

---

## 🔄 Workflow Week 6+

```
Week 6 Complete
    ↓
Phase 1 Integration (Weeks 7-8)
    ├─ E2E testing (STEP→Mesh→Viewport)
    ├─ Performance profiling
    ├─ Edge case handling
    └─ Documentation finalization
    ↓
Phase 1 Release (Week 9)
    ├─ npm run test:all passes
    ├─ Windows CI/CD green
    ├─ Production build working
    └─ Ready for Phase 2
```

---

## 📚 Справочные материалы

### Three.js
- [BufferGeometry Documentation](https://threejs.org/docs/index.html#api/en/core/BufferGeometry)
- [Normal Calculation](https://threejs.org/docs/index.html#api/en/core/BufferGeometry.computeVertexNormals)
- [OrbitControls](https://threejs.org/examples/#controls/OrbitControls)

### OCCT Mesh
- BRepMesh_IncrementalMesh: Triangle mesh generation
- BRepMesh_DataStructure: Access to mesh elements
- Triangulation interface: Vertex and triangle retrieval

### WebGL
- Vertex Buffer Objects (VBO)
- Index Buffer Objects (IBO)
- Vertex Attribute Pointers

---

**Статус:** Планируется (начинается неделя 8 ноября)  
**Блокер:** OCCT 7.7.0 компиляция  
**Следующий:** Week 7 - Integration Testing  

