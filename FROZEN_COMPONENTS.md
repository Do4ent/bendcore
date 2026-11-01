# FROZEN COMPONENTS - NO MODIFICATIONS ALLOWED

**Date:** November 1, 2026  
**Status:** ACTIVE - STRICT ENFORCEMENT  
**Authority:** Project Lead  

---

## ❌ COMPLETELY FROZEN - NO CHANGES PERMITTED

### 1. FEA (Finite Element Analysis)
**Location:** `src/fea/`, `backend/src/fea/`  
**Status:** 🔒 FROZEN - ZERO MODIFICATIONS  
**Reason:** Stable production component  
**Duration:** Indefinite until explicit authorization  

**What's FROZEN:**
- ✅ LinearElasticFEA engine (src/fea/)
- ✅ Tetrahedron element library
- ✅ Material database
- ✅ Von Mises stress computation
- ✅ All FEA-related routes

**Prohibited Actions:**
```
❌ DO NOT modify FEA algorithm
❌ DO NOT add FEA features
❌ DO NOT integrate FEA with new components
❌ DO NOT optimize FEA code
❌ DO NOT refactor FEA
❌ DO NOT touch FEA files
```

---

### 2. API Routes
**Location:** `backend/src/api/routes/`  
**Status:** 🔒 FROZEN - ZERO MODIFICATIONS  
**Reason:** Stable API contract  
**Duration:** Indefinite until explicit authorization  

**What's FROZEN:**
- ✅ `/api/import/analyze` - FROZEN
- ✅ `/api/import/export-drawing` - FROZEN
- ✅ `/api/import/analyze-fea` - FROZEN
- ✅ `/api/import/topology-optimize` - FROZEN
- ✅ `/api/import/` - FROZEN
- ✅ All REST API endpoints - FROZEN
- ✅ All backend routes - FROZEN

**Prohibited Actions:**
```
❌ DO NOT modify API endpoints
❌ DO NOT add new routes
❌ DO NOT change request/response format
❌ DO NOT update API documentation
❌ DO NOT refactor API code
❌ DO NOT touch API files except reading
```

---

### 3. Collaboration & Team Features
**Location:** `ui/src/components/Collaboration/`, `backend/src/collaboration/`  
**Status:** 🔒 FROZEN - ZERO MODIFICATIONS  
**Reason:** Not part of Phase 4 scope  
**Duration:** Indefinite until explicit authorization  

**What's FROZEN:**
- ✅ Collaboration UI components - FROZEN
- ✅ Real-time sync features - FROZEN
- ✅ Team workspace features - FROZEN
- ✅ Sharing & permissions system - FROZEN
- ✅ Multi-user editing - FROZEN
- ✅ Comments & annotations - FROZEN
- ✅ Version control features - FROZEN

**Prohibited Actions:**
```
❌ DO NOT create collaboration components
❌ DO NOT add sharing/permissions features
❌ DO NOT implement multi-user editing
❌ DO NOT add team workspace UI
❌ DO NOT integrate collaboration APIs
```

---

### 4. In-App Documentation System
**Location:** `ui/src/components/Documentation/`, `backend/src/documentation/`  
**Status:** 🔒 FROZEN - ZERO MODIFICATIONS  
**Reason:** Not part of Phase 4 scope  
**Duration:** Indefinite until explicit authorization  

**What's FROZEN:**
- ✅ Help system UI - FROZEN
- ✅ Tutorials & guides - FROZEN
- ✅ In-app documentation viewer - FROZEN
- ✅ Context-sensitive help - FROZEN
- ✅ Knowledge base integration - FROZEN
- ✅ Video tutorials player - FROZEN
- ✅ API documentation generator - FROZEN
- ✅ User manual generation - FROZEN

**Prohibited Actions:**
```
❌ DO NOT create documentation UI components
❌ DO NOT add help system
❌ DO NOT implement tutorials
❌ DO NOT add in-app guides
❌ DO NOT create documentation generator
❌ DO NOT add knowledge base features
```

---

## ✅ WHAT IS NOT FROZEN (Available for Phase 4)

### UI/Frontend Components (UNFROZEN - Phase 4 Only)
- ✅ Parametric Design UI (NEW)
- ✅ Geometry operation UI (NEW)
- ✅ Assembly system UI (NEW)
- ✅ UI styling (for Phase 4 components only)
- ✅ Viewport components (for Phase 4 features)
- ✅ Dialog components (for Phase 4 features)
- ✅ Import workflow UI (existing only)

**LOCKED - NOT for Phase 4:**
- ❌ Collaboration UI
- ❌ Documentation UI
- ❌ Team workspace UI
- ❌ Help system components

### Geometry & Modeling (UNFROZEN)
- ✅ GeometryKernel (for Phase 4 Extrude/Revolve)
- ✅ BRepBuilder (for Phase 4 Boolean ops)
- ✅ CurveOperations (for Phase 4 features)
- ✅ SurfaceOperations (for Phase 4 features)
- ✅ Feature operations (NEW - Extrude, Revolve, Boolean)

### Assembly System (UNFROZEN)
- ✅ Assembly constraints (NEW - Phase 4)
- ✅ Assembly hierarchy (NEW - Phase 4)
- ✅ Constraint resolver (NEW - Phase 4)

### Parametric Design (UNFROZEN)
- ✅ Parameter engine (NEW - Phase 4)
- ✅ Parameter UI (NEW - Phase 4)
- ✅ Sensitivity analysis (NEW - Phase 4)

---

## 🔐 ENFORCEMENT RULES

### Rule 1: No FEA Modifications
```
ANY commit that touches:
  src/fea/
  backend/src/fea/
  Or files containing "FEA", "fea_", "LinearElastic"

WILL BE REJECTED with message:
  "❌ FEA FROZEN - This component is locked for modifications"
```

### Rule 2: No API Modifications
```
ANY commit that touches:
  backend/src/api/routes/
  Or files containing API route definitions

WILL BE REJECTED with message:
  "❌ API FROZEN - This component is locked for modifications"
```

### Rule 3: No Collaboration Features
```
ANY commit that touches:
  ui/src/components/Collaboration/
  backend/src/collaboration/
  Or files containing "collaboration", "sharing", "team", "multi-user"

WILL BE REJECTED with message:
  "❌ COLLABORATION FROZEN - This component is locked for modifications"
```

### Rule 4: No In-App Documentation
```
ANY commit that touches:
  ui/src/components/Documentation/
  backend/src/documentation/
  Or files containing "documentation", "help", "tutorial", "guide"

WILL BE REJECTED with message:
  "❌ DOCUMENTATION FROZEN - This component is locked for modifications"
```

### Rule 5: Pre-commit Hook Enforcement
```bash
# .git/hooks/pre-commit (UPDATED)

# Check 1: FEA files
FEA_FILES=$(git diff --name-only --staged | grep -E "src/fea/|fea_")
if [ ! -z "$FEA_FILES" ]; then
    echo "❌ ERROR: FEA component is FROZEN"
    echo "Frozen files: $FEA_FILES"
    exit 1
fi

# Check 2: API routes
API_FILES=$(git diff --name-only --staged | grep "backend/src/api/routes/")
if [ ! -z "$API_FILES" ]; then
    echo "❌ ERROR: API routes are FROZEN"
    echo "Frozen files: $API_FILES"
    exit 1
fi

# Check 3: Collaboration features
COLLAB_FILES=$(git diff --name-only --staged | grep -E "ui/src/components/Collaboration/|backend/src/collaboration/")
if [ ! -z "$COLLAB_FILES" ]; then
    echo "❌ ERROR: Collaboration features are FROZEN"
    echo "Frozen files: $COLLAB_FILES"
    exit 1
fi

# Check 4: Documentation system
DOC_FILES=$(git diff --name-only --staged | grep -E "ui/src/components/Documentation/|backend/src/documentation/")
if [ ! -z "$DOC_FILES" ]; then
    echo "❌ ERROR: In-app Documentation system is FROZEN"
    echo "Frozen files: $DOC_FILES"
    exit 1
fi

exit 0
```

### Rule 6: Code Review Enforcement
```
Code Review Checklist Addition:

BEFORE MERGE:
- [ ] Does this PR modify FEA code? → REJECT IF YES
- [ ] Does this PR modify API routes? → REJECT IF YES
- [ ] Does this PR modify Collaboration features? → REJECT IF YES
- [ ] Does this PR modify Documentation system? → REJECT IF YES
- [ ] Only allowed changes (Geometry, UI, Assembly, Parametric)? → MUST BE YES
```

---

## 📋 PHASE 4 REVISED (Without FEA/API)

### Original Plan (INVALID):
```
Week 1-2: FEA Solver Integration ❌ CANCELLED
Week 3: Stress Visualization ❌ CANCELLED
Week 4: Parametric Design ✅ AVAILABLE
Week 5-6: Extrude/Revolve ✅ AVAILABLE
Week 7: Boolean Operations ✅ AVAILABLE
Week 8-9: Assembly System ✅ AVAILABLE
```

### REVISED Phase 4 Plan (FROZEN COMPONENTS EXCLUDED):

```
Week 1-2: Parametric Design Variables
  ├── Parameter definition & linking
  ├── Parameter slider panel UI
  ├── Sensitivity analysis
  └── Real-time updates

Week 3-4: Extrude & Revolve Features
  ├── Extrude from 2D sketches
  ├── Revolve (rotation) feature
  ├── B-Rep topology generation
  └── Integration testing

Week 5-6: Boolean Operations
  ├── Union (Add)
  ├── Difference (Subtract)
  ├── Intersection
  └── B-Rep reconstruction

Week 7-8: Assembly System
  ├── Assembly constraints (8 types)
  ├── Constraint resolver
  ├── Assembly UI/visualization
  └── Multi-level hierarchy

Week 9-12: Integration & Testing
  ├── All components together
  ├── Full test suite
  ├── Documentation
  └── Performance verification
```

---

## 🚨 WHAT DEVELOPERS MUST DO

### Before Phase 4 Starts:
1. ✅ Read this document FULLY
2. ✅ Understand FEA is FROZEN
3. ✅ Understand API is FROZEN
4. ✅ Do NOT attempt modifications
5. ✅ Focus on Parametric Design & Geometry Creation

### During Phase 4:
1. ✅ Check git pre-commit hooks
2. ✅ If you accidentally modify FEA/API:
   - Do NOT commit
   - Restore frozen files: `git restore [frozen-file]`
   - Continue with non-frozen work

### Code Review:
1. ✅ Reject ANY PR that modifies FEA
2. ✅ Reject ANY PR that modifies API
3. ✅ Enforce with comment: "FEA/API frozen - cannot approve"
4. ✅ Close PR if author doesn't fix

---

## ✅ ACCEPTABLE WORK (Phase 4)

### Parametric Design (Week 1-2) ✅
```cpp
// ALLOWED: Create new parameter system
src/parameters/parameter_engine.cpp (NEW)

// NOT ALLOWED: Modify existing FEA
src/fea/... ❌
```

### Geometry Creation (Week 3-6) ✅
```cpp
// ALLOWED: Create new features
src/features/extrude.cpp (NEW)
src/features/revolve.cpp (NEW)
src/geometry/boolean_operations.cpp (NEW)

// NOT ALLOWED: Modify API routes
backend/src/api/routes/... ❌
```

### Assembly System (Week 7-8) ✅
```cpp
// ALLOWED: Create assembly constraints
src/assembly/assembly_constraints.cpp (NEW)

// NOT ALLOWED: Modify FEA
src/fea/... ❌
```

---

## 🔍 AUTOMATED VERIFICATION

### GitHub Actions: Frozen Component Check

```yaml
name: Frozen Components Verification

on: [push, pull_request]

jobs:
  check-frozen:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Check FEA modifications
        run: |
          if git diff HEAD~1 --name-only | grep -E "src/fea/|LinearElastic"; then
            echo "❌ ERROR: FEA component is FROZEN"
            exit 1
          fi
      
      - name: Check API modifications
        run: |
          if git diff HEAD~1 --name-only | grep "backend/src/api/routes/"; then
            echo "❌ ERROR: API routes are FROZEN"
            exit 1
          fi
      
      - name: Check Collaboration modifications
        run: |
          if git diff HEAD~1 --name-only | grep -E "ui/src/components/Collaboration/|backend/src/collaboration/"; then
            echo "❌ ERROR: Collaboration features are FROZEN"
            exit 1
          fi
      
      - name: Check Documentation modifications
        run: |
          if git diff HEAD~1 --name-only | grep -E "ui/src/components/Documentation/|backend/src/documentation/"; then
            echo "❌ ERROR: In-app Documentation system is FROZEN"
            exit 1
          fi
      
      - name: Report status
        run: echo "✅ No frozen components modified"
```

---

## 📞 IF YOU NEED TO MODIFY FROZEN COMPONENTS

**Current Status:** FORBIDDEN without explicit authorization

**To request unfreezing:**
1. Create GitHub Issue with:
   - Title: "[FREEZE] Request to unfreeze [component]"
   - Reason: Why modification is necessary
   - Impact: What will change
2. Wait for Project Lead approval
3. DO NOT proceed without explicit written approval

**Approval Required From:**
- [ ] Project Lead (mandatory)
- [ ] Architecture Lead (recommended)
- [ ] QA Lead (recommended)

---

## 📊 FROZEN STATUS SUMMARY

| Component | Status | Until | Authority |
|-----------|--------|-------|-----------|
| FEA (src/fea/) | 🔒 FROZEN | Authorization | Project Lead |
| API Routes | 🔒 FROZEN | Authorization | Project Lead |
| Collaboration Features | 🔒 FROZEN | Authorization | Project Lead |
| In-App Documentation | 🔒 FROZEN | Authorization | Project Lead |
| Parametric Design | ✅ OPEN | Phase 4 | Available |
| Geometry Ops (Extrude/Revolve/Boolean) | ✅ OPEN | Phase 4 | Available |
| Assembly System | ✅ OPEN | Phase 4 | Available |

---

## 🎯 PHASE 4 PRIORITIES (Revised)

### With Frozen Components:
```
Week 1-2:   Parametric Design ✅
Week 3-6:   Geometry Creation ✅
Week 7-8:   Assembly System ✅
Week 9-12:  Integration ✅

Total: 12 weeks
Code: 6500+ LOC (without FEA/API)
Tests: 120+ cases
Coverage: >85%
```

---

## 🚀 STARTING PHASE 4

**First Week (Week 1-2):**
- ❌ DO NOT touch FEA
- ❌ DO NOT modify API
- ✅ DO implement Parametric Design

**FROZEN components are LOCKED from modifications!**

---

**Status:** ACTIVE ENFORCEMENT  
**Authority:** Project Lead  
**Effective:** Immediately  
**Last Updated:** November 1, 2026
