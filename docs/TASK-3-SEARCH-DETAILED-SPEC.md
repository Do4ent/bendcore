# TASK 3: Advanced Search & Filtering Engine
## BendCore Q3 2027 - Enterprise Features (Part 1)

**Duration:** Days 9-24 (16 days, parallel with Tasks 1 Phase 3 & 2)  
**Target LOC:** 900+ (C++ + React + tests)  
**Deadline:** 2026-11-08  
**Dependencies:** Task 1 Phase 3 (imported objects) ✅  
**Status:** SPECIFICATION PHASE  

---

## 🎯 Task 3 Mission

Build enterprise-grade search and filtering system for tube inventory. Enable users to:
- Search by 6+ dimensions and properties
- Combine filters (AND/OR logic)
- Full-text search with fuzzy matching
- Save search queries
- Index imported CAD objects
- Fast queries (<500ms on 1000+ items)

---

## 📋 Subtask Breakdown (5 subtasks)

### **Subtask 1: Search Index Infrastructure** (Days 1-3, 200 LOC)
**Goal:** Build high-performance search index

```cpp
// include/bendcore/search_index.hpp
class SearchIndex {
public:
  struct IndexedTube {
    std::string id;
    std::string designation;       // User-friendly name
    double outer_diameter;
    double inner_diameter;
    double wall_thickness;
    double total_length;
    std::string material;
    std::string material_grade;
    std::string manufacturing_method;
    std::vector<double> bend_angles;
    std::vector<double> bend_radii;
    std::string author;
    std::string created_at;
    std::string source_file;       // Which CAD file imported from
    std::vector<std::string> tags;
    double weight_kg;
    double cost_usd;
    std::string full_text;         // Concatenated searchable text
  };
  
  // Indexing
  void add_tube(const IndexedTube& tube);
  void remove_tube(const std::string& id);
  void update_tube(const IndexedTube& tube);
  void reindex_all(const std::vector<IndexedTube>& tubes);
  
  // Statistics
  size_t get_index_size() const;
  std::chrono::milliseconds get_last_reindex_time() const;
};

// include/bendcore/search_tokenizer.hpp
class SearchTokenizer {
public:
  std::vector<std::string> tokenize(const std::string& text);
  std::string normalize(const std::string& token);
  
private:
  // Normalize: lowercase, remove punctuation, handle unicode
};
```

**Indexing Strategy:**
1. For each tube, concatenate all searchable text
2. Tokenize (split by spaces, punctuation)
3. Normalize (lowercase, remove accents)
4. Store tokens with document ID for quick lookup
5. Build inverted index (token → [doc1, doc2, ...])

**Features:**
- Add/remove/update tubes
- Full-text tokenization
- Normalization (lowercase, accents)
- Inverted index (for fast lookup)
- In-memory index (no database)
- Reindex timing

**Files to Create:**
- `include/bendcore/search_index.hpp` (index class)
- `include/bendcore/search_tokenizer.hpp` (tokenizer)
- `src/search_index.cpp` (implementation)
- `src/search_tokenizer.cpp` (tokenizer implementation)

**Tests:**
- `tests/test_search_index.cpp` (10 tests)
  * Add tube to index
  * Remove tube
  * Update tube
  * Index size calculation
  * Reindex all tubes
  * Tokenization
  * Normalization
  * Unicode handling
  * Performance (1000 tubes)
  * Memory usage

---

### **Subtask 2: Query Engine with Filters** (Days 3-6, 250 LOC)
**Goal:** Build query language and execution engine

```cpp
// include/bendcore/search_query.hpp
enum class FilterOperator {
  EQUALS, NOT_EQUALS,
  GREATER_THAN, LESS_THAN, BETWEEN,
  CONTAINS, STARTS_WITH, ENDS_WITH,
  IN_LIST
};

enum class CombineLogic {
  AND, OR
};

struct FilterCriteria {
  std::string field;              // "od", "wall", "material", etc.
  FilterOperator op;
  std::string value;              // "25.4", "Steel", etc.
};

struct SearchQuery {
  std::string full_text_search;   // Optional: "25mm steel bend"
  std::vector<FilterCriteria> filters;
  CombineLogic logic = CombineLogic::AND;
  
  // Sorting
  std::string sort_by;            // "od", "cost", "created_at"
  bool sort_ascending = true;
  
  // Pagination
  int limit = 100;
  int offset = 0;
};

class QueryEngine {
public:
  struct SearchResult {
    std::vector<IndexedTube> tubes;
    int total_count;
    std::chrono::milliseconds query_time;
  };
  
  SearchResult execute(
    const SearchIndex& index,
    const SearchQuery& query
  );
  
private:
  std::vector<IndexedTube> apply_full_text(
    const SearchIndex& index,
    const std::string& text
  );
  std::vector<IndexedTube> apply_filters(
    const std::vector<IndexedTube>& candidates,
    const std::vector<FilterCriteria>& filters,
    CombineLogic logic
  );
};
```

**Query Examples:**

```
// Simple: Find all steel tubes
Query {
  filters: [
    { field: "material", op: EQUALS, value: "Steel" }
  ]
}

// Complex: 25-30mm OD, aluminum, cost <$100
Query {
  filters: [
    { field: "od", op: BETWEEN, value: "25-30" },
    { field: "material", op: EQUALS, value: "Aluminum" },
    { field: "cost_usd", op: LESS_THAN, value: "100" }
  ],
  logic: AND
}

// Full-text: "25mm steel 90 degree"
Query {
  full_text_search: "25mm steel 90 degree"
}

// Combined
Query {
  full_text_search: "bend",
  filters: [
    { field: "od", op: BETWEEN, value: "20-50" },
    { field: "material", op: IN_LIST, value: "Steel,Aluminum" }
  ],
  logic: AND,
  sort_by: "cost_usd",
  sort_ascending: true,
  limit: 50
}
```

**Fields Supported:**
- `od`: Outer diameter (numeric range)
- `wall`: Wall thickness (numeric range)
- `length`: Total length (numeric range)
- `material`: Material name (string, exact match)
- `material_grade`: Grade (e.g., "304 Stainless")
- `method`: Manufacturing method
- `author`: Who created it
- `cost_usd`: Cost (numeric)
- `weight_kg`: Weight (numeric)
- `created_at`: Creation date (date range)
- Full-text: Any text in designation, tags, notes

**Files to Create:**
- `include/bendcore/search_query.hpp` (query structures)
- `include/bendcore/query_engine.hpp` (query executor)
- `src/query_engine.cpp` (implementation)

**Tests:**
- `tests/test_query_engine.cpp` (12 tests)
  * Simple equality filter
  * Range filter (between)
  * Full-text search
  * Multiple filters (AND)
  * Multiple filters (OR)
  * Sorting by field
  * Pagination (limit/offset)
  * Performance (1000 tubes, <500ms)
  * Complex combined query
  * Nonexistent field handling
  * Empty result set
  * Special characters in search

---

### **Subtask 3: Full-Text Search with Fuzzy Matching** (Days 6-9, 180 LOC)
**Goal:** Implement Levenshtein distance for typo tolerance

```cpp
// include/bendcore/fuzzy_search.hpp
class FuzzySearcher {
public:
  struct FuzzyMatch {
    std::string term;
    double similarity;  // 0.0-1.0
  };
  
  std::vector<FuzzyMatch> find_matches(
    const std::string& search_term,
    const std::vector<std::string>& candidates,
    double min_similarity = 0.8
  );
  
private:
  double calculate_similarity(
    const std::string& s1,
    const std::string& s2
  );  // Levenshtein distance
};
```

**Algorithm:**
```
For "25mm steel bend" search:
1. Tokenize: ["25mm", "steel", "bend"]
2. For each token, find similar terms in index:
   - "25mm" → exact: ["25mm"], fuzzy: ["25.4mm" @ 0.95, "25mm" @ 1.0]
   - "steel" → exact: ["Steel"] (normalize), fuzzy: ["stainless" @ 0.7]
   - "bend" → exact: ["bend"], fuzzy: ["bends" @ 0.95]
3. Score documents containing these terms
4. Combine scores with BM25 ranking
5. Return top results
```

**Levenshtein Distance:**
- Measures minimum edits (insert, delete, replace) to transform one string to another
- Convert to similarity: `1.0 - (levenshtein_distance / max_length)`
- Min similarity threshold: 0.8 (allows ~20% typos)

**Features:**
- Fuzzy matching for typos ("sleel" → "Steel")
- Stem-based matching ("bending" → "bend")
- BM25 ranking (TF-IDF variant)
- Performance: <50ms for 1000-term search

**Files to Create:**
- `include/bendcore/fuzzy_search.hpp` (fuzzy searcher)
- `src/fuzzy_search.cpp` (Levenshtein + BM25 implementation)

**Tests:**
- `tests/test_fuzzy_search.cpp` (8 tests)
  * Exact match
  * Single character typo
  * Multiple typos
  * Case insensitive
  * Stemming (bend/bends)
  * Similarity threshold
  * Performance (1000 terms)
  * No matches found

---

### **Subtask 4: React SearchPanel Component** (Days 9-12, 250 LOC)
**Goal:** Build user interface for advanced search

```typescript
// src/ui/SearchPanel.tsx
interface SearchPanelProps {
  onSearch: (results: IndexedTube[]) => void;
  onSaveSearch: (name: string, query: SearchQuery) => void;
}

export const SearchPanel: React.FC<SearchPanelProps> = ({
  onSearch,
  onSaveSearch,
}) => {
  // Layout:
  // 1. Search Bar (text input with autocomplete)
  // 2. Filter Panel (expandable sections)
  //    - Dimension filters (OD, wall, length)
  //    - Property filters (material, method, author)
  //    - Cost range slider
  //    - Date range picker
  // 3. Filter Logic toggle (AND vs OR)
  // 4. Sort options (field + direction)
  // 5. Results display (virtualized table)
  // 6. Save search button
};

// src/ui/SearchResults.tsx
// Virtualized table (1000+ rows, smooth scrolling):
// Columns: Designation, OD, Wall, Length, Material, Method, Cost, Created
// Row click → open in TubeDialog
// Export to CSV button

// src/ui/SavedSearches.tsx
// List of saved queries:
// - Quick search buttons
// - Edit/delete saved searches
// - localStorage persistence
// - Drag-drop to reorder

// src/ui/FilterPanel.tsx
// Expandable filter sections with:
// - Range inputs (OD, wall, length)
// - Dropdown (material, method)
// - Autocomplete (author, tags)
// - Date pickers (created_at range)
// - Toggle AND/OR logic
```

**Features:**
- Full-text search with autocomplete
- 6+ filter fields (OD, wall, material, method, cost, date)
- AND/OR filter logic toggle
- Sorting (by any field, ascending/descending)
- Pagination (50/100/200 per page)
- Virtualized results table (fast scrolling 1000+ items)
- Save search queries (5+ saved searches)
- Export to CSV
- Real-time result count
- Dark/light mode support
- Full keyboard navigation
- WCAG AA accessibility

**Files to Create:**
- `src/ui/SearchPanel.tsx` (main search panel)
- `src/ui/SearchResults.tsx` (results table)
- `src/ui/FilterPanel.tsx` (filter UI)
- `src/ui/SavedSearches.tsx` (saved queries)
- `src/ui/styles/SearchUI.css` (styling)
- `src/ui/hooks/useSearch.ts` (React hook for API)

**Tests:**
- `src/ui/__tests__/SearchPanel.test.tsx` (15 tests)
  * Text search input
  * Filter input (OD range, material)
  * AND/OR toggle
  * Sort options
  * Results display (virtual list)
  * Pagination
  * Save search
  * Load saved search
  * Delete saved search
  * Export to CSV
  * Autocomplete
  * Dark mode
  * Keyboard navigation
  * Accessibility
  * Real-time result count

---

### **Subtask 5: API Integration & Final QA** (Days 12-16, 20 LOC)
**Goal:** Backend endpoints and comprehensive testing

```typescript
// src/api/search_routes.ts
router.post('/search', async (req, res) => {
  // Execute search query
  // Request: SearchQuery object
  // Response: { tubes: IndexedTube[], total_count, query_time_ms }
});

router.get('/search/saved', (req, res) => {
  // Get user's saved searches
  // Response: { searches: SavedSearch[] }
});

router.post('/search/saved', (req, res) => {
  // Save new search
  // Request: { name: string, query: SearchQuery }
  // Response: { success, id }
});

router.delete('/search/saved/:id', (req, res) => {
  // Delete saved search
  // Response: { success }
});

router.post('/search/export', (req, res) => {
  // Export search results to CSV
  // Request: SearchQuery
  // Response: CSV file download
});
```

**E2E Tests:**
```
1. Load 1000 tubes from imported CAD files
2. Index all tubes
3. Execute complex query (25-30mm OD, steel, cost <$100)
4. Verify results in <500ms
5. Sort by cost
6. Verify sorting correct
7. Save search query
8. Load saved search
9. Verify results same as original
10. Export to CSV
11. Verify CSV format
```

**Final QA:**
- All 45+ tests passing (>85% coverage)
- Performance: <500ms for 1000 items
- Memory: no leaks
- UI responsive and smooth
- Accessibility compliant (WCAG AA)
- Dark mode working
- Documentation complete

**Files to Create:**
- `src/api/search_routes.ts` (endpoints)
- `docs/SEARCH_USER_GUIDE.md` (user documentation)
- `docs/SEARCH_API_REFERENCE.md` (API reference)

**Tests:**
- `tests/test_search_api.ts` (5 tests)
  * POST /search executes
  * GET /search/saved retrieves
  * POST /search/saved saves
  * DELETE /search/saved/:id deletes
  * POST /search/export returns CSV

---

## 📊 Task 3 Summary

| Subtask | LOC | Days | Tests | Status |
|---------|-----|------|-------|--------|
| 1. Search Index | 200 | 3 | 10 | ⏳ |
| 2. Query Engine | 250 | 3 | 12 | ⏳ |
| 3. Fuzzy Search | 180 | 3 | 8 | ⏳ |
| 4. React UI | 250 | 3 | 15 | ⏳ |
| 5. API & QA | 20 | 4 | E2E | ⏳ |
| **TOTAL** | **900** | **16** | **45+** | ⏳ |

---

## 🎯 Success Criteria

- ✅ 6+ searchable fields
- ✅ Full-text search with fuzzy matching
- ✅ AND/OR filter logic
- ✅ Saved search queries (localStorage)
- ✅ <500ms search on 1000+ items
- ✅ React UI fully functional
- ✅ 45+ tests passing (>85% coverage)
- ✅ Export to CSV
- ✅ Windows-only constraint enforced
- ✅ Documentation complete

---

**Specification Status:** ✅ READY FOR IMPLEMENTATION  
**Last Updated:** 2026-10-23 22:45 UTC+2  
**Dependency:** Task 1 Phase 3 (imported objects for indexing)
