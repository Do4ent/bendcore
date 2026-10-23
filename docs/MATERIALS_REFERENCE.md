# BendCore Materials Reference

## Overview

The BendCore materials library contains 30+ material types with comprehensive properties for tube bending design and analysis. Each material includes physical properties, standards compliance information, and application guidelines.

## Database File

Location: `/bendcore/data/materials.json`

Format: JSON with material definitions organized by category.

## Material Properties

Each material in the database contains:

- **ID**: Unique identifier (e.g., "steel-1020")
- **Name**: Display name (e.g., "Carbon Steel 1020")
- **Category**: Material classification
- **Density**: g/cm³
- **Properties**:
  - Young's Modulus: GPa
  - Yield Strength: MPa
  - Tensile Strength: MPa
  - Elongation: %
  - Hardness: HV
  - Cost: $/kg
- **Standards**: Applicable standards (DIN, EN, ASTM, ISO)
- **Applications**: Common use cases
- **Availability**: Common, Standard, Premium, Custom
- **Color**: Hex color for UI display (#RRGGBB)

---

## Carbon Steels (8 types)

### Carbon Steel 1020
- **ID**: `steel-1020`
- **Density**: 7.85 g/cm³
- **Young's Modulus**: 200 GPa
- **Yield Strength**: 355 MPa
- **Tensile Strength**: 420 MPa
- **Standards**: DIN 17100, EN 10025, ASTM A36
- **Applications**: General purpose tubing, Hydraulic systems, Structural components
- **Availability**: Common
- **Cost**: $1.50/kg

### Carbon Steel 1045
- **ID**: `steel-1045`
- **Density**: 7.85 g/cm³
- **Young's Modulus**: 205 GPa
- **Yield Strength**: 450 MPa
- **Tensile Strength**: 565 MPa
- **Standards**: DIN 17100, EN 10025, ASTM 1045
- **Applications**: Medium strength tubing, Drive shafts, Cranks
- **Availability**: Common
- **Cost**: $1.75/kg

### Carbon Steel 1060
- **ID**: `steel-1060`
- **Density**: 7.85 g/cm³
- **Young's Modulus**: 210 GPa
- **Yield Strength**: 530 MPa
- **Tensile Strength**: 625 MPa
- **Standards**: DIN 17100, EN 10025, ASTM 1060
- **Applications**: High strength tubing, Springs, Heavy machinery
- **Availability**: Common
- **Cost**: $1.95/kg

### Carbon Steel CK45 (C45)
- **ID**: `steel-ck45`
- **Density**: 7.85 g/cm³
- **Young's Modulus**: 205 GPa
- **Yield Strength**: 435 MPa
- **Tensile Strength**: 570 MPa
- **Standards**: DIN 1652, EN 10083-2, ISO 683
- **Applications**: European standard tubing, Machinery components, Fasteners
- **Availability**: Common
- **Cost**: $1.80/kg

### Carbon Steel 1018
- **ID**: `steel-1018`
- **Density**: 7.85 g/cm³
- **Young's Modulus**: 200 GPa
- **Yield Strength**: 340 MPa
- **Tensile Strength**: 400 MPa
- **Standards**: ASTM 1018, SAE 1018, EN 10083
- **Applications**: Carburizing, Case hardening, General purpose
- **Availability**: Common
- **Cost**: $1.45/kg

### Alloy Steel 4140
- **ID**: `steel-4140`
- **Density**: 7.85 g/cm³
- **Young's Modulus**: 210 GPa
- **Yield Strength**: 1000 MPa
- **Tensile Strength**: 1310 MPa
- **Standards**: ASTM 4140, SAE 4140, EN 19
- **Applications**: Aircraft hydraulic tubing, High strength applications, Landing gear
- **Availability**: Standard
- **Cost**: $3.20/kg

### Alloy Steel 4340
- **ID**: `steel-4340`
- **Density**: 7.85 g/cm³
- **Young's Modulus**: 210 GPa
- **Yield Strength**: 1160 MPa
- **Tensile Strength**: 1470 MPa
- **Standards**: ASTM 4340, SAE 4340, EN 26
- **Applications**: Aerospace components, High performance tubing, Racing
- **Availability**: Standard
- **Cost**: $3.80/kg

### Alloy Steel 9310
- **ID**: `steel-9310`
- **Density**: 7.85 g/cm³
- **Young's Modulus**: 210 GPa
- **Yield Strength**: 1310 MPa
- **Tensile Strength**: 1550 MPa
- **Standards**: ASTM 9310, SAE 9310, EN 18CrNiMo
- **Applications**: Aircraft tubing, Aerospace structures, Premium racing
- **Availability**: Premium
- **Cost**: $4.50/kg

---

## Stainless Steels (8 types)

### Stainless Steel 304 (Austenitic)
- **ID**: `ss-304`
- **Density**: 8.0 g/cm³
- **Young's Modulus**: 193 GPa
- **Yield Strength**: 205 MPa
- **Tensile Strength**: 515 MPa
- **Standards**: ASTM 304, EN 1.4301, ISO 5347
- **Applications**: Chemical processing, Food industry, Aerospace
- **Availability**: Common
- **Cost**: $3.50/kg

### Stainless Steel 304L
- **ID**: `ss-304l`
- **Density**: 8.0 g/cm³
- **Young's Modulus**: 193 GPa
- **Yield Strength**: 170 MPa
- **Tensile Strength**: 485 MPa
- **Standards**: ASTM 304L, EN 1.4306, ISO 5347
- **Applications**: Welded structures, Chemical tubing, Cryogenic
- **Availability**: Common
- **Cost**: $3.75/kg

### Stainless Steel 316 (Austenitic)
- **ID**: `ss-316`
- **Density**: 8.0 g/cm³
- **Young's Modulus**: 193 GPa
- **Yield Strength**: 170 MPa
- **Tensile Strength**: 510 MPa
- **Standards**: ASTM 316, EN 1.4401, ISO 5347
- **Applications**: Marine environments, Seawater resistant, Corrosive service
- **Availability**: Common
- **Cost**: $4.50/kg

### Stainless Steel 316L
- **ID**: `ss-316l`
- **Density**: 8.0 g/cm³
- **Young's Modulus**: 193 GPa
- **Yield Strength**: 170 MPa
- **Tensile Strength**: 485 MPa
- **Standards**: ASTM 316L, EN 1.4404, ISO 5347
- **Applications**: Welded marine tubing, Cryogenic service, Pharmaceutical
- **Availability**: Common
- **Cost**: $4.75/kg

### Stainless Steel 430 (Ferritic)
- **ID**: `ss-430`
- **Density**: 7.73 g/cm³
- **Young's Modulus**: 200 GPa
- **Yield Strength**: 275 MPa
- **Tensile Strength**: 450 MPa
- **Standards**: ASTM 430, EN 1.4016, ISO 5347
- **Applications**: Automotive exhaust, Decorative applications, Low cost tubing
- **Availability**: Common
- **Cost**: $2.75/kg

### Stainless Steel 17-4 PH (Precipitation Hardening)
- **ID**: `ss-174ph`
- **Density**: 7.8 g/cm³
- **Young's Modulus**: 207 GPa
- **Yield Strength**: 1310 MPa
- **Tensile Strength**: 1450 MPa
- **Standards**: ASTM 630, EN 1.4548, AMS 5604
- **Applications**: Aerospace fasteners, High strength tubing, Jet engines
- **Availability**: Premium
- **Cost**: $6.50/kg

### Stainless Steel 630 (17-7 PH)
- **ID**: `ss-630`
- **Density**: 7.8 g/cm³
- **Young's Modulus**: 207 GPa
- **Yield Strength**: 1380 MPa
- **Tensile Strength**: 1520 MPa
- **Standards**: ASTM 630, EN 1.4568, AMS 5604
- **Applications**: Aircraft components, Springs, Fasteners
- **Availability**: Premium
- **Cost**: $7.00/kg

### Stainless Steel 15-5 PH
- **ID**: `ss-15-5ph`
- **Density**: 7.8 g/cm³
- **Young's Modulus**: 207 GPa
- **Yield Strength**: 1310 MPa
- **Tensile Strength**: 1450 MPa
- **Standards**: ASTM 15-5, EN 1.4545, AMS 5604
- **Applications**: Aerospace tubing, High strength applications, Pressure vessels
- **Availability**: Premium
- **Cost**: $6.25/kg

---

## Aluminum Alloys (6 types)

### Aluminum Alloy 6061
- **ID**: `aluminum-6061`
- **Density**: 2.70 g/cm³
- **Young's Modulus**: 69 GPa
- **Yield Strength**: 170 MPa
- **Tensile Strength**: 310 MPa
- **Standards**: ASTM B221, EN AW-6061, ISO 209
- **Applications**: General purpose tubing, Automotive, Structural components
- **Availability**: Common
- **Cost**: $2.00/kg

### Aluminum Alloy 6061-T6
- **ID**: `aluminum-6061-t6`
- **Density**: 2.70 g/cm³
- **Young's Modulus**: 69 GPa
- **Yield Strength**: 275 MPa
- **Tensile Strength**: 310 MPa
- **Standards**: ASTM B221-T6, EN AW-6061-T6, ISO 209
- **Applications**: Structural tubing, Aircraft components, Aerospace
- **Availability**: Common
- **Cost**: $2.25/kg

### Aluminum Alloy 7075
- **ID**: `aluminum-7075`
- **Density**: 2.81 g/cm³
- **Young's Modulus**: 72 GPa
- **Yield Strength**: 505 MPa
- **Tensile Strength**: 570 MPa
- **Standards**: ASTM B221, EN AW-7075, ISO 209
- **Applications**: High strength tubing, Aircraft structures, Premium racing
- **Availability**: Standard
- **Cost**: $3.50/kg

### Aluminum Alloy 7075-T6
- **ID**: `aluminum-7075-t6`
- **Density**: 2.81 g/cm³
- **Young's Modulus**: 72 GPa
- **Yield Strength**: 505 MPa
- **Tensile Strength**: 570 MPa
- **Standards**: ASTM B221-T6, EN AW-7075-T6, ISO 209
- **Applications**: Aerospace tubing, High performance, Racing frames
- **Availability**: Standard
- **Cost**: $4.00/kg

### Aluminum Alloy 5083 (Marine Grade)
- **ID**: `aluminum-5083`
- **Density**: 2.66 g/cm³
- **Young's Modulus**: 70 GPa
- **Yield Strength**: 215 MPa
- **Tensile Strength**: 345 MPa
- **Standards**: ASTM B221, EN AW-5083, ISO 209
- **Applications**: Marine tubing, Seawater resistant, Naval vessels
- **Availability**: Common
- **Cost**: $2.75/kg

### Aluminum Alloy 5754
- **ID**: `aluminum-5754`
- **Density**: 2.66 g/cm³
- **Young's Modulus**: 70 GPa
- **Yield Strength**: 215 MPa
- **Tensile Strength**: 250 MPa
- **Standards**: ASTM B221, EN AW-5754, ISO 209
- **Applications**: Marine applications, Offshore structures, Windsurfing
- **Availability**: Standard
- **Cost**: $2.50/kg

---

## Copper & Brass (4 types)

### Pure Copper
- **ID**: `copper-pure`
- **Density**: 8.96 g/cm³
- **Young's Modulus**: 130 GPa
- **Yield Strength**: 69 MPa
- **Tensile Strength**: 200 MPa
- **Standards**: ASTM B152, EN 1652, DIN 1707
- **Applications**: Electrical tubing, Heat transfer, Plumbing
- **Availability**: Common
- **Cost**: $4.50/kg

### Brass 70/30 (Yellow Brass)
- **ID**: `brass-7030`
- **Density**: 8.47 g/cm³
- **Young's Modulus**: 103 GPa
- **Yield Strength**: 276 MPa
- **Tensile Strength**: 380 MPa
- **Standards**: ASTM B134, EN 12165, CW614N
- **Applications**: Decorative tubing, General purpose brass, Fittings
- **Availability**: Common
- **Cost**: $3.75/kg

### Bronze (Cu-Sn)
- **ID**: `bronze-cu-sn`
- **Density**: 8.75 g/cm³
- **Young's Modulus**: 120 GPa
- **Yield Strength**: 245 MPa
- **Tensile Strength**: 380 MPa
- **Standards**: ASTM B105, EN 1982, ISO 1338
- **Applications**: Bearing tubing, High strength brass, Marine components
- **Availability**: Standard
- **Cost**: $5.00/kg

### Cupro-Nickel 90/10
- **ID**: `cupro-nickel-9010`
- **Density**: 8.94 g/cm³
- **Young's Modulus**: 150 GPa
- **Yield Strength**: 200 MPa
- **Tensile Strength**: 340 MPa
- **Standards**: ASTM B467, EN 12420, ISO 5385
- **Applications**: Seawater tubing, Condenser tubes, Marine services
- **Availability**: Standard
- **Cost**: $5.50/kg

---

## Titanium (3 types)

### Titanium Grade 2 (Commercially Pure)
- **ID**: `titanium-grade2`
- **Density**: 4.51 g/cm³
- **Young's Modulus**: 103 GPa
- **Yield Strength**: 345 MPa
- **Tensile Strength**: 450 MPa
- **Standards**: ASTM B265, EN 17861, ISO 5832
- **Applications**: Aerospace tubing, Chemical processing, Medical implants
- **Availability**: Premium
- **Cost**: $12.00/kg

### Titanium Grade 5 (Ti-6Al-4V)
- **ID**: `titanium-grade5`
- **Density**: 4.54 g/cm³
- **Young's Modulus**: 103 GPa
- **Yield Strength**: 880 MPa
- **Tensile Strength**: 930 MPa
- **Standards**: ASTM B265, EN 17861, AMS 4911
- **Applications**: Aircraft structures, Jet engines, Aerospace frames
- **Availability**: Premium
- **Cost**: $15.00/kg

### Titanium Grade 23 (Ti-6Al-2.5V ELI)
- **ID**: `titanium-grade23`
- **Density**: 4.56 g/cm³
- **Young's Modulus**: 100 GPa
- **Yield Strength**: 790 MPa
- **Tensile Strength**: 860 MPa
- **Standards**: ASTM B265, EN 17861, AMS 4918
- **Applications**: Surgical implants, Biomedical devices, High purity aerospace
- **Availability**: Premium
- **Cost**: $18.00/kg

---

## Nickel Alloys (3 types)

### Inconel 625 (Nickel Alloy)
- **ID**: `inconel-625`
- **Density**: 8.44 g/cm³
- **Young's Modulus**: 207 GPa
- **Yield Strength**: 515 MPa
- **Tensile Strength**: 895 MPa
- **Standards**: ASTM B443, EN 2.4856, AMS 5599
- **Applications**: Jet engines, Cryogenic tubing, High temperature service
- **Availability**: Premium
- **Cost**: $20.00/kg

### Hastelloy C-276 (Nickel Alloy)
- **ID**: `hastelloy-c276`
- **Density**: 8.89 g/cm³
- **Young's Modulus**: 210 GPa
- **Yield Strength**: 448 MPa
- **Tensile Strength**: 793 MPa
- **Standards**: ASTM B575, EN 2.4819, AMS 5536
- **Applications**: Chemical processing, Extreme corrosion resistance, High temp
- **Availability**: Premium
- **Cost**: $25.00/kg

### Monel 400 (Nickel-Copper Alloy)
- **ID**: `monel-400`
- **Density**: 8.80 g/cm³
- **Young's Modulus**: 180 GPa
- **Yield Strength**: 276 MPa
- **Tensile Strength**: 585 MPa
- **Standards**: ASTM B163, EN 2.4360, AMS 4675
- **Applications**: Seawater service, Aerospace tubing, Chemical plants
- **Availability**: Premium
- **Cost**: $12.50/kg

---

## Custom Materials

### Custom Material
- **ID**: `custom`
- **Density**: 5.0 g/cm³ (default)
- **Availability**: Custom
- **Cost**: $0.00/kg (user-defined)

Users can add custom materials with their own properties, standards, and applications.

---

## MaterialDatabase API

### C++ API

```cpp
#include "bendcore/analysis.hpp"

// Get singleton instance
MaterialDatabase& db = MaterialDatabase::instance();

// Load materials from JSON file
db.loadMaterialsFromFile("data/materials.json");

// Lookup operations
std::optional<MaterialProperties> mat = db.getMaterial("steel-1020");
std::vector<MaterialProperties> allMats = db.getAllMaterials();
std::vector<MaterialProperties> steels = db.getMaterialsByCategory("Carbon Steel");

// Custom material operations
MaterialProperties custom;
custom.id = "custom-001";
custom.name = "My Custom Material";
custom.density = 5.5;
db.addCustomMaterial(custom);

// Utility methods
double density = db.getMaterialDensity("steel-1020");  // 7.85
std::vector<std::string> categories = db.getCategories();

// Standards compliance
bool compliant = db.checkStandardsCompliance("steel-1020", {"DIN 17100"});
```

### IPC Commands

```json
// Get all materials
{
  "command": "get-materials",
  "args": {}
}

// Get specific material
{
  "command": "get-material",
  "args": {
    "id": "steel-1020"
  }
}

// Get materials by category
{
  "command": "get-materials-by-category",
  "args": {
    "category": "Carbon Steel"
  }
}

// Add custom material
{
  "command": "add-custom-material",
  "args": {
    "id": "custom-001",
    "name": "My Material",
    "density": 5.5,
    "category": "Custom",
    "availability": "Custom",
    "properties": {
      "youngsModulus": 200,
      "yieldStrength": 400,
      "tensileStrength": 500,
      "elongation": 10,
      "hardness": 200,
      "cost": 0
    }
  }
}
```

---

## Best Practices

1. **Material Selection**: Choose materials based on:
   - Operating temperature requirements
   - Corrosion environment (marine, chemical, etc.)
   - Strength requirements
   - Cost constraints
   - Manufacturing capabilities

2. **Standards Compliance**: Always verify that the selected material meets:
   - Customer specifications
   - Industry standards (DIN, EN, ASTM, ISO)
   - Application requirements

3. **Cost Optimization**: Consider:
   - Material availability and lead times
   - Bulk discount opportunities
   - Alternative materials with similar properties
   - Total cost of ownership (including machining)

4. **Temperature Considerations**:
   - Carbon steels: up to 400°C
   - Stainless steels: up to 500°C (304), 600°C (316)
   - Titanium: up to 700°C
   - Nickel alloys: up to 1000°C+

---

## Version History

- **v1.0** (2026-10-23): Initial release with 30+ materials, comprehensive properties and standards database.

