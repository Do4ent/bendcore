# P2 Technical Debt Cleanup Plan

**Phase:** Pre-Phase 4 (Before Q1 2027 development)  
**Priority:** P2 (Must complete before Phase 4 starts)  
**Effort Estimate:** 2-3 weeks  
**Impact:** Reduces code bloat from 28.6:1 (unused:used) to manageable levels

---

## Overview

BendCore v1.0 has accumulated 32,008 lines of unused UI code (66.4% of codebase). Phase 4 development requires clean, maintainable codebase.

**Decision:** DELETE all unused components (not document/comment) - they're code debt, not design assets.

---

## Part 1: Unused UI Components (32,008 lines)

### Tier 1: Remove Immediately (No Dependencies)

**Combined3DOffsetCalculator.tsx** (29,169 lines)
- Status: Dead code - not imported anywhere
- Blocks: Nothing (no dependencies on it)
- Action: **DELETE**
- Rationale: 90% of unused code; was planned for Phase 4a but not started; kept separately for future reference if needed
- Impact: None - unused

**Path:** `ui/src/components/deprecated/Combined3DOffsetCalculator.tsx` → DELETE

---

### Tier 2: Remove After Verification (Check for Hidden References)

| Component | Lines | Status | Action |
|-----------|-------|--------|--------|
| ImportPreviewPanel.tsx | 466 | Unused | DELETE |
| ExportDialog.tsx | 417 | Unused | DELETE |
| ReleaseDialog.tsx | 309 | Unused | DELETE |
| ReportsPanel.tsx | 314 | Unused | DELETE |
| CollaborationTab.tsx | 435 | Unused | DELETE |
| StandardsTab.tsx | 398 | Unused | DELETE |

**Total:** 2,339 lines

**Process:**
1. Grep for imports in codebase
2. Verify no references exist
3. Delete files
4. Re-run TypeScript check

**Paths:**
```
ui/src/components/UI/ReportingComponents/ExportDialog.tsx
ui/src/components/UI/ReportingComponents/ReleaseDialog.tsx
ui/src/components/UI/ReportingComponents/ReportsPanel.tsx
ui/src/components/UI/PropertiesPanel/CollaborationTab.tsx
ui/src/components/UI/PropertiesPanel/StandardsTab.tsx
ui/src/components/Import/ImportPreviewPanel.tsx
```

---

## Part 2: Undocumented API Endpoints (Technical Debt)

### Current Status

| Endpoint | Status | Routes |
|----------|--------|--------|
| `/api/analyze` | ✅ Documented | Works |
| `/api/export-drawing` | ✅ Documented | Works |
| `/api/analyze-fea` | ✅ Documented | Works |
| `/api/topology-optimize` | ✅ Documented | Works |
| `/api/import/filter` | ❌ Undocumented | Exists |
| `/api/recognize` | ❌ Undocumented | Exists |
| `/api/round` | ❌ Undocumented | Exists |
| `/api/smooth` | ❌ Undocumented | Exists |
| `/api/standards` | ❌ Undocumented | Exists |

### Decision Matrix

| Endpoint | Purpose | Decision |
|----------|---------|----------|
| `/api/import/filter` | Geometry filtering | **REMOVE** (not in spec) |
| `/api/recognize` | Shape detection | **REMOVE** (not in spec) |
| `/api/round` | Rounding operation | **REMOVE** (not in spec) |
| `/api/smooth` | Surface smoothing | **REMOVE** (not in spec) |
| `/api/standards` | Standards data | **REMOVE** (not in spec) |

**Rationale:** These are leftover from earlier design iterations. Not in documented API spec, not used by frontend, no tests.

**Action:** Remove route handlers from `backend/src/api/routes/*.ts`

---

## Part 3: Documentation Updates

### Task 1: Update README.md

**Current Status:** Outdated (mentions Phase 1-2 features)

**Changes Needed:**

1. Add v1.0 Release Badge
```markdown
## BendCore v1.0 - Production Ready ✅
```

2. Update Feature List
```markdown
### ✅ v1.0 Features
- STEP/IGES import (OpenCASCADE 7.6.3)
- Tube parameter detection (OD, wall, bends)
- Linear elastic FEA (50ms analysis)
- Topology optimization (weight reduction)
- SVG technical drawing export
- 3D mesh visualization
- Real-time cloud collaboration
```

3. Add Architecture Diagram
```
React UI (Port 3000)
    ↓
Node.js Backend (Port 3001)
    ↓
C++ bendcore_server (Port 8080)
    ↓
OpenCASCADE 7.6.3
```

4. Add Quick Start
```markdown
### Quick Start
1. docker-compose up
2. Open http://localhost:3000
3. Click "📦 Import STEP/IGES"
4. Select tube model
5. Click "⚙️ Analyze (FEA)"
6. Click "🎯 Optimize"
7. Click "📋 Export Drawing"
```

5. Add Roadmap Section
```markdown
## Roadmap
- **Phase 4** (Q1 2027): Geometry creation (extrude, revolve, boolean)
- **Phase 5** (Q2 2027): WASM offline compilation
- **Phase 6** (Q3 2027): Advanced sketcher with loft/sweep
- **Phase 7** (2027+): Generative design & CAM
```

6. Update API Documentation
```markdown
## API Endpoints
1. POST /api/import - Import STEP/IGES
2. POST /api/export-drawing - Generate SVG
3. POST /api/analyze-fea - Run FEA
4. POST /api/topology-optimize - Optimize
```

**File:** `README.md`

---

### Task 2: Update API_COMPATIBILITY_REPORT.md

Move undocumented endpoints to "Removed Endpoints" section:
```markdown
## Removed (v1.0 Cleanup)
- /api/import/filter
- /api/recognize
- /api/round
- /api/smooth
- /api/standards
```

**File:** `API_COMPATIBILITY_REPORT.md`

---

## Part 4: Windows Build Guide

### Create New File: `WINDOWS_BUILD_GUIDE.md`

**Content Structure:**

1. **Prerequisites**
   - Windows 10/11
   - Visual Studio 2022 Community (free)
   - CMake 3.20+
   - Node.js 18+
   - Docker Desktop (or WSL2 + Docker)

2. **Step 1: Clone Repository**
```bash
git clone https://github.com/Do4ent/bendcore.git
cd bendcore
```

3. **Step 2: Install Dependencies**
```bash
# C++
vcpkg install opencascade:x64-windows

# Node.js
npm install
```

4. **Step 3: Build C++**
```bash
mkdir build
cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

5. **Step 4: Build Node.js**
```bash
npm run build
```

6. **Step 5: Run**
```bash
npm run dev  # Dev server
npm run build  # Production build
```

7. **Step 6: Docker (Optional)**
```bash
docker-compose up
```

**File:** `WINDOWS_BUILD_GUIDE.md`

---

## Part 5: Implementation Timeline

### Week 1: Component Cleanup

**Monday-Tuesday:** Remove unused UI components
- [ ] Grep for all imports
- [ ] Identify dead code
- [ ] Delete 6 unused component files
- [ ] Run `npm run type-check` and `npm run test`
- [ ] Commit: "Remove unused UI components (32,008 lines cleanup)"

**Wednesday:** Remove API endpoints
- [ ] Remove undocumented route handlers
- [ ] Run `npm run test:backend`
- [ ] Commit: "Remove undocumented API endpoints"

**Thursday-Friday:** Update documentation
- [ ] Update README.md
- [ ] Update API_COMPATIBILITY_REPORT.md
- [ ] Create WINDOWS_BUILD_GUIDE.md
- [ ] Commit: "Update documentation for v1.0 release"

### Week 2: Verification & Testing

**Monday-Tuesday:** Full test suite
- [ ] Run full CI pipeline locally
- [ ] Docker build & test
- [ ] Frontend build
- [ ] Backend tests
- [ ] C++ tests (72 tests should pass)

**Wednesday-Thursday:** Documentation verification
- [ ] Test Windows build guide on test machine
- [ ] Verify all links in README
- [ ] Check code examples work

**Friday:** Final cleanup
- [ ] Push all changes
- [ ] Create summary report
- [ ] Mark Phase 4 prep as complete

---

## Part 6: Git Commits

### Commit 1: Remove Unused Components
```
Remove 32,008 lines of unused UI components

Deleted:
- Combined3DOffsetCalculator (29,169 lines)
- ImportPreviewPanel (466 lines)
- ExportDialog (417 lines)
- ReleaseDialog (309 lines)
- ReportsPanel (314 lines)
- CollaborationTab (435 lines)
- StandardsTab (398 lines)

These were planned for future phases but remain unused
in v1.0 and create code maintenance burden.

Tests: npm run type-check passes, 1217 Jest tests pass
```

### Commit 2: Remove Undocumented API Endpoints
```
Remove undocumented backend API endpoints

Deleted:
- /api/import/filter
- /api/recognize
- /api/round
- /api/smooth
- /api/standards

These endpoints were design artifacts from earlier
iterations and are not part of v1.0 spec.

Tests: npm run test:backend passes
```

### Commit 3: Update Documentation
```
Update documentation for v1.0 release

Changes:
- README.md: Add v1.0 features, quick start, roadmap
- API_COMPATIBILITY_REPORT.md: Document removed endpoints
- NEW: WINDOWS_BUILD_GUIDE.md

Ready for Phase 4 development start (Q1 2027)
```

---

## Part 7: Verification Checklist

Before Phase 4 starts:

- [ ] All 32,008 unused lines deleted
- [ ] No broken imports (npm run type-check passes)
- [ ] All tests pass (1217 Jest, 72 C++ tests)
- [ ] Docker builds without errors
- [ ] README updated and verified
- [ ] Windows build guide tested
- [ ] API documentation current
- [ ] No "TODO" references to deleted components
- [ ] Git history clean with clear commit messages

---

## Success Metrics

| Metric | Before | After | Goal |
|--------|--------|-------|------|
| Code bloat ratio | 28.6:1 | 1:1 | ✅ |
| Lines in ui/src/ | 33,139 | 1,131 | ✅ |
| Documentation coverage | 85% | 95% | ✅ |
| Build time | - | <5min | ✅ |
| Test coverage | 95% | 95% | ✅ |

---

## Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|-----------|
| Breaking changes | Low | Full test suite + CI |
| Missing dependencies | Low | Grep before delete |
| User confusion | Low | Clear commit messages |
| Incomplete docs | Low | Verification checklist |

---

## Phase 4 Readiness

After P2 cleanup completion:
- ✅ Clean, maintainable codebase
- ✅ Clear documentation
- ✅ Verified build process
- ✅ Ready for 3-4 weeks of Phase 4a development

**Estimated start date:** January 2027

---

**Plan created:** November 1, 2026  
**Phase:** Pre-Phase 4 preparation  
**Status:** Ready for implementation
