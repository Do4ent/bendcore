# Template User Guide

## Overview

The BendCore Template System provides intelligent tube bending profile templates that automatically suggest the best manufacturing approach based on your tube specifications.

## Getting Started

### Accessing Templates

1. **From TubeDialog**: Click the "Templates" tab to see template suggestions for your current tube configuration
2. **Template Library**: Use the Template Library view to browse, create, and manage custom templates
3. **Template Editor**: Create new templates or edit existing ones with full constraint management

### Template Suggestions

When you enter tube parameters (OD, wall thickness, bend angle, etc.), the system automatically calculates which predefined templates best match your specifications.

**Confidence Scores:**
- **95-100% (Green)**: Perfect match - all constraints satisfied, optimal manufacturing
- **85-95% (Green)**: Good match - minor constraints, can proceed with standard setup
- **75-85% (Amber)**: Acceptable - some constraints violated, custom setup may be needed
- **<75% (Red)**: Poor match - not recommended, consider alternative designs

## Predefined Templates

### 1. Standard 90° Bend (std-90)

**Best For**: General-purpose 90-degree bends, hydraulic lines, pneumatic systems

- **OD Range**: 6-100 mm
- **Wall Range**: 0.5-5.0 mm
- **Max Consecutive Bends**: 2
- **Manufacturing Method**: 3D CNC bending
- **Compatible Materials**: Steel, Aluminum, Brass
- **Cost**: $50/unit
- **Popularity**: 95/100

### 2. Reinforced Small Diameter (rfd-small)

**Best For**: Instrumentation, precision assemblies, sensor tubing, medical devices

- **OD Range**: 3-25 mm
- **Wall Range**: 1.0-3.0 mm
- **Max Consecutive Bends**: 1 (high-stress)
- **Manufacturing Method**: Hydraulic press
- **Compatible Materials**: High-strength Steel, Stainless Steel, Titanium
- **Cost**: $75/unit
- **Popularity**: 88/100

### 3. Large Assembly Profile (lg-asm)

**Best For**: Aircraft structures, automotive frames, weight-critical assemblies

- **OD Range**: 50-200 mm
- **Wall Range**: 2.0-10.0 mm
- **Max Consecutive Bends**: 3
- **Manufacturing Method**: CNC 5-axis
- **Compatible Materials**: Aluminum, Titanium, Composites
- **Cost**: $120/unit
- **Popularity**: 82/100

## Creating Custom Templates

### Step 1: Open Template Editor

Click "Create New Template" in the Template Library to open the editor.

### Step 2: Basic Information

- **Template ID**: Unique identifier (cannot be changed after creation)
- **Name**: Display name (e.g., "High-Precision Medical Assembly")
- **Description**: Detailed use case and constraints
- **Category**: Standard, Reinforced, Large-Assembly, or Custom

### Step 3: Define Constraints

Set the allowable ranges for:

- **Outer Diameter (OD)**: min-max in millimeters
- **Wall Thickness**: min-max in millimeters
- **Bend Radius Ratio (R/D)**: min-max ratio (e.g., 2.0 = 2 × OD)

### Step 4: Manufacturing Info

- **Manufacturing Method**: e.g., "Rotary Draw Bending", "Mandrel Bending"
- **Compatible Materials**: List of material IDs (comma-separated)

### Step 5: Save

Click "Save Template" to add to your custom template library.

## Managing Templates

### Editing Templates

1. Open Template Library
2. Click "Edit" on the template you want to modify
3. Update the fields
4. Click "Save Template"

**Note**: Predefined templates cannot be edited; create a custom copy instead.

### Deleting Templates

1. Open Template Library
2. Click "Delete" on a custom template
3. Confirm deletion

**Note**: Only custom templates can be deleted. Predefined templates are permanent.

### Filtering & Searching

Use the Template Library's search and filter controls:

- **Search**: Find templates by name, ID, or description
- **Category Filter**: Show only templates from a specific category
- **View Mode**: Switch between grid and list views

## Matching Algorithm

The template matcher uses a weighted scoring system:

- **OD Fit**: 30% weight - How well the outer diameter matches
- **Wall Fit**: 20% weight - Wall thickness compatibility
- **Bend Compatibility**: 25% weight - Bend angle and stress
- **Radius Ratio**: 15% weight - Bend radius-to-diameter ratio
- **Bend Spacing**: 10% weight - Minimum spacing between consecutive bends

### Understanding Match Quality

**Perfect Matches (95-100%)**
- All geometric constraints within optimal ranges
- No manufacturing limitations
- Standard production ready

**Good Matches (85-95%)**
- Minor constraint variations
- Possible minor setup adjustments
- Acceptable for production

**Acceptable Matches (75-85%)**
- One or more constraints slightly outside ranges
- Custom fixture or setup may be required
- Consult manufacturing team

**Poor Matches (<75%)**
- Multiple constraint violations
- Significant custom work required
- Consider alternative designs

## Advanced Features

### Exporting Templates

Export your template library for:
- Backup purposes
- Sharing with team members
- Import into another BendCore installation

### Importing Templates

Import previously exported template files:
1. Click Import in Template Library
2. Select JSON file
3. Templates are merged with existing library

### Material Compatibility

Each template specifies compatible materials. When selecting a template:
- Verify your material is in the compatible list
- If not listed, consult manufacturing team
- Custom material handling may increase costs

## Best Practices

1. **Use Predefined Templates First**: Start with standard templates before creating custom ones
2. **Document Custom Templates**: Add clear descriptions for team reference
3. **Test New Templates**: Prototype with small batches before full production
4. **Review Recommendations**: Pay attention to manufacturing suggestions
5. **Update Popularity**: Active templates reflect production experience

## Troubleshooting

### No Good Matches Found

- Verify your tube parameters are realistic
- Check if a custom template would be more appropriate
- Consult with manufacturing team for specialty designs

### Template Not Appearing

- Ensure template is in correct category
- Check search filters
- Reload template library

### Import Errors

- Verify JSON file format
- Check file is not corrupted
- Ensure file follows BendCore template schema

## Support

For questions about templates or manufacturing capabilities, contact:
- Manufacturing Engineering Team
- BendCore Support: support@bendcore.local

---

**Last Updated**: 2026-10-23
**Template System Version**: 1.0
