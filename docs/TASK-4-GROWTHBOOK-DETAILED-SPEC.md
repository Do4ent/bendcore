# TASK 4: GrowthBook Analytics + A/B Testing Framework
## BendCore Q3 2027 - Enterprise Features (Part 2)

**Duration:** Days 9-22 (14 days, parallel with Tasks 1-3)  
**Target LOC:** 850+ (C++ + React + TypeScript)  
**Deadline:** 2026-11-06 (SHORTEST - critical for Q3 completion)  
**Dependencies:** Task 1 Phase 3 (import workflow) ✅  
**Status:** SPECIFICATION PHASE  

---

## 🎯 Task 4 Mission

Integrate GrowthBook analytics platform for feature tracking and A/B testing. Enable:
- Track 50+ user events (import, template match, search, formula creation)
- A/B test Q3 features with 50/50 split
- Feature flags for gradual rollout
- Analytics dashboard with 3+ experiments
- Real-time event batching and delivery
- Privacy-compliant tracking

---

## 📋 Subtask Breakdown (7 subtasks)

### **Subtask 1: GrowthBook SDK Integration** (Days 1-2, 120 LOC)
**Goal:** Setup GrowthBook client and configuration

```cpp
// include/bendcore/growthbook_client.hpp
class GrowthBookClient {
public:
  struct Config {
    std::string api_key;           // GrowthBook API key
    std::string sdk_key;           // SDK key for feature flags
    std::string environment;       // "development", "staging", "production"
    bool enable_analytics = true;
    bool track_page_views = false;
    double event_batch_delay_ms = 5000;  // Batch every 5 seconds
  };
  
  static GrowthBookClient& getInstance();
  
  void initialize(const Config& config);
  bool is_initialized() const;
  
  // Feature flag evaluation
  bool is_feature_enabled(const std::string& feature_name);
  std::string get_feature_value(const std::string& feature_name);
  
private:
  Config config_;
  bool initialized_ = false;
  std::mutex mutex_;
};
```

**Configuration:**
```
GrowthBook API Key: [from environment variable GB_API_KEY]
SDK Key: [from environment variable GB_SDK_KEY]
Environment: production (Windows deployment)
Event batching: 5 second intervals
Enable A/B testing: true
```

**Features:**
- Initialize GrowthBook with environment config
- Load feature flags from GrowthBook
- Cache flags locally with TTL
- Thread-safe flag evaluation
- Error handling for network issues

**Files to Create:**
- `include/bendcore/growthbook_client.hpp` (client class)
- `src/growthbook_client.cpp` (implementation)

**Tests:**
- `tests/test_growthbook_client.cpp` (6 tests)
  * Initialize client
  * Load feature flags
  * Evaluate flags
  * Network error handling
  * Cache behavior
  * Thread safety

---

### **Subtask 2: Analytics Event System** (Days 2-4, 200 LOC)
**Goal:** Build event tracking infrastructure

```cpp
// include/bendcore/analytics_event.hpp
enum class EventCategory {
  IMPORT,           // CAD import events
  TEMPLATE,         // Template operations
  SEARCH,           // Search & filtering
  FORMULA,          // Parametric dimension creation
  TUBE_DESIGN,      // Tube design operations
  EXPORT,           // Output/export operations
};

struct AnalyticsEvent {
  std::string event_name;        // "import_step_success", "template_match", etc.
  EventCategory category;
  std::map<std::string, std::string> properties;  // {od: "25.4", material: "Steel"}
  std::string user_id;           // Anonymous user ID or email
  std::string session_id;
  std::chrono::system_clock::time_point timestamp;
  std::string version;           // BendCore version
  
  json to_json() const;
};

class AnalyticsEventQueue {
public:
  void track_event(const AnalyticsEvent& event);
  void flush();  // Send batched events
  
  size_t get_queue_size() const;
  std::chrono::milliseconds get_last_flush_time() const;
  
private:
  std::vector<AnalyticsEvent> event_queue_;
  std::mutex mutex_;
  std::thread flush_thread_;
  
  void background_flush_loop();
};
```

**50+ Event Types:**

**Import Events (8):**
- `import_step_opened` - User opens STEP import dialog
- `import_step_success` - STEP file imported successfully
- `import_step_error` - STEP import failed
- `import_dxf_opened`
- `import_dxf_success`
- `import_dxf_error`
- `import_cancel` - User cancelled import
- `import_duration_seconds` - How long import took

**Template Events (8):**
- `template_list_opened` - Template library opened
- `template_match_requested` - Auto-match triggered
- `template_match_success` - Match found
- `template_match_no_results` - No good matches
- `template_selected` - User selected template
- `template_create` - User created custom template
- `template_edit` - User edited template
- `template_delete` - User deleted template

**Search Events (8):**
- `search_opened` - Search panel opened
- `search_query` - User executed search
- `search_results_count` - Result count
- `search_filter_applied` - Filter used
- `search_sort_applied` - Sort applied
- `search_save` - User saved search
- `search_load_saved` - User loaded saved search
- `search_export_csv` - User exported results

**Formula Events (8):**
- `formula_created` - Parametric dimension created
- `formula_edited` - Formula edited
- `formula_evaluated` - Formula evaluated
- `formula_error` - Formula evaluation error
- `formula_complexity` - Formula complexity (simple/moderate/complex)
- `formula_save` - Formula saved
- `formula_delete` - Formula deleted
- `formula_import_from_step` - Formula extracted from import

**Design Events (8):**
- `tube_created` - New tube created
- `tube_edited` - Tube modified
- `tube_preview` - 3D preview opened
- `tube_saved` - Tube design saved
- `tube_delete` - Tube deleted
- `bend_added` - Bend added to design
- `bend_modified` - Bend angle/radius changed
- `constraint_violation` - Design constraint violated

**Output Events (8):**
- `export_dwf` - Export to DWFx
- `export_pdf` - Export to PDF
- `export_step` - Export to STEP
- `export_csv` - Export data to CSV
- `export_success` - Export completed
- `export_error` - Export failed
- `print_preview` - Print preview opened
- `print_sent` - Sent to printer

**System Events (4):**
- `session_start` - User session started
- `session_end` - User session ended
- `app_error` - Application error occurred
- `feature_flag_evaluated` - Feature flag checked

**Files to Create:**
- `include/bendcore/analytics_event.hpp` (event structures)
- `src/analytics_event_queue.cpp` (event queue implementation)

**Tests:**
- `tests/test_analytics_event.cpp` (8 tests)
  * Create event
  * Queue event
  * Batch events
  * Flush queue
  * JSON serialization
  * Background flush loop
  * Thread safety
  * Event timestamp

---

### **Subtask 3: A/B Testing Framework** (Days 4-6, 150 LOC)
**Goal:** Implement feature flag variant assignment

```cpp
// include/bendcore/ab_testing.hpp
class ABTestManager {
public:
  enum class Variant {
    CONTROL,      // Original behavior
    TREATMENT,    // New feature
  };
  
  struct ExperimentConfig {
    std::string experiment_id;    // "template_match_v1", "search_ui_v2"
    double control_percentage;    // 50.0 for 50/50 split
    bool force_variant = false;   // For testing
    Variant forced_variant = Variant::CONTROL;
  };
  
  Variant get_variant(
    const ExperimentConfig& exp,
    const std::string& user_id
  );
  
private:
  uint32_t hash_user_to_variant(
    const std::string& user_id,
    const std::string& exp_id
  );
};
```

**Variant Assignment Algorithm:**
```
1. Hash user_id + experiment_id
2. Get hash modulo 100
3. If hash < control_percentage → CONTROL
4. Otherwise → TREATMENT
5. Consistent for same user (same hash always)
```

**A/B Tests (3 initial):**

1. **Template Match UI**
   - CONTROL: Show top 3 results in dropdown
   - TREATMENT: Show full template matcher panel with advanced options
   - Metrics: Conversion (template selected), time to decision
   - Duration: 2 weeks

2. **Search Filter Layout**
   - CONTROL: Vertical filter sidebar
   - TREATMENT: Horizontal filter toolbar
   - Metrics: Search volume, filter usage, result accuracy
   - Duration: 2 weeks

3. **Formula Editor UX**
   - CONTROL: Text input only (Phase 2 implementation)
   - TREATMENT: Enhanced with syntax highlighting + autocomplete
   - Metrics: Formula creation rate, error rate, user satisfaction
   - Duration: 2 weeks

**Files to Create:**
- `include/bendcore/ab_testing.hpp` (A/B test manager)
- `src/ab_testing.cpp` (implementation)

**Tests:**
- `tests/test_ab_testing.cpp` (6 tests)
  * 50/50 split distribution
  * Consistent variant per user
  * User ID hashing
  * Variant assignment
  * Force variant for testing
  * Experiment configuration

---

### **Subtask 4: Event Batching & Delivery** (Days 6-8, 100 LOC)
**Goal:** Batch events and send to GrowthBook

```cpp
// include/bendcore/event_batch_sender.hpp
class EventBatchSender {
public:
  struct BatchConfig {
    size_t max_batch_size = 50;          // Send when 50 events accumulated
    std::chrono::milliseconds max_wait = std::chrono::seconds(5);  // Or 5s timeout
  };
  
  void start(const BatchConfig& config);
  void stop();
  
  void send_batch(const std::vector<AnalyticsEvent>& events);
  
private:
  std::thread batch_thread_;
  void batch_sender_loop();
};
```

**Batching Strategy:**
1. Accumulate events in queue
2. When queue reaches 50 events OR 5 seconds elapsed:
   a. Create JSON batch
   b. Send to GrowthBook API
   c. Clear queue
   d. Log success/failure

**Features:**
- Batch up to 50 events per request
- Max 5 second delay
- Async background sender
- Network error retry with exponential backoff
- Event deduplication (same event within 1 second)

**Files to Create:**
- `include/bendcore/event_batch_sender.hpp` (batch sender)
- `src/event_batch_sender.cpp` (implementation)

**Tests:**
- `tests/test_event_batch_sender.cpp` (6 tests)
  * Batch accumulation
  * Size threshold trigger
  * Time threshold trigger
  * JSON format
  * Network error handling
  * Event deduplication

---

### **Subtask 5: React Analytics Dashboard** (Days 8-11, 250 LOC)
**Goal:** Build analytics visualization UI

```typescript
// src/ui/AnalyticsDashboard.tsx
interface AnalyticsDashboardProps {
  startDate: Date;
  endDate: Date;
  refreshInterval: number;  // seconds
}

export const AnalyticsDashboard: React.FC<AnalyticsDashboardProps> = ({
  startDate,
  endDate,
  refreshInterval,
}) => {
  // Sections:
  // 1. KPI Cards (user sessions, events, conversion rate)
  // 2. Feature Flag Status (enabled flags for current user)
  // 3. Active A/B Experiments (3 tests with metrics)
  // 4. Event Timeline (last 20 events)
  // 5. Funnel Analysis (import → design → export)
  // 6. Cohort Analysis (by feature flag variant)
};

// src/ui/ExperimentCard.tsx
// For each A/B test:
// - Experiment name
// - Variants (control vs treatment)
// - Sample sizes
// - Conversion rates
// - Statistical significance
// - Start/end dates
// - Status (running, completed, paused)

// src/ui/EventTimeline.tsx
// Show recent events:
// - Event name
// - User ID (anonymized)
// - Properties (OD: 25mm, material: Steel)
// - Timestamp
// - Variant assignment (if A/B test)
```

**Features:**
- Real-time KPI metrics (user sessions, events/minute)
- Feature flag status (show which flags enabled for current user)
- Active A/B experiment metrics (3 experiments)
- Event timeline (last 20 events with properties)
- Funnel visualization (import → design → export completion rates)
- Cohort analysis (split by variant: control vs treatment)
- Dark/light mode
- Auto-refresh every 30 seconds
- Export metrics to CSV
- WCAG AA accessibility

**Files to Create:**
- `src/ui/AnalyticsDashboard.tsx` (main dashboard)
- `src/ui/ExperimentCard.tsx` (experiment display)
- `src/ui/EventTimeline.tsx` (event history)
- `src/ui/KPICards.tsx` (metrics cards)
- `src/ui/FunnelChart.tsx` (funnel visualization)
- `src/ui/styles/AnalyticsDashboard.css` (styling)
- `src/ui/hooks/useAnalytics.ts` (React hook)

**Tests:**
- `src/ui/__tests__/AnalyticsDashboard.test.tsx` (15 tests)
  * Dashboard renders
  * KPI cards display
  * Feature flags shown
  * Experiments displayed
  * Event timeline loads
  * Funnel chart renders
  * Auto-refresh works
  * Export to CSV
  * Dark mode
  * Responsive layout
  * Keyboard navigation
  * Accessibility
  * Date range filter
  * Real-time updates
  * Error states

---

### **Subtask 6: API Integration** (Days 11-13, 80 LOC)
**Goal:** Backend endpoints for analytics

```typescript
// src/api/analytics_routes.ts
router.post('/events', (req, res) => {
  // Track individual event
  // Request: { event_name, category, properties }
  // Response: { success, event_id }
});

router.get('/experiments', (req, res) => {
  // Get active A/B experiments
  // Response: { experiments: Experiment[] }
});

router.get('/analytics/kpi', (req, res) => {
  // Get KPI metrics
  // Request: { start_date, end_date }
  // Response: { sessions, events, conversion_rate, variant_distribution }
});

router.get('/analytics/events', (req, res) => {
  // Get event timeline
  // Response: { events: Event[], total_count }
});

router.post('/analytics/export', (req, res) => {
  // Export metrics to CSV
  // Request: { start_date, end_date, metrics: string[] }
  // Response: CSV file download
});
```

**Features:**
- Track events from frontend
- Get active experiments
- Fetch KPI metrics
- Get event timeline
- Export analytics to CSV

**Files to Create:**
- `src/api/analytics_routes.ts` (Express routes)
- `src/ui/hooks/useAnalytics.ts` (React hook)

**Tests:**
- `tests/test_analytics_api.ts` (5 tests)
  * POST /events succeeds
  * GET /experiments returns active
  * GET /analytics/kpi returns metrics
  * GET /analytics/events returns timeline
  * POST /analytics/export returns CSV

---

### **Subtask 7: Integration Testing & Documentation** (Days 13-22, 50 LOC)
**Goal:** Complete testing and documentation

**E2E Test Workflow:**
```
1. Initialize GrowthBook client
2. Get feature flags
3. Evaluate A/B test variants
4. Track events throughout session:
   - Session start
   - Import STEP file
   - Select template
   - Create formula
   - Save design
   - Session end
5. Batch events and send
6. Verify events in GrowthBook
7. Check analytics dashboard
8. Verify A/B metrics calculated
```

**Documentation:**
- `docs/GROWTHBOOK_SETUP.md` - How to setup GrowthBook account, get API keys
- `docs/ANALYTICS_TRACKING.md` - Event tracking guide, all 50+ events documented
- `docs/AB_TESTING_GUIDE.md` - Running A/B tests, interpreting results
- `docs/ANALYTICS_API_REFERENCE.md` - Complete API reference

**Final QA:**
- All 40+ tests passing
- >85% code coverage
- No memory leaks
- Events properly batched and sent
- A/B variants correctly assigned
- Dashboard displays real data
- Performance: <50ms for analytics operations
- Privacy compliant (no PII tracking)
- Accessibility compliant (WCAG AA)

**Files to Create:**
- `tests/e2e_analytics.cpp` (end-to-end test)
- `docs/GROWTHBOOK_SETUP.md`
- `docs/ANALYTICS_TRACKING.md`
- `docs/AB_TESTING_GUIDE.md`
- `docs/ANALYTICS_API_REFERENCE.md`

**Tests:**
- `tests/e2e_analytics.cpp` (E2E test)
- Additional integration tests

---

## 📊 Task 4 Summary

| Subtask | LOC | Days | Tests | Status |
|---------|-----|------|-------|--------|
| 1. GrowthBook Setup | 120 | 2 | 6 | ⏳ |
| 2. Event System | 200 | 2 | 8 | ⏳ |
| 3. A/B Testing | 150 | 2 | 6 | ⏳ |
| 4. Batch Delivery | 100 | 2 | 6 | ⏳ |
| 5. React Dashboard | 250 | 3 | 15 | ⏳ |
| 6. API Integration | 80 | 2 | 5 | ⏳ |
| 7. Testing & Docs | 50 | 9 | E2E | ⏳ |
| **TOTAL** | **950** | **14** | **46+** | ⏳ |

---

## 🎯 Success Criteria

- ✅ GrowthBook SDK fully integrated
- ✅ 50+ event types tracked
- ✅ 3+ A/B experiments running
- ✅ Event batching with 5s timeout
- ✅ Analytics dashboard fully functional
- ✅ 46+ tests passing (>85% coverage)
- ✅ <50ms analytics operations
- ✅ Windows-only constraint enforced
- ✅ Documentation complete
- ✅ Privacy compliant

---

**Specification Status:** ✅ READY FOR IMPLEMENTATION  
**Last Updated:** 2026-10-23 22:50 UTC+2  
**Dependencies:** Task 1 Phase 3 (import events)
**Critical Note:** SHORTEST DEADLINE (Day 22) - Completes Q3 2027
