# PHASE 29: CommandManager - Complete Command Reference

**Document:** Command Registry and Mapping  
**Status:** Locked for Implementation  
**Date:** 2026-10-22  
**Total Commands:** 34

---

## Overview Table

| Tab | Group | Commands | Color Category |
|-----|-------|----------|-----------------|
| Home | Clipboard | 3 | Neutral, Create |
| Home | File | 6 | Neutral, Edit |
| Part | Sketch | 6 | Create, Edit, Freeze |
| Part | Tube | 5 | Tube, Edit |
| Assembly | Import | 3 | Import |
| Assembly | Assembly | 3 | Create, Links, Import |
| Assembly | Tools | 2 | Links, Update |
| Analysis | Validation | 3 | Update, Delete |
| Analysis | Inspection | 4 | Update |
| Drawing | View | 3 | Create, Edit |
| Drawing | Export | 3 | Update, Neutral |
| **TOTAL** | | **45** | **11 colors** |

---

## HOME TAB (9 commands)

### Group: Clipboard

| ID | Name | Command | Label | Hotkey | Category | Prerequisites |
|----|------|---------|-------|--------|----------|-----------------|
| 1 | Copy | `clipboard.copy` | Copy | Ctrl+C | Neutral | Selection required |
| 2 | Cut | `clipboard.cut` | Cut | Ctrl+X | Edit | Selection required |
| 3 | Paste | `clipboard.paste` | Paste | Ctrl+V | Create | Clipboard not empty |

**Icon Strategy:**
- Copy: Copy symbol (two rectangles)
- Cut: Scissors icon
- Paste: Clipboard with paste symbol

---

### Group: File

| ID | Name | Command | Label | Hotkey | Category | Prerequisites |
|----|------|---------|-------|--------|----------|-----------------|
| 4 | New Document | `file.new` | New | Ctrl+N | Neutral | None |
| 5 | Open Document | `file.open` | Open | Ctrl+O | Neutral | None |
| 6 | Save Document | `file.save` | Save | Ctrl+S | Neutral | Document must be modified |
| 7 | Save As | `file.saveAs` | Save As | Shift+Ctrl+S | Edit | Document must exist |
| 8 | Print Document | `file.print` | Print | Ctrl+P | Neutral | Document must be valid |
| 9 | Options | `file.options` | Options | (none) | Edit | None |

**Icon Strategy:**
- New: New document (blank page)
- Open: Open folder or file
- Save: Floppy disk icon
- Save As: Save with rename
- Print: Printer icon
- Options: Gear/settings icon

**Implementation Details:**
```cpp
// C++ Backend (HomeTabHandler)
CommandResponse handleFileNew(const CommandRequest& req);
CommandResponse handleFileOpen(const CommandRequest& req);
CommandResponse handleFileSave(const CommandRequest& req);
CommandResponse handleFileSaveAs(const CommandRequest& req);
CommandResponse handleFilePrint(const CommandRequest& req);
CommandResponse handleFileOptions(const CommandRequest& req);
```

---

## PART TAB (11 commands)

### Group: Sketch (6 commands)

| ID | Name | Command | Label | Hotkey | Category | Prerequisites |
|----|------|---------|-------|--------|----------|-----------------|
| 10 | Line | `sketch.line` | Line | L | Create | Sketch active |
| 11 | Arc | `sketch.arc` | Arc | A | Create | Sketch active |
| 12 | Point | `sketch.point` | Point | (none) | Create | Sketch active |
| 13 | Dimension | `sketch.dimension` | Dimension | (none) | Edit | 2+ elements selected |
| 14 | Constraint | `sketch.constraint` | Constraint | (none) | Freeze | 2+ elements selected |
| 15 | Solve Sketch | `sketch.solve` | Solve | F5 | Update | Constraints defined |

**Icon Strategy:**
- Line: Straight line segment
- Arc: Curved arc segment
- Point: Single point marker
- Dimension: Dimension arrow/line
- Constraint: Lock/constraint symbol
- Solve: Checkmark or solve icon

**Parameters by Command:**

*sketch.line:*
```json
{
  "startX": number,
  "startY": number,
  "endX": number,
  "endY": number
}
```

*sketch.arc:*
```json
{
  "centerX": number,
  "centerY": number,
  "radius": number,
  "startAngle": number,
  "endAngle": number
}
```

*sketch.dimension:*
```json
{
  "elementIds": string[],
  "dimensionType": "distance" | "angle" | "radius",
  "value": number
}
```

---

### Group: Tube (5 commands)

| ID | Name | Command | Label | Hotkey | Category | Prerequisites |
|----|------|---------|-------|--------|----------|-----------------|
| 16 | Create Tube | `tube.create` | Tube | (none) | Tube | Valid sketch path |
| 17 | Blank Properties | `tube.blank` | Blank | (none) | Tube | Tube selected |
| 18 | Material | `tube.material` | Material | (none) | Edit | Tube selected |
| 19 | Diameter | `tube.diameter` | Diameter | (none) | Edit | Tube selected |
| 20 | Technology | `tube.technology` | Technology | (none) | Edit | Tube selected |

**Icon Strategy:**
- Create Tube: Tube/cylinder icon
- Blank: Coil/blank representation
- Material: Material palette or chart
- Diameter: Measurement/dimension icon
- Technology: CNC or tools icon

**Parameters by Command:**

*tube.create:*
```json
{
  "sketchPathId": string,
  "outerDiameter": number,
  "wallThickness": number
}
```

*tube.material:*
```json
{
  "tubeId": string,
  "materialName": string,
  "density": number
}
```

*tube.technology:*
```json
{
  "tubeId": string,
  "instrumentType": "manual" | "cnc" | "mechanized",
  "bendRadius": number,
  "minStraightBetween": number
}
```

---

## ASSEMBLY TAB (8 commands)

### Group: Import (3 commands)

| ID | Name | Command | Label | Hotkey | Category | Prerequisites |
|----|------|---------|-------|--------|----------|-----------------|
| 21 | Import STEP | `import.step` | STEP | (none) | Import | None |
| 22 | Import IGES | `import.iges` | IGES | (none) | Import | None |
| 23 | Import SAT | `import.sat` | SAT | (none) | Import | None |

**Icon Strategy:**
- STEP: STEP file format icon
- IGES: IGES format icon
- SAT: SAT/ACIS format icon
- All three: Similar import theme

**Parameters:**
```json
{
  "filePath": string,
  "importOptions": {
    "mergeColinear": boolean,
    "validateGeometry": boolean,
    "autoFix": boolean,
    "tolerance": number
  }
}
```

---

### Group: Assembly (3 commands)

| ID | Name | Command | Label | Hotkey | Category | Prerequisites |
|----|------|---------|-------|--------|----------|-----------------|
| 24 | Insert Part | `assembly.insert` | Insert | (none) | Create | Part available |
| 25 | Create Port | `port.create` | Port | (none) | Links | Tube selected |
| 26 | Trace Path | `trace.command` | Trace | (none) | Import | Environment loaded |

**Icon Strategy:**
- Insert: Plus + component
- Port: Port/connection point
- Trace: Path tracing icon

**Parameters:**

*assembly.insert:*
```json
{
  "componentId": string,
  "position": [number, number, number],
  "rotation": [number, number, number]
}
```

*port.create:*
```json
{
  "tubeId": string,
  "portType": "inlet" | "outlet",
  "diameter": number,
  "offset": number
}
```

*trace.command:*
```json
{
  "environmentGeometryId": string,
  "snapTolerance": number
}
```

---

### Group: Tools (2 commands)

| ID | Name | Command | Label | Hotkey | Category | Prerequisites |
|----|------|---------|-------|--------|----------|-----------------|
| 27 | Link Environment | `link.environment` | Links | (none) | Links | Environment available |
| 28 | Recognize Parts | `recognize.parts` | Recognize | (none) | Update | Geometry loaded |

**Icon Strategy:**
- Links: Chain or link symbol
- Recognize: Magnifying glass or detection icon

**Parameters:**

*link.environment:*
```json
{
  "environmentId": string,
  "autoAssociate": boolean
}
```

*recognize.parts:*
```json
{
  "geometry": "all" | "tubes" | "environment",
  "tolerance": number,
  "mergeSimilar": boolean
}
```

---

## ANALYSIS TAB (7 commands)

### Group: Validation (3 commands)

| ID | Name | Command | Label | Hotkey | Category | Prerequisites |
|----|------|---------|-------|--------|----------|-----------------|
| 29 | Check All | `analysis.checkAll` | Check | F5 | Update | Document valid |
| 30 | Check Collisions | `analysis.collisions` | Collisions | (none) | Delete | Geometry complete |
| 31 | Check Gaps | `analysis.gaps` | Gaps | (none) | Update | Assembly complete |

**Icon Strategy:**
- Check All: Checkmark or validation icon
- Collisions: Collision/warning icon (red)
- Gaps: Gap/distance measurement

**Parameters:**

*analysis.checkAll:*
```json
{
  "checkTypes": [
    "geometry",
    "collisions",
    "gaps",
    "material",
    "machine"
  ]
}
```

*analysis.collisions:*
```json
{
  "tolerance": number,
  "excludeEnvironment": boolean
}
```

*analysis.gaps:*
```json
{
  "minClearance": number,
  "checkAssembly": boolean
}
```

---

### Group: Inspection (4 commands)

| ID | Name | Command | Label | Hotkey | Category | Prerequisites |
|----|------|---------|-------|--------|----------|-----------------|
| 32 | Measure Distance | `measure.distance` | Measure | (none) | Update | Elements selected |
| 33 | Measure Angle | `measure.angle` | Angle | (none) | Update | Elements selected |
| 34 | Calculate Mass | `mass.calculate` | Mass | (none) | Update | Tube with material |
| 35 | Generate Report | `report.generate` | Report | (none) | Update | Analysis complete |

**Icon Strategy:**
- Measure: Ruler or measurement tool
- Angle: Angle symbol
- Mass: Weight or scale icon
- Report: Document or clipboard

**Parameters:**

*measure.distance:*
```json
{
  "element1Id": string,
  "element2Id": string,
  "measurementType": "point-to-point" | "surface-to-surface"
}
```

*mass.calculate:*
```json
{
  "tubeId": string,
  "includeFittings": boolean
}
```

*report.generate:*
```json
{
  "reportType": "production" | "analysis" | "full",
  "formats": ["PDF", "CSV", "JSON"],
  "outputPath": string
}
```

---

## DRAWING TAB (6 commands)

### Group: View (3 commands)

| ID | Name | Command | Label | Hotkey | Category | Prerequisites |
|----|------|---------|-------|--------|----------|-----------------|
| 36 | New Sheet | `drawing.newSheet` | Sheet | (none) | Create | Drawing active |
| 37 | New View | `drawing.newView` | View | (none) | Create | Sheet active |
| 38 | Annotations | `drawing.annotations` | Annotations | (none) | Edit | View selected |

**Icon Strategy:**
- Sheet: Blank sheet/page
- View: View/viewport icon
- Annotations: Text or annotation symbol

**Parameters:**

*drawing.newSheet:*
```json
{
  "templateName": "A4" | "A3" | "A2" | "A1",
  "orientation": "portrait" | "landscape"
}
```

*drawing.newView:*
```json
{
  "viewType": "front" | "top" | "side" | "isometric",
  "scale": number
}
```

*drawing.annotations:*
```json
{
  "annotationType": "text" | "dimension" | "callout",
  "position": [number, number],
  "content": string
}
```

---

### Group: Export (3 commands)

| ID | Name | Command | Label | Hotkey | Category | Prerequisites |
|----|------|---------|-------|--------|----------|-----------------|
| 39 | Bend Tables | `drawing.tables` | Tables | (none) | Update | Analysis complete |
| 40 | Unfolding | `drawing.unfolding` | Unfold | (none) | Update | Tube valid |
| 41 | Export Drawing | `drawing.export` | Export | (none) | Neutral | Drawing complete |

**Icon Strategy:**
- Tables: Table/grid icon
- Unfold: Unfolding/expansion icon
- Export: Export/save icon

**Parameters:**

*drawing.tables:*
```json
{
  "tableType": "bend" | "dimensions" | "material",
  "includeNotes": boolean
}
```

*drawing.unfolding:*
```json
{
  "scale": number,
  "showAllowances": boolean,
  "dimensionFormat": "metric" | "imperial"
}
```

*drawing.export:*
```json
{
  "format": "PDF" | "DWG" | "DXF" | "STEP",
  "outputPath": string,
  "includeDrawingViews": boolean
}
```

---

## Color Category Summary

### Commands by Color

**Neutral (Gray #757575)** - 7 commands
- file.new, file.open, file.save, file.print
- clipboard.copy
- drawing.export

**Create (Green #2E9E5B)** - 6 commands
- clipboard.paste
- sketch.line, sketch.arc, sketch.point
- assembly.insert
- drawing.newSheet, drawing.newView

**Tube (Blue #1F6FC5)** - 3 commands
- tube.create, tube.blank

**Edit (Orange #E8820C)** - 6 commands
- clipboard.cut
- file.saveAs, file.options
- sketch.dimension
- tube.material, tube.diameter, tube.technology
- drawing.annotations

**Import (Cyan #0E97A3)** - 4 commands
- import.step, import.iges, import.sat
- trace.command

**Links (Purple #8A4FD8)** - 3 commands
- port.create
- link.environment

**Update (Yellow #F5B517)** - 7 commands
- sketch.solve
- recognize.parts
- analysis.checkAll, analysis.gaps
- measure.distance, measure.angle, mass.calculate
- report.generate
- drawing.tables, drawing.unfolding

**Delete (Red #D93A3A)** - 1 command
- analysis.collisions

**Freeze (Light Blue #2D9CDB)** - 1 command
- sketch.constraint

---

## C++ Implementation Map

### HomeTabHandler Commands
```cpp
class HomeTabHandler : public CommandHandler {
  CommandResponse handleFileNew();
  CommandResponse handleFileOpen();
  CommandResponse handleFileSave();
  CommandResponse handleFileSaveAs();
  CommandResponse handleFilePrint();
  CommandResponse handleFileOptions();
  CommandResponse handleClipboardCopy();
  CommandResponse handleClipboardCut();
  CommandResponse handleClipboardPaste();
};
```

### PartTabHandler Commands
```cpp
class PartTabHandler : public CommandHandler {
  CommandResponse handleSketchLine();
  CommandResponse handleSketchArc();
  CommandResponse handleSketchPoint();
  CommandResponse handleSketchDimension();
  CommandResponse handleSketchConstraint();
  CommandResponse handleSketchSolve();
  CommandResponse handleTubeCreate();
  CommandResponse handleTubeBlank();
  CommandResponse handleTubeMaterial();
  CommandResponse handleTubeDiameter();
  CommandResponse handleTubeTechnology();
};
```

### AssemblyTabHandler Commands
```cpp
class AssemblyTabHandler : public CommandHandler {
  CommandResponse handleImportStep();
  CommandResponse handleImportIges();
  CommandResponse handleImportSat();
  CommandResponse handleAssemblyInsert();
  CommandResponse handlePortCreate();
  CommandResponse handleTraceCommand();
  CommandResponse handleLinkEnvironment();
  CommandResponse handleRecognizeParts();
};
```

### AnalysisTabHandler Commands
```cpp
class AnalysisTabHandler : public CommandHandler {
  CommandResponse handleCheckAll();
  CommandResponse handleCollisions();
  CommandResponse handleGaps();
  CommandResponse handleMeasureDistance();
  CommandResponse handleMeasureAngle();
  CommandResponse handleMassCalculate();
  CommandResponse handleReportGenerate();
};
```

### DrawingTabHandler Commands
```cpp
class DrawingTabHandler : public CommandHandler {
  CommandResponse handleNewSheet();
  CommandResponse handleNewView();
  CommandResponse handleAnnotations();
  CommandResponse handleTables();
  CommandResponse handleUnfolding();
  CommandResponse handleExportDrawing();
};
```

---

## Command Validation Rules

### Global Rules
- All commands require valid document context
- All commands validate parameters before execution
- All commands return CommandResponse with success flag

### Prerequisites Validation
- `requiresSelection: true` → Check AppState.selectedElements.length > 0
- `requiresSketch: true` → Check AppState.sketchActive
- `requiresDocument: true` → Check AppState.documentLoaded

### Parameter Validation
- Numeric parameters → Range checks (min/max)
- String parameters → Non-empty validation
- File paths → Existence checks
- Object IDs → Registry lookups

---

## Future Extension Points

### Adding New Commands

1. **Add to COMMAND_REGISTRY** (types/commands.ts)
2. **Update Tab Structure** (RibbonTab.tsx TAB_STRUCTURE constant)
3. **Create C++ Handler** (new method in appropriate TabHandler)
4. **Add Icon** (IconComponents for color category)
5. **Create Tests** (unit + integration)

### Adding New Color Category

1. **Add to ColorCategory type** (ColorScheme.ts)
2. **Add to colorScheme object** (with hex, rgb, description)
3. **Map commands to category** (getCategoryForCommand function)
4. **Create CSS variable** (exportCSSVariables)
5. **Update color reference docs**

---

## Testing Checklist

For each command:
- [ ] Icon renders correctly
- [ ] Color applied correctly
- [ ] Tooltip displays on hover
- [ ] Button disables when prerequisites not met
- [ ] IPC message sent on click
- [ ] Response handled correctly
- [ ] Error handling works

---

## Reference

- **Specification:** BENDCORE_SPECIFICATION_FINAL_LOCKED.md (Section 6)
- **Architecture:** PHASE_29_ARCHITECTURE.md
- **Implementation:** PHASE_29_IMPLEMENTATION_PLAN.md
- **Quick Start:** PHASE_29_QUICK_START.md

---

**Created:** 2026-10-22  
**Status:** Locked for Implementation  
**Total Commands:** 34  
**Color Categories:** 11  
**Tabs:** 5

---

*Use this as a definitive reference during implementation. All commands listed here MUST be implemented exactly as specified.*
