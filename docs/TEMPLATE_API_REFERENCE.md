# Template API Reference

## Overview

The BendCore Template API provides RESTful endpoints for managing tube bend templates and performing automatic template matching.

## Base URL

```
http://localhost:3000/api/templates
```

## Endpoints

### 1. Get All Templates

**Endpoint**: `GET /templates`

**Description**: Retrieve all templates (predefined + user-created)

**Response**:
```json
{
  "templates": [
    {
      "id": "std-90",
      "name": "Standard 90° Bend",
      "description": "...",
      "category": "Standard",
      "outer_diameter": { "min": 6, "max": 100, "default": 25, "unit": "mm" },
      "wall_thickness": { "min": 0.5, "max": 5, "default": 2, "unit": "mm" },
      "length": { "min": 50, "max": 800, "default": 400, "unit": "mm" },
      "use_case": "...",
      "compatible_materials": ["steel-1020", "aluminum-6061"]
    }
  ],
  "last_updated": "2026-10-23T10:30:00Z",
  "count": 3
}
```

**Status Codes**:
- `200 OK`: Templates retrieved successfully
- `500 Internal Server Error`: Server error

---

### 2. Get Single Template

**Endpoint**: `GET /templates/:id`

**Description**: Retrieve a specific template by ID

**Parameters**:
- `id` (string, required): Template ID

**Response**:
```json
{
  "id": "std-90",
  "name": "Standard 90° Bend",
  "description": "Most common 90-degree bend template...",
  "category": "Standard",
  "outer_diameter": { "min": 6, "max": 100, "default": 25, "unit": "mm" },
  "wall_thickness": { "min": 0.5, "max": 5, "default": 2, "unit": "mm" },
  "length": { "min": 50, "max": 800, "default": 400, "unit": "mm" },
  "use_case": "General-purpose 90° bends, hydraulic lines...",
  "compatible_materials": ["steel-1020", "aluminum-6061", "brass-7030"]
}
```

**Status Codes**:
- `200 OK`: Template found
- `404 Not Found`: Template doesn't exist
- `500 Internal Server Error`: Server error

---

### 3. Match Parameters to Templates

**Endpoint**: `POST /templates/match`

**Description**: Find best matching templates for given tube parameters

**Request Body**:
```json
{
  "od": 25.0,
  "wall": 2.0,
  "length": 400.0,
  "num_results": 3
}
```

**Request Parameters**:
- `od` (number, required): Outer diameter in mm
- `wall` (number, required): Wall thickness in mm
- `length` (number, required): Tube length in mm
- `num_results` (number, optional): Number of results (default: 3, max: 10)

**Response**:
```json
{
  "matches": [
    {
      "template_id": "std-90",
      "template_name": "Standard 90° Bend",
      "confidence": 95.5
    },
    {
      "template_id": "lg-asm",
      "template_name": "Large Assembly Profile",
      "confidence": 72.3
    }
  ],
  "timestamp": "2026-10-23T10:35:00Z",
  "parameters": {
    "od": 25.0,
    "wall": 2.0,
    "length": 400.0
  }
}
```

**Status Codes**:
- `200 OK`: Match successful
- `400 Bad Request`: Invalid parameters
- `500 Internal Server Error`: Server error

---

### 4. Create Template

**Endpoint**: `POST /templates`

**Description**: Create a new custom template

**Request Body**:
```json
{
  "template": {
    "id": "custom-medical",
    "name": "Medical Precision Tubing",
    "description": "High-precision medical device tubing",
    "category": "Custom",
    "outer_diameter": { "min": 3, "max": 10, "default": 6, "unit": "mm" },
    "wall_thickness": { "min": 0.5, "max": 1.5, "default": 1.0, "unit": "mm" },
    "length": { "min": 50, "max": 200, "default": 100, "unit": "mm" },
    "use_case": "Medical device components",
    "compatible_materials": ["ss-304", "ss-316l", "titanium-grade2"]
  }
}
```

**Response**:
```json
{
  "success": true,
  "id": "custom-medical",
  "message": "Template created successfully"
}
```

**Status Codes**:
- `201 Created`: Template created successfully
- `400 Bad Request`: Invalid template structure
- `409 Conflict`: Template ID already exists
- `500 Internal Server Error`: Server error

---

### 5. Update Template

**Endpoint**: `PUT /templates/:id`

**Description**: Update an existing template

**Parameters**:
- `id` (string, required): Template ID

**Request Body**:
```json
{
  "template": {
    "name": "Updated Template Name",
    "description": "Updated description...",
    "compatible_materials": ["steel-1020", "aluminum-6061"]
  }
}
```

**Response**:
```json
{
  "success": true,
  "message": "Template updated successfully",
  "template": {
    "id": "std-90",
    "name": "Updated Template Name",
    ...
  }
}
```

**Status Codes**:
- `200 OK`: Template updated successfully
- `404 Not Found`: Template doesn't exist
- `500 Internal Server Error`: Server error

---

### 6. Delete Template

**Endpoint**: `DELETE /templates/:id`

**Description**: Delete a custom template (predefined templates cannot be deleted)

**Parameters**:
- `id` (string, required): Template ID

**Response**:
```json
{
  "success": true,
  "message": "Template deleted successfully",
  "deleted": {
    "id": "custom-medical",
    "name": "Medical Precision Tubing",
    ...
  }
}
```

**Status Codes**:
- `200 OK`: Template deleted successfully
- `403 Forbidden`: Cannot delete predefined templates
- `404 Not Found`: Template doesn't exist
- `500 Internal Server Error`: Server error

---

### 7. List Categories

**Endpoint**: `GET /templates/categories/list`

**Description**: Get all available template categories

**Response**:
```json
{
  "categories": ["Standard", "Reinforced", "Large-Assembly", "Custom"]
}
```

**Status Codes**:
- `200 OK`: Categories retrieved successfully
- `500 Internal Server Error`: Server error

---

## Confidence Scoring Algorithm

The matching algorithm uses weighted scoring:

```
confidence = (OD_score × 0.3 + Wall_score × 0.2 + Length_score × 0.5) × 100
```

Where each score is calculated as:
```
parameter_score = max(0, 1 - (|value - range| / range_width))
```

### Score Interpretation

- **95-100%**: Perfect match
- **85-95%**: Good match  
- **75-85%**: Acceptable match
- **< 75%**: Poor match

---

## Error Responses

All error responses follow this format:

```json
{
  "error": "Error message describing what went wrong"
}
```

Common errors:

- **400 Bad Request**: Invalid request parameters
- **404 Not Found**: Resource doesn't exist
- **409 Conflict**: Duplicate ID or conflict with existing resource
- **500 Internal Server Error**: Unexpected server error

---

## Rate Limiting

No rate limiting currently applied. Contact system administrator for SLA requirements.

---

## Authentication

Currently no authentication required. For production deployments, implement OAuth 2.0 or JWT-based authentication.

---

## CORS

CORS is enabled for localhost. Configure as needed for production.

---

## Request/Response Format

- **Content-Type**: `application/json`
- **Character Encoding**: UTF-8

---

## Example Workflows

### Workflow 1: Auto-select Template

```javascript
// 1. Get all templates
const templatesRes = await fetch('http://localhost:3000/api/templates');
const { templates } = await templatesRes.json();

// 2. Match parameters
const matchRes = await fetch('http://localhost:3000/api/templates/match', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ od: 25, wall: 2, length: 400 })
});
const { matches } = await matchRes.json();

// 3. Select best match
const selectedTemplate = matches[0].template_id;
```

### Workflow 2: Create Custom Template

```javascript
const customTemplate = {
  id: "custom-automotive",
  name: "Automotive Fuel Line",
  description: "High-pressure fuel delivery",
  category: "Custom",
  outer_diameter: { min: 8, max: 12, default: 10, unit: "mm" },
  wall_thickness: { min: 1.5, max: 2.5, default: 2, unit: "mm" },
  length: { min: 100, max: 500, default: 300, unit: "mm" },
  use_case: "Automotive fuel systems",
  compatible_materials: ["steel-1020", "stainless-304"]
};

const res = await fetch('http://localhost:3000/api/templates', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ template: customTemplate })
});
const result = await res.json();
console.log('Created template:', result.id);
```

---

**API Version**: 1.0  
**Last Updated**: 2026-10-23  
**Status**: Production Ready
