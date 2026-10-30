import { GrowthBook } from "@growthbook/growthbook";

/**
 * GrowthBook initialization and configuration
 * Handles A/B testing and feature flags for BendCore CAD
 */

interface GrowthBookAttributes {
  user_id: string;
  experiment_id?: string;
  build_version: string;
  platform: "desktop" | "web";
  session_id: string;
  created_at: number;
}

class GrowthBookManager {
  private instance: GrowthBook | null = null;
  private attributes: GrowthBookAttributes | null = null;
  private sessionId: string;

  constructor() {
    this.sessionId = this.generateSessionId();
  }

  /**
   * Initialize GrowthBook with API key and configuration
   */
  async initialize(userId: string, platform: "desktop" | "web" = "web"): Promise<void> {
    try {
      const apiKey = process.env.REACT_APP_GROWTHBOOK_API_KEY ||
                     process.env.GROWTHBOOK_API_KEY ||
                     process.env.VUE_APP_GROWTHBOOK_API_KEY ||
                     "";

      if (!apiKey) {
        console.warn("GrowthBook API key not configured. Feature flags will be disabled.");
      }

      this.instance = new GrowthBook({
        apiHost: "https://cdn.growthbook.io",
        clientKey: apiKey,
      });

      // Set attributes for experiment targeting
      this.attributes = {
        user_id: userId,
        build_version: this.getBuildVersion(),
        platform: platform,
        session_id: this.sessionId,
        created_at: Date.now(),
      };

      this.instance.setAttributes(this.attributes);

      // Load features from the API; autoRefresh keeps them updated (SSE) while the app runs.
      await this.instance.loadFeatures({ autoRefresh: true });

      console.log("GrowthBook initialized successfully");
    } catch (error) {
      console.error("Failed to initialize GrowthBook:", error);
      // Graceful fallback - create a disabled instance
      this.createFallbackInstance();
    }
  }

  /**
   * Create a fallback instance when API is unavailable
   */
  private createFallbackInstance(): void {
    this.instance = new GrowthBook({
      // Empty features - all flags will be disabled
      features: {},
    });

    this.instance.setAttributes(this.attributes || {});
  }

  /**
   * Get the GrowthBook instance
   */
  getInstance(): GrowthBook | null {
    return this.instance;
  }

  /**
   * Check if a feature flag is enabled
   */
  isFeatureEnabled(featureName: string): boolean {
    if (!this.instance) {
      return false;
    }

    // Check if the current user is in the experiment
    const result = this.instance.evalFeature(featureName);
    return result.on === true;
  }

  /**
   * Get experiment variant for a feature
   */
  getExperimentVariant(experimentId: string): string | null {
    if (!this.instance) {
      return null;
    }

    const allResults = this.instance.getAllResults();
    const entry = allResults[experimentId];
    if (!entry) {
      return null;
    }

    return String(entry.result.variationId ?? '') || null;
  }

  /**
   * Get all experiment results
   */
  getExperimentResults() {
    if (!this.instance) {
      return {};
    }

    return this.instance.getAllResults();
  }

  /**
   * Get all feature flag results
   */
  getFeatureResults() {
    if (!this.instance) {
      return {};
    }

    return this.instance.getAllResults();
  }

  /**
   * Set user attributes for experiment targeting
   */
  setAttributes(attributes: Partial<GrowthBookAttributes>): void {
    if (!this.instance) {
      return;
    }

    this.attributes = {
      ...this.attributes,
      ...attributes,
    } as GrowthBookAttributes;

    this.instance.setAttributes(this.attributes);
  }

  /**
   * Update experiment ID for tracking
   */
  setExperimentId(experimentId: string): void {
    this.setAttributes({ experiment_id: experimentId });
  }

  /**
   * Get current session ID
   */
  getSessionId(): string {
    return this.sessionId;
  }

  /**
   * Get current attributes
   */
  getAttributes(): GrowthBookAttributes | null {
    return this.attributes;
  }

  /**
   * Generate a unique session ID
   */
  private generateSessionId(): string {
    return `session_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  /**
   * Get the build version
   */
  private getBuildVersion(): string {
    // Try to get from package.json or environment
    return (
      process.env.REACT_APP_VERSION ||
      process.env.VUE_APP_VERSION ||
      process.env.APP_VERSION ||
      "2.0.0"
    );
  }

  /**
   * Destroy the instance and cleanup
   */
  destroy(): void {
    if (this.instance) {
      // Cleanup if needed
      this.instance = null;
    }
  }
}

// Singleton instance
let growthbookManager: GrowthBookManager | null = null;

export function getGrowthBookManager(): GrowthBookManager {
  if (!growthbookManager) {
    growthbookManager = new GrowthBookManager();
  }
  return growthbookManager;
}

export function initializeGrowthBook(
  userId: string,
  platform: "desktop" | "web" = "web"
): Promise<void> {
  return getGrowthBookManager().initialize(userId, platform);
}

export type { GrowthBookAttributes };
export { GrowthBookManager };
