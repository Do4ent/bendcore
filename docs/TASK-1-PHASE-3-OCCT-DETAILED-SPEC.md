# TASK 1 Phase 3: OCCT Integration + Parametric Extraction
## BendCore Q3 2027 - Parametric Dimensions + CAD Import

**Duration:** Days 5-23 (19 days)  
**Target LOC:** 1,200+ (header + implementation + tests)  
**Deadline:** 2026-11-15  
**Critical Path:** YES (blocks Tasks 2, 3, 4)  
**Status:** SPECIFICATION PHASE  

---

## 🎯 Phase 3 Mission

Integrate Open Cascade Technology (OCCT) for professional CAD file import and parametric dimension extraction. Enable users to:
- Import STEP/IGES files (product definitions)
- Extract geometric parameters (OD, wall thickness, length, bend angles)
- Build parametric dimensions from imported geometry
- Cache geometry for performance
- Support DXF import via libdxfrw

---

## 📋 Subtask Breakdown (10 subtasks)

### **Subtask 1: OCCT Integration Setup** (Days 1-2, 200 LOC)
**Goal:** Configure OCCT in build system, verify compilation

**Implementation:**
```cpp
// include/bendcore/occt_config.hpp
- OpenCASCADE::Standard_Version check (v7.7+)
- Windows x64 platform detection (#if defined(_WIN64))
- OCCT library selection (Modeling, Shape analysis, etc.)
- Error codes: OCCT_SUCCESS, OCCT_FILE_NOT_FOUND, OCCT_INVALID_FORMAT

// src/occt_init.cpp
- Initialize OCCT (Standard_Version, message reporting)
- Load OCCT plugin libraries dynamically
- Setup exception handling (Standard_Failure catch)
- Provide cleanup/shutdown functions
```

**Files to Create:**
- `include/bendcore/occt_config.hpp` (OCCT constants/types)
- `src/occt_init.cpp` (initialization)
- CMakeLists.txt entry for OCCT (find_package(OpenCASCADE REQUIRED))
- vcpkg.json manifest entry

**Tests:**
- `tests/test_occt_init.cpp` (5 tests)
  * OCCT version detected
  * Platform validation (Windows x64)
  * Initialization succeeds
  * Exception handling works
  * Cleanup called

**Quality Gate:** All 5 tests passing, no memory leaks, Windows-only enforcement

---

### **Subtask 2: STEP File Reader** (Days 2-4, 250 LOC)
**Goal:** Read STEP files and extract solid geometry

**Implementation:**
```cpp
// include/bendcore/step_reader.hpp
class STEPReader {
public:
  struct ImportResult {
    bool success;
    std::vector<TopoDS_Shape> shapes;  // Imported solids
    std::map<std::string, std::string> properties;  // Metadata
    std::string error_message;
    int warning_count;
  };
  
  ImportResult read(const std::string& file_path);
  
private:
  Handle(TDocStd_Document) doc_;
  STEPCAFControl_Reader reader_;
  void extract_shapes(Handle(TDataStd_TreeNode) root);
};

// src/step_reader.cpp
- STEPCAFControl_Reader initialization
- XDE (Extended Data Exchange) document loading
- Shape extraction from OCCT Document
- Property/metadata extraction
- Error handling (file not found, corrupted, unsupported features)
- Windows path handling (UTF-8 to Windows encoding)
```

**Features:**
- Read STEP files (.step, .stp)
- Extract all solid shapes
- Get product metadata (name, description)
- Handle assemblies (multi-part files)
- Warning reporting (unsupported features)
- File validation (not DWG/DXF/ACIS)

**Files to Create:**
- `include/bendcore/step_reader.hpp` (class definition)
- `src/step_reader.cpp` (implementation)

**Tests:**
- `tests/test_step_reader.cpp` (8 tests)
  * Valid STEP file loads
  * Multiple shapes extracted
  * Properties retrieved
  * Invalid file error handling
  * Corrupted file detection
  * Assembly handling
  * Path encoding (UTF-8)
  * Memory cleanup

**Quality Gate:** All 8 tests passing, handle 100+ part assemblies, <500ms load time for typical files

---

### **Subtask 3: Geometric Parameter Extraction** (Days 4-6, 300 LOC)
**Goal:** Extract dimensions from imported solids

**Implementation:**
```cpp
// include/bendcore/geometry_analyzer.hpp
class GeometryAnalyzer {
public:
  struct ExtractedParams {
    double outer_diameter;  // OD from bounding cylinder
    double inner_diameter;  // ID = OD - 2*wall
    double wall_thickness;
    double total_length;
    std::vector<double> bend_angles;
    std::vector<double> bend_radii;
    double bend_arc_length;
    std::string shape_type;  // "TUBE", "PIPE", "BEND", "COMPLEX"
    double confidence;  // 0.0-1.0 how sure we are
  };
  
  ExtractedParams analyze(const TopoDS_Shape& shape);
  
private:
  Bnd_Box bounding_box_;
  std::vector<gp_Pnt> edge_points_;
  double fit_cylinder(std::vector<gp_Pnt>& points);
  std::vector<double> detect_bends(const TopoDS_Shape& shape);
};

// src/geometry_analyzer.cpp
- Bounding box calculation (Bnd_Box)
- Cylinder fitting (least-squares OD/ID detection)
- Edge extraction (BRepLib_MakeEdge)
- Bend detection (curvature analysis)
- Axis-aligned dimension extraction
- Tolerance handling (0.01mm precision)
```

**Algorithm:**
1. Get bounding box → rough OD/length
2. Extract all edges → analyze curvature
3. Fit cylinder to curved edges → precise OD
4. Calculate wall thickness (trace radius)
5. Find bends (curvature peaks) → bend angles/radii
6. Validate confidence (>80% = accept)

**Files to Create:**
- `include/bendcore/geometry_analyzer.hpp` (class definition)
- `src/geometry_analyzer.cpp` (implementation)

**Tests:**
- `tests/test_geometry_analyzer.cpp` (10 tests)
  * Straight tube OD extraction
  * Straight tube length measurement
  * Wall thickness calculation
  * Single bend detection
  * Multiple bends detection
  * Bend angle calculation
  * Bend radius estimation
  * Complex geometry handling
  * Confidence scoring
  * Edge cases (thin walls, large bends)

**Quality Gate:** All 10 tests passing, ±0.1mm accuracy on OD, ±1° accuracy on angles

---

### **Subtask 4: Parametric Dimension Builder** (Days 6-8, 250 LOC)
**Goal:** Create DimensionFormula objects from extracted parameters

**Implementation:**
```cpp
// include/bendcore/parametric_builder.hpp
class ParametricDimensionBuilder {
public:
  struct DimensionSet {
    std::vector<DimensionFormula> dimensions;
    std::map<std::string, double> extracted_values;
    std::string source_file;
    std::string creation_timestamp;
  };
  
  DimensionSet build(
    const GeometryAnalyzer::ExtractedParams& params,
    const std::string& source_file,
    const std::string& author
  );
  
private:
  DimensionFormula create_od_dimension(double value);
  DimensionFormula create_wall_dimension(double value);
  DimensionFormula create_length_dimension(double value);
  DimensionFormula create_bend_angle_dimension(double value, int bend_index);
  DimensionFormula create_bend_radius_dimension(double value, int bend_index);
};

// src/parametric_builder.cpp
- Create DimensionFormula for each extracted parameter
- Assign meaningful names ($OD, $Wall, $Length, $BendAngle_1, etc.)
- Set author = "STEP Importer" + timestamp
- Create associated formulas (e.g., $ID = $OD - 2*$Wall)
- Validate all formulas before returning
```

**Creates Dimensions:**
- `$OD` = extracted outer diameter
- `$Wall` = extracted wall thickness
- `$ID` = $OD - 2*$Wall (formula)
- `$Length` = extracted total length
- `$BendAngle_1`, `$BendAngle_2`, etc. = bend angles
- `$BendRadius_1`, etc. = bend radii
- `$BendArcLength` = arc length formula

**Files to Create:**
- `include/bendcore/parametric_builder.hpp` (class definition)
- `src/parametric_builder.cpp` (implementation)

**Tests:**
- `tests/test_parametric_builder.cpp` (8 tests)
  * OD dimension created
  * Wall dimension created
  * ID formula created and evaluated
  * Length dimension created
  * Bend angle dimensions created
  * Bend radius dimensions created
  * All formulas validate
  * Timestamp and author set correctly

**Quality Gate:** All 8 tests passing, all formulas evaluate without error

---

### **Subtask 5: Geometry Caching System** (Days 8-10, 200 LOC)
**Goal:** Cache imported geometry for performance

**Implementation:**
```cpp
// include/bendcore/geometry_cache.hpp
class GeometryCache {
public:
  using CacheKey = std::string;  // SHA256 hash
  
  struct CacheEntry {
    TopoDS_Shape shape;
    GeometryAnalyzer::ExtractedParams params;
    std::chrono::system_clock::time_point timestamp;
    size_t memory_bytes;
  };
  
  CacheKey add(
    const std::string& source_file,
    const TopoDS_Shape& shape,
    const GeometryAnalyzer::ExtractedParams& params
  );
  
  std::optional<CacheEntry> get(const CacheKey& key);
  void invalidate(const CacheKey& key);
  void clear();
  
private:
  std::map<CacheKey, CacheEntry> cache_;
  size_t max_memory_bytes_ = 100 * 1024 * 1024;  // 100MB
  CacheKey compute_hash(const std::string& source_file);
};

// src/geometry_cache.cpp
- File-based hash (SHA256 of file content + metadata)
- In-memory LRU eviction when >100MB
- Thread-safe access (std::mutex)
- Serialization support (optional: save to disk)
```

**Features:**
- Fast lookup by file hash
- Automatic eviction (LRU, 100MB limit)
- Thread-safe
- Memory tracking

**Files to Create:**
- `include/bendcore/geometry_cache.hpp` (class definition)
- `src/geometry_cache.cpp` (implementation)

**Tests:**
- `tests/test_geometry_cache.cpp` (7 tests)
  * Add entry to cache
  * Retrieve entry from cache
  * Miss on different file
  * Eviction when >100MB
  * Thread-safe concurrent access
  * Invalidate specific entry
  * Clear all entries

**Quality Gate:** All 7 tests passing, <50ms lookup time, proper eviction

---

### **Subtask 6: DXF File Reader** (Days 10-12, 180 LOC)
**Goal:** Read DXF files for 2D geometry

**Implementation:**
```cpp
// include/bendcore/dxf_reader.hpp
class DXFReader {
public:
  struct DXFGeometry {
    std::vector<dxfrw::LWPolyline> polylines;
    std::vector<dxfrw::Circle> circles;
    std::vector<dxfrw::Line> lines;
    std::vector<dxfrw::Spline> splines;
    std::map<std::string, std::string> layers;
  };
  
  DXFGeometry read(const std::string& file_path);
  
private:
  dxfrw::DxfReader reader_;
  void extract_entities(DXFGeometry& geom);
};

// src/dxf_reader.cpp
- libdxfrw initialization
- Entity extraction (lines, circles, polylines, splines)
- Layer information
- Error handling
- Windows path encoding
```

**Features:**
- Read DXF files (.dxf)
- Extract lines, circles, polylines, splines
- Get layer information
- Handle common DXF versions (R12, R2000, R2004, R2010)

**Files to Create:**
- `include/bendcore/dxf_reader.hpp` (class definition)
- `src/dxf_reader.cpp` (implementation)
- Link libdxfrw in CMakeLists.txt

**Tests:**
- `tests/test_dxf_reader.cpp` (6 tests)
  * Valid DXF file loads
  * Lines extracted
  * Circles extracted
  * Polylines extracted
  * Layer information preserved
  * Invalid file error handling

**Quality Gate:** All 6 tests passing, support common DXF versions

---

### **Subtask 7: Import Workflow Manager** (Days 12-14, 250 LOC)
**Goal:** Orchestrate full import workflow

**Implementation:**
```cpp
// include/bendcore/import_workflow.hpp
class ImportWorkflow {
public:
  struct ImportConfig {
    std::string file_path;
    std::string author;
    bool use_cache = true;
    double precision_mm = 0.01;
    bool auto_create_formulas = true;
  };
  
  struct ImportResult {
    bool success;
    std::vector<DimensionFormula> formulas;
    std::vector<std::string> warnings;
    std::string log;
    double execution_time_ms;
  };
  
  ImportResult import(const ImportConfig& config);
  
private:
  STEPReader step_reader_;
  DXFReader dxf_reader_;
  GeometryAnalyzer analyzer_;
  ParametricDimensionBuilder builder_;
  GeometryCache cache_;
  
  std::string detect_format(const std::string& file_path);
  void log_step(const std::string& step_name);
};

// src/import_workflow.cpp
- Format detection (.step, .stp, .iges, .igs, .dxf)
- Cache lookup
- Read file (STEP/DXF)
- Analyze geometry
- Build parametric dimensions
- Save to cache
- Return results with timing
```

**Workflow:**
```
1. Detect file format
2. Check cache → found? return cached
3. Read file (STEP reader or DXF reader)
4. Extract geometry (for STEP: bounding box, edges)
5. Analyze parameters (OD, wall, length, bends)
6. Build parametric formulas
7. Cache results
8. Return formulas + warnings
```

**Files to Create:**
- `include/bendcore/import_workflow.hpp` (class definition)
- `src/import_workflow.cpp` (implementation)

**Tests:**
- `tests/test_import_workflow.cpp` (8 tests)
  * STEP import workflow succeeds
  * DXF import workflow succeeds
  * Cache hit skips processing
  * Format detection works
  * Error handling (missing file, corrupted)
  * Warnings logged
  * Timing measured
  * Formulas created and validated

**Quality Gate:** All 8 tests passing, <1s total time (including geometry analysis)

---

### **Subtask 8: React Import UI Component** (Days 14-16, 300 LOC)
**Goal:** Build user interface for file import

**Implementation:**
```typescript
// src/ui/ImportDialog.tsx
interface ImportDialogProps {
  onImport: (formulas: DimensionFormula[]) => void;
  onCancel: () => void;
}

export const ImportDialog: React.FC<ImportDialogProps> = ({
  onImport,
  onCancel,
}) => {
  // Features:
  // - File selector (STEP/IGES/DXF)
  // - Author input (defaults to current user)
  // - Options: use cache, auto-create formulas
  // - Progress indicator during import
  // - Results preview (formulas created)
  // - Error display with suggestions
  // - Drag-drop file support
};

// src/ui/ImportProgress.tsx
// Real-time import status:
// - Reading file...
// - Analyzing geometry... (25%)
// - Extracting parameters... (50%)
// - Creating formulas... (75%)
// - Caching results... (90%)
// - Complete (100%)

// src/ui/ImportResults.tsx
// Display imported formulas:
// - $OD = 25.4 mm
// - $Wall = 2.11 mm
// - $Length = 150.0 mm
// - $BendAngle_1 = 90.0°
// - Warnings (if any)
// - Apply/Cancel buttons
```

**Features:**
- File selector (drag-drop + button)
- Author field
- Options checkbox
- Progress bar (0-100%)
- Results preview
- Error dialog
- Success notification

**Files to Create:**
- `src/ui/ImportDialog.tsx` (main dialog)
- `src/ui/ImportProgress.tsx` (progress bar)
- `src/ui/ImportResults.tsx` (results preview)
- `src/ui/ImportDialog.css` (styling)

**Tests:**
- `src/ui/__tests__/ImportDialog.test.tsx` (10 tests)
  * Component renders
  * File selector works
  * Drag-drop accepted
  * Progress updates
  * Results displayed
  * Apply callback fired
  * Cancel callback fired
  * Error displayed
  * Accessibility (WCAG AA)
  * Dark mode support

**Quality Gate:** All 10 tests passing, smooth animations, responsive design

---

### **Subtask 9: API Endpoint Integration** (Days 16-18, 200 LOC)
**Goal:** Connect React UI to backend

**Implementation:**
```typescript
// src/api/import_routes.ts
router.post('/import-file', async (req: Request, res: Response) => {
  // Multipart form: file + author + options
  // Returns: { success, formulas, warnings, execution_time_ms }
});

router.post('/detect-format', (req: Request, res: Response) => {
  // Input: file extension or mime type
  // Returns: { format: "STEP" | "IGES" | "DXF", supported: boolean }
});

router.get('/import-history', (req: Request, res: Response) => {
  // Returns list of recently imported files
});

// src/ui/hooks/useImport.ts
const useImport = () => {
  const [importing, setImporting] = useState(false);
  const [progress, setProgress] = useState(0);
  
  const importFile = async (file: File, author: string) => {
    const formData = new FormData();
    formData.append('file', file);
    formData.append('author', author);
    
    const response = await fetch('/api/bendcore/import-file', {
      method: 'POST',
      body: formData,
    });
    
    return response.json();
  };
  
  return { importFile, importing, progress };
};
```

**Features:**
- File upload endpoint
- Format detection endpoint
- Import history tracking
- React hook for component usage

**Files to Create:**
- `src/api/import_routes.ts` (Express routes)
- `src/ui/hooks/useImport.ts` (React hook)

**Tests:**
- `tests/test_import_api.ts` (6 tests)
  * POST /import-file succeeds
  * File validation
  * Author required
  * Error on unsupported format
  * Progress tracking
  * Response schema validation

**Quality Gate:** All 6 tests passing, multipart upload working

---

### **Subtask 10: Integration Testing & Documentation** (Days 18-23, 150 LOC)
**Goal:** End-to-end testing, documentation, quality gates

**Implementation:**

**E2E Tests:**
```cpp
// tests/e2e_import.cpp (50 lines)
// Test complete import workflow:
// 1. Load STEP file (sample_tube_90deg.step)
// 2. Extract parameters
// 3. Create formulas
// 4. Verify formula evaluation
// 5. Cache verification
// 6. DXF import
// 7. Results persistence
```

**Documentation:**
```markdown
// docs/IMPORT_USER_GUIDE.md
- How to import STEP files
- Understanding extracted parameters
- DXF import limitations
- Troubleshooting common issues

// docs/IMPORT_API_REFERENCE.md
- STEPReader API
- GeometryAnalyzer API
- DimensionBuilder API
- ImportWorkflow API

// docs/OCCT_INTEGRATION.md
- OCCT setup instructions
- Supported STEP versions
- Performance characteristics
- Memory management
```

**Files to Create:**
- `tests/e2e_import.cpp` (end-to-end test)
- `docs/IMPORT_USER_GUIDE.md`
- `docs/IMPORT_API_REFERENCE.md`
- `docs/OCCT_INTEGRATION.md`

**Quality Gates:**
- ✅ All 60+ subtask tests passing
- ✅ E2E test with real STEP file succeeds
- ✅ Code coverage >85%
- ✅ No memory leaks (valgrind clean)
- ✅ Performance: <1s typical import
- ✅ Windows-only enforced (#if _WIN32)
- ✅ Documentation complete
- ✅ Accessibility reviewed
- ✅ Security review passed
- ✅ Code style consistent (clang-format)

---

## 📊 Phase 3 Summary

| Subtask | LOC | Days | Tests | Status |
|---------|-----|------|-------|--------|
| 1. OCCT Setup | 200 | 2 | 5 | ⏳ |
| 2. STEP Reader | 250 | 2 | 8 | ⏳ |
| 3. Geometry Analysis | 300 | 2 | 10 | ⏳ |
| 4. Parametric Builder | 250 | 2 | 8 | ⏳ |
| 5. Geometry Cache | 200 | 2 | 7 | ⏳ |
| 6. DXF Reader | 180 | 2 | 6 | ⏳ |
| 7. Workflow Manager | 250 | 2 | 8 | ⏳ |
| 8. React UI | 300 | 2 | 10 | ⏳ |
| 9. API Integration | 200 | 2 | 6 | ⏳ |
| 10. Testing & Docs | 150 | 5 | E2E | ⏳ |
| **TOTAL** | **2,280** | **19** | **68+** | ⏳ |

**Expected Results:**
- ✅ 2,280 LOC production code
- ✅ 68+ unit + E2E tests
- ✅ >85% code coverage
- ✅ Full STEP/IGES/DXF support
- ✅ Parametric dimensions extracted
- ✅ Production-ready import system
- ✅ Ready for Tasks 2, 3, 4

---

## 🎯 Success Criteria

- ✅ All 10 subtasks implemented
- ✅ 2,280+ LOC production code
- ✅ 68+ tests passing (>85% coverage)
- ✅ Real STEP files import successfully
- ✅ Parametric formulas created and validated
- ✅ React UI integrated
- ✅ Windows-only constraint enforced
- ✅ <1s typical import time
- ✅ Zero memory leaks
- ✅ Documentation complete
- ✅ Ready for Phase 3 → Tasks 2, 3, 4

---

## 📅 Timeline

```
Days 1-2:   OCCT Setup + STEP Reader
Days 3-4:   Geometry Analysis + Parametric Builder
Days 5-6:   Geometry Cache + DXF Reader
Days 7-8:   Workflow Manager
Days 9-10:  React UI Component
Days 11-12: API Integration
Days 13-19: Integration Testing + Documentation + Polish
Days 20-23: Buffer for blocking issues + final QA
```

---

## 🚀 Start Phase 3 When Ready

All specifications complete. Ready to launch autonomous agent development.

**Estimated effort:** 19 days (2026-10-24 to 2026-11-15)  
**Completion target:** 2026-11-15  
**Quality bar:** Enterprise-ready, production-grade  

**Next Steps:**
1. Review this specification
2. Launch autonomous agent for Phase 3
3. Agent implements all 10 subtasks in parallel (Days 1-19)
4. Final QA and polish (Days 20-23)
5. Completion: 2026-11-15 ✅

---

**Specification Status:** ✅ READY FOR IMPLEMENTATION  
**Last Updated:** 2026-10-23 22:15 UTC+2  
**Critical Path:** YES (blocks Tasks 2, 3, 4)
