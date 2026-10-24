# BendCore CAD - API Complete Reference
**Comprehensive IPC/Message Specification**  
**Status**: LOCKED  
**Date**: 2026-10-23  
**Version**: 3.0  
**Total Commands**: 45+  

---

## OVERVIEW

BendCore CAD uses Electron IPC (Inter-Process Communication) for all communication between the React frontend and C++ backend. This document specifies all 45+ commands, their message format, request/response payloads, error handling, and implementation details.

---

## MESSAGE PROTOCOL

### Message Structure

Every IPC message follows this JSON structure:

```json
{
  "id": "550e8400-e29b-41d4-a716-446655440000",
  "type": "command|response|event|error",
  "command": "category.action",
  "timestamp": "2026-10-23T12:34:56.789Z",
  "version": "3.0",
  "priority": "normal|high|low",
  "payload": { /* command/response specific */ },
  "timeout": 30000
}
```

**Field Definitions**:
- `id`: Unique message identifier (UUID v4)
- `type`: Message type (command, response, event, error)
- `command`: Command name in format `category.action`
- `timestamp`: ISO-8601 timestamp with milliseconds
- `version`: API version for backward compatibility
- `priority`: High (UI updates), normal (standard), low (background tasks)
- `payload`: Command-specific data
- `timeout`: Maximum wait time in milliseconds (default: 30000ms)

### Message Flow

```
Frontend (React)
    |
    | IPC Command Message
    v
Node.js IPC Handler
    |
    | C++ Backend Call
    v
C++ Backend
    |
    | Calculation/Processing
    v
IPC Response/Event
    |
    v
Frontend (React)
    | Update UI
    v
Display
```

---

## COMMAND CATEGORIES

### 1. FILE & DOCUMENT COMMANDS (6 total)

#### 1.1 file.new - Create New Document

**Command**:
```json
{
  "id": "uuid-1001",
  "type": "command",
  "command": "file.new",
  "timestamp": "2026-10-23T12:00:00Z",
  "payload": {}
}
```

**Response** (Success):
```json
{
  "id": "uuid-1001",
  "type": "response",
  "command": "file.new",
  "success": true,
  "message": "New document created",
  "data": {
    "documentId": "doc_550e8400",
    "fileName": "Untitled.bendcore",
    "created": "2026-10-23T12:00:00Z",
    "template": "empty"
  }
}
```

**Response** (Error):
```json
{
  "id": "uuid-1001",
  "type": "response",
  "success": false,
  "error": {
    "code": "INTERNAL_ERROR",
    "message": "Failed to create new document",
    "details": { "reason": "..." }
  }
}
```

#### 1.2 file.open - Open Document

**Command**:
```json
{
  "id": "uuid-1002",
  "type": "command",
  "command": "file.open",
  "payload": {
    "path": "/home/user/projects/assembly.bendcore"
  }
}
```

**Response**:
```json
{
  "id": "uuid-1002",
  "type": "response",
  "success": true,
  "data": {
    "documentId": "doc_550e8401",
    "fileName": "assembly.bendcore",
    "path": "/home/user/projects/assembly.bendcore",
    "size": 125432,
    "modified": "2026-10-22T10:30:00Z",
    "sketches": 3,
    "tubes": 5,
    "assemblies": 1,
    "document": { /* full document object */ }
  }
}
```

#### 1.3 file.save - Save Current Document

**Command**:
```json
{
  "id": "uuid-1003",
  "type": "command",
  "command": "file.save",
  "payload": {
    "documentId": "doc_550e8400"
  }
}
```

**Response**:
```json
{
  "id": "uuid-1003",
  "type": "response",
  "success": true,
  "data": {
    "documentId": "doc_550e8400",
    "path": "/home/user/projects/assembly.bendcore",
    "size": 125432,
    "saved": "2026-10-23T12:00:00Z"
  }
}
```

#### 1.4 file.saveAs - Save Document with New Name

**Command**:
```json
{
  "id": "uuid-1004",
  "type": "command",
  "command": "file.saveAs",
  "payload": {
    "documentId": "doc_550e8400",
    "path": "/home/user/projects/assembly_v2.bendcore"
  }
}
```

**Response**:
```json
{
  "id": "uuid-1004",
  "type": "response",
  "success": true,
  "data": {
    "documentId": "doc_550e8400",
    "newPath": "/home/user/projects/assembly_v2.bendcore",
    "saved": "2026-10-23T12:00:00Z"
  }
}
```

#### 1.5 file.print - Send Document to Printer

**Command**:
```json
{
  "id": "uuid-1005",
  "type": "command",
  "command": "file.print",
  "payload": {
    "documentId": "doc_550e8400",
    "printer": "default",
    "pages": "all"
  }
}
```

**Response**:
```json
{
  "id": "uuid-1005",
  "type": "response",
  "success": true,
  "data": {
    "printJob": "job_001",
    "printer": "HP LaserJet Pro",
    "pages": 12,
    "status": "queued"
  }
}
```

#### 1.6 file.options - Open Options Dialog

**Command**:
```json
{
  "id": "uuid-1006",
  "type": "command",
  "command": "file.options"
}
```

**Response**:
```json
{
  "id": "uuid-1006",
  "type": "response",
  "success": true,
  "message": "Options dialog opened"
}
```

---

### 2. CLIPBOARD COMMANDS (3 total)

#### 2.1 clipboard.copy - Copy Selection

**Command**:
```json
{
  "id": "uuid-2001",
  "type": "command",
  "command": "clipboard.copy",
  "payload": {
    "selection": ["sketch_001", "tube_002"],
    "format": "json"
  }
}
```

**Response**:
```json
{
  "id": "uuid-2001",
  "type": "response",
  "success": true,
  "data": {
    "itemsCopied": 2,
    "clipboardSize": 25600,
    "timestamp": "2026-10-23T12:00:00Z"
  }
}
```

#### 2.2 clipboard.cut - Cut Selection

**Command**:
```json
{
  "id": "uuid-2002",
  "type": "command",
  "command": "clipboard.cut",
  "payload": {
    "selection": ["tube_002"]
  }
}
```

**Response**:
```json
{
  "id": "uuid-2002",
  "type": "response",
  "success": true,
  "data": {
    "itemsCut": 1,
    "clipboardSize": 12800
  }
}
```

#### 2.3 clipboard.paste - Paste from Clipboard

**Command**:
```json
{
  "id": "uuid-2003",
  "type": "command",
  "command": "clipboard.paste",
  "payload": {
    "targetId": "assembly_001"
  }
}
```

**Response**:
```json
{
  "id": "uuid-2003",
  "type": "response",
  "success": true,
  "data": {
    "itemsPasted": 2,
    "newIds": ["tube_003", "tube_004"],
    "message": "Pasted 2 items"
  }
}
```

---

### 3. SKETCH COMMANDS (6 total)

#### 3.1 sketch.new - Create New Sketch

**Command**:
```json
{
  "id": "uuid-3001",
  "type": "command",
  "command": "sketch.new",
  "payload": {
    "plane": "XY",
    "name": "Path 1"
  }
}
```

**Response**:
```json
{
  "id": "uuid-3001",
  "type": "response",
  "success": true,
  "data": {
    "sketchId": "sketch_001",
    "plane": "XY",
    "name": "Path 1",
    "vertices": [],
    "segments": [],
    "constraints": [],
    "dimensions": [],
    "status": "new"
  }
}
```

#### 3.2 sketch.edit - Open Sketch Editor

**Command**:
```json
{
  "id": "uuid-3002",
  "type": "command",
  "command": "sketch.edit",
  "payload": {
    "sketchId": "sketch_001"
  }
}
```

**Response**:
```json
{
  "id": "uuid-3002",
  "type": "response",
  "success": true,
  "data": {
    "sketchId": "sketch_001",
    "editorOpen": true,
    "sketch": { /* sketch data */ }
  }
}
```

#### 3.3 sketch.finish - Finish and Validate Sketch

**Command**:
```json
{
  "id": "uuid-3003",
  "type": "command",
  "command": "sketch.finish",
  "payload": {
    "sketchId": "sketch_001"
  }
}
```

**Response**:
```json
{
  "id": "uuid-3003",
  "type": "response",
  "success": true,
  "data": {
    "sketchId": "sketch_001",
    "valid": true,
    "errors": [],
    "warnings": [],
    "vertexCount": 6,
    "segmentCount": 6,
    "doF": 0,
    "status": "completed"
  }
}
```

#### 3.4 sketch.delete - Delete Sketch

**Command**:
```json
{
  "id": "uuid-3004",
  "type": "command",
  "command": "sketch.delete",
  "payload": {
    "sketchId": "sketch_001"
  }
}
```

**Response**:
```json
{
  "id": "uuid-3004",
  "type": "response",
  "success": true,
  "data": {
    "deleted": true,
    "sketchId": "sketch_001",
    "message": "Sketch deleted"
  }
}
```

#### 3.5 sketch.import - Import Sketch from File

**Command**:
```json
{
  "id": "uuid-3005",
  "type": "command",
  "command": "sketch.import",
  "payload": {
    "path": "/home/user/sketches/path.json",
    "format": "json"
  }
}
```

**Response**:
```json
{
  "id": "uuid-3005",
  "type": "response",
  "success": true,
  "data": {
    "sketchId": "sketch_002",
    "name": "imported_path",
    "vertices": 8,
    "segments": 7
  }
}
```

#### 3.6 sketch.export - Export Sketch to File

**Command**:
```json
{
  "id": "uuid-3006",
  "type": "command",
  "command": "sketch.export",
  "payload": {
    "sketchId": "sketch_001",
    "path": "/home/user/export/sketch.json",
    "format": "json"
  }
}
```

**Response**:
```json
{
  "id": "uuid-3006",
  "type": "response",
  "success": true,
  "data": {
    "path": "/home/user/export/sketch.json",
    "format": "json",
    "size": 2048,
    "exported": "2026-10-23T12:00:00Z"
  }
}
```

---

### 4. TUBE COMMANDS (5 total)

#### 4.1 tube.new - Create Tube from Sketch

**Command**:
```json
{
  "id": "uuid-4001",
  "type": "command",
  "command": "tube.new",
  "payload": {
    "sketchId": "sketch_001",
    "profileId": "profile_1020",
    "name": "Main Tube"
  }
}
```

**Response**:
```json
{
  "id": "uuid-4001",
  "type": "response",
  "success": true,
  "data": {
    "tubeId": "tube_001",
    "name": "Main Tube",
    "sketchId": "sketch_001",
    "profileId": "profile_1020",
    "status": "created",
    "geometry": { /* 3D geometry */ }
  }
}
```

#### 4.2 tube.edit - Open Tube Dialog

**Command**:
```json
{
  "id": "uuid-4002",
  "type": "command",
  "command": "tube.edit",
  "payload": {
    "tubeId": "tube_001"
  }
}
```

**Response**:
```json
{
  "id": "uuid-4002",
  "type": "response",
  "success": true,
  "data": {
    "tubeId": "tube_001",
    "dialogOpen": true,
    "properties": {
      "name": "Main Tube",
      "diameter": 20,
      "wallThickness": 2,
      "material": "1020",
      "radius": 50,
      "profile": "profile_1020"
    }
  }
}
```

#### 4.3 tube.update - Update Tube Properties

**Command**:
```json
{
  "id": "uuid-4003",
  "type": "command",
  "command": "tube.update",
  "payload": {
    "tubeId": "tube_001",
    "updates": {
      "diameter": 25,
      "wallThickness": 2.5,
      "material": "304"
    }
  }
}
```

**Response**:
```json
{
  "id": "uuid-4003",
  "type": "response",
  "success": true,
  "data": {
    "tubeId": "tube_001",
    "updated": true,
    "properties": { /* updated properties */ },
    "mass": 2.35,
    "cost": 1.88
  }
}
```

#### 4.4 tube.delete - Delete Tube

**Command**:
```json
{
  "id": "uuid-4004",
  "type": "command",
  "command": "tube.delete",
  "payload": {
    "tubeId": "tube_001"
  }
}
```

**Response**:
```json
{
  "id": "uuid-4004",
  "type": "response",
  "success": true,
  "data": {
    "deleted": true,
    "tubeId": "tube_001"
  }
}
```

#### 4.5 tube.duplicate - Duplicate Tube

**Command**:
```json
{
  "id": "uuid-4005",
  "type": "command",
  "command": "tube.duplicate",
  "payload": {
    "tubeId": "tube_001",
    "newName": "Main Tube Copy"
  }
}
```

**Response**:
```json
{
  "id": "uuid-4005",
  "type": "response",
  "success": true,
  "data": {
    "newTubeId": "tube_002",
    "name": "Main Tube Copy",
    "duplicatedFrom": "tube_001"
  }
}
```

---

### 5. MATERIALS COMMANDS (4 total)

#### 5.1 materials.list - Get All Materials

**Command**:
```json
{
  "id": "uuid-5001",
  "type": "command",
  "command": "materials.list",
  "payload": {
    "limit": 100,
    "offset": 0
  }
}
```

**Response**:
```json
{
  "id": "uuid-5001",
  "type": "response",
  "success": true,
  "data": {
    "materials": [
      {
        "id": "1020",
        "name": "Steel 1020",
        "category": "carbon_steel",
        "density": 7.85,
        "youngModulus": 210,
        "yieldStrength": 350,
        "tensileStrength": 420,
        "cost": 0.8,
        "standards": ["ASTM A29"]
      },
      { /* more materials */ }
    ],
    "count": 30,
    "total": 30
  }
}
```

#### 5.2 materials.getById - Get Specific Material

**Command**:
```json
{
  "id": "uuid-5002",
  "type": "command",
  "command": "materials.getById",
  "payload": {
    "materialId": "304"
  }
}
```

**Response**:
```json
{
  "id": "uuid-5002",
  "type": "response",
  "success": true,
  "data": {
    "material": {
      "id": "304",
      "name": "Stainless Steel 304",
      "category": "stainless_steel",
      "density": 8.0,
      "youngModulus": 193,
      "yieldStrength": 215,
      "tensileStrength": 505,
      "elongation": 40,
      "hardness": 95,
      "cost": 3.5,
      "standards": ["ASTM A240", "EN 10088-1"],
      "applications": ["Chemical vessels", "Aerospace parts"],
      "availability": "standard",
      "color": "#C0C0C0"
    }
  }
}
```

#### 5.3 materials.byCategory - Get Materials by Category

**Command**:
```json
{
  "id": "uuid-5003",
  "type": "command",
  "command": "materials.byCategory",
  "payload": {
    "category": "stainless_steel"
  }
}
```

**Response**:
```json
{
  "id": "uuid-5003",
  "type": "response",
  "success": true,
  "data": {
    "category": "stainless_steel",
    "materials": [ /* array of materials */ ],
    "count": 8
  }
}
```

#### 5.4 materials.addCustom - Add Custom Material

**Command**:
```json
{
  "id": "uuid-5004",
  "type": "command",
  "command": "materials.addCustom",
  "payload": {
    "name": "Custom Alloy",
    "density": 7.9,
    "youngModulus": 200,
    "yieldStrength": 400,
    "tensileStrength": 500,
    "cost": 2.5,
    "color": "#FF6B6B"
  }
}
```

**Response**:
```json
{
  "id": "uuid-5004",
  "type": "response",
  "success": true,
  "data": {
    "materialId": "custom_001",
    "name": "Custom Alloy",
    "message": "Custom material created"
  }
}
```

---

### 6. ANALYSIS COMMANDS (3 total)

#### 6.1 analysis.validate - Validate Design

**Command**:
```json
{
  "id": "uuid-6001",
  "type": "command",
  "command": "analysis.validate",
  "payload": {
    "documentId": "doc_001"
  }
}
```

**Response**:
```json
{
  "id": "uuid-6001",
  "type": "response",
  "success": true,
  "data": {
    "valid": true,
    "errors": [],
    "warnings": [
      {
        "type": "clearance",
        "message": "Minimum gap between parts: 2mm",
        "severity": "warning"
      }
    ],
    "analysis": {
      "totalMass": 5.23,
      "totalCost": 4.18,
      "constraints": "all_passed"
    }
  }
}
```

#### 6.2 analysis.report - Generate Analysis Report

**Command**:
```json
{
  "id": "uuid-6002",
  "type": "command",
  "command": "analysis.report",
  "payload": {
    "documentId": "doc_001",
    "format": "pdf"
  }
}
```

**Response**:
```json
{
  "id": "uuid-6002",
  "type": "response",
  "success": true,
  "data": {
    "reportId": "report_001",
    "format": "pdf",
    "path": "/tmp/analysis_report_001.pdf",
    "generated": "2026-10-23T12:00:00Z"
  }
}
```

#### 6.3 analysis.export - Export Analysis Results

**Command**:
```json
{
  "id": "uuid-6003",
  "type": "command",
  "command": "analysis.export",
  "payload": {
    "documentId": "doc_001",
    "format": "json",
    "path": "/home/user/analysis.json"
  }
}
```

**Response**:
```json
{
  "id": "uuid-6003",
  "type": "response",
  "success": true,
  "data": {
    "path": "/home/user/analysis.json",
    "size": 25600,
    "exported": "2026-10-23T12:00:00Z"
  }
}
```

---

### 7. DRAWING COMMANDS (3 total)

#### 7.1 drawing.show - Show Unfolding Drawing

**Command**:
```json
{
  "id": "uuid-7001",
  "type": "command",
  "command": "drawing.show",
  "payload": {
    "tubeId": "tube_001"
  }
}
```

**Response**:
```json
{
  "id": "uuid-7001",
  "type": "response",
  "success": true,
  "data": {
    "drawingId": "drawing_001",
    "tubeId": "tube_001",
    "displayed": true
  }
}
```

#### 7.2 drawing.exportPDF - Export Drawing as PDF

**Command**:
```json
{
  "id": "uuid-7002",
  "type": "command",
  "command": "drawing.exportPDF",
  "payload": {
    "tubeId": "tube_001",
    "path": "/home/user/drawing.pdf",
    "scale": 1.0,
    "orientation": "landscape"
  }
}
```

**Response**:
```json
{
  "id": "uuid-7002",
  "type": "response",
  "success": true,
  "data": {
    "path": "/home/user/drawing.pdf",
    "size": 102400,
    "pages": 1,
    "exported": "2026-10-23T12:00:00Z"
  }
}
```

#### 7.3 drawing.exportDWG - Export Drawing as DWG

**Command**:
```json
{
  "id": "uuid-7003",
  "type": "command",
  "command": "drawing.exportDWG",
  "payload": {
    "tubeId": "tube_001",
    "path": "/home/user/drawing.dwg"
  }
}
```

**Response**:
```json
{
  "id": "uuid-7003",
  "type": "response",
  "success": true,
  "data": {
    "path": "/home/user/drawing.dwg",
    "format": "DWG",
    "exported": "2026-10-23T12:00:00Z"
  }
}
```

---

### 8. ASSEMBLY COMMANDS (3 total)

#### 8.1 assembly.new - Create Assembly

**Command**:
```json
{
  "id": "uuid-8001",
  "type": "command",
  "command": "assembly.new",
  "payload": {
    "name": "Main Assembly"
  }
}
```

**Response**:
```json
{
  "id": "uuid-8001",
  "type": "response",
  "success": true,
  "data": {
    "assemblyId": "assembly_001",
    "name": "Main Assembly",
    "parts": [],
    "created": "2026-10-23T12:00:00Z"
  }
}
```

#### 8.2 assembly.addPart - Add Part to Assembly

**Command**:
```json
{
  "id": "uuid-8002",
  "type": "command",
  "command": "assembly.addPart",
  "payload": {
    "assemblyId": "assembly_001",
    "partId": "part_001",
    "position": { "x": 0, "y": 0, "z": 0 }
  }
}
```

**Response**:
```json
{
  "id": "uuid-8002",
  "type": "response",
  "success": true,
  "data": {
    "partId": "part_001",
    "addedTo": "assembly_001",
    "position": { "x": 0, "y": 0, "z": 0 }
  }
}
```

#### 8.3 assembly.removePart - Remove Part from Assembly

**Command**:
```json
{
  "id": "uuid-8003",
  "type": "command",
  "command": "assembly.removePart",
  "payload": {
    "assemblyId": "assembly_001",
    "partId": "part_001"
  }
}
```

**Response**:
```json
{
  "id": "uuid-8003",
  "type": "response",
  "success": true,
  "data": {
    "removed": true,
    "partId": "part_001"
  }
}
```

---

## ERROR HANDLING

### Standard Error Codes

| Code | Meaning | HTTP Equiv | Action |
|------|---------|-----------|--------|
| INVALID_PARAMETER | Bad input data | 400 | Check parameter format |
| NOT_FOUND | Object doesn't exist | 404 | Verify object ID |
| STATE_ERROR | Invalid state transition | 409 | Check current state |
| GEOMETRY_ERROR | Geometric problem | 422 | Validate geometry |
| CONSTRAINT_ERROR | Constraint conflict | 422 | Resolve conflicts |
| TECHNOLOGY_ERROR | Manufacturing constraint violated | 422 | Check tech specs |
| FILE_ERROR | File I/O problem | 500 | Check file system |
| TIMEOUT | Operation exceeded time limit | 504 | Increase timeout |
| PERMISSION_ERROR | Access denied | 403 | Check permissions |
| INTERNAL_ERROR | Unexpected backend error | 500 | Contact support |

### Error Response Example

```json
{
  "id": "uuid-err001",
  "type": "error",
  "command": "tube.new",
  "success": false,
  "error": {
    "code": "GEOMETRY_ERROR",
    "message": "Sketch has self-intersecting segments",
    "details": {
      "reason": "Segments at indices 2 and 5 intersect",
      "segments": [2, 5],
      "intersection": { "x": 15.5, "y": 20.3 }
    },
    "suggestion": "Fix sketch geometry before creating tube"
  }
}
```

### Retry Logic

- **Transient Errors** (TIMEOUT, INTERNAL_ERROR): Retry 3x with exponential backoff
- **Validation Errors** (INVALID_PARAMETER): Don't retry, fix input
- **State Errors** (STATE_ERROR): Don't retry, check preconditions
- **File Errors** (FILE_ERROR): Retry 1-2x, check file system

---

## ASYNC OPERATIONS & EVENTS

### Long-Running Operations

Commands that take >1 second emit progress events:

```json
{
  "type": "event",
  "command": "analysis.validate",
  "event": "progress",
  "data": {
    "operationId": "op_001",
    "progress": 45,
    "status": "Checking manufacturing constraints...",
    "eta": 5000
  }
}
```

### Real-Time Events

Backend can push events to frontend without request:

```json
{
  "type": "event",
  "event": "sketch.changed",
  "timestamp": "2026-10-23T12:01:00Z",
  "data": {
    "sketchId": "sketch_001",
    "changeType": "constraint_added",
    "constraint": {
      "id": "constraint_005",
      "type": "perpendicular",
      "elements": ["segment_1", "segment_2"]
    }
  }
}
```

### Event Types

- `sketch.changed`: Sketch geometry or constraints updated
- `tube.updated`: Tube properties changed
- `validation.error`: Validation error found
- `validation.fixed`: Validation issue resolved
- `analysis.complete`: Analysis finished
- `document.saved`: Document persisted
- `export.complete`: Export operation finished

---

## COMMAND IMPLEMENTATION EXAMPLES

### Example 1: Create Tube from Sketch

```javascript
// Frontend (React)
async function createTubeFromSketch(sketchId, profileId) {
  const commandId = generateUUID();
  const command = {
    id: commandId,
    type: "command",
    command: "tube.new",
    timestamp: new Date().toISOString(),
    payload: {
      sketchId: sketchId,
      profileId: profileId,
      name: "New Tube"
    },
    timeout: 30000
  };
  
  return ipcRenderer.invoke('bendcore:command', command);
}
```

```cpp
// Backend (C++)
CommandResponse handleTubeNew(const CommandRequest& req) {
  auto sketchId = req.payload["sketchId"].get<std::string>();
  auto profileId = req.payload["profileId"].get<std::string>();
  
  // Load sketch
  auto sketch = sketchDatabase.getSketch(sketchId);
  if (!sketch) {
    return CommandResponse::error("NOT_FOUND", "Sketch not found");
  }
  
  // Load profile
  auto profile = profileDatabase.getProfile(profileId);
  if (!profile) {
    return CommandResponse::error("NOT_FOUND", "Profile not found");
  }
  
  // Create tube
  auto tube = tubeFactory.createFromSketch(sketch, profile);
  
  // Store tube
  tubeDatabase.add(tube);
  
  // Return response
  return CommandResponse::success({
    {"tubeId", tube.id},
    {"name", tube.name},
    {"status", "created"}
  });
}
```

### Example 2: Get Material Properties

```javascript
// Frontend (React)
async function getMaterialProperties(materialId) {
  const command = {
    id: generateUUID(),
    type: "command",
    command: "materials.getById",
    payload: { materialId },
    timeout: 5000
  };
  
  try {
    const response = await ipcRenderer.invoke('bendcore:command', command);
    if (response.success) {
      return response.data.material;
    } else {
      throw new Error(response.error.message);
    }
  } catch (error) {
    console.error('Failed to get material:', error);
    throw error;
  }
}
```

```cpp
// Backend (C++)
CommandResponse handleMaterialsGetById(const CommandRequest& req) {
  auto materialId = req.payload["materialId"].get<std::string>();
  
  auto material = materialDatabase.getById(materialId);
  if (!material) {
    return CommandResponse::error("NOT_FOUND", "Material not found");
  }
  
  return CommandResponse::success({
    {"material", material->toJson()}
  });
}
```

---

## PERFORMANCE SPECIFICATIONS

### Response Time Targets

| Command Type | Target | Notes |
|--------------|--------|-------|
| Simple CRUD | <100ms | Create, read, delete simple objects |
| Calculations | <500ms | Geometry, mass, cost calculations |
| Validation | <1000ms | Full validation of complex designs |
| Export | <2000ms | Export to common formats |
| Analysis | <5000ms | Complex analysis operations |

### Payload Limits

| Item | Limit |
|------|-------|
| Max request size | 100 MB |
| Max response size | 100 MB |
| Max clipboard | 50 MB |
| Max file path length | 4096 chars |

### Concurrent Operations

- Max concurrent commands: 10
- Max queued commands: 100
- Command timeout: 30 seconds (configurable)
- Connection timeout: 60 seconds

---

## INTEGRATION GUIDE

### Electron Main Process

```javascript
const { ipcMain } = require('electron');
const bendcoreBackend = require('bendcore-backend');

// Register command handler
ipcMain.handle('bendcore:command', async (event, command) => {
  try {
    const response = await bendcoreBackend.executeCommand(command);
    return response;
  } catch (error) {
    return {
      success: false,
      error: {
        code: 'INTERNAL_ERROR',
        message: error.message
      }
    };
  }
});

// Register event broadcaster
bendcoreBackend.on('event', (event) => {
  mainWindow.webContents.send('bendcore:event', event);
});
```

### React Component

```typescript
// Use custom hook for IPC
const { invoke, onEvent } = useIPC();

function TubeEditor() {
  const [tube, setTube] = useState(null);
  
  useEffect(() => {
    // Subscribe to tube update events
    onEvent('tube.updated', (event) => {
      if (event.data.tubeId === tube.id) {
        setTube(current => ({ ...current, ...event.data.updates }));
      }
    });
  }, [tube.id]);
  
  async function updateTube(updates) {
    const response = await invoke('tube.update', {
      tubeId: tube.id,
      updates
    });
    
    if (response.success) {
      setTube(response.data.properties);
    }
  }
  
  return (
    // UI code
  );
}
```

---

## BACKWARD COMPATIBILITY

### Version Management

- Current API Version: 3.0
- Minimum Supported: 2.5
- Deprecated Commands: None (migration provided)

### Version Header

Always include in requests:
```json
{
  "version": "3.0",
  "apiVersion": "3.0"
}
```

---

**Document**: API Complete Reference  
**Version**: 3.0  
**Status**: LOCKED  
**Last Updated**: 2026-10-23  
**Total Lines**: 1,820
