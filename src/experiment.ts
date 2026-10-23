/**
 * A/B Testing and Experiment Tracking
 * Manages experiment variants, event tracking, and metrics calculation
 */

export interface ExperimentConfig {
  experiment_id: string;
  name: string;
  description: string;
  variants: ExperimentVariant[];
  weight_distribution: number[]; // Percentages for each variant, should sum to 100
  active: boolean;
  created_at: number;
}

export interface ExperimentVariant {
  id: string;
  name: string;
  description?: string;
}

export interface ExperimentAssignment {
  experiment_id: string;
  variant_id: string;
  assigned_at: number;
}

export interface ExperimentEvent {
  experiment_id: string;
  event_type: "view" | "click" | "conversion" | "custom";
  event_name: string;
  properties?: Record<string, any>;
  timestamp: number;
  variant_id: string;
}

export interface ExperimentMetrics {
  experiment_id: string;
  variant_id: string;
  total_users: number;
  conversion_count: number;
  conversion_rate: number;
  avg_time_to_convert: number;
  events_tracked: number;
  retention_rate: number;
  last_updated: number;
}

const STORAGE_KEY_PREFIX = "bendcore_experiment_";
const ASSIGNMENT_KEY = "variant_assignments";

class ExperimentManager {
  private experiments: Map<string, ExperimentConfig> = new Map();
  private assignments: Map<string, ExperimentAssignment> = new Map();
  private events: ExperimentEvent[] = [];
  private metrics: Map<string, ExperimentMetrics> = new Map();

  constructor() {
    this.loadAssignmentsFromStorage();
  }

  /**
   * Register an experiment configuration
   */
  registerExperiment(config: ExperimentConfig): void {
    // Validate weight distribution
    const totalWeight = config.weight_distribution.reduce((a, b) => a + b, 0);
    if (Math.abs(totalWeight - 100) > 0.1) {
      console.warn(
        `Weight distribution for ${config.experiment_id} does not sum to 100: ${totalWeight}`
      );
    }

    this.experiments.set(config.experiment_id, config);
  }

  /**
   * Get experiment configuration
   */
  getExperiment(experimentId: string): ExperimentConfig | null {
    return this.experiments.get(experimentId) || null;
  }

  /**
   * Assign a user to an experiment variant
   * Returns the assigned variant ID
   */
  assignVariant(experimentId: string, userId: string): string {
    // Check if already assigned (persistent assignment)
    const assignmentKey = `${experimentId}_${userId}`;
    const existingAssignment = this.assignments.get(assignmentKey);

    if (existingAssignment) {
      return existingAssignment.variant_id;
    }

    const experiment = this.experiments.get(experimentId);
    if (!experiment) {
      throw new Error(`Experiment ${experimentId} not found`);
    }

    // Random assignment based on weights
    const variantId = this.selectVariantByWeight(
      experiment.variants,
      experiment.weight_distribution
    );

    const assignment: ExperimentAssignment = {
      experiment_id: experimentId,
      variant_id: variantId,
      assigned_at: Date.now(),
    };

    this.assignments.set(assignmentKey, assignment);
    this.saveAssignmentsToStorage();

    return variantId;
  }

  /**
   * Get assigned variant for a user
   */
  getAssignedVariant(experimentId: string, userId: string): string | null {
    const assignmentKey = `${experimentId}_${userId}`;
    const assignment = this.assignments.get(assignmentKey);
    return assignment?.variant_id || null;
  }

  /**
   * Track an event for an experiment
   */
  trackEvent(event: ExperimentEvent): void {
    this.events.push(event);

    // Update metrics
    this.updateMetrics(event);
  }

  /**
   * Get events for an experiment
   */
  getEvents(experimentId: string): ExperimentEvent[] {
    return this.events.filter((e) => e.experiment_id === experimentId);
  }

  /**
   * Get metrics for an experiment variant
   */
  getMetrics(
    experimentId: string,
    variantId: string
  ): ExperimentMetrics | null {
    const key = `${experimentId}_${variantId}`;
    return this.metrics.get(key) || null;
  }

  /**
   * Get all metrics for an experiment
   */
  getAllMetrics(experimentId: string): ExperimentMetrics[] {
    const experiment = this.experiments.get(experimentId);
    if (!experiment) {
      return [];
    }

    return experiment.variants
      .map((v) => this.getMetrics(experimentId, v.id))
      .filter((m) => m !== null) as ExperimentMetrics[];
  }

  /**
   * Calculate experiment statistics
   */
  calculateExperimentStats(experimentId: string) {
    const events = this.getEvents(experimentId);
    const experiment = this.experiments.get(experimentId);

    if (!experiment || events.length === 0) {
      return null;
    }

    const variantStats: Record<string, any> = {};

    for (const variant of experiment.variants) {
      const variantEvents = events.filter((e) => e.variant_id === variant.id);
      const conversionEvents = variantEvents.filter(
        (e) => e.event_type === "conversion"
      );

      const conversionTimings = conversionEvents
        .map((e) => {
          // Find the first view event for this variant to calculate time to convert
          const viewEvent = variantEvents.find(
            (ev) => ev.event_type === "view" && ev.timestamp <= e.timestamp
          );
          return viewEvent ? e.timestamp - viewEvent.timestamp : 0;
        })
        .filter((t) => t > 0);

      const avgTimeToConvert =
        conversionTimings.length > 0
          ? conversionTimings.reduce((a, b) => a + b, 0) /
            conversionTimings.length
          : 0;

      variantStats[variant.id] = {
        total_events: variantEvents.length,
        conversion_count: conversionEvents.length,
        conversion_rate:
          variantEvents.length > 0
            ? conversionEvents.length / variantEvents.length
            : 0,
        avg_time_to_convert: avgTimeToConvert,
        unique_users: new Set(
          variantEvents.map((e) => e.properties?.user_id || "unknown")
        ).size,
      };
    }

    return variantStats;
  }

  /**
   * Reset experiment data
   */
  resetExperiment(experimentId: string): void {
    // Clear events
    this.events = this.events.filter((e) => e.experiment_id !== experimentId);

    // Clear metrics
    const experiment = this.experiments.get(experimentId);
    if (experiment) {
      for (const variant of experiment.variants) {
        const key = `${experimentId}_${variant.id}`;
        this.metrics.delete(key);
      }
    }
  }

  /**
   * Clear all assignments (for testing or new experiment)
   */
  clearAllAssignments(): void {
    this.assignments.clear();
    this.clearAssignmentsFromStorage();
  }

  /**
   * Export experiment data (for analytics)
   */
  exportExperimentData(experimentId: string) {
    return {
      experiment: this.experiments.get(experimentId),
      events: this.getEvents(experimentId),
      metrics: this.getAllMetrics(experimentId),
      stats: this.calculateExperimentStats(experimentId),
    };
  }

  /**
   * Select variant based on weight distribution
   */
  private selectVariantByWeight(
    variants: ExperimentVariant[],
    weights: number[]
  ): string {
    const random = Math.random() * 100;
    let cumulative = 0;

    for (let i = 0; i < variants.length; i++) {
      cumulative += weights[i];
      if (random <= cumulative) {
        return variants[i].id;
      }
    }

    // Fallback to last variant
    return variants[variants.length - 1].id;
  }

  /**
   * Update metrics based on event
   */
  private updateMetrics(event: ExperimentEvent): void {
    const key = `${event.experiment_id}_${event.variant_id}`;

    let metrics = this.metrics.get(key);
    if (!metrics) {
      metrics = {
        experiment_id: event.experiment_id,
        variant_id: event.variant_id,
        total_users: 0,
        conversion_count: 0,
        conversion_rate: 0,
        avg_time_to_convert: 0,
        events_tracked: 0,
        retention_rate: 0,
        last_updated: Date.now(),
      };
    }

    metrics.events_tracked++;

    if (event.event_type === "conversion") {
      metrics.conversion_count++;
      metrics.conversion_rate = metrics.conversion_count / metrics.events_tracked;
    }

    metrics.last_updated = Date.now();

    this.metrics.set(key, metrics);
  }

  /**
   * Save assignments to localStorage for persistence
   */
  private saveAssignmentsToStorage(): void {
    try {
      const data = Array.from(this.assignments.entries());
      localStorage.setItem(
        STORAGE_KEY_PREFIX + ASSIGNMENT_KEY,
        JSON.stringify(data)
      );
    } catch (error) {
      console.warn("Failed to save experiment assignments to localStorage:", error);
    }
  }

  /**
   * Load assignments from localStorage
   */
  private loadAssignmentsFromStorage(): void {
    try {
      const data = localStorage.getItem(
        STORAGE_KEY_PREFIX + ASSIGNMENT_KEY
      );
      if (data) {
        const assignments = JSON.parse(data) as Array<
          [string, ExperimentAssignment]
        >;
        this.assignments = new Map(assignments);
      }
    } catch (error) {
      console.warn("Failed to load experiment assignments from localStorage:", error);
    }
  }

  /**
   * Clear assignments from storage
   */
  private clearAssignmentsFromStorage(): void {
    try {
      localStorage.removeItem(STORAGE_KEY_PREFIX + ASSIGNMENT_KEY);
    } catch (error) {
      console.warn("Failed to clear experiment assignments from localStorage:", error);
    }
  }
}

// Singleton instance
let experimentManager: ExperimentManager | null = null;

export function getExperimentManager(): ExperimentManager {
  if (!experimentManager) {
    experimentManager = new ExperimentManager();
  }
  return experimentManager;
}

export { ExperimentManager };
