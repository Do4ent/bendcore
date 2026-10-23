# BendCore Import User Guide

## Importing CAD Files

BendCore supports importing STEP, IGES, and DXF files for automatic parametric dimension extraction.

### Supported Formats

- **STEP/STP**: ISO 10303 Standard for Exchange of Product (All versions AP203-AP242)
- **IGES/IGS**: Initial Graphics Exchange Specification
- **DXF**: Drawing Exchange Format (2D geometry)

### Workflow

1. Click "Import" in the main menu
2. Select a STEP/IGES/DXF file
3. Configure import options:
   - **Use Cache**: Faster re-import of previously imported files
   - **Auto-Create Formulas**: Automatically create parametric dimensions
   - **Author**: Your name for attribution
4. Review extracted parameters
5. Apply to create new dimensions

## Understanding Extracted Parameters

The import process extracts these parametric dimensions:

- **$OD**: Outer Diameter (mm)
- **$Wall**: Wall Thickness (mm)
- **$ID**: Inner Diameter (calculated: $OD - 2*$Wall)
- **$Length**: Total Length (mm)
- **$BendAngle_N**: Bend angle N in degrees
- **$BendRadius_N**: Bend radius N in mm

## DXF Import Limitations

- DXF import currently supports 2D geometry only
- 3D features (bends) are detected from 2D projections
- Limited parametric extraction from DXF

## Troubleshooting

### File Not Recognized
- Verify file extension (.step, .stp, .iges, .igs, or .dxf)
- Check file is not corrupted
- Try exporting from CAD software again

### Low Confidence Results
- Complex geometries may show lower confidence
- Manual adjustment of extracted parameters recommended
- Check geometry for unusual wall thickness ratios

### Import Timeout
- Large files (>100MB) may take longer to process
- Check available disk space
- Try importing a subset of the assembly

## Best Practices

1. Ensure CAD model has clean geometry (no extra surfaces)
2. Use consistent units (mm recommended)
3. Model tubes with proper cross-sections
4. Save files in latest format version
5. Test with small files first

