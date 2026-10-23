/**
 * Analytics Event Tracking System
 * Tracks user interactions and feature usage for BendCore CAD
 * Batches events for efficient API transmission
 */

import { getGrowthBookManager } from "./growthbook";
import { getExperimentManager } from "./experiment";

export interface AnalyticsEvent {
  event_type: string;
  event_name: string;
  user_id: string;
  session_id: string;
  timestamp: number;
  properties?: Record<string, any>;
  variant_id?: string;
  experiment_id?: string;
}

export interface AnalyticsEventBatch {
  events: AnalyticsEvent[];
  batch_id: string;
  sent_at?: number;
  status: "pending" | "sent" | "failed";
}

const BATCH_INTERVAL = 30000; // 30 seconds
const MAX_BATCH_SIZE = 100;
const STORAGE_KEY = "bendcore_analytics_events";

// Standard event types for BendCore CAD
export const STANDARD_EVENTS = {
  // Tube operations
  TUBE_CREATED: "tube_created",
  TUBE_DELETED: "tube_deleted",
  TUBE_DUPLICATED: "tube_duplicated",
  TUBE_SAVED: "tube_saved",

  // Dimension operations
  DIMENSION_ADDED: "dimension_added",
  DIMENSION_REMOVED: "dimension_removed",
  DIMENSION_MODIFIED: "dimension_modified",

  // Formula/Parametric operations
  FORMULA_EVALUATED: "formula_evaluated",
  PARAMETRIC_FORMULA_CHANGED: "parametric_formula_changed",
  PARAMETRIC_FEATURE_ENABLED: "parametric_feature_enabled",
  PARAMETRIC_FEATURE_DISABLED: "parametric_feature_disabled",

  // Template operations
  TEMPLATE_SELECTED: "template_selected",
  TEMPLATE_AUTO_SELECTED: "template_auto_selected",
  CUSTOM_TEMPLATE_SAVED: "custom_template_saved",
  TEMPLATE_EDITOR_OPENED: "template_editor_opened",
  SMART_TEMPLATE_FEATURE_ENABLED: "smart_template_feature_enabled",

  // Search and filter operations
  SEARCH_EXECUTED: "search_executed",
  FILTER_APPLIED: "filter_applied",
  SEARCH_FILTER_FEATURE_ENABLED: "search_filter_feature_enabled",

  // Version control
  VERSION_ROLLBACK: "version_rollback",
  VERSION_CREATED: "version_created",

  // Feature flag exposure
  FEATURE_FLAG_EXPOSED: "feature_flag_exposed",
  FEATURE_FLAG_EVALUATION: "feature_flag_evaluation",

  // Experiment events
  EXPERIMENT_ASSIGNED: "experiment_assigned",
  EXPERIMENT_CONVERSION: "experiment_conversion",
  EXPERIMENT_VIEW: "experiment_view",

  // Import/Export
  FILE_IMPORTED: "file_imported",
  FILE_EXPORTED: "file_exported",
  IMPORT_COMPLETED: "import_completed",

  // Collaboration
  COLLABORATION_ENABLED: "collaboration_enabled",
  COMMENT_ADDED: "comment_added",
  COMMENT_RESOLVED: "comment_resolved",

  // Analysis
  ANALYSIS_STARTED: "analysis_started",
  ANALYSIS_COMPLETED: "analysis_completed",

  // Production reporting
  PRODUCTION_REPORT_GENERATED: "production_report_generated",
  PRODUCTION_REPORT_EXPORTED: "production_report_exported",

  // UI interactions
  TOOLBAR_CUSTOMIZED: "toolbar_customized",
  SETTINGS_CHANGED: "settings_changed",
  PROFILE_SWITCHED: "profile_switched",

  // Performance
  APP_INITIALIZED: "app_initialized",
  APP_CLOSED: "app_closed",
  PERFORMANCE_ISSUE_DETECTED: "performance_issue_detected",

  // Standards
  STANDARDS_CHECKED: "standards_checked",
  STANDARD_APPLIED: "standard_applied",

  // Errors
  ERROR_OCCURRED: "error_occurred",
  WARNING_TRIGGERED: "warning_triggered",
};

class AnalyticsManager {
  private eventQueue: AnalyticsEvent[] = [];
  private batchQueue: AnalyticsEventBatch[] = [];
  private batchInterval: NodeJS.Timeout | null = null;
  private userId: string = "";
  private sessionId: string = "";
  private enabled: boolean = true;

  constructor() {
    this.loadPendingBatches();
    this.startBatchTimer();

    // Save pending events on app close
    if (typeof window !== "undefined") {
      window.addEventListener("beforeunload", () => this.flushBatch());
    }
  }

  /**
   * Initialize analytics with user ID
   */
  initialize(userId: string, sessionId: string): void {
    this.userId = userId;
    this.sessionId = sessionId;

    // Track app initialization
    this.track(STANDARD_EVENTS.APP_INITIALIZED, {
      user_id: userId,
      session_id: sessionId,
    });
  }

  /**
   * Track an analytics event
   */
  track(
    eventType: string,
    properties?: Record<string, any>,
    experimentId?: string,
    variantId?: string
  ): void {
    if (!this.enabled || !this.userId) {
      return;
    }

    const event: AnalyticsEvent = {
      event_type: eventType,
      event_name: this.formatEventName(eventType),
      user_id: this.userId,
      session_id: this.sessionId,
      timestamp: Date.now(),
      properties: properties || {},
      experiment_id: experimentId,
      variant_id: variantId,
    };

    this.eventQueue.push(event);

    // Flush if batch is full
    if (this.eventQueue.length >= MAX_BATCH_SIZE) {
      this.flushBatch();
    }
  }

  /**
   * Track a feature flag exposure
   */
  trackFeatureFlagExposure(
    featureName: string,
    enabled: boolean,
    experimentId?: string,
    variantId?: string
  ): void {
    this.track(
      STANDARD_EVENTS.FEATURE_FLAG_EXPOSED,
      {
        feature_name: featureName,
        enabled: enabled,
      },
      experimentId,
      variantId
    );
  }

  /**
   * Track experiment exposure
   */
  trackExperimentExposure(
    experimentId: string,
    variantId: string,
    variantName: string
  ): void {
    this.track(
      STANDARD_EVENTS.EXPERIMENT_ASSIGNED,
      {
        experiment_id: experimentId,
        variant_id: variantId,
        variant_name: variantName,
      },
      experimentId,
      variantId
    );
  }

  /**
   * Track experiment conversion
   */
  trackExperimentConversion(
    experimentId: string,
    variantId: string,
    conversionType: string
  ): void {
    this.track(
      STANDARD_EVENTS.EXPERIMENT_CONVERSION,
      {
        experiment_id: experimentId,
        variant_id: variantId,
        conversion_type: conversionType,
      },
      experimentId,
      variantId
    );
  }

  /**
   * Get pending events
   */
  getPendingEvents(): AnalyticsEvent[] {
    return [...this.eventQueue];
  }

  /**
   * Get event count
   */
  getEventCount(): number {
    return this.eventQueue.length + this.batchQueue.reduce((sum, b) => sum + b.events.length, 0);
  }

  /**
   * Enable/disable analytics
   */
  setEnabled(enabled: boolean): void {
    this.enabled = enabled;
  }

  /**
   * Flush event batch to API
   */
  async flushBatch(): Promise<boolean> {
    if (this.eventQueue.length === 0) {
      return true;
    }

    const batch: AnalyticsEventBatch = {
      events: [...this.eventQueue],
      batch_id: this.generateBatchId(),
      status: "pending",
    };

    this.eventQueue = [];
    this.batchQueue.push(batch);
    this.savePendingBatches();

    // Try to send immediately
    return this.sendBatch(batch);
  }

  /**
   * Send batch to GrowthBook API
   */
  private async sendBatch(batch: AnalyticsEventBatch): Promise<boolean> {
    const growthbook = getGrowthBookManager().getInstance();

    if (!growthbook) {
      console.warn("GrowthBook not initialized, queuing batch for later");
      batch.status = "failed";
      this.savePendingBatches();
      return false;
    }

    try {
      // In a real implementation, this would send to the GrowthBook API
      // For now, we'll just log it
      console.log(`Sending analytics batch ${batch.batch_id}:`, batch.events);

      batch.sent_at = Date.now();
      batch.status = "sent";

      // Remove from queue
      this.batchQueue = this.batchQueue.filter((b) => b.batch_id !== batch.batch_id);
      this.savePendingBatches();

      return true;
    } catch (error) {
      console.error("Failed to send analytics batch:", error);
      batch.status = "failed";
      this.savePendingBatches();
      return false;
    }
  }

  /**
   * Retry failed batches
   */
  async retryFailedBatches(): Promise<void> {
    const failedBatches = this.batchQueue.filter((b) => b.status === "failed");

    for (const batch of failedBatches) {
      await this.sendBatch(batch);
    }
  }

  /**
   * Start batch timer
   */
  private startBatchTimer(): void {
    if (this.batchInterval) {
      clearInterval(this.batchInterval);
    }

    this.batchInterval = setInterval(() => {
      if (this.eventQueue.length > 0) {
        this.flushBatch();
      }
    }, BATCH_INTERVAL);
  }

  /**
   * Stop batch timer
   */
  stopBatchTimer(): void {
    if (this.batchInterval) {
      clearInterval(this.batchInterval);
      this.batchInterval = null;
    }
  }

  /**
   * Format event type to event name
   */
  private formatEventName(eventType: string): string {
    return eventType
      .split("_")
      .map((word) => word.charAt(0).toUpperCase() + word.slice(1))
      .join(" ");
  }

  /**
   * Generate batch ID
   */
  private generateBatchId(): string {
    return `batch_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  /**
   * Save pending batches to localStorage
   */
  private savePendingBatches(): void {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(this.batchQueue));
    } catch (error) {
      console.warn("Failed to save pending batches:", error);
    }
  }

  /**
   * Load pending batches from localStorage
   */
  private loadPendingBatches(): void {
    try {
      const data = localStorage.getItem(STORAGE_KEY);
      if (data) {
        this.batchQueue = JSON.parse(data);
      }
    } catch (error) {
      console.warn("Failed to load pending batches:", error);
    }
  }

  /**
   * Clear all analytics data (for testing)
   */
  clear(): void {
    this.eventQueue = [];
    this.batchQueue = [];
    try {
      localStorage.removeItem(STORAGE_KEY);
    } catch (error) {
      console.warn("Failed to clear analytics data:", error);
    }
  }

  /**
   * Destroy manager
   */
  destroy(): void {
    this.flushBatch();
    this.stopBatchTimer();
  }
}

// Singleton instance
let analyticsManager: AnalyticsManager | null = null;

export function getAnalyticsManager(): AnalyticsManager {
  if (!analyticsManager) {
    analyticsManager = new AnalyticsManager();
  }
  return analyticsManager;
}

export function trackEvent(
  eventType: string,
  properties?: Record<string, any>,
  experimentId?: string,
  variantId?: string
): void {
  getAnalyticsManager().track(eventType, properties, experimentId, variantId);
}

export function trackFeatureFlagExposure(
  featureName: string,
  enabled: boolean,
  experimentId?: string,
  variantId?: string
): void {
  getAnalyticsManager().trackFeatureFlagExposure(
    featureName,
    enabled,
    experimentId,
    variantId
  );
}

export { AnalyticsManager };
