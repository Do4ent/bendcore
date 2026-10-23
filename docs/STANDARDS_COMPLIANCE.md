# BendCore Standards Compliance Guide

## Overview

This document describes how BendCore implements standards compliance checking for materials and tube bending design. The system supports DIN, EN, ASTM, and ISO standards.

## Standards Reference

### Carbon Steel Standards

| Standard | Organization | Application | Notes |
|----------|--------------|-------------|-------|
| DIN 17100 | German | General purpose carbon steel | European standard |
| EN 10025 | European | Structural and general purpose steel | Replaced DIN 17100 |
| ASTM A36 | American | Structural steel | Most common in USA |
| ASTM 1018 | American | Low carbon steel | Carburizing applications |
| ASTM 1020 | American | Low carbon steel | General purpose |
| ASTM 1045 | American | Medium carbon steel | Medium strength |
| ASTM 1060 | American | High carbon steel | High strength springs |
| SAE 1020 | American | SAE designation | Equivalent to ASTM 1020 |
| DIN 1652 | German | European equivalents | Alloyed steels |
| EN 10083 | European | Heat treatable steels | Specification for testing |

### Stainless Steel Standards

| Standard | Organization | Application | Notes |
|----------|--------------|-------------|-------|
| ASTM 304 | American | Austenitic stainless | Most versatile |
| ASTM 304L | American | Low carbon austenitic | Better weldability |
| ASTM 316 | American | Molybdenum austenitic | Enhanced corrosion |
| ASTM 316L | American | Low carbon 316 | Better weldability |
| ASTM 430 | American | Ferritic stainless | Lower cost |
| EN 1.4301 | European | Equivalent to 304 | European designation |
| EN 1.4306 | European | Equivalent to 304L | European designation |
| EN 1.4401 | European | Equivalent to 316 | European designation |
| EN 1.4404 | European | Equivalent to 316L | European designation |
| EN 1.4016 | European | Equivalent to 430 | European designation |
| ISO 5347 | International | Stainless steel tubing | General specification |

### Precipitation Hardening (PH) Stainless Steels

| Standard | Material | Yield Strength | Application |
|----------|----------|-----------------|-------------|
| ASTM 630 | 17-4 PH | 1310 MPa | Aerospace, high strength |
| AMS 5604 | 17-4 PH | 1310 MPa | Aerospace material specification |
| EN 1.4548 | 17-4 PH | 1310 MPa | European equivalent |
| AMS 5604 | 17-7 PH | 1380 MPa | Enhanced properties |
| AMS 4918 | Ti-6Al-2.5V | 790 MPa | Surgical implants |

### Aluminum Standards

| Standard | Organization | Application | Notes |
|----------|--------------|-------------|-------|
| ASTM B221 | American | Aluminum extrusions | General specification |
| EN AW-6061 | European | 6061 alloy | European designation |
| EN AW-7075 | European | 7075 alloy | High strength |
| EN AW-5083 | European | 5083 alloy | Marine grade |
| ISO 209 | International | Aluminum alloy designation | Numbering system |

### Copper and Brass Standards

| Standard | Organization | Application | Notes |
|----------|--------------|-------------|-------|
| ASTM B152 | American | Copper tube | General specification |
| ASTM B134 | American | Brass rod/tube | Yellow brass |
| ASTM B105 | American | Bronze rod/tube | Cu-Sn alloys |
| ASTM B467 | American | Cupro-nickel | Seawater service |
| EN 1652 | European | Copper | European specification |
| DIN 1707 | German | Copper | German specification |
| EN 12165 | European | Brass | European specification |
| EN 1982 | European | Bronze | European specification |
| ISO 5385 | International | Cupro-nickel | International standard |

### Titanium Standards

| Standard | Organization | Application | Notes |
|----------|--------------|-------------|-------|
| ASTM B265 | American | Titanium | General specification |
| EN 17861 | European | Titanium | European designation |
| ISO 5832 | International | Titanium for implants | Medical grade |
| AMS 4911 | American | Ti-6Al-4V | Aerospace standard |
| AMS 4918 | American | Ti-6Al-2.5V | Aerospace standard |

### Nickel Alloys

| Standard | Organization | Application | Notes |
|----------|--------------|-------------|-------|
| ASTM B443 | American | Inconel 625 | Nickel-chromium-molybdenum |
| ASTM B575 | American | Hastelloy C-276 | Extreme corrosion resistance |
| ASTM B163 | American | Monel 400 | Nickel-copper |
| EN 2.4856 | European | Inconel 625 | European designation |
| EN 2.4819 | European | Hastelloy C-276 | European designation |
| AMS 5599 | American | Inconel 625 | Aerospace specification |
| AMS 5536 | American | Hastelloy C-276 | Aerospace specification |

---

## Material Standards by Category

### Carbon Steels

**Grade 1020 (Low Carbon)**
- Standards: DIN 17100, EN 10025, ASTM A36, ASTM 1020, SAE 1020
- Typical Applications: Hydraulic tubing, general purpose
- Yield Strength: 355 MPa
- Best For: Cost-effective, general applications

**Grade 1045 (Medium Carbon)**
- Standards: ASTM 1045, SAE 1045, EN 10083
- Typical Applications: Drive shafts, cranks, medium strength
- Yield Strength: 450 MPa
- Best For: Medium strength requirements

**Grade 1060 (High Carbon)**
- Standards: ASTM 1060, SAE 1060
- Typical Applications: Springs, high strength applications
- Yield Strength: 530 MPa
- Best For: Springs and high strength parts

**Alloy Steel 4140**
- Standards: ASTM 4140, SAE 4140, EN 19
- Typical Applications: Aircraft hydraulic, landing gear
- Yield Strength: 1000 MPa
- Best For: Aerospace, high strength

**Alloy Steel 4340**
- Standards: ASTM 4340, SAE 4340, EN 26
- Typical Applications: Aerospace, racing
- Yield Strength: 1160 MPa
- Best For: Premium aerospace applications

### Stainless Steels

**Grade 304 (Austenitic)**
- Standards: ASTM 304, EN 1.4301, ISO 5347
- Typical Applications: Chemical, food, general corrosion
- Yield Strength: 205 MPa
- Corrosion Resistance: Good in most environments
- Best For: General purpose stainless steel

**Grade 304L (Low Carbon)**
- Standards: ASTM 304L, EN 1.4306
- Typical Applications: Welded structures, cryogenic
- Yield Strength: 170 MPa
- Best For: Welded applications (better weldability)

**Grade 316 (Molybdenum)**
- Standards: ASTM 316, EN 1.4401
- Typical Applications: Marine, chemical processing
- Yield Strength: 170 MPa
- Corrosion Resistance: Superior in salt water
- Best For: Marine and harsh chemical environments

**Grade 316L (Low Carbon)**
- Standards: ASTM 316L, EN 1.4404
- Typical Applications: Welded marine, pharmaceutical
- Yield Strength: 170 MPa
- Best For: Welded marine applications

**Grade 17-4 PH (Precipitation Hardened)**
- Standards: ASTM 630, EN 1.4548, AMS 5604
- Typical Applications: Aerospace, high strength
- Yield Strength: 1310 MPa
- Best For: Aerospace with high strength and corrosion resistance

### Aluminum Alloys

**6061 (General Purpose)**
- Standards: ASTM B221, EN AW-6061
- Typical Applications: Structural, automotive
- Yield Strength: 170 MPa (annealed)
- Best For: General purpose applications

**6061-T6 (Heat Treated)**
- Standards: ASTM B221-T6, EN AW-6061-T6
- Typical Applications: Structural, aerospace
- Yield Strength: 275 MPa
- Best For: Structural tubing with higher strength

**7075 (High Strength)**
- Standards: ASTM B221, EN AW-7075
- Typical Applications: Aircraft structures, racing
- Yield Strength: 505 MPa
- Best For: High strength lightweight structures

**7075-T6 (Heat Treated)**
- Standards: ASTM B221-T6, EN AW-7075-T6
- Typical Applications: Aerospace, racing frames
- Yield Strength: 505 MPa
- Best For: Aerospace with ultimate strength needs

**5083 (Marine Grade)**
- Standards: ASTM B221, EN AW-5083
- Typical Applications: Marine vessels, seawater
- Yield Strength: 215 MPa
- Best For: Marine and saltwater applications

---

## Compliance Checking in BendCore

### MaterialDatabase API

The `MaterialDatabase` class provides compliance checking:

```cpp
// Check if a material meets required standards
bool compliant = db.checkStandardsCompliance(
    "steel-1020",
    {"DIN 17100", "EN 10025"}
);

// Get all standards for a material
std::vector<std::string> standards = db.getCommonStandards("steel-1020");
// Returns: {"DIN 17100", "EN 10025", "ASTM A36"}
```

### Bend Radius Standards

Different standards specify minimum bend radii based on material properties:

```
For carbon steel tubes:
- DIN 2391: Min bend radius = 4 × OD
- EN ISO 5949: Min bend radius = 3 × OD (annealed)
- ASTM A513: Min bend radius = 3 × OD to 5 × OD

For stainless steel:
- ASTM A269: Min bend radius = 4 × OD to 6 × OD
- EN 10088-2: Min bend radius = 3 × OD (annealed)

For aluminum:
- ASTM B210: Min bend radius = 2 × OD to 4 × OD
- EN 12412-2: Min bend radius = 2 × OD (annealed)
```

### Material Selection Checklist

1. **Standards Verification**
   - [ ] Customer required standards listed
   - [ ] Material complies with all standards
   - [ ] Documentation attached to design

2. **Properties Verification**
   - [ ] Young's Modulus appropriate for application
   - [ ] Yield Strength meets load requirements
   - [ ] Elongation sufficient for bending
   - [ ] Hardness suitable for machining

3. **Environment Verification**
   - [ ] Temperature range compatible
   - [ ] Corrosion environment addressed
   - [ ] Chemical compatibility confirmed
   - [ ] Fatigue requirements met

4. **Manufacturing Verification**
   - [ ] Bend radius achievable on equipment
   - [ ] Springback factor known
   - [ ] Surface finish compatible
   - [ ] Lead time acceptable

---

## Common Standard Combinations

### Aerospace Applications
- Primary: ASTM, AMS
- Secondary: EN, ISO
- Materials: 4140, 4340, 9310, 17-4 PH, Ti-6Al-4V

### Marine Applications
- Primary: ASTM B467, EN 12420
- Materials: 316L, 5083, Cupro-nickel 90/10

### Automotive Applications
- Primary: DIN, EN, ISO
- Materials: 1020, 1045, 6061, Stainless 430

### Chemical Processing
- Primary: ASTM B75, ASTM B163
- Materials: 304, 316, Hastelloy, Monel

### Food/Pharmaceutical
- Primary: ASTM 304L, ASTM 316L
- Materials: 304L, 316L, Titanium Grade 23

---

## Regulatory Considerations

### PED (Pressure Equipment Directive)
- Applies to EU pressure equipment
- Requires material certification for Article 4 equipment
- All materials in BendCore database are PED-compliant options

### ASME Boiler and Pressure Vessel Code
- US standard for pressure equipment
- Specifies approved materials in Section II
- Section VIII Division 1 lists pressure boundary materials

### ISO 1127 (Stainless Steel Tubing)
- International standard for tube sizes and tolerances
- All stainless steel grades must meet dimensions
- Specifies surface condition and marking

### API Standards
- API 5L (Petroleum pipelines)
- API 579 (Fitness-for-service)
- Applicable to oil/gas applications

---

## Documentation & Traceability

Each material in the BendCore database includes:

1. **Material Certificate**: Mill test report with composition analysis
2. **Standards Certification**: Verification against applicable standards
3. **Property Verification**: Test data for mechanical properties
4. **Availability Status**: Current stock and lead times
5. **Application Notes**: Recommended uses and limitations

---

## Version History

- **v1.0** (2026-10-23): Initial standards compliance documentation covering DIN, EN, ASTM, and ISO standards for 30+ materials.

