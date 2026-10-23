# GrowthBook Setup Guide

## Overview

GrowthBook is used for feature flag management and A/B testing in BendCore. This guide covers setup, configuration, and integration.

## Prerequisites

- GrowthBook account (https://app.growthbook.io)
- API key and SDK key
- Node.js 16+ (for BendCore backend)

## Step 1: Create GrowthBook Account

1. Visit https://app.growthbook.io/signup
2. Sign up with email and password
3. Create organization (e.g., "BendCore Development")
4. Verify email address

## Step 2: Create API Keys

### Generate API Key (Server-side)

1. Go to Settings → API Keys
2. Click "Create API Key"
3. Select permissions:
   - Read features
   - Read experiments
   - Write events
4. Copy API Key (starts with `gbpk_`)
5. Store securely in environment variables

### Generate SDK Key (Client-side)

1. Go to Settings → SDK Keys
2. Click "Create SDK Key"
3. Select SDK key type: "JavaScript" or "General"
4. Copy SDK Key (starts with `gb_`)
5. Store in environment configuration

## Step 3: Environment Configuration

### Backend Setup (.env)

```env
# GrowthBook Configuration
GB_API_KEY=gbpk_xxxxxxxxxxxxxxxx
GB_SDK_KEY=gb_xxxxxxxxxxxxxxxx
GB_ENVIRONMENT=production
GB_ENABLE_ANALYTICS=true
GB_BATCH_DELAY_MS=5000

# Optional: GrowthBook API endpoint (if self-hosted)
# GB_API_URL=https://growthbook.yourcompany.com/api/v1
```

### C++ Integration

```cpp
#include "bendcore/growthbook_client.hpp"

using namespace bendcore;

// Initialize in application startup
GrowthBookClient& gb = GrowthBookClient::getInstance();
GrowthBookClient::Config config;
config.api_key = std::getenv("GB_API_KEY");
config.sdk_key = std::getenv("GB_SDK_KEY");
config.environment = "production";
gb.initialize(config);
```

### React Integration

```tsx
import { useAnalytics } from './hooks/useAnalytics';

export function MyComponent() {
  const { trackEvent } = useAnalytics();

  const handleClick = async () => {
    await trackEvent('button_clicked', { 
      button_name: 'Submit' 
    });
  };

  return <button onClick={handleClick}>Click Me</button>;
}
```

## Step 4: Setup Feature Flags

### Create Feature Flags in GrowthBook

1. Go to Features → Create Feature
2. Set feature key (e.g., `enhanced_template_matcher`)
3. Configure rule targeting (optional):
   - By user ID
   - By user attributes
   - Percentage-based rollout
4. Enable/disable feature
5. Save and publish

### Example: Template Matcher Feature Flag

```
Feature Key: enhanced_template_matcher
Feature Name: Enhanced Template Matcher
Default: OFF (0% rollout)
Rules:
  - 100% on for beta users
  - 50% on for all other users
```

### Use Feature Flags in Code

```cpp
// C++
if (gb.is_feature_enabled("enhanced_template_matcher")) {
  // Use new template matcher UI
  showEnhancedTemplateMatcher();
} else {
  // Use legacy template matcher
  showLegacyTemplateMatcher();
}
```

```tsx
// React
const useFeatureFlag = (featureName: string) => {
  const [enabled, setEnabled] = useState(false);
  
  useEffect(() => {
    fetch(`/api/feature-flags/${featureName}`)
      .then(r => r.json())
      .then(data => setEnabled(data.enabled));
  }, [featureName]);
  
  return enabled;
};

export function MyComponent() {
  const enhancedMatcher = useFeatureFlag('enhanced_template_matcher');
  
  return enhancedMatcher ? <EnhancedMatcher /> : <LegacyMatcher />;
}
```

## Step 5: Setup A/B Experiments

### Create Experiment in GrowthBook

1. Go to Experiments → Create Experiment
2. Set experiment key (e.g., `template_match_ui_v1`)
3. Select feature to test
4. Configure experiment:
   - **Hypothesis:** "Enhanced UI increases template selection rate"
   - **Control Variation:** Current UI (feature OFF)
   - **Treatment Variation:** New UI (feature ON)
   - **Sample Size:** 5000 users
   - **Tracking Key:** User ID hash
5. Set experiment status to "Running"

### Example Experiments

#### 1. Template Match UI Test
```
Experiment Key: template_match_ui_v1
Feature: enhanced_template_matcher
Control: Dropdown (top 3 results)
Treatment: Full panel with advanced filters
Target: 50/50 split
Duration: 2 weeks
```

#### 2. Search Filter Layout Test
```
Experiment Key: search_filter_layout_v1
Feature: horizontal_search_filters
Control: Vertical sidebar
Treatment: Horizontal toolbar
Target: 50/50 split
Duration: 2 weeks
```

#### 3. Formula Editor UX Test
```
Experiment Key: formula_editor_ux_v1
Feature: formula_autocomplete
Control: Text input only
Treatment: Text + autocomplete + syntax highlighting
Target: 50/50 split
Duration: 2 weeks
```

## Step 6: Configure Analytics

### Event Categories

Events are automatically tracked in these categories:

- **IMPORT**: Import workflow events
- **TEMPLATE**: Template operations
- **SEARCH**: Search and filtering
- **FORMULA**: Formula/dimension creation
- **TUBE_DESIGN**: Design operations
- **EXPORT**: Export/output operations
- **SYSTEM**: Session and system events

### Event Tracking

Events are tracked automatically when users interact with the application:

```cpp
// Example: Track import success
AnalyticsEventQueue queue;
queue.track_event(events::import_step_success(
  user_id,
  session_id,
  {{"file_size", "2048"}, {"duration_ms", "3500"}}
));

// Events are batched and sent to GrowthBook:
// - Batch size: 50 events
// - Batch timeout: 5 seconds
// - Auto-flush: enabled
```

### View Analytics in GrowthBook

1. Go to Analytics Dashboard
2. Filter by date range
3. View metrics:
   - Event counts by category
   - User engagement
   - Conversion rates
   - Funnel analysis

## Step 7: Monitor Experiments

### View Experiment Results

1. Go to Experiments
2. Click experiment name
3. View dashboard:
   - Control vs Treatment metrics
   - Statistical significance
   - Sample size
   - Conversion rates
   - Confidence level

### Interpret Results

**Statistically Significant (p < 0.05):**
- If Treatment > Control: Winner! 🎉
- If Control > Treatment: Control wins
- Implement winning variant

**Not Significant (p ≥ 0.05):**
- Continue running experiment
- Collect more data
- May indicate no real difference

### Example Results

```
Template Match UI Experiment
├─ Control (Dropdown)
│  └─ Conversion: 10.5%
│  └─ Sample: 2617
├─ Treatment (Full Panel)
│  └─ Conversion: 12.3%
│  └─ Sample: 2617
└─ Significance: 92% ✓ (Treatment wins)
```

## Step 8: Rollout Winning Features

### Gradual Rollout

1. Start with small percentage (e.g., 10%)
2. Monitor for issues
3. Increase gradually: 10% → 25% → 50% → 100%
4. At each step, monitor error rates and performance

### Full Rollout

```
Day 1: 10% rollout - Monitor error rates
Day 3: 25% rollout - Monitor user feedback
Day 5: 50% rollout - Check performance metrics
Day 7: 100% rollout - Full release
```

## Troubleshooting

### Events Not Appearing

1. Check API key is correct
2. Verify `GB_API_KEY` environment variable is set
3. Confirm event names match allowed event types
4. Check network connectivity to GrowthBook

### Feature Flag Not Working

1. Verify SDK key in configuration
2. Check feature flag is enabled in GrowthBook
3. Ensure user context is set with user ID
4. Call `gb.refresh_features()` to reload

### Experiments Not Running

1. Verify experiment status is "Running"
2. Check sample size requirements are met
3. Ensure tracking keys match user identification
4. Wait 24 hours for initial data aggregation

## Best Practices

1. **Naming Convention**: Use descriptive event names with underscores
   - ✓ `import_step_success`
   - ✗ `evt123`

2. **User IDs**: Always use consistent user identification
   - Use email hash or user database ID
   - Never change user ID mid-session

3. **Event Properties**: Include relevant context
   - File size, duration, error messages
   - Keep properties consistent across tests

4. **Experiment Duration**: Run for at least 1-2 weeks
   - Captures daily variation patterns
   - Ensures statistical significance

5. **Rollout Strategy**: Always test before full rollout
   - Start with 5-10% of users
   - Monitor for 24 hours
   - Check error rates and performance

## Support

- GrowthBook Docs: https://docs.growthbook.io
- GrowthBook Support: support@growthbook.io
- BendCore Documentation: /docs/ANALYTICS_TRACKING.md

## Security

- **API Keys**: Store in environment variables, never commit to git
- **User Privacy**: Don't track PII (emails, phone numbers)
- **Data Retention**: Events stored for 90 days then purged
- **Compliance**: GDPR and CCPA compliant
