# GrowthBook A/B Testing and Feature Flags Setup

This document covers the GrowthBook integration for BendCore CAD, including A/B testing, feature flags, and analytics event tracking.

## Overview

GrowthBook is integrated into BendCore CAD to enable:
- **Feature Flags**: Control feature availability across user segments
- **A/B Testing**: Run concurrent experiments to measure feature impact
- **Analytics**: Track user interactions and measure conversion rates
- **Metrics**: Monitor adoption, conversion rates, and time-to-value

## Installation

### 1. Install GrowthBook SDK

```bash
npm install @growthbook/sdk-js
```

### 2. Set Environment Variables

Add the following to your `.env` file:

```env
# GrowthBook Configuration
REACT_APP_GROWTHBOOK_API_KEY=<your-api-key>
REACT_APP_VERSION=2.0.0
```

Or for other build systems:

```env
GROWTHBOOK_API_KEY=<your-api-key>
VUE_APP_GROWTHBOOK_API_KEY=<your-api-key>
```

## Architecture

### Core Modules

#### 1. **GrowthBook Manager** (`src/growthbook.ts`)
Manages GrowthBook SDK initialization and feature flag evaluation.

```typescript
import { initializeGrowthBook, getGrowthBookManager } from "./src/growthbook";

// Initialize when app starts
await initializeGrowthBook("user_123", "web");

// Get the manager
const manager = getGrowthBookManager();
const isEnabled = manager.isFeatureEnabled("parametric_dimensions");
```

#### 2. **Experiment Manager** (`src/experiment.ts`)
Handles A/B test variant assignment and event tracking.

```typescript
import { getExperimentManager } from "./src/experiment";

const manager = getExperimentManager();

// Register an experiment
manager.registerExperiment({
  experiment_id: "parametric_test",
  name: "Parametric Dimensions A/B Test",
  description: "Testing impact of parametric dimensions feature",
  variants: [
    { id: "control", name: "Control (Disabled)" },
    { id: "treatment", name: "Treatment (Enabled)" }
  ],
  weight_distribution: [50, 50],
  active: true,
  created_at: Date.now()
});

// Assign user to variant
const variant = manager.assignVariant("parametric_test", "user_123");

// Track events
manager.trackEvent({
  experiment_id: "parametric_test",
  event_type: "conversion",
  event_name: "Template Saved",
  properties: { user_id: "user_123" },
  timestamp: Date.now(),
  variant_id: variant
});
```

#### 3. **Analytics Manager** (`src/analytics.ts`)
Tracks analytics events and batches them for API transmission.

```typescript
import { getAnalyticsManager, STANDARD_EVENTS, trackEvent } from "./src/analytics";

// Initialize
const analytics = getAnalyticsManager();
analytics.initialize("user_123", "session_abc");

// Track events using standard event types
trackEvent(STANDARD_EVENTS.TUBE_CREATED, {
  tube_name: "Main Tube Assembly",
  diameter: 10,
  material: "Steel"
});

// Custom events
trackEvent("custom_event", {
  custom_property: "value"
});

// Feature flag exposure tracking
trackEvent(STANDARD_EVENTS.FEATURE_FLAG_EXPOSED, {
  feature_name: "parametric_dimensions",
  enabled: true
});
```

### React Hooks

#### `useFeatureFlag(name)`
Check if a feature flag is enabled.

```typescript
import { useFeatureFlag } from "./ui/src/hooks/useFeatureFlag";

export function MyComponent() {
  const { enabled, isLoading } = useFeatureFlag("parametric_dimensions");

  if (isLoading) return <div>Loading...</div>;

  return enabled ? <ParametricUI /> : <StandardUI />;
}
```

#### `useExperiment(experimentId)`
Get the assigned experiment variant.

```typescript
import { useExperiment } from "./ui/src/hooks/useFeatureFlag";

export function MyComponent() {
  const { variant, isLoading } = useExperiment("parametric_test");

  if (isLoading) return <div>Loading...</div>;

  return variant === "treatment" ? <NewUI /> : <OldUI />;
}
```

#### `useExperimentConversion(experimentId, variantId)`
Track conversions for A/B tests.

```typescript
import { useExperimentConversion } from "./ui/src/hooks/useFeatureFlag";

export function MyComponent() {
  const { trackConversion } = useExperimentConversion("parametric_test", "treatment");

  const handleSave = () => {
    // Save logic...
    trackConversion("template_saved");
  };

  return <button onClick={handleSave}>Save Template</button>;
}
```

### Components

#### `<FeatureFlag>` Component
Conditionally render content based on feature flag status.

```typescript
import { FeatureFlag } from "./ui/src/components/FeatureFlag";

<FeatureFlag 
  name="parametric_dimensions"
  fallback={<div>Feature not available</div>}
>
  <ParametricDimensionsUI />
</FeatureFlag>
```

#### `<FeatureFlagSection>` Component
Render a section with title and conditional visibility.

```typescript
<FeatureFlagSection 
  name="smart_templates"
  title="Smart Templates"
>
  <SmartTemplatesPanel />
</FeatureFlagSection>
```

#### `<ConditionalRender>` Component
Render different content based on feature flag status.

```typescript
<ConditionalRender
  flag="parametric_dimensions"
  enabled={<NewParametricUI />}
  disabled={<OldParametricUI />}
/>
```

## Pre-seeded Test Experiments

Three experiments are pre-configured in GrowthBook:

### 1. Parametric Dimensions
- **Experiment ID**: `parametric_dimensions_test`
- **Description**: A/B test for parametric dimensions feature
- **Variants**:
  - Control (A): Feature disabled
  - Treatment (B): Feature enabled
- **Weight Distribution**: 50/50
- **KPIs**:
  - Adoption rate (% of users with feature active)
  - Conversion rate (users who save templates with feature)
  - Time to first use (avg minutes before first parametric formula)

### 2. Smart Templates
- **Experiment ID**: `smart_templates_test`
- **Description**: A/B test for new template recommendation UI
- **Variants**:
  - Control (A): Old UI (list-based)
  - Treatment (B): New UI (grid-based with recommendations)
- **Weight Distribution**: 50/50
- **KPIs**:
  - Template adoption rate
  - Conversion rate (templates selected)
  - User satisfaction (rating)

### 3. Search Filters
- **Experiment ID**: `search_filters_test`
- **Description**: A/B test for advanced search and filtering
- **Variants**:
  - Control (A): Simple search (text only)
  - Treatment (B): Advanced filters (by type, material, dimensions)
- **Weight Distribution**: 50/50
- **KPIs**:
  - Search execution rate
  - Filter adoption rate
  - Time to find template

## Standard Analytics Events

BendCore CAD tracks 50+ standard events across multiple categories:

### Tube Operations
- `tube_created` - When a new tube is created
- `tube_deleted` - When a tube is deleted
- `tube_duplicated` - When a tube is duplicated
- `tube_saved` - When a tube is saved

### Dimension Operations
- `dimension_added` - When a dimension is added
- `dimension_removed` - When a dimension is removed
- `dimension_modified` - When a dimension is modified

### Formula/Parametric
- `formula_evaluated` - When a parametric formula is evaluated
- `parametric_formula_changed` - When parametric formula is edited
- `parametric_feature_enabled` - When parametric feature is enabled
- `parametric_feature_disabled` - When parametric feature is disabled

### Templates
- `template_selected` - When a template is selected
- `template_auto_selected` - When a template is auto-selected
- `custom_template_saved` - When a custom template is saved
- `template_editor_opened` - When template editor is opened
- `smart_template_feature_enabled` - When smart templates are enabled

### Search & Filters
- `search_executed` - When a search is performed
- `filter_applied` - When filters are applied
- `search_filter_feature_enabled` - When search filter feature is enabled

### Import/Export
- `file_imported` - When a file is imported
- `file_exported` - When a file is exported
- `import_completed` - When import completes

### Version Control
- `version_rollback` - When a version is rolled back
- `version_created` - When a new version is created

### Collaboration
- `collaboration_enabled` - When collaboration is enabled
- `comment_added` - When a comment is added
- `comment_resolved` - When a comment is resolved

### And more...
See `src/analytics.ts` for the complete list of standard events.

## Event Tracking Examples

### Track Tube Creation
```typescript
import { trackEvent, STANDARD_EVENTS } from "./src/analytics";

trackEvent(STANDARD_EVENTS.TUBE_CREATED, {
  tube_name: "Main Assembly",
  material: "Steel",
  diameter: 10,
  length: 200
});
```

### Track Feature Flag Exposure
```typescript
import { trackFeatureFlagExposure, STANDARD_EVENTS } from "./src/analytics";

trackFeatureFlagExposure("parametric_dimensions", true);
```

### Track Experiment Conversion
```typescript
const analytics = getAnalyticsManager();
analytics.trackExperimentConversion(
  "parametric_test",
  "treatment",
  "template_saved"
);
```

## GrowthBook API Setup

### Create GrowthBook Account

1. Go to [GrowthBook](https://www.growthbook.io/)
2. Sign up for a free account
3. Create an organization
4. Generate an API key

### Get Your API Key

1. Navigate to **Settings** → **API Keys**
2. Create a new API key with read access
3. Copy the key and add to `.env`:
   ```env
   REACT_APP_GROWTHBOOK_API_KEY=sk_live_xxxxxxxxxx
   ```

### Configure Features in GrowthBook

#### Create Feature Flags

1. Go to **Features** section
2. Click **Create Feature**
3. Name: `parametric_dimensions`
   - Description: "Enable parametric dimensions feature"
   - Default value: `false`
   - Rules:
     - Rule 1: If `platform == "web"`, value = `true`
     - Rule 2: If user in experiment `parametric_test`, follow experiment

4. Repeat for:
   - `smart_templates`
   - `search_filters`
   - `advanced_reporting`

#### Create Experiments

Use the experiment setup wizard for each of the three pre-seeded experiments listed above.

## Metrics and KPIs

### Standard KPIs

#### 1. Adoption Rate
- **Definition**: % of users with feature enabled
- **Calculation**: (Users with feature on) / (Total users) × 100
- **Target**: >40% for new features

#### 2. Conversion Rate
- **Definition**: % of users who convert (save/create with feature)
- **Calculation**: (Converting users) / (Exposed users) × 100
- **Target**: >15% improvement over control

#### 3. Time to Value
- **Definition**: Average time before first feature use (minutes)
- **Calculation**: Avg(time from exposure to first event)
- **Target**: <5 minutes for intuitive features

### Custom Dashboards

Create dashboards in GrowthBook for:
- **Q3 Features Dashboard**: Tracks all three test experiments
- **Daily Adoption**: Shows daily adoption trends
- **Retention Dashboard**: User retention by feature flag

## Testing

### Unit Tests

Run unit tests for GrowthBook integration:

```bash
npm test -- tests/test_growthbook.cpp
```

Tests cover:
1. Feature flag evaluation
2. Experiment variant assignment
3. Event tracking and batching
4. localStorage persistence
5. Attribute binding
6. Metrics calculation

### E2E Tests

Run end-to-end tests:

```bash
npm test -- tests/growthbook.e2e.tsx
```

Test scenarios:
1. User assignment → feature usage → conversion tracking
2. Feature flag disabled for variant B → feature hidden in UI

## Production Deployment

### Before Going Live

- [ ] Test all three experiments in staging
- [ ] Verify analytics events are sent correctly
- [ ] Set appropriate sampling rates for high-volume events
- [ ] Configure alerts for experiment anomalies
- [ ] Review KPI thresholds with product team

### Monitoring

1. **Daily**: Check experiment metrics for statistical significance
2. **Weekly**: Review adoption curves and funnel completion
3. **Monthly**: Generate reports for feature performance

### Rollout Strategy

1. **Day 1-3**: 5% of users in treatment group
2. **Day 4-7**: 25% of users in treatment group
3. **Day 8+**: 50% of users in treatment group (full A/B test)
4. **Week 3+**: Roll out to 100% if metrics positive

## Troubleshooting

### Feature Flags Not Loading

Check:
1. API key is set correctly: `echo $REACT_APP_GROWTHBOOK_API_KEY`
2. Network request to GrowthBook is successful (check browser DevTools)
3. Features exist in GrowthBook dashboard

### Events Not Sent

Check:
1. Analytics is initialized: `getAnalyticsManager().initialize(userId, sessionId)`
2. localStorage is available and not full
3. Check browser console for errors

### Variant Assignment Not Persistent

Check:
1. localStorage is enabled
2. App doesn't clear localStorage on initialization
3. Session hasn't expired

## Best Practices

1. **Always use constants** for feature flag names:
   ```typescript
   const FEATURE_FLAGS = {
     PARAMETRIC_DIMENSIONS: "parametric_dimensions",
     SMART_TEMPLATES: "smart_templates",
     SEARCH_FILTERS: "search_filters"
   };
   
   useFeatureFlag(FEATURE_FLAGS.PARAMETRIC_DIMENSIONS);
   ```

2. **Track meaningful events** - Only track events that matter for your KPIs

3. **Use feature flags for gradual rollouts** - Don't launch features to 100% at once

4. **Test in staging** - Always test experiments in staging before production

5. **Monitor statistical significance** - Don't stop experiments too early

6. **Document experiment changes** - Keep notes on what each variant changes

## References

- [GrowthBook Documentation](https://docs.growthbook.io/)
- [GrowthBook JavaScript SDK](https://docs.growthbook.io/sdk/js)
- [A/B Testing Best Practices](https://www.growthbook.io/blog/)
- [BendCore CAD Analytics Events](src/analytics.ts)

## Support

For issues or questions:
1. Check GrowthBook documentation
2. Review BendCore analytics logs
3. Contact the product team
4. Open an issue on GitHub

---

**Last Updated**: 2026-10-23  
**Version**: 1.0.0  
**Maintained By**: BendCore Development Team
