# TASK 2: Smart Templates + Auto-Selection Engine
## BendCore Q3 2027 - Parametric Dimensions (Part 2)

**Duration:** Days 9-25 (17 days, parallel with Task 1 Phase 3)  
**Target LOC:** 850+ (C++ + React + tests)  
**Deadline:** 2026-11-09  
**Dependencies:** Task 1 Phase 3 (parametric dimensions) ✅  
**Status:** SPECIFICATION PHASE  

---

## 🎯 Task 2 Mission

Build intelligent template system for tube bending profiles. Auto-select optimal templates based on imported geometry or manual parameters. Enable users to:
- Choose from 3+ predefined tube bending templates
- Auto-match imported tubes to best template
- Create custom templates
- Manage template library
- Get manufacturing recommendations

---

## 📋 Subtask Breakdown (7 subtasks)

### **Subtask 1: Template Data Model** (Days 1-2, 120 LOC)
**Goal:** Define template structure and persistence

```cpp
// include/bendcore/template_model.hpp
struct TubeTemplate {
  std::string id;                    // "std-90-deg", "reinforced-45", etc.
  std::string name;                  // "Standard 90° Bend"
  std::string description;
  
  // Geometry constraints
  double min_od;                     // Minimum outer diameter
  double max_od;                     // Maximum outer diameter
  double min_wall;
  double max_wall;
  double min_bend_radius_ratio;      // R/D ratio (e.g., 2.0 = 2*OD)
  
  // Bend constraints
  int max_consecutive_bends;
  double max_bend_angle;             // e.g., 180.0
  double min_bend_spacing;           // Minimum distance between bends
  
  // Manufacturing info
  std::string material_family;       // "Steel", "Aluminum", "Copper"
  std::vector<std::string> recommended_materials;
  std::string manufacturing_method;  // "3D bending", "CNC", "Hydraulic"
  double estimated_setup_time_min;
  
  // Cost estimation
  double cost_per_unit;
  double min_batch_size;
  
  // Metadata
  std::string author;
  std::string created_at;
  int popularity_score;              // 0-100 based on usage
};

class TemplateLibrary {
public:
  std::vector<TubeTemplate> get_all();
  std::optional<TubeTemplate> get_by_id(const std::string& id);
  void add_template(const TubeTemplate& tmpl);
  void update_template(const TubeTemplate& tmpl);
  void save_to_file(const std::string& path);
  void load_from_file(const std::string& path);
};
```

**3 Predefined Templates:**

1. **Standard 90° Bend** (std-90)
   - OD range: 6-100mm
   - Wall: 0.5-5mm
   - R/D ratio: 1.5-3.0
   - Max consecutive: 2
   - Material: Steel, Aluminum
   - Method: 3D CNC bending
   - Cost: $50/unit
   - Popularity: 95/100

2. **Reinforced Small Diameter** (rfd-small)
   - OD range: 3-25mm
   - Wall: 1.0-3mm
   - R/D ratio: 2.0-4.0
   - Max consecutive: 1
   - Material: Steel (high-strength)
   - Method: Hydraulic
   - Cost: $75/unit
   - Popularity: 88/100

3. **Large Assembly Profile** (lg-asm)
   - OD range: 50-200mm
   - Wall: 2.0-10mm
   - R/D ratio: 3.0-6.0
   - Max consecutive: 3
   - Material: Aluminum (weight-critical)
   - Method: CNC 5-axis
   - Cost: $120/unit
   - Popularity: 82/100

**Files to Create:**
- `include/bendcore/template_model.hpp` (template structures)
- `src/template_model.cpp` (persistence, serialization)
- `data/templates.json` (predefined templates library)

**Tests:**
- `tests/test_template_model.cpp` (8 tests)
  * Load/save templates
  * Get by ID
  * Add/update templates
  * JSON serialization
  * Constraint validation
  * Cost estimation

---

### **Subtask 2: Auto-Selection Engine** (Days 2-4, 200 LOC)
**Goal:** Match tubes to best templates with confidence scoring

```cpp
// include/bendcore/template_matcher.hpp
class TemplateMatcher {
public:
  struct MatchResult {
    std::string template_id;
    double confidence;      // 0.0-1.0
    std::vector<std::string> constraint_violations;  // What doesn't fit
    std::vector<std::string> recommendations;
  };
  
  std::vector<MatchResult> find_best_matches(
    const DimensionFormula::Variables& tube_params,
    int num_results = 3
  );
  
private:
  double score_od_fit(double od, const TubeTemplate& tmpl);
  double score_wall_fit(double wall, const TubeTemplate& tmpl);
  double score_bend_fit(double bend_angle, const TubeTemplate& tmpl);
  double score_radius_ratio(double actual_ratio, const TubeTemplate& tmpl);
};
```

**Matching Algorithm:**

```
1. Iterate through all templates
2. For each template:
   a. Check OD range → score 0-1 (1.0 if in range, 0.5 if slightly over/under, 0.0 if way off)
   b. Check wall range → score 0-1
   c. Check bend angles → score 0-1 (penalize if max_angle exceeded)
   d. Check bend spacing → score 0-1
   e. Calculate R/D ratio from imported geometry → score against template range
   f. Combine scores (weighted average, weights: OD=30%, wall=20%, bend=25%, radius=15%, spacing=10%)
   g. Identify violations (hard constraints)
   h. Generate recommendations ("Wall thickness slightly thin, consider Type XYZ instead")
3. Sort by confidence
4. Return top 3 matches
```

**Confidence Scoring:**
- 95-100%: Perfect match (all constraints satisfied, within optimal ranges)
- 85-95%: Good match (minor issues, can proceed)
- 75-85%: Acceptable (some constraints violated, custom setup needed)
- <75%: Poor match (not recommended, suggest alternatives)

**Files to Create:**
- `include/bendcore/template_matcher.hpp` (matcher class)
- `src/template_matcher.cpp` (matching algorithm)

**Tests:**
- `tests/test_template_matcher.cpp` (10 tests)
  * Perfect match scoring
  * OD slightly out of range
  * Wall thickness violation
  * Bend angle violation
  * Multiple violations
  * Confidence calculation
  * Top 3 results
  * No matches found
  * Edge cases
  * Recommendation generation

---

### **Subtask 3: Template Editor React Component** (Days 4-6, 200 LOC)
**Goal:** UI for viewing/editing templates

```typescript
// src/ui/TemplateEditor.tsx
interface TemplateEditorProps {
  template: TubeTemplate;
  onSave: (template: TubeTemplate) => void;
  onCancel: () => void;
}

export const TemplateEditor: React.FC<TemplateEditorProps> = ({
  template,
  onSave,
  onCancel,
}) => {
  // Sections:
  // 1. Basic Info (name, description, author)
  // 2. Geometry Constraints (OD min/max, wall min/max, R/D ratio)
  // 3. Bend Constraints (max bends, max angle, min spacing)
  // 4. Manufacturing (method, material family, estimated time)
  // 5. Cost (per unit, min batch)
  // 6. Popularity score (read-only, calculated from usage)
  
  // Features:
  // - Input validation (ranges must be logical)
  // - Real-time constraint checking
  // - Save/Cancel buttons
  // - Dark/light mode
  // - WCAG AA accessibility
};

// src/ui/TemplateLibraryView.tsx
// List view of all templates with:
// - Grid/list toggle
// - Filter by material, method
// - Sort by name, popularity, cost
// - Preview card for each template (name, OD range, cost, popularity)
// - Create new template button
// - Edit/delete buttons

// src/ui/TemplateMatcher.tsx
// Match results display:
// - Input section (OD, wall, length, bend count from imported geometry)
// - Results table (template name, confidence %, constraints, recommendations)
// - Color coding (green=good, yellow=warning, red=violation)
// - Apply button (select template for current tube)
```

**Features:**
- View all templates in grid/list view
- Create new template from scratch
- Edit existing templates
- Delete custom templates (keep predefined ones)
- Filter by material, manufacturing method
- Sort by name, popularity, cost
- See real-time constraint checking
- Match imported tubes to templates with scoring
- Dark/light mode support
- Full keyboard navigation
- WCAG AA accessibility

**Files to Create:**
- `src/ui/TemplateEditor.tsx` (edit component)
- `src/ui/TemplateLibraryView.tsx` (library list)
- `src/ui/TemplateMatcher.tsx` (matching results)
- `src/ui/styles/TemplateUI.css` (styling)

**Tests:**
- `src/ui/__tests__/TemplateUI.test.tsx` (12 tests)
  * Template list renders
  * Filter works
  * Sort works
  * Create template
  * Edit template
  * Delete template
  * Constraint validation
  * Match results display
  * Color coding
  * Dark mode
  * Keyboard navigation
  * Accessibility

---

### **Subtask 4: Template Persistence** (Days 6-8, 100 LOC)
**Goal:** Save/load templates to database or JSON

```cpp
// include/bendcore/template_storage.hpp
class TemplateStorage {
public:
  void save_user_templates(const std::vector<TubeTemplate>& templates);
  std::vector<TubeTemplate> load_user_templates();
  void load_default_templates();
  
private:
  std::string get_templates_path();  // %APPDATA%/BendCore/templates.json
};
```

**Storage Location:**
- Windows: `%APPDATA%/BendCore/templates.json`
- JSON format with pretty-print
- Auto-backup on save
- Version field for future compatibility

**Files to Create:**
- `include/bendcore/template_storage.hpp` (storage class)
- `src/template_storage.cpp` (persistence implementation)

**Tests:**
- `tests/test_template_storage.cpp` (5 tests)
  * Save templates to file
  * Load templates from file
  * Backup created on save
  * Corrupt file handling
  * Default templates loaded

---

### **Subtask 5: API Endpoints** (Days 8-10, 80 LOC)
**Goal:** Backend endpoints for template operations

```typescript
// src/api/template_routes.ts
router.get('/templates', (req, res) => {
  // Returns all templates (predefined + user)
  // Response: { templates: TubeTemplate[], last_updated: timestamp }
});

router.post('/templates/match', (req, res) => {
  // Match tube parameters to templates
  // Request: { od, wall, length, bend_count, bend_angles, bend_radii }
  // Response: { matches: MatchResult[], timestamp }
});

router.post('/templates', (req, res) => {
  // Create new template
  // Request: { template: TubeTemplate }
  // Response: { success, id, message }
});

router.put('/templates/:id', (req, res) => {
  // Update template
  // Request: { template: TubeTemplate }
  // Response: { success, message }
});

router.delete('/templates/:id', (req, res) => {
  // Delete user template (not predefined)
  // Response: { success, message }
});
```

**Features:**
- Get all templates
- Match parameters to templates (confidence scoring)
- CRUD operations (create, read, update, delete)
- User template persistence
- Error handling

**Files to Create:**
- `src/api/template_routes.ts` (Express routes)

**Tests:**
- `tests/test_template_api.ts` (6 tests)
  * GET /templates returns all
  * POST /templates/match works
  * POST /templates creates
  * PUT /templates/:id updates
  * DELETE /templates/:id removes
  * Error handling

---

### **Subtask 6: TubeDialog Integration** (Days 10-12, 100 LOC)
**Goal:** Integrate templates into TubeDialog

```typescript
// Existing: src/ui/TubeDialog.tsx
// Add new tab: "Template Suggestions"
// 
// When user enters OD/Wall/Length/Bends:
// 1. Auto-call /api/templates/match
// 2. Display top 3 templates
// 3. Show confidence % for each
// 4. Allow quick apply (fill in dialog from template)
// 5. Link to full template editor
```

**Features:**
- New "Template Suggestions" tab in TubeDialog
- Real-time matching as user types
- Top 3 suggestions with confidence scores
- Quick apply button (pre-fill dialog)
- Open template editor link
- Show manufacturing recommendations
- Color-coded confidence (green/yellow/red)

**Files to Modify:**
- `src/ui/TubeDialog.tsx` (add template tab)
- `src/ui/TubeDialog.css` (styling for new tab)

**Tests:**
- Update `src/ui/__tests__/TubeDialog.test.tsx` (4 new tests)
  * Template tab renders
  * Real-time matching works
  * Quick apply fills dialog
  * Template editor link works

---

### **Subtask 7: Documentation + Final QA** (Days 12-17, 50 LOC)
**Goal:** Complete documentation and integration testing

**Documentation:**
- `docs/TEMPLATE_USER_GUIDE.md` (how to use templates, create custom)
- `docs/TEMPLATE_API_REFERENCE.md` (all template APIs)
- `docs/TEMPLATE_MATCHING_ALGORITHM.md` (how auto-selection works)

**E2E Tests:**
- Load imported tube
- Auto-match to templates
- Select template
- Apply template to tube
- Verify formulas updated
- Save and reload

**Final QA:**
- All 31 unit tests passing
- E2E tests passing
- Performance: <100ms for matching
- Memory: no leaks
- Documentation complete
- Accessibility reviewed (WCAG AA)
- Code style consistent

---

## 📊 Task 2 Summary

| Subtask | LOC | Days | Tests | Status |
|---------|-----|------|-------|--------|
| 1. Template Model | 120 | 2 | 8 | ⏳ |
| 2. Auto-Selection | 200 | 2 | 10 | ⏳ |
| 3. React UI | 200 | 2 | 12 | ⏳ |
| 4. Persistence | 100 | 2 | 5 | ⏳ |
| 5. API Endpoints | 80 | 2 | 6 | ⏳ |
| 6. TubeDialog Integration | 100 | 2 | 4 | ⏳ |
| 7. Documentation & QA | 50 | 5 | E2E | ⏳ |
| **TOTAL** | **850** | **17** | **45+** | ⏳ |

**Expected Results:**
- ✅ 850 LOC production code
- ✅ 45+ unit + E2E tests
- ✅ >85% code coverage
- ✅ 3 predefined templates
- ✅ Auto-matching engine
- ✅ Full React UI
- ✅ Production-ready

---

## 🎯 Success Criteria

- ✅ 3+ predefined templates implemented
- ✅ Auto-selection algorithm matches tubes with 90%+ accuracy
- ✅ React UI fully integrated with TubeDialog
- ✅ Template CRUD operations working
- ✅ Confidence scoring accurate
- ✅ 45+ tests passing (>85% coverage)
- ✅ <100ms matching time
- ✅ Windows-only constraint enforced
- ✅ Documentation complete
- ✅ Ready for Task 3

---

## 📅 Timeline

```
Days 1-2:   Template Model + predefined templates
Days 3-4:   Auto-Selection Engine
Days 5-6:   React UI Components
Days 7-8:   Persistence + API Endpoints
Days 9-10:  TubeDialog Integration
Days 11-17: Documentation, E2E tests, final QA
```

Parallel with Task 1 Phase 3 (Days 9-25)
Parallel with Tasks 3, 4 (Days 16-25)

---

**Specification Status:** ✅ READY FOR IMPLEMENTATION  
**Last Updated:** 2026-10-23 22:40 UTC+2  
**Dependency:** Task 1 Phase 3 (OCCT integration)
