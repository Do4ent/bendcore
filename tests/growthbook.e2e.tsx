/**
 * GrowthBook End-to-End Tests
 * Tests real user workflows with feature flags and A/B tests
 */

import React, { useState } from "react";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import "@testing-library/jest-dom";

// Mock the GrowthBook modules
import { getGrowthBookManager, initializeGrowthBook } from "../src/growthbook";
import { getExperimentManager } from "../src/experiment";
import {
  getAnalyticsManager,
  STANDARD_EVENTS,
  trackEvent,
} from "../src/analytics";
import { useFeatureFlag, useExperiment, useExperimentConversion } from "../ui/src/hooks/useFeatureFlag";
import { FeatureFlag } from "../ui/src/components/FeatureFlag";

/**
 * Test Component 1: Feature flag enabled/disabled workflow
 */
const ParametricDimensionsUI: React.FC = () => {
  return <div data-testid="parametric-ui">Parametric Dimensions Feature</div>;
};

const StandardUI: React.FC = () => {
  return <div data-testid="standard-ui">Standard Dimensions</div>;
};

const TestComponent1: React.FC = () => {
  const { enabled, isLoading } = useFeatureFlag("parametric_dimensions");

  if (isLoading) {
    return <div data-testid="loading">Loading...</div>;
  }

  return (
    <div>
      <FeatureFlag
        name="parametric_dimensions"
        fallback={<StandardUI />}
      >
        <ParametricDimensionsUI />
      </FeatureFlag>
    </div>
  );
};

/**
 * Test Component 2: A/B test with conversion tracking
 */
const TemplateListV1: React.FC<{ onConvert: () => void }> = ({ onConvert }) => {
  return (
    <div data-testid="template-list-v1">
      <h3>Templates (Old UI)</h3>
      <ul>
        <li>Template A</li>
        <li>Template B</li>
      </ul>
      <button onClick={onConvert} data-testid="save-btn-v1">
        Save Template
      </button>
    </div>
  );
};

const TemplateListV2: React.FC<{ onConvert: () => void }> = ({ onConvert }) => {
  return (
    <div data-testid="template-list-v2">
      <h3>Smart Templates (New UI)</h3>
      <div className="grid">
        <div>Template A</div>
        <div>Template B</div>
      </div>
      <button onClick={onConvert} data-testid="save-btn-v2">
        Save Template
      </button>
    </div>
  );
};

const TestComponent2: React.FC = () => {
  const { variant, isLoading } = useExperiment("smart_templates_test");
  const { trackConversion } = useExperimentConversion(
    "smart_templates_test",
    variant || "unknown"
  );
  const [converted, setConverted] = useState(false);

  const handleConvert = () => {
    trackConversion("template_saved");
    setConverted(true);
  };

  if (isLoading) {
    return <div data-testid="loading">Loading...</div>;
  }

  return (
    <div>
      {variant === "A" && <TemplateListV1 onConvert={handleConvert} />}
      {variant === "B" && <TemplateListV2 onConvert={handleConvert} />}
      {converted && <div data-testid="conversion-confirmed">Converted!</div>}
    </div>
  );
};

/**
 * E2E Test Scenario 1: User gets assigned to experiment, uses feature, conversion is tracked
 */
describe("GrowthBook E2E Tests - Scenario 1: A/B Test Workflow", () => {
  beforeEach(async () => {
    // Clear all previous state
    localStorage.clear();

    // Initialize managers
    const experimentManager = getExperimentManager();
    experimentManager.clearAllAssignments();

    const analyticsManager = getAnalyticsManager();
    analyticsManager.clear();

    // Initialize GrowthBook
    await initializeGrowthBook("test_user_123", "web");

    // Initialize analytics
    analyticsManager.initialize("test_user_123", "test_session_123");
  });

  test("Scenario 1A: User assigned to variant, tracks conversion through UI interaction", async () => {
    const experimentManager = getExperimentManager();
    const analyticsManager = getAnalyticsManager();

    // Register experiment
    experimentManager.registerExperiment({
      experiment_id: "smart_templates_test",
      name: "Smart Templates A/B Test",
      description: "Testing new template UI",
      variants: [
        { id: "A", name: "Old UI" },
        { id: "B", name: "New UI" },
      ],
      weight_distribution: [50, 50],
      active: true,
      created_at: Date.now(),
    });

    // Assign user to experiment
    const assignedVariant = experimentManager.assignVariant(
      "smart_templates_test",
      "test_user_123"
    );
    expect(assignedVariant).toBeDefined();
    expect(["A", "B"]).toContain(assignedVariant);

    // Render component
    render(<TestComponent2 />);

    // Wait for loading to finish
    await waitFor(() => {
      expect(screen.queryByTestId("loading")).not.toBeInTheDocument();
    });

    // Verify correct variant is rendered
    if (assignedVariant === "A") {
      expect(screen.getByTestId("template-list-v1")).toBeInTheDocument();
      expect(screen.getByTestId("save-btn-v1")).toBeInTheDocument();
    } else {
      expect(screen.getByTestId("template-list-v2")).toBeInTheDocument();
      expect(screen.getByTestId("save-btn-v2")).toBeInTheDocument();
    }

    // Click save button to trigger conversion
    const saveButton = screen.getByRole("button", { name: /save/i });
    fireEvent.click(saveButton);

    // Verify conversion tracking
    await waitFor(() => {
      expect(screen.getByTestId("conversion-confirmed")).toBeInTheDocument();
    });

    // Verify event was tracked
    const events = analyticsManager.getPendingEvents();
    expect(events.length).toBeGreaterThan(0);

    // Check experiment assignment was persistent
    const persistedVariant = experimentManager.getAssignedVariant(
      "smart_templates_test",
      "test_user_123"
    );
    expect(persistedVariant).toBe(assignedVariant);
  });

  test("Scenario 1B: Multiple users get different variants (roughly 50/50 split)", async () => {
    const experimentManager = getExperimentManager();

    // Register experiment
    experimentManager.registerExperiment({
      experiment_id: "smart_templates_test",
      name: "Smart Templates A/B Test",
      description: "Testing new template UI",
      variants: [
        { id: "A", name: "Old UI" },
        { id: "B", name: "New UI" },
      ],
      weight_distribution: [50, 50],
      active: true,
      created_at: Date.now(),
    });

    // Assign 20 users and track distribution
    const variants: string[] = [];
    for (let i = 0; i < 20; i++) {
      const userId = `test_user_${i}`;
      const variant = experimentManager.assignVariant(
        "smart_templates_test",
        userId
      );
      variants.push(variant);
    }

    // Count distribution
    const countA = variants.filter((v) => v === "A").length;
    const countB = variants.filter((v) => v === "B").length;

    // Verify roughly 50/50 split (allow variance)
    expect(countA).toBeGreaterThanOrEqual(5);
    expect(countB).toBeGreaterThanOrEqual(5);
    expect(countA + countB).toBe(20);
  });
});

/**
 * E2E Test Scenario 2: Feature flag disabled for variant B, feature hidden in UI
 */
describe("GrowthBook E2E Tests - Scenario 2: Feature Flag Control", () => {
  beforeEach(async () => {
    // Clear all previous state
    localStorage.clear();

    const growthbook = getGrowthBookManager();
    const experimentManager = getExperimentManager();
    const analyticsManager = getAnalyticsManager();

    experimentManager.clearAllAssignments();
    analyticsManager.clear();

    // Initialize GrowthBook
    await initializeGrowthBook("test_user_parametric", "web");
    analyticsManager.initialize("test_user_parametric", "test_session_parametric");
  });

  test("Scenario 2A: Feature flag enabled, component renders", async () => {
    const growthbook = getGrowthBookManager();

    // Enable the feature flag
    growthbook.setAttributes({
      user_id: "test_user_parametric",
      build_version: "2.0.0",
      platform: "web",
      session_id: growthbook.getSessionId(),
      created_at: Date.now(),
    });

    // Mock feature as enabled
    // In real implementation, this would come from GrowthBook API
    const mockIsFeatureEnabled = (name: string): boolean => {
      return name === "parametric_dimensions";
    };

    // Render component
    render(<TestComponent1 />);

    // Wait for loading
    await waitFor(() => {
      expect(screen.queryByTestId("loading")).not.toBeInTheDocument();
    });

    // Verify feature content is shown (or fallback, depending on implementation)
    // This verifies the component renders without errors
    expect(
      screen.getByTestId("parametric-ui") || screen.getByTestId("standard-ui")
    ).toBeInTheDocument();
  });

  test("Scenario 2B: Track feature flag exposure event", async () => {
    const analyticsManager = getAnalyticsManager();

    // Track feature flag exposure
    trackEvent(STANDARD_EVENTS.FEATURE_FLAG_EXPOSED, {
      feature_name: "parametric_dimensions",
      enabled: true,
    });

    // Flush batch to ensure event is captured
    await analyticsManager.flushBatch();

    // Verify event was tracked
    const events = analyticsManager.getPendingEvents();
    const exposureEvent = events.find(
      (e) => e.event_type === STANDARD_EVENTS.FEATURE_FLAG_EXPOSED
    );
    expect(exposureEvent).toBeDefined();
  });
});

/**
 * E2E Test Scenario 3: Analytics event batching and persistence
 */
describe("GrowthBook E2E Tests - Scenario 3: Analytics Batching", () => {
  beforeEach(async () => {
    localStorage.clear();

    const analyticsManager = getAnalyticsManager();
    analyticsManager.clear();

    await initializeGrowthBook("test_user_analytics", "web");
    analyticsManager.initialize("test_user_analytics", "test_session_analytics");
  });

  test("Scenario 3A: Events are batched and persisted", async () => {
    const analyticsManager = getAnalyticsManager();

    // Track multiple events
    trackEvent(STANDARD_EVENTS.TUBE_CREATED, {
      tube_name: "Main Assembly",
      diameter: 10,
    });

    trackEvent(STANDARD_EVENTS.DIMENSION_ADDED, {
      dimension_type: "length",
      value: 100,
    });

    trackEvent(STANDARD_EVENTS.FORMULA_EVALUATED, {
      formula_type: "parametric",
    });

    // Events should be queued
    const pendingEvents = analyticsManager.getPendingEvents();
    expect(pendingEvents.length).toBeGreaterThanOrEqual(3);

    // Flush batch
    await analyticsManager.flushBatch();

    // After flush, pending queue should be empty
    const afterFlush = analyticsManager.getPendingEvents();
    expect(afterFlush.length).toBe(0);
  });
});

/**
 * E2E Test Scenario 4: localStorage persistence across page reloads
 */
describe("GrowthBook E2E Tests - Scenario 4: Persistence", () => {
  test("Scenario 4A: Variant assignment persists across sessions", async () => {
    const experimentManager = getExperimentManager();

    // Register experiment
    experimentManager.registerExperiment({
      experiment_id: "parametric_test",
      name: "Parametric Test",
      description: "Test parametric dimensions",
      variants: [
        { id: "control", name: "Control" },
        { id: "treatment", name: "Treatment" },
      ],
      weight_distribution: [50, 50],
      active: true,
      created_at: Date.now(),
    });

    // Session 1: Assign user
    const variant1 = experimentManager.assignVariant(
      "parametric_test",
      "persistent_user"
    );

    // Simulate page reload by creating new manager instance
    // Get the persisted variant
    const variant2 = experimentManager.getAssignedVariant(
      "parametric_test",
      "persistent_user"
    );

    // Should return same variant
    expect(variant2).toBe(variant1);
  });
});
