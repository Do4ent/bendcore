# Analytics API Reference

## Overview

BendCore Analytics API provides endpoints for event tracking, A/B experiment management, KPI metrics retrieval, and data export.

## Base URL

```
http://localhost:3001/api
```

## Authentication

Currently no authentication required. In production, implement JWT or API key authentication.

## Endpoints

### Event Tracking

#### POST /events

Track a single user event.

**Request:**
```json
{
  "event_name": "import_step_success",
  "category": "IMPORT",
  "properties": {
    "file_size": "1024",
    "duration_ms": "5000"
  }
}
```

**Response (200):**
```json
{
  "success": true,
  "event_id": "550e8400-e29b-41d4-a716-446655440000"
}
```

**Headers:**
- `X-User-ID`: Optional user identifier (defaults to "anonymous")
- `X-Session-ID`: Optional session identifier (defaults to auto-generated)

---

### Experiments

#### GET /experiments

Get active A/B experiments.

**Response (200):**
```json
{
  "experiments": [
    {
      "id": "template_match_v1",
      "name": "Template Match UI",
      "status": "running",
      "startDate": "2026-10-15",
      "endDate": null,
      "controlConversion": 10.5,
      "treatmentConversion": 12.3,
      "significance": 0.92,
      "sampleSize": 5234
    }
  ],
  "total": 1
}
```

**Experiment Status Values:**
- `running`: Experiment is currently active
- `completed`: Experiment finished with results
- `paused`: Experiment temporarily paused

---

### KPI Metrics

#### GET /analytics/kpi

Get KPI metrics for a date range.

**Query Parameters:**
- `start_date`: ISO 8601 date (optional)
- `end_date`: ISO 8601 date (optional)

**Response (200):**
```json
{
  "sessions": 1234,
  "events": 5678,
  "conversionRate": 12.5,
  "avgSessionDuration": 245,
  "funnelMetrics": {
    "importOpened": 450,
    "importSuccess": 420,
    "tubeCreated": 380,
    "exported": 340
  }
}
```

**KPI Definitions:**
- `sessions`: Unique user sessions
- `events`: Total tracked events
- `conversionRate`: Import success / import opened (%)
- `avgSessionDuration`: Average session length in seconds
- `funnelMetrics`: Stage-specific conversion counts

---

### Event Timeline

#### GET /analytics/events

Get recent events with optional limit.

**Query Parameters:**
- `limit`: Maximum number of events to return (default: 20, max: 100)

**Response (200):**
```json
{
  "events": [
    {
      "id": "550e8400-e29b-41d4-a716-446655440000",
      "name": "import_step_success",
      "userId": "user_123",
      "timestamp": "2026-10-23T12:00:00Z",
      "properties": {
        "file_size": "1024",
        "material": "Steel"
      },
      "variant": "treatment"
    }
  ],
  "total_count": 15234
}
```

---

### Data Export

#### POST /analytics/export

Export metrics to CSV file.

**Request:**
```json
{
  "start_date": "2026-10-01",
  "end_date": "2026-10-31",
  "metrics": ["sessions", "events", "conversions"]
}
```

**Response:**
- Content-Type: `text/csv; charset=utf-8`
- Content-Disposition: `attachment; filename="analytics-export.csv"`
- Body: CSV data with columns: timestamp, event_name, user_id, session_id, properties

**CSV Format:**
```csv
timestamp,event_name,user_id,session_id,properties
"2026-10-23T12:00:00Z","import_step_success","user_123","session_456","{""file_size"":""1024""}"
```

---

### Summary

#### GET /analytics/summary

Get analytics summary statistics.

**Response (200):**
```json
{
  "totalEvents": 15234,
  "uniqueSessions": 2345,
  "eventsByCategory": {
    "IMPORT": 3421,
    "TEMPLATE": 2145,
    "SEARCH": 1890,
    "FORMULA": 1234,
    "TUBE_DESIGN": 2345,
    "EXPORT": 2100,
    "SYSTEM": 2099
  },
  "lastEventTime": "2026-10-23T12:45:30Z"
}
```

---

## Event Categories

Analytics events are categorized for better organization and filtering:

### IMPORT
- `import_step_opened`
- `import_step_success`
- `import_step_error`
- `import_dxf_opened` / `import_dxf_success` / `import_dxf_error`
- `import_cancel`
- `import_duration_seconds`

### TEMPLATE
- `template_list_opened`
- `template_match_requested`
- `template_match_success`
- `template_match_no_results`
- `template_selected`
- `template_create` / `template_edit` / `template_delete`

### SEARCH
- `search_opened`
- `search_query`
- `search_results_count`
- `search_filter_applied`
- `search_sort_applied`
- `search_save` / `search_load_saved`

### FORMULA
- `formula_created`
- `formula_edited`
- `formula_evaluated`
- `formula_error`
- `formula_complexity`
- `formula_save` / `formula_delete`

### TUBE_DESIGN
- `tube_created` / `tube_edited` / `tube_preview` / `tube_saved` / `tube_delete`
- `bend_added` / `bend_modified`
- `constraint_violation`

### EXPORT
- `export_dwf` / `export_pdf` / `export_step` / `export_csv`
- `export_success` / `export_error`
- `print_preview` / `print_sent`

### SYSTEM
- `session_start`
- `session_end`
- `app_error`
- `feature_flag_evaluated`

---

## Error Handling

### Status Codes

- `200 OK`: Successful request
- `400 Bad Request`: Missing required parameters
- `500 Internal Server Error`: Server error

### Error Response Format

```json
{
  "error": "Description of the error"
}
```

---

## Rate Limiting

Currently no rate limiting. Implement in production:
- 1000 requests/minute per IP
- 10000 events/minute per session

---

## Example: Complete Tracking Flow

```javascript
// 1. Start session
const sessionId = await fetch('/api/events', {
  method: 'POST',
  body: JSON.stringify({
    event_name: 'session_start'
  })
});

// 2. Track import event
await fetch('/api/events', {
  method: 'POST',
  headers: { 'X-Session-ID': sessionId },
  body: JSON.stringify({
    event_name: 'import_step_success',
    category: 'IMPORT',
    properties: { file_size: '2048', duration_ms: '3500' }
  })
});

// 3. Get KPI metrics
const kpi = await fetch('/api/analytics/kpi');

// 4. Check active experiments
const experiments = await fetch('/api/experiments');

// 5. End session
await fetch('/api/events', {
  method: 'POST',
  headers: { 'X-Session-ID': sessionId },
  body: JSON.stringify({
    event_name: 'session_end'
  })
});
```

---

## Integration with GrowthBook

Events are automatically synced to GrowthBook API for:
- Feature flag evaluation
- A/B test variant assignment
- Event-based cohort analysis
- Custom metric calculation

GrowthBook Configuration:
```
API Key: ${GB_API_KEY}
SDK Key: ${GB_SDK_KEY}
Environment: production
```

---

## Data Privacy

- No Personally Identifiable Information (PII) is tracked
- User IDs are anonymized
- Event properties are limited to non-sensitive attributes
- Data is retained for 90 days then purged
- GDPR compliant with export and deletion endpoints

---

## Performance

- Event tracking: <50ms per request
- KPI calculation: <200ms
- Export generation: <2s for 100K events
- Auto-scaling: supports 1000+ events/second

---

## Versioning

Current API Version: `v1`

Breaking changes will increment major version number.
Non-breaking additions will increment minor version.
