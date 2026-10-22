# BendCore Import Guide: CSV & JSON Formats

## Overview

BendCore supports importing tube bending models from CSV and JSON formats. This guide explains how to prepare your data and use the import functionality.

## Supported Formats

### CSV Format

Comma-separated values with headers and data rows.

**Basic Structure:**
```
tube_id,outer_diameter,wall_thickness,material
T001,20.0,1.5,Copper

segment,type,parameter,value,unit
1,straight,length,50.0,mm
2,bend,radius,20.0,mm
2,bend,angle,90.0,deg
```

**Column Definitions:**

#### Tube Parameters (Optional header section)
| Column | Type | Required | Description |
|--------|------|----------|-------------|
| `tube_id` | string | Yes | Unique identifier for the tube |
| `outer_diameter` | float | Yes | Outer diameter in mm |
| `wall_thickness` | float | Yes | Wall thickness in mm |
| `material` | string | No | Material type (Copper, Steel, etc.) |
| `material_grade` | string | No | Material grade/specification |
| `bending_machine` | string | No | Machine type used for bending |
| `state` | string | No | Supply state (Soft, Half-hard, Hard) |

#### Path Segments (Required section)
| Column | Type | Required | Description |
|--------|------|----------|-------------|
| `segment` | integer | Yes | Segment number (1, 2, 3, ...) |
| `type` | enum | Yes | `straight` or `bend` |
| `parameter` | string | Yes | `length` for straight, `radius`/`angle` for bend |
| `value` | float | Yes | Numeric value |
| `unit` | string | Yes | `mm` for length/radius, `deg` for angle |

**Example with Comments:**
```csv
# Copper tube path - simple assembly
# Created: 2026-10-22

tube_id,outer_diameter,wall_thickness,material
T001,20.0,1.5,Copper

# Segment definition
segment,type,parameter,value,unit
1,straight,length,50.0,mm
2,bend,radius,20.0,mm
2,bend,angle,90.0,deg
3,straight,length,30.0,mm
```

### JSON Format

JavaScript Object Notation with nested structure.

**Basic Structure:**
```json
{
  "tube": {
    "id": "T001",
    "outerDiameter": 20.0,
    "wallThickness": 1.5,
    "material": "Copper"
  },
  "path": [
    {
      "type": "straight",
      "length": 50.0
    },
    {
      "type": "bend",
      "radius": 20.0,
      "angle": 90.0
    }
  ]
}
```

**Object Structure:**

#### tube (Required)
```json
{
  "id": "string",
  "name": "string (optional)",
  "outerDiameter": "float (mm)",
  "wallThickness": "float (mm)",
  "material": "string (optional)",
  "materialGrade": "string (optional)",
  "bendingMachine": "string (optional)",
  "state": "string (optional)"
}
```

#### path (Required)
Array of segment objects:
```json
[
  {
    "segment": "integer (optional)",
    "type": "straight|bend",
    "length": "float (mm, for straight only)",
    "radius": "float (mm, for bend only)",
    "angle": "float (degrees, for bend only)",
    "direction": "clockwise|counterclockwise (optional, for bend)"
  }
]
```

#### ports (Optional)
```json
{
  "id": "string",
  "segmentIndex": "integer",
  "position": "[x, y, z] array",
  "direction": "[dx, dy, dz] array",
  "diameter": "float (mm)",
  "type": "connection (or other types)",
  "offset": "float (mm, optional)"
}
```

#### specifications (Optional)
Additional bending specifications:
```json
{
  "blankLength": "float (mm)",
  "blankAllowance": "float (mm)",
  "bendingAllowance": "float (mm)",
  "minStraightSegment": "float (mm)",
  "maxBendRadius": "float (mm)",
  "minBendRadius": "float (mm)",
  "toleranceClass": "string",
  "surfaceFinish": "string"
}
```

#### metadata (Optional)
```json
{
  "version": "string",
  "created": "ISO 8601 date",
  "author": "string",
  "description": "string"
}
```

## Validation Rules

### Geometric Constraints

1. **Outer Diameter (OD)**
   - Must be > 0 mm
   - Typical range: 6-54 mm (standard tubing)
   - Default validation accepts values in mm

2. **Wall Thickness**
   - Must be > 0 mm
   - Must be < OD/2
   - Typical range: 0.5-3.0 mm

3. **Bend Radius**
   - Must be ≥ OD (minimum bend radius)
   - Typical range: OD to 10×OD
   - Affects springback and stress in material

4. **Straight Length**
   - Must be ≥ 0 mm
   - Minimum practical length depends on bending machine
   - End segments (first and last) can be 0 mm (trimmed after bending)

5. **Bend Angle**
   - Must be > 0° and ≤ 360°
   - Typical range: 15° to 180° for most applications
   - Small angles (<0.5°) are considered degenerate

### Material Compatibility

Supported materials:
- **Copper**: DHP (Phosphorous Deoxidized), OFE (Oxygen-Free)
- **Steel**: Galvanized, Stainless, Carbon
- **Aluminum**: 3003, 5005, 6061 (limited bending)
- **Brass**: C36000 (free-cutting) and others

### Segment Sequencing

1. Path must start with a straight segment
2. Cannot have consecutive bends without a straight between them
3. Path must end with either straight or bend
4. Minimum spacing between bends depends on machine type

## Import Options

### Validation Settings

- **Validate Geometry**: Check for geometric errors
  - Out-of-range values
  - Invalid bend radii
  - Conflicting parameters

- **Auto-fix Errors**: Attempt to fix minor issues
  - Round values to tolerance
  - Adjust bend radius to minimum if necessary
  - Merge colinear segments

- **Merge Colinear Segments**: Combine consecutive straight segments
  - Only if they are colinear (same direction)
  - Reduces complexity

- **Tolerance (mm)**: Rounding tolerance for geometry
  - Default: 0.1 mm
  - Range: 0.01 - 1.0 mm

## Example Files

### Simple Copper Tube (CSV)

```csv
tube_id,outer_diameter,wall_thickness,material
T001,20.0,1.5,Copper

segment,type,parameter,value,unit
1,straight,length,50.0,mm
2,bend,radius,20.0,mm
2,bend,angle,90.0,deg
3,straight,length,30.0,mm
4,bend,radius,20.0,mm
4,bend,angle,45.0,deg
5,straight,length,25.0,mm
```

### Complete Tube with Ports (JSON)

```json
{
  "metadata": {
    "version": "1.0",
    "created": "2026-10-22",
    "author": "CAD System"
  },
  "tube": {
    "id": "T001",
    "outerDiameter": 20.0,
    "wallThickness": 1.5,
    "material": "Copper"
  },
  "path": [
    {
      "type": "straight",
      "length": 50.0
    },
    {
      "type": "bend",
      "radius": 20.0,
      "angle": 90.0
    },
    {
      "type": "straight",
      "length": 30.0
    }
  ],
  "ports": [
    {
      "id": "P1",
      "position": [0, 0, 0],
      "direction": [0, 0, 1],
      "diameter": 20.0
    },
    {
      "id": "P2",
      "position": [30, 0, 50],
      "direction": [1, 0, 0],
      "diameter": 20.0
    }
  ]
}
```

## Importing in BendCore

### Steps

1. **Open File → Import** or click **Import** button
2. **Select CSV or JSON file**
3. **Preview** the parsed data
4. **Configure options** (validation, auto-fix, tolerance)
5. **Review warnings** (if any)
6. **Click Import** to create tube model

### Import Dialog

The import dialog shows:
- **File preview**: First 500 characters
- **Tube summary**: ID, OD, wall, material
- **Path visualization**: Number of segments, total length
- **Validation status**: Errors, warnings, valid indicators
- **Progress**: Real-time import progress percentage

### Conflict Resolution

If validation detects issues:
1. **Errors** (red): Must be fixed before import
   - Out-of-range values
   - Invalid geometry
   
2. **Warnings** (orange): Can proceed but review recommended
   - Non-standard values
   - Potential stress concentration

3. **Auto-fix**: Automatically correct minor issues
   - Enable in Options → Auto-fix Errors
   - Shows what was changed

## API Reference

### JavaScript/TypeScript

```typescript
// Import tube from CSV or JSON
const result = await bendcore.importTube({
  format: 'csv' | 'json',
  content: string,           // File content
  outerDiameter: number,     // mm
  wallThickness: number,     // mm
  material?: string,
  options?: {
    validateGeometry?: boolean,
    autoFixErrors?: boolean,
    mergeColinearSegments?: boolean,
    toleranceMm?: number
  }
});

// Result structure
{
  ok: boolean,
  id?: string,           // Tube ID if successful
  error?: string,        // Error message if failed
  warnings?: string[],   // Import warnings
  statistics?: {
    segmentCount: number,
    totalLength: number,
    bendCount: number
  }
}
```

## Troubleshooting

### Common Import Errors

**"Invalid file format"**
- Ensure file is valid CSV or JSON
- Check for syntax errors
- Validate headers and required fields

**"Missing tube parameters"**
- CSV: Ensure tube_id, outer_diameter, wall_thickness in first section
- JSON: Ensure tube object with id, outerDiameter, wallThickness

**"Invalid geometry"**
- Check bend radius ≥ OD
- Check wall thickness < OD/2
- Check all values are positive

**"No path segments"**
- CSV: Ensure segment, type, parameter, value, unit columns
- JSON: Ensure path array is present and not empty

### Warnings and Best Practices

1. **Empty segment numbers**: Auto-numbered sequentially
2. **Colinear segments**: Consider merging to simplify
3. **Small radii**: May cause stress concentration
4. **Very long segments**: May require intermediate supports
5. **Material compatibility**: Verify material is appropriate for bending

## Format Conversion

### CSV → JSON

When importing CSV, BendCore creates equivalent JSON internally:

```csv
segment,type,parameter,value,unit
1,straight,length,50.0,mm
2,bend,radius,20.0,mm
2,bend,angle,90.0,deg
```

Converts to:
```json
{
  "path": [
    {"type": "straight", "length": 50.0},
    {"type": "bend", "radius": 20.0, "angle": 90.0}
  ]
}
```

### JSON → CSV

JSON can be exported back to CSV-like format for spreadsheet use.

## Support

For issues or questions about import:
- Check sample files in `tests/samples/`
- Review validation messages in import dialog
- Consult tube bending standards (DIN 2391, EN 10228)

**Version**: 1.0  
**Last Updated**: 2026-10-22
