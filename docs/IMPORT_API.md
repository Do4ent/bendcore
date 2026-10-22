# BendCore Import API Reference

## Overview

The BendCore import system provides programmatic access to CSV and JSON tube model import functionality. This document describes the TypeScript/JavaScript API for integrating import capabilities into your application.

## Core API

### `importTube(options: ImportOptions): Promise<ImportResult>`

Main import function for CSV and JSON tube models.

**Parameters:**
```typescript
interface ImportOptions {
  format: 'csv' | 'json';           // File format
  content: string;                   // File content (text)
  outerDiameter: number;            // Outer diameter in mm
  wallThickness: number;            // Wall thickness in mm
  material?: string;                 // Material name
  materialGrade?: string;           // Material grade (optional)
  bendingMachine?: string;          // Machine type (optional)
  state?: string;                   // Supply state (optional)
  options?: ImportSettings;         // Import options
}
```

**Return Value:**
```typescript
interface ImportResult {
  ok: boolean;                      // Success flag
  id?: string;                      // Generated tube ID
  error?: string;                   // Error message if failed
  warnings?: string[];              // Import warnings
  statistics?: {
    segmentCount: number;           // Number of path segments
    totalLength: number;            // Total axial length (mm)
    bendCount: number;              // Number of bends
    straightCount: number;          // Number of straights
  }
}
```

**Example:**
```typescript
const result = await bendcore.importTube({
  format: 'csv',
  content: csvContent,
  outerDiameter: 20.0,
  wallThickness: 1.5,
  material: 'Copper',
  options: {
    validateGeometry: true,
    autoFixErrors: false,
    toleranceMm: 0.1
  }
});

if (result.ok) {
  console.log(`Imported tube: ${result.id}`);
  console.log(`Segments: ${result.statistics?.segmentCount}`);
} else {
  console.error(`Import failed: ${result.error}`);
}
```

## Import Options

### `ImportSettings`

Configuration for import behavior.

```typescript
interface ImportSettings {
  validateGeometry?: boolean;       // Validate all values (default: true)
  autoFixErrors?: boolean;          // Auto-correct minor issues (default: false)
  mergeColinearSegments?: boolean;  // Combine colinear straights (default: true)
  toleranceMm?: number;             // Rounding tolerance (default: 0.1)
  validateMaterial?: boolean;       // Check material compatibility (default: true)
  preserveMetadata?: boolean;       // Keep JSON metadata (default: true)
}
```

## Data Structures

### `TubeModel`

Internal representation of imported tube.

```typescript
interface TubeModel {
  id: string;
  name?: string;
  outerDiameter: number;           // mm
  wallThickness: number;           // mm
  material: string;
  path: PathSegment[];             // Array of segments
  ports?: Port[];                  // Connection points
  specifications?: Specifications;
  metadata?: Metadata;
}
```

### `PathSegment`

Individual segment in tube path.

```typescript
interface PathSegment {
  segment: number;                 // Segment number
  type: 'straight' | 'bend';       // Segment type
  
  // For straight segments:
  length?: number;                 // Length in mm
  
  // For bend segments:
  radius?: number;                 // Bend radius in mm
  angle?: number;                  // Bend angle in degrees
  direction?: 'clockwise' | 'counterclockwise';
  
  // Computed properties:
  axialLength?: number;            // Axial distance covered
  actualLength?: number;           // Arc length (for bends)
}
```

### `Port`

Connection point on tube.

```typescript
interface Port {
  id: string;
  segmentIndex: number;            // Which path segment
  position: [number, number, number];    // 3D coordinates
  direction: [number, number, number];   // Normal vector
  diameter: number;                // Port diameter in mm
  type?: 'connection' | 'vent' | 'drain';
  offset?: number;                 // Axial offset in mm
}
```

### `Specifications`

Bending and manufacturing specifications.

```typescript
interface Specifications {
  blankLength?: number;            // Total blank length (mm)
  blankAllowance?: number;         // Allowance for trimming (mm)
  bendingAllowance?: number;       // Material used in bends (mm)
  minStraightSegment?: number;     // Minimum straight between bends (mm)
  maxBendRadius?: number;          // Maximum allowed radius (mm)
  minBendRadius?: number;          // Minimum allowed radius (mm)
  toleranceClass?: string;         // Tolerance class (Class1, Class2, etc.)
  surfaceFinish?: string;          // Surface finish specification
}
```

### `Metadata`

Import metadata and tracking information.

```typescript
interface Metadata {
  version?: string;
  created?: string;                // ISO 8601 datetime
  author?: string;
  description?: string;
  source?: string;                 // Original file name
  importedAt?: string;             // Import timestamp
  importedBy?: string;             // User who imported
}
```

## Validation API

### `validateTube(model: TubeModel): ValidationResult`

Validate a tube model after import.

```typescript
interface ValidationResult {
  valid: boolean;
  errors: ValidationError[];
  warnings: ValidationWarning[];
}

interface ValidationError {
  segment: number;                 // Segment number (0 for tube-level)
  code: string;                    // Error code
  message: string;
  severity: 'error' | 'warning';
  suggestion?: string;
}
```

**Example:**
```typescript
const validation = bendcore.validateTube(tubeModel);
if (!validation.valid) {
  validation.errors.forEach(err => {
    console.error(`Segment ${err.segment}: ${err.message}`);
  });
}
```

## Parsing API

### `parseCsv(content: string): ParseResult`

Parse CSV content and extract tube parameters.

```typescript
interface ParseResult {
  success: boolean;
  tube?: TubeParameters;
  segments?: PathSegment[];
  error?: string;
  parseErrors?: {
    line: number;
    column: string;
    issue: string;
  }[];
}

interface TubeParameters {
  id: string;
  outerDiameter: number;
  wallThickness: number;
  material?: string;
  [key: string]: any;
}
```

**Example:**
```typescript
const parsed = bendcore.parseCsv(csvContent);
if (parsed.success) {
  console.log(`Tube ID: ${parsed.tube?.id}`);
  console.log(`Segments: ${parsed.segments?.length}`);
} else {
  console.error(`Parse failed: ${parsed.error}`);
  parsed.parseErrors?.forEach(err => {
    console.error(`Line ${err.line}, ${err.column}: ${err.issue}`);
  });
}
```

### `parseJson(content: string): ParseResult`

Parse JSON content and extract tube parameters.

**Usage is same as `parseCsv`**, but expects JSON format.

## Format Conversion

### `csvToJson(csvContent: string, tube: TubeParameters): TubeModel`

Convert CSV to JSON TubeModel format.

```typescript
const model = bendcore.csvToJson(csvContent, {
  id: 'T001',
  outerDiameter: 20.0,
  wallThickness: 1.5,
  material: 'Copper'
});
```

### `jsonToJson(jsonContent: string): TubeModel`

Parse JSON string to TubeModel object.

## Statistics API

### `calculateStatistics(model: TubeModel): Statistics`

Calculate geometry statistics for a tube model.

```typescript
interface Statistics {
  segmentCount: number;
  straightCount: number;
  bendCount: number;
  totalLength: number;             // Sum of all segment axial lengths
  totalArcLength: number;          // Sum of bend arc lengths
  blankLength: number;             // With material allowance
  minBendRadius: number;
  maxBendRadius: number;
  totalBendingAngle: number;       // Sum of all bend angles
  averageSegmentLength: number;
}
```

**Example:**
```typescript
const stats = bendcore.calculateStatistics(model);
console.log(`Total path length: ${stats.totalLength} mm`);
console.log(`Total bends: ${stats.bendCount}`);
console.log(`Blank length needed: ${stats.blankLength} mm`);
```

## Compatibility API

### `checkMaterialCompatibility(material: string): Compatibility`

Check if material is supported and get specifications.

```typescript
interface Compatibility {
  supported: boolean;
  properties?: {
    density: number;
    yieldStrength: number;
    tensileStrength: number;
    elasticModulus: number;
    notes: string;
  };
  standardGrades: string[];
}
```

### `checkMachineCompatibility(machine: string, tube: TubeModel): Compatibility`

Check if tube can be bent on specified machine.

## Error Handling

### Error Codes

```typescript
enum ImportErrorCode {
  // File format errors
  INVALID_FORMAT = 'ERR_INVALID_FORMAT',
  PARSE_ERROR = 'ERR_PARSE_ERROR',
  MISSING_HEADERS = 'ERR_MISSING_HEADERS',
  
  // Validation errors
  INVALID_GEOMETRY = 'ERR_INVALID_GEOMETRY',
  OUT_OF_RANGE = 'ERR_OUT_OF_RANGE',
  INCOMPATIBLE_MATERIAL = 'ERR_INCOMPATIBLE_MATERIAL',
  INVALID_MACHINE = 'ERR_INVALID_MACHINE',
  
  // Logic errors
  NO_SEGMENTS = 'ERR_NO_SEGMENTS',
  CONSECUTIVE_BENDS = 'ERR_CONSECUTIVE_BENDS',
  INVALID_SEQUENCE = 'ERR_INVALID_SEQUENCE'
}
```

### Handling Errors

```typescript
try {
  const result = await bendcore.importTube(options);
  if (!result.ok) {
    switch (result.error?.code) {
      case ImportErrorCode.INVALID_FORMAT:
        console.error('File format not recognized');
        break;
      case ImportErrorCode.INVALID_GEOMETRY:
        console.error('Geometric constraints violated');
        break;
      default:
        console.error(`Import failed: ${result.error?.message}`);
    }
  }
} catch (error) {
  console.error('Unexpected error:', error);
}
```

## Integration Examples

### React Component Integration

```typescript
// useImport.ts - Custom hook
export const useImport = () => {
  const [result, setResult] = useState<ImportResult | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const importTube = async (options: ImportOptions) => {
    setLoading(true);
    setError(null);
    
    try {
      const result = await bendcore.importTube(options);
      setResult(result);
      
      if (!result.ok) {
        setError(result.error);
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Import failed');
    } finally {
      setLoading(false);
    }
  };

  return { importTube, result, loading, error };
};
```

### Batch Import

```typescript
// Import multiple files
async function batchImport(files: File[]): Promise<ImportResult[]> {
  const results: ImportResult[] = [];
  
  for (const file of files) {
    const content = await file.text();
    const format = file.name.endsWith('.json') ? 'json' : 'csv';
    
    const result = await bendcore.importTube({
      format: format as 'csv' | 'json',
      content,
      outerDiameter: 20.0,
      wallThickness: 1.5
    });
    
    results.push(result);
  }
  
  return results;
}
```

## Performance Considerations

- CSV parsing is optimized for files up to 10,000 segments
- JSON parsing is faster but uses more memory
- Auto-fix options add 5-10% processing time
- Large files (>100KB) may require progress callbacks

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-10-22 | Initial release with CSV/JSON support |

## Related Documentation

- [Import Guide](./IMPORT_GUIDE.md) - User guide and examples
- [Tube Model Specification](./TUBE_MODEL.md) - TubeModel data structure
- [Validation Rules](./VALIDATION_RULES.md) - Geometry constraints

---

**Last Updated**: 2026-10-22  
**BendCore Version**: 1.0
