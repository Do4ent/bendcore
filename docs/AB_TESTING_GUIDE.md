# A/B Testing Guide

## Overview

A/B testing enables data-driven feature improvements by comparing user experience variants. BendCore uses hash-based assignment for consistent, reproducible variant allocation.

## Quick Start

### 1. Define Hypothesis

```
Feature: Enhanced Template Matcher
Hypothesis: Showing more template options increases selection rate
Control: Current dropdown UI (top 3 results)
Treatment: New full panel UI with filters
Expected Impact: +20% conversion rate
```

### 2. Create Experiment

```cpp
#include "bendcore/ab_testing.hpp"

ABTestManager& manager = ABTestManager::getInstance();

ABTestManager::ExperimentConfig config;
config.experiment_id = "template_match_v1";
config.control_percentage = 50.0;  // 50/50 split
config.force_variant = false;

manager.register_experiment(config);
```

### 3. Assign Variant

```cpp
auto variant = manager.get_variant(config, user_id);

if (variant == ABTestManager::Variant::CONTROL) {
  showDropdownUI();
} else {
  showPanelUI();
}
```

### 4. Track Conversion Event

```cpp
queue.track_event(events::template_selected(user_id, session_id, 
  template_id));
```

### 5. Analyze Results

Monitor experiment in GrowthBook dashboard:
- Conversion rates for control and treatment
- Statistical significance
- Sample size
- Confidence intervals

## How Variant Assignment Works

### Hash-Based Consistency

Each user gets consistent variant assignment across sessions:

```cpp
Hash(user_id + experiment_id) % 100 < control_percentage
↓
CONTROL: 0-49 (50%)
TREATMENT: 50-99 (50%)
```

**Benefits:**
- Same user always gets same variant
- No "flickering" between variations
- Deterministic and reproducible
- Server-independent

### Example Calculation

```
User: user_123
Experiment: template_match_v1
Combined: "user_123::template_match_v1"
Hash: 7924... (mod 100) = 42
Control Threshold: 50
Result: 42 < 50 → CONTROL
```

## Experiment Lifecycle

### Phase 1: Design (Days 1-2)

1. **Define Hypothesis**
   - What change are you testing?
   - What metric indicates success?
   - Expected impact size?

2. **Choose Sample Size**
   - Minimum: 5,000 users per variant
   - Recommended: 10,000+ for stable results
   - Duration: Run for 1-2 weeks

3. **Plan Rollout**
   - How to implement feature?
   - Fallback plan if things go wrong?
   - Monitoring strategy?

### Phase 2: Launch (Day 3)

1. **Create Feature Flag**
```
Feature: template_match_enhanced
Rule: 50% to all users
```

2. **Deploy Code**
```cpp
if (gb.is_feature_enabled("template_match_enhanced")) {
  showEnhancedUI();
} else {
  showCurrentUI();
}
```

3. **Create Experiment**
```
Name: Template Match UI v1
Tracking Key: user_id
Sample Split: 50/50
Metric: template_selected (conversion)
```

4. **Start Tracking**
- Verify events flowing to GrowthBook
- Check data quality
- Monitor error rates

### Phase 3: Analysis (Days 3-14)

1. **Daily Check-ins**
   - Sample sizes balanced?
   - Significant difference appearing?
   - Any error spikes?

2. **Watch for Issues**
   - Performance degradation?
   - User complaints?
   - Data anomalies?

3. **Statistical Monitoring**
   - After ~1 week: preliminary results?
   - After 2 weeks: statistically significant?
   - P-value < 0.05 indicates significance

### Phase 4: Decision (Day 14+)

**If Treatment Wins (p < 0.05):**
- ✅ Feature is better
- → Rollout to 100%
- → Remove feature flag

**If Control Wins (p < 0.05):**
- ✅ Current implementation is better
- → Keep existing feature
- → Iterate on hypothesis

**If Inconclusive (p ≥ 0.05):**
- ⚠️ Insufficient data
- → Continue running experiment
- → Increase sample size
- → Run longer (2-3 weeks)

## The 3 Active Experiments

### Experiment 1: Template Match UI

```
Experiment ID: template_match_v1
Feature: enhanced_template_matcher
Duration: 2 weeks
Sample: 10,000 users per variant

Control (50%):
├─ Dropdown UI
├─ Top 3 results shown
└─ Click to select

Treatment (50%):
├─ Full panel UI
├─ Advanced filters
├─ Sorting options
└─ Click to select

Metric: template_selected
Expected: +15% conversion
```

**Tracking Events:**
```
template_match_requested → (baseline)
template_match_success → (successful match)
template_selected → (conversion)
```

### Experiment 2: Search Filter Layout

```
Experiment ID: search_filter_layout_v1
Feature: horizontal_search_filters
Duration: 2 weeks
Sample: 10,000 users per variant

Control (50%):
├─ Vertical sidebar
├─ Left side of screen
└─ Fixed width

Treatment (50%):
├─ Horizontal toolbar
├─ Top of search results
└─ Collapsible

Metric: search_query (engagement)
Expected: +10% search volume
```

**Tracking Events:**
```
search_opened → (baseline)
search_filter_applied → (filter usage)
search_results_count → (engagement)
```

### Experiment 3: Formula Editor UX

```
Experiment ID: formula_editor_ux_v1
Feature: formula_autocomplete
Duration: 2 weeks
Sample: 10,000 users per variant

Control (50%):
├─ Text input only
├─ Manual variable entry
└─ No hints

Treatment (50%):
├─ Text input with autocomplete
├─ Syntax highlighting
├─ Function hints
└─ Error detection

Metric: formula_created (adoption)
Expected: +20% creation rate
```

**Tracking Events:**
```
formula_created → (adoption)
formula_error → (error rate)
formula_evaluated → (success rate)
```

## Monitoring & QA

### Pre-Launch Checklist

- [ ] Feature flag created in GrowthBook
- [ ] Code implements feature flag check
- [ ] Tracking events implemented
- [ ] Edge cases handled (logged out users, etc.)
- [ ] Performance tested (< 50ms overhead)
- [ ] Error handling in place
- [ ] Fallback mechanism ready

### Launch Checklist

- [ ] 50/50 split configured correctly
- [ ] Events flowing to GrowthBook
- [ ] Sample sizes balanced
- [ ] No error spikes
- [ ] User experience verified on both variants
- [ ] Monitoring alerts configured

### Daily Monitoring

```sql
-- Check event volume (in GrowthBook Analytics)
SELECT COUNT(*) as event_count
WHERE date >= TODAY() - INTERVAL '1 day'

-- Check variant distribution
SELECT variant, COUNT(DISTINCT user_id)
FROM events
GROUP BY variant

-- Check conversion rate by variant
SELECT 
  variant,
  COUNT(CASE WHEN event_name = 'template_selected' THEN 1 END) * 100.0 / 
  COUNT(DISTINCT user_id) as conversion_pct
GROUP BY variant
```

## Troubleshooting

### Unbalanced Sample Sizes

**Problem:** 60% control, 40% treatment

**Causes:**
- Variant assignment bug
- User ID not consistent
- Feature flag misconfiguration

**Solution:**
1. Check hash function is symmetric
2. Verify user IDs don't change mid-session
3. Restart experiment with new hash seed

### No Statistical Significance After 2 Weeks

**Problem:** P-value still > 0.05

**Causes:**
- Difference too small to detect
- Natural variation too high
- Conversion metric not sensitive enough

**Solution:**
1. Continue running 1+ more week
2. Try different metric (e.g., time-to-conversion)
3. Increase sample size
4. Check conversion metrics are defined correctly

### Unexpected Performance Issues

**Problem:** Treatment variant causes slowdown

**Causes:**
- Enhanced UI has rendering overhead
- New feature is not optimized
- Memory leak in new code

**Solution:**
1. Profile the feature (Chrome DevTools)
2. Identify bottleneck
3. Optimize or revert
4. Disable feature with flag

### User Complaints About Treatment

**Problem:** Users prefer control variant

**Causes:**
- UX design issues
- Unfamiliar interface
- Missing functionality

**Solution:**
1. Gather user feedback (surveys, support tickets)
2. Compare with metric data
3. If metrics disagree with feedback:
   - Users may adjust over time
   - Try A/A test (control vs same control)
4. If metrics confirm: revert and redesign

## Statistical Concepts

### Significance Level (p-value)

- **p < 0.05**: Statistically significant (95% confidence)
- **p < 0.01**: Highly significant (99% confidence)
- **p ≥ 0.05**: Not significant (could be random variation)

### Sample Size

Larger sample = more confident results:
- 1,000 users: ±5% confidence intervals
- 10,000 users: ±1.5% confidence intervals
- 100,000 users: ±0.5% confidence intervals

### Conversion Rate Difference

- Control: 10.5%
- Treatment: 12.3%
- Difference: +1.8 percentage points
- Relative improvement: +17% ((12.3-10.5)/10.5)

### Sequential Testing

Monitoring results during experiment increases Type I error.
Solution: Set fixed sample size and duration beforehand.

## Best Practices

### Before Launching

1. ✓ Define hypothesis before running test
2. ✓ Set sample size based on power analysis
3. ✓ Plan decision criteria in advance
4. ✓ Have contingency plan

### During Experiment

1. ✓ Let experiment run for full planned duration
2. ✓ Don't "peek" too frequently at results
3. ✓ Monitor for technical issues daily
4. ✓ Track all events accurately

### When Analyzing

1. ✓ Use pre-defined metrics
2. ✓ Check statistical significance (p-value)
3. ✓ Look at practical significance (effect size)
4. ✓ Consider external factors (seasonality, etc.)

### When Rolling Out

1. ✓ Start with small percentage (5-10%)
2. ✓ Monitor for 24 hours
3. ✓ Increase gradually: 10% → 25% → 50% → 100%
4. ✓ Keep feature flag for quick rollback

## Advanced Topics

### Multivariate Testing

Test multiple variables simultaneously:

```cpp
// Not just UI layout, but also colors
ABTestManager::ExperimentConfig config;
config.experiment_id = "template_ui_multivariate";
// Could test 2x2 = 4 combinations:
// - Layout (dropdown vs panel) x Color (blue vs green)
```

### Cohort Analysis

Analyze subgroups separately:

```sql
SELECT 
  os,
  variant,
  COUNT(DISTINCT user_id) as users,
  template_selection_rate
FROM events
GROUP BY os, variant
```

Different OS might respond differently to change!

### Frequentist vs Bayesian

- **Frequentist** (GrowthBook default): Fixed sample size, p-values
- **Bayesian**: Probability distributions, can stop early

## Resources

- GrowthBook Docs: https://docs.growthbook.io
- Statistics Primer: https://analytics.google.com/analytics/web/demo
- BendCore Events: /docs/ANALYTICS_TRACKING.md
- API Reference: /docs/ANALYTICS_API_REFERENCE.md

## Support

For questions about A/B testing methodology:
- GrowthBook Support: support@growthbook.io
- Statistics Resources: www.optimizely.com/optimization-glossary/
- Internal Wiki: [Contact BendCore team]
