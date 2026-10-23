import { useEffect, useState } from "react";
import { getGrowthBookManager } from "../../../src/growthbook";
import { getExperimentManager } from "../../../src/experiment";
import { trackFeatureFlagExposure } from "../../../src/analytics";

/**
 * Hook for checking if a feature flag is enabled
 * Automatically tracks feature flag exposure
 */
export const useFeatureFlag = (
  featureName: string
): {
  enabled: boolean;
  isLoading: boolean;
} => {
  const [enabled, setEnabled] = useState(false);
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    try {
      const manager = getGrowthBookManager();
      const isEnabled = manager.isFeatureEnabled(featureName);

      setEnabled(isEnabled);

      // Track feature flag exposure
      const attributes = manager.getAttributes();
      if (attributes) {
        trackFeatureFlagExposure(featureName, isEnabled);
      }

      setIsLoading(false);
    } catch (error) {
      console.error(`Failed to check feature flag ${featureName}:`, error);
      setEnabled(false);
      setIsLoading(false);
    }
  }, [featureName]);

  return { enabled, isLoading };
};

/**
 * Hook for getting experiment variant assignment
 */
export const useExperiment = (
  experimentId: string
): {
  variant: string | null;
  isLoading: boolean;
  metadata: {
    assigned_at?: number;
    experiment_id: string;
  };
} => {
  const [variant, setVariant] = useState<string | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [metadata, setMetadata] = useState({
    experiment_id: experimentId,
  });

  useEffect(() => {
    try {
      const growthbook = getGrowthBookManager();
      const attributes = growthbook.getAttributes();

      if (!attributes) {
        setVariant(null);
        setIsLoading(false);
        return;
      }

      // Get variant from GrowthBook
      const assignedVariant = growthbook.getExperimentVariant(experimentId);
      setVariant(assignedVariant);

      // Also try experiment manager for additional context
      const experimentManager = getExperimentManager();
      const assignment = experimentManager.getAssignedVariant(
        experimentId,
        attributes.user_id
      );

      if (assignment) {
        setMetadata({
          experiment_id: experimentId,
          assigned_at: Date.now(),
        });
      }

      setIsLoading(false);
    } catch (error) {
      console.error(`Failed to get experiment variant for ${experimentId}:`, error);
      setVariant(null);
      setIsLoading(false);
    }
  }, [experimentId]);

  return { variant, isLoading, metadata };
};

/**
 * Hook for tracking feature flag usage
 */
export const useFeatureFlagTracking = (featureName: string, enabled: boolean) => {
  useEffect(() => {
    // Track when feature flag state changes
    const growthbook = getGrowthBookManager();
    const attributes = growthbook.getAttributes();

    if (attributes) {
      trackFeatureFlagExposure(featureName, enabled);
    }
  }, [featureName, enabled]);
};

/**
 * Hook for A/B test conversion tracking
 */
export const useExperimentConversion = (experimentId: string, variantId: string) => {
  const trackConversion = (conversionType: string = "default") => {
    try {
      const experimentManager = getExperimentManager();
      const growthbook = getGrowthBookManager();
      const attributes = growthbook.getAttributes();

      if (!attributes) {
        return;
      }

      // Track in experiment manager
      experimentManager.trackEvent({
        experiment_id: experimentId,
        event_type: "conversion",
        event_name: `Conversion: ${conversionType}`,
        properties: {
          user_id: attributes.user_id,
          conversion_type: conversionType,
        },
        timestamp: Date.now(),
        variant_id: variantId,
      });

      // Track analytics event
      const { trackExperimentConversion } = require("../../../src/analytics");
      trackExperimentConversion(experimentId, variantId, conversionType);
    } catch (error) {
      console.error("Failed to track experiment conversion:", error);
    }
  };

  return { trackConversion };
};
