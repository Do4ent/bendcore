# Analytics Event Tracking Guide

## Overview

This guide documents all 50+ event types tracked in BendCore analytics. Events are organized by category and include recommended properties for each event type.

## Event Tracking Overview

### How Events Work

1. User performs action (e.g., imports STEP file)
2. Event is tracked with event name and properties
3. Events are batched (up to 50 events or 5 second timeout)
4. Batch is sent to GrowthBook API
5. GrowthBook aggregates events for analytics
6. Experiments measure event-based metrics

### Event Properties

Each event can include custom properties (key-value pairs):

```cpp
AnalyticsEvent event;
event.properties["file_size"] = "2048";
event.properties["duration_ms"] = "3500";
event.properties["success"] = "true";
```

## Event Categories & Types

### 1. IMPORT Events (8 events)

Events related to CAD file import workflows.

#### import_step_opened
- **Description**: User opens STEP import dialog
- **When**: User clicks "Import" → "STEP File..."
- **Properties**:
  - `dialog_type`: "step" | "dxf"
  - `from_menu`: true | false
- **Tracking Code**:
```cpp
queue.track_event(events::import_step_opened(user_id, session_id));
```

#### import_step_success
- **Description**: STEP file imported successfully
- **When**: Import dialog closes with success
- **Properties**:
  - `file_size`: File size in bytes (string)
  - `duration_ms`: Import duration in milliseconds (string)
  - `geometry_count`: Number of geometries detected
  - `face_count`: Number of faces in model
- **Tracking Code**:
```cpp
queue.track_event(events::import_step_success(user_id, session_id, {
  {"file_size", "2048"},
  {"duration_ms", "3500"},
  {"geometry_count", "12"}
}));
```

#### import_step_error
- **Description**: STEP import failed
- **When**: Import dialog shows error message
- **Properties**:
  - `error`: Error message text
  - `error_code`: Error code
  - `duration_ms`: How long before error (string)
- **Tracking Code**:
```cpp
queue.track_event(events::import_step_error(user_id, session_id, 
  "Unsupported STEP version"));
```

#### import_dxf_opened / import_dxf_success / import_dxf_error
- **Similar to STEP events** but for DXF file format
- Same property schema as STEP events

#### import_cancel
- **Description**: User cancelled import
- **When**: User clicks Cancel in import dialog
- **Properties**: None

#### import_duration_seconds
- **Description**: Total import operation duration
- **When**: Import complete
- **Properties**:
  - `duration_seconds`: Duration in seconds (string)
  - `success`: true | false

### 2. TEMPLATE Events (8 events)

Events related to template matching and selection.

#### template_list_opened
- **Description**: Template library dialog opened
- **When**: User clicks "Templates" menu item
- **Properties**: None

#### template_match_requested
- **Description**: Auto-match feature triggered
- **When**: User clicks "Find Matching Templates" button
- **Properties**:
  - `search_type`: "auto" | "manual"

#### template_match_success
- **Description**: Matching found templates
- **When**: Match completes with results
- **Properties**:
  - `match_count`: Number of matches found (string)
  - `top_score`: Best match score 0-1 (string)
  - `duration_ms`: Match duration (string)

#### template_match_no_results
- **Description**: No matching templates found
- **When**: Match completes with no results
- **Properties**:
  - `search_criteria`: Brief description of search

#### template_selected
- **Description**: User selected a template
- **When**: User double-clicks or confirms template selection
- **Properties**:
  - `template_id`: Template identifier
  - `template_name`: Template display name
  - `match_score`: Match score if from auto-match (string)

#### template_create
- **Description**: User created custom template
- **When**: Custom template saved
- **Properties**:
  - `template_name`: New template name
  - `base_template`: Base template used (if any)

#### template_edit
- **Description**: User edited template
- **When**: Template modifications saved
- **Properties**:
  - `template_id`: Template identifier
  - `fields_modified`: Count of fields changed (string)

#### template_delete
- **Description**: User deleted template
- **When**: Template deletion confirmed
- **Properties**:
  - `template_id`: Template identifier
  - `is_custom`: true | false

### 3. SEARCH Events (8 events)

Events related to search and filtering functionality.

#### search_opened
- **Description**: Search panel opened
- **When**: User clicks search icon/button
- **Properties**: None

#### search_query
- **Description**: User executed search
- **When**: User enters search text and presses Enter
- **Properties**:
  - `query`: Search query text
  - `query_length`: Query text length (string)
  - `filter_count`: Number of filters applied (string)

#### search_results_count
- **Description**: Search returned results
- **When**: Results displayed
- **Properties**:
  - `result_count`: Number of results (string)
  - `display_count`: Results shown on screen (string)
  - `query_time_ms`: Query execution time (string)

#### search_filter_applied
- **Description**: User applied filter
- **When**: Filter checkbox/selection changed
- **Properties**:
  - `filter_name`: Name of filter (e.g., "material")
  - `filter_value`: Selected value
  - `active_filters`: Total active filters (string)

#### search_sort_applied
- **Description**: User changed sort order
- **When**: Sort dropdown changed
- **Properties**:
  - `sort_field`: Field being sorted (e.g., "relevance")
  - `sort_direction`: "asc" | "desc"

#### search_save
- **Description**: User saved search
- **When**: User names and saves current search
- **Properties**:
  - `search_name`: Name given to saved search
  - `filter_count`: Number of filters (string)

#### search_load_saved
- **Description**: User loaded saved search
- **When**: User selects saved search
- **Properties**:
  - `search_name`: Name of loaded search

#### search_export_csv
- **Description**: User exported search results
- **When**: Export CSV completed
- **Properties**:
  - `result_count`: Number of results exported (string)
  - `export_time_ms`: Export duration (string)

### 4. FORMULA Events (8 events)

Events related to parametric dimension creation.

#### formula_created
- **Description**: User created formula/dimension
- **When**: New formula entered and confirmed
- **Properties**:
  - `formula_length`: Formula text length (string)
  - `variable_count`: Number of variables (string)

#### formula_edited
- **Description**: User modified formula
- **When**: Formula text changed and saved
- **Properties**:
  - `formula_id`: Formula identifier
  - `change_type`: "text" | "variable" | "operator"

#### formula_evaluated
- **Description**: Formula evaluated successfully
- **When**: Formula calculations completed
- **Properties**:
  - `formula_id`: Formula identifier
  - `result_value`: Calculated result (string)
  - `evaluation_time_ms`: Calculation time (string)

#### formula_error
- **Description**: Formula evaluation error
- **When**: Formula contains syntax/math error
- **Properties**:
  - `error_type`: "syntax" | "math" | "undefined"
  - `error_message`: Error description

#### formula_complexity
- **Description**: Complex formula detected
- **When**: Formula complexity exceeds threshold
- **Properties**:
  - `complexity_score`: Complexity value (string)
  - `complexity_level": "simple" | "moderate" | "complex"

#### formula_save
- **Description**: User saved formula
- **When**: Formula stored for reuse
- **Properties**:
  - `formula_id`: Formula identifier
  - `formula_name`: Display name

#### formula_delete
- **Description**: User deleted formula
- **When**: Formula removed from library
- **Properties**:
  - `formula_id`: Formula identifier

#### formula_import_from_step
- **Description**: Formula extracted from STEP import
- **When**: Dimension found during import
- **Properties**:
  - `extracted_count`: Number of dimensions (string)
  - `import_id`: Related import event ID

### 5. TUBE_DESIGN Events (8 events)

Events related to tube/pipe design operations.

#### tube_created
- **Description**: New tube design created
- **When**: User creates new tube
- **Properties**:
  - `od`: Outer diameter (string)
  - `wall_thickness`: Wall thickness (string)
  - `material`: Material type

#### tube_edited
- **Description**: Tube properties modified
- **When**: Tube properties changed
- **Properties**:
  - `tube_id`: Tube identifier
  - `property_changed`: Property name
  - `old_value`: Previous value (string)
  - `new_value`: New value (string)

#### tube_preview
- **Description**: 3D preview opened
- **When**: User clicks preview button
- **Properties**:
  - `tube_id`: Tube identifier
  - `preview_mode`: "3d" | "2d" | "cross-section"

#### tube_saved
- **Description**: Tube design saved
- **When**: Design persisted to storage
- **Properties**:
  - `tube_id`: Tube identifier
  - `design_name`: User-given name
  - `geometry_complexity`: Vertex count (string)

#### tube_delete
- **Description**: Tube design deleted
- **When**: Design removed
- **Properties**:
  - `tube_id`: Tube identifier

#### bend_added
- **Description**: Bend added to design
- **When**: Bend point/segment created
- **Properties**:
  - `tube_id`: Tube identifier
  - `bend_radius`: Bend radius (string)
  - `bend_angle`: Bend angle degrees (string)

#### bend_modified
- **Description**: Bend parameters changed
- **When**: Bend angle/radius adjusted
- **Properties**:
  - `tube_id`: Tube identifier
  - `bend_index`: Bend position (string)
  - `old_radius`: Previous radius (string)
  - `new_radius`: New radius (string)

#### constraint_violation
- **Description**: Design constraint violated
- **When**: Design violates manufacturing limits
- **Properties**:
  - `tube_id`: Tube identifier
  - `constraint_type`: Type of constraint (string)
  - `constraint_value`: Constraint limit (string)
  - `actual_value`: Actual value (string)

### 6. EXPORT Events (8 events)

Events related to output and export operations.

#### export_dwf / export_pdf / export_step / export_csv
- **Description**: File exported in specific format
- **When**: Export completed
- **Properties**:
  - `file_size`: Output file size (string)
  - `export_duration_ms`: Export time (string)
  - `include_dimensions`: true | false
  - `include_materials`: true | false

#### export_success
- **Description**: Export completed successfully
- **When**: File written to disk
- **Properties**:
  - `export_format`: "dwf" | "pdf" | "step" | "csv"
  - `file_path`: Export file path (last 50 chars)

#### export_error
- **Description**: Export failed
- **When**: Export encountered error
- **Properties**:
  - `export_format`: Format being exported
  - `error`: Error message
  - `duration_ms`: Time before failure (string)

#### print_preview
- **Description**: Print preview opened
- **When**: User opens print dialog
- **Properties**:
  - `page_count`: Number of pages (string)

#### print_sent
- **Description**: Design sent to printer
- **When**: Print job submitted
- **Properties**:
  - `printer_name`: Printer name (sanitized)
  - `page_count`: Pages printing (string)
  - `paper_size`: Paper size (e.g., "A4")

### 7. SYSTEM Events (4 events)

System-level events.

#### session_start
- **Description**: User session started
- **When**: Application launched or user logged in
- **Properties**:
  - `app_version`: BendCore version
  - `os`: Operating system (Windows/Web/etc)
  - `browser`: Browser name (if web)

#### session_end
- **Description**: User session ended
- **When**: User logs out or closes application
- **Properties**:
  - `session_duration_seconds`: Session length (string)
  - `event_count`: Total events in session (string)

#### app_error
- **Description**: Application error occurred
- **When**: Exception or error condition
- **Properties**:
  - `error_type`: Error type/class
  - `error_message`: Error description
  - `stack_trace`: Stack trace (truncated)
  - `severity`: "error" | "warning" | "critical"

#### feature_flag_evaluated
- **Description**: Feature flag evaluated
- **When**: Code checks feature flag
- **Properties**:
  - `feature_flag_name`: Flag name
  - `feature_enabled`: true | false
  - `variant`: "control" | "treatment"

## Best Practices

### Event Naming
- Use snake_case: `import_step_success`
- Be specific: `import_step_success` not `import_complete`
- Include context: `template_match_success` not `match_success`

### Properties
- Keep property names consistent across similar events
- Use strings for all property values (including numbers)
- Avoid PII: no email addresses, phone numbers, etc.
- Timestamp is automatic (don't duplicate)

### Sampling
- Track all user-triggered events
- Sample heavy events (e.g., mouse moves): 1 in 10
- Track critical path events: 100%

### Performance
- Event tracking: <1ms latency
- Batch automatically: 50 events or 5 seconds
- Non-blocking: events tracked asynchronously

## Event Filtering & Segments

### Segment by Variant
Events include A/B test variant information:
```
variant: "control" | "treatment"
experiment_id: "template_match_v1"
```

### Filter by Date Range
All events include timestamp for time-based analysis.

### Cohort Analysis
Events enable user-level cohort analysis:
- Users who completed import → design → export
- Template matcher adoption rate
- Search feature usage patterns

## Debugging Events

### View Events in GrowthBook
1. Go to Analytics → Events
2. Filter by date range
3. Search by event name
4. View event properties

### Local Testing
```cpp
AnalyticsEventQueue queue;
std::vector<AnalyticsEvent> events = queue.get_events();
for (const auto& event : events) {
  std::cout << "Event: " << event.event_name << std::endl;
  for (const auto& [key, value] : event.properties) {
    std::cout << "  " << key << ": " << value << std::endl;
  }
}
```

## Related Documentation

- GrowthBook Setup: /docs/GROWTHBOOK_SETUP.md
- A/B Testing Guide: /docs/AB_TESTING_GUIDE.md
- API Reference: /docs/ANALYTICS_API_REFERENCE.md
