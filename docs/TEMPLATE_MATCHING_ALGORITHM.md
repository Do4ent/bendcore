# Template Matching Algorithm

## Overview

The BendCore Template Matching Algorithm automatically identifies the best tube bend template for a given set of tube specifications using a sophisticated weighted scoring system.

## Algorithm Goals

1. **Accuracy**: Achieve 90%+ match accuracy for standard use cases
2. **Performance**: Execute matching in <100ms for responsive UI
3. **Transparency**: Provide clear scoring rationale
4. **Extensibility**: Support new parameters and templates easily

## Weighted Scoring System

The matching algorithm combines multiple parameters with carefully calibrated weights:

### Formula

```
confidence = (OD_score × 0.30 + 
              Wall_score × 0.20 + 
              Bend_score × 0.25 + 
              Radius_score × 0.15 + 
              Spacing_score × 0.10) × 100
```

### Weight Rationale

- **OD (30%)**: Outer diameter is the primary sizing parameter
  - Most critical for selecting appropriate tooling
  - Largest range of variation across templates
  - Direct impact on bending machine selection

- **Wall Thickness (20%)**: Material stress and tooling requirements
  - Secondary sizing parameter
  - Affects springback compensation and bend quality
  - Influences material grade requirements

- **Bend Angle (25%)**: Manufacturing complexity
  - Critical for determining stress and springback
  - 90° bends vs. 180° bends have very different requirements
  - Affects machine selection more than geometry alone

- **Radius Ratio (15%)**: Bend radius-to-diameter ratio (R/D)
  - Tertiary parameter after OD and wall
  - Affects bend quality and springback
  - Typical range: 1.5–6.0

- **Bend Spacing (10%)**: Minimum distance between consecutive bends
  - Least critical parameter
  - Affects cooling and setup time
  - Default 15–50mm spacing

## Parameter Scoring

Each parameter is scored individually from 0.0 to 1.0:

### Scoring Function

```javascript
function scoreParameter(value, param) {
  // param = { min, max, default }
  
  if (value >= param.min && value <= param.max) {
    return 1.0;  // Perfect match
  }
  
  const distance = value < param.min 
    ? param.min - value 
    : value - param.max;
  
  const range = param.max - param.min;
  
  // Linear penalty: 0% match at distance = range
  return Math.max(0, 1.0 - (distance / range));
}
```

### Score Interpretation

- **1.0**: Value is within template's optimal range
- **0.5**: Value is at the edge of acceptable range
- **0.0**: Value is outside acceptable range (distance = range)
- **<0.0**: Clamped to 0.0 (completely unacceptable)

## Special Parameter Handling

### Bend Angle

For bend angles beyond the template's maximum:

```javascript
function scoreBendAngle(angle, maxAngle) {
  if (angle <= maxAngle) return 1.0;
  
  // Penalty increases as angle exceeds max
  const overage = angle - maxAngle;
  return Math.max(0, 1.0 - (overage / maxAngle));
}
```

### Bend Spacing

Spacing is only penalized if value is less than minimum:

```javascript
function scoreBendSpacing(spacing, minSpacing) {
  if (spacing >= minSpacing) return 1.0;
  
  const shortfall = minSpacing - spacing;
  return Math.max(0, 1.0 - (shortfall / minSpacing));
}
```

## Example Calculation

### Scenario: General Tube Design

**Input Parameters**:
- OD: 25 mm
- Wall: 2.0 mm
- Bend Angle: 90°
- Radius Ratio: 2.0 (R/D)
- Bend Spacing: 20 mm

**Template: Standard 90° Bend**
- OD Range: 6–100 mm
- Wall Range: 0.5–5.0 mm
- Max Bend Angle: 180°
- R/D Range: 1.5–3.0
- Min Bend Spacing: 15 mm

**Calculation**:

```
OD_score = 1.0 (25 is within 6-100)
Wall_score = 1.0 (2.0 is within 0.5-5.0)
Bend_score = 1.0 (90 is within 0-180)
Radius_score = 1.0 (2.0 is within 1.5-3.0)
Spacing_score = 1.0 (20 >= 15)

confidence = (1.0×0.30 + 1.0×0.20 + 1.0×0.25 + 1.0×0.15 + 1.0×0.10) × 100
           = 1.0 × 100
           = 100%
```

### Result

**Perfect Match (100%)** - All parameters within optimal ranges

---

## Confidence Score Bands

### 95-100%: Perfect Match

**Criteria**:
- All parameters within or near optimal ranges
- No constraint violations
- Standard manufacturing setup applicable

**Recommendation**: Use this template immediately

**Example**:
- OD: 25mm (template: 6-100mm)
- Wall: 2mm (template: 0.5-5mm)
- Bend: 90° (template max: 180°)

### 85-95%: Good Match

**Criteria**:
- Most parameters optimal, 1-2 slightly marginal
- Minor constraint considerations
- Possible small setup adjustments

**Recommendation**: Good choice, verify with manufacturing

**Example**:
- OD: 5mm (template min: 6mm) = 0.67 score
- Wall: 2mm (optimal) = 1.0 score
- Combined: ~85% confidence

### 75-85%: Acceptable Match

**Criteria**:
- Multiple parameters slightly outside ranges
- Noticeable constraint violations
- Custom tooling or setup likely required

**Recommendation**: Consult manufacturing team

**Example**:
- OD: 2mm (template min: 6mm) = 0.33 score
- Wall: 1mm (template min: 0.5mm) = 0.67 score
- Combined: ~75% confidence

### <75%: Poor Match

**Criteria**:
- Significant parameter violations
- Multiple constraints outside ranges
- Template fundamentally misaligned

**Recommendation**: Do not use, consider alternative designs

**Example**:
- OD: 1mm (template min: 6mm) = 0.0 score
- Wall: 0.1mm (template min: 0.5mm) = 0.0 score
- Combined: 0% confidence

---

## Constraint Checking

Beyond scoring, the algorithm also generates constraint violation warnings:

```cpp
std::vector<std::string> violations;

if (od < template.min_od) violations.push_back("OD too small");
if (od > template.max_od) violations.push_back("OD too large");
if (wall < template.min_wall) violations.push_back("Wall too thin");
if (wall > template.max_wall) violations.push_back("Wall too thick");
if (bend > template.max_bend) violations.push_back("Bend angle too large");
if (spacing < template.min_spacing) violations.push_back("Spacing too close");
```

## Recommendation Generation

Based on violations, the algorithm generates actionable recommendations:

```
Violation: "OD too small"
→ Recommendation: "Consider template: Reinforced Small Diameter"

Violation: "Wall too thick"
→ Recommendation: "Consider template: Large Assembly Profile"

Violation: "Bend angle too large"
→ Recommendation: "Multiple bends may be required"
```

## Matching Workflow

```
┌─────────────────────┐
│  Input Parameters   │
│ (OD, Wall, Angle...) │
└──────────┬──────────┘
           │
           ↓
┌─────────────────────────────────┐
│ Score each parameter individually│
│ (0.0 to 1.0 for each)           │
└──────────┬──────────────────────┘
           │
           ↓
┌─────────────────────────────────┐
│ Apply weighted combination       │
│ (0.30 OD, 0.20 Wall, etc.)      │
└──────────┬──────────────────────┘
           │
           ↓
┌─────────────────────────────────┐
│ Convert to confidence percentage │
│ (0-100)                         │
└──────────┬──────────────────────┘
           │
           ↓
┌─────────────────────────────────┐
│ Check for constraint violations  │
│ Generate recommendations        │
└──────────┬──────────────────────┘
           │
           ↓
┌─────────────────────┐
│  Ranked Match List  │
│  (Top 3-5 results)  │
└─────────────────────┘
```

## Performance Characteristics

### Time Complexity

- **Per template**: O(1) - constant time scoring
- **For N templates**: O(N) - linear scan
- **Typical**: ~100 templates scored in <10ms

### Space Complexity

- O(N) - store N match results
- Negligible template memory footprint

### Optimization

- Vectorizable scoring function (SIMD potential)
- Lazy constraint checking (only for final results)
- Early termination possible (not used currently)

## Calibration and Tuning

### Adjusting Weights

To emphasize OD over wall thickness:

```javascript
// Default: OD 30%, Wall 20%, Bend 25%, Radius 15%, Spacing 10%
// Custom: OD 40%, Wall 10%, Bend 25%, Radius 15%, Spacing 10%
```

### Calibration Procedure

1. Collect ground truth: 50+ user selections
2. Calculate weight sensitivity for each parameter
3. Optimize weights to maximize accuracy
4. A/B test with user group
5. Deploy updated weights

## Testing

### Unit Tests

- Parameter scoring function (edge cases)
- Weight combination logic
- Confidence band boundaries
- Constraint violation detection

### Integration Tests

- Full pipeline with real templates
- Performance benchmarking (<100ms)
- Accuracy on test dataset

### User Testing

- Template selection accuracy
- User satisfaction with suggestions
- Time to select template
- Template match against actual manufacturing

---

## Future Enhancements

### Planned Improvements

1. **Material-aware scoring**: Weight templates based on selected material
2. **Cost optimization**: Recommend cheapest option at acceptable quality
3. **Machine learning**: Train weights from actual production data
4. **Multi-objective**: Pareto frontier for cost vs. quality
5. **Constraint propagation**: Suggest parameter ranges for preferred templates

### Research Directions

- Deep learning for complex constraint interactions
- Fuzzy logic for soft constraints
- Genetic algorithms for weight optimization
- Bayesian probabilistic matching

---

**Algorithm Version**: 1.0  
**Last Updated**: 2026-10-23  
**Accuracy Target**: 90%+  
**Performance Target**: <100ms
