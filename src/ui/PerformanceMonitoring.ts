/**
 * Performance Monitoring and Profiling Framework
 * PERF-001: Application Startup Profiling
 * PERF-006: Memory Management
 *
 * Provides comprehensive performance metrics collection and analysis
 */

export interface PerformanceMetric {
  name: string;
  timestamp: number;
  duration: number;
  category: 'startup' | 'render' | 'interaction' | 'memory' | 'network' | 'solver';
  metadata?: Record<string, any>;
}

export interface MemorySnapshot {
  timestamp: number;
  usedJSHeapSize: number;
  totalJSHeapSize: number;
  jsHeapSizeLimit: number;
  externalMemoryUsage?: number;
}

export class PerformanceMonitor {
  private metrics: PerformanceMetric[] = [];
  private memorySnapshots: MemorySnapshot[] = [];
  private startTimes: Map<string, number> = new Map();
  private markerStartTimes: Map<string, number> = new Map();

  private static instance: PerformanceMonitor;

  private constructor() {
    // Mark app start
    this.mark('app-initialization');
  }

  static getInstance(): PerformanceMonitor {
    if (!PerformanceMonitor.instance) {
      PerformanceMonitor.instance = new PerformanceMonitor();
    }
    return PerformanceMonitor.instance;
  }

  /**
   * Mark a point in time for measurements
   */
  mark(name: string): void {
    const now = performance.now();
    this.markerStartTimes.set(name, now);

    if (typeof window !== 'undefined' && window.performance?.mark) {
      try {
        window.performance.mark(name);
      } catch (e) {
        // Mark may already exist
      }
    }
  }

  /**
   * Measure time between two marks or since mark
   */
  measure(name: string, startMark: string, endMark?: string): number {
    let duration = 0;

    if (endMark) {
      const startTime = this.markerStartTimes.get(startMark) || 0;
      const endTime = this.markerStartTimes.get(endMark) || 0;
      duration = endTime - startTime;
    } else if (startMark) {
      const startTime = this.markerStartTimes.get(startMark) || 0;
      duration = performance.now() - startTime;
    }

    if (typeof window !== 'undefined' && window.performance?.measure) {
      try {
        window.performance.measure(name, startMark, endMark);
      } catch (e) {
        // Ignore if marks don't exist
      }
    }

    return duration;
  }

  /**
   * Start timing a named operation
   */
  startTimer(operationName: string): void {
    this.startTimes.set(operationName, performance.now());
  }

  /**
   * End timing and record metric
   */
  endTimer(
    operationName: string,
    category: PerformanceMetric['category'] = 'interaction',
    metadata?: Record<string, any>
  ): number {
    const startTime = this.startTimes.get(operationName);
    if (!startTime) {
      console.warn(`Timer not found: ${operationName}`);
      return 0;
    }

    const duration = performance.now() - startTime;
    this.metrics.push({
      name: operationName,
      timestamp: startTime,
      duration,
      category,
      metadata,
    });

    this.startTimes.delete(operationName);
    return duration;
  }

  /**
   * Record a custom metric
   */
  recordMetric(
    name: string,
    duration: number,
    category: PerformanceMetric['category'] = 'interaction',
    metadata?: Record<string, any>
  ): void {
    this.metrics.push({
      name,
      timestamp: performance.now(),
      duration,
      category,
      metadata,
    });
  }

  /**
   * Capture memory snapshot
   */
  captureMemory(): MemorySnapshot | null {
    if (typeof window !== 'undefined' && (window.performance as any)?.memory) {
      const memory = (window.performance as any).memory;
      const snapshot: MemorySnapshot = {
        timestamp: performance.now(),
        usedJSHeapSize: memory.usedJSHeapSize,
        totalJSHeapSize: memory.totalJSHeapSize,
        jsHeapSizeLimit: memory.jsHeapSizeLimit,
        externalMemoryUsage: memory.externalMemoryUsage,
      };
      this.memorySnapshots.push(snapshot);
      return snapshot;
    }
    return null;
  }

  /**
   * Get current memory usage in MB
   */
  getCurrentMemoryMB(): number {
    const snapshot = this.captureMemory();
    if (snapshot) {
      return snapshot.usedJSHeapSize / (1024 * 1024);
    }
    return 0;
  }

  /**
   * Get all recorded metrics
   */
  getMetrics(): PerformanceMetric[] {
    return [...this.metrics];
  }

  /**
   * Get metrics by category
   */
  getMetricsByCategory(category: PerformanceMetric['category']): PerformanceMetric[] {
    return this.metrics.filter(m => m.category === category);
  }

  /**
   * Calculate statistics for a metric group
   */
  getMetricStats(metricNames?: string[]) {
    let metricsToAnalyze = this.metrics;
    if (metricNames) {
      metricsToAnalyze = this.metrics.filter(m => metricNames.includes(m.name));
    }

    if (metricsToAnalyze.length === 0) {
      return null;
    }

    const durations = metricsToAnalyze.map(m => m.duration);
    const sum = durations.reduce((a, b) => a + b, 0);
    const avg = sum / durations.length;
    const min = Math.min(...durations);
    const max = Math.max(...durations);
    const sorted = [...durations].sort((a, b) => a - b);
    const p50 = sorted[Math.floor(sorted.length * 0.5)];
    const p95 = sorted[Math.floor(sorted.length * 0.95)];
    const p99 = sorted[Math.floor(sorted.length * 0.99)];

    return {
      count: metricsToAnalyze.length,
      sum,
      avg,
      min,
      max,
      p50,
      p95,
      p99,
    };
  }

  /**
   * Get memory statistics
   */
  getMemoryStats() {
    if (this.memorySnapshots.length === 0) {
      return null;
    }

    const usedSizes = this.memorySnapshots.map(s => s.usedJSHeapSize / (1024 * 1024));
    const sum = usedSizes.reduce((a, b) => a + b, 0);
    const avg = sum / usedSizes.length;
    const min = Math.min(...usedSizes);
    const max = Math.max(...usedSizes);

    return {
      count: this.memorySnapshots.length,
      avgMB: avg,
      minMB: min,
      maxMB: max,
      currentMB: usedSizes[usedSizes.length - 1],
    };
  }

  /**
   * Generate performance report
   */
  generateReport(): {
    summary: string;
    metrics: Record<string, any>;
    memory: Record<string, any>;
  } {
    const startupMetrics = this.getMetricsByCategory('startup');
    const renderMetrics = this.getMetricsByCategory('render');
    const solverMetrics = this.getMetricsByCategory('solver');

    const startupStats = this.getMetricStats(startupMetrics.map(m => m.name));
    const renderStats = this.getMetricStats(renderMetrics.map(m => m.name));
    const solverStats = this.getMetricStats(solverMetrics.map(m => m.name));
    const memoryStats = this.getMemoryStats();

    // Calculate total startup time
    const totalStartupTime = startupMetrics.reduce((sum, m) => sum + m.duration, 0);

    return {
      summary: `Total startup time: ${totalStartupTime.toFixed(2)}ms, ` +
               `Render metrics: ${this.metrics.filter(m => m.category === 'render').length} samples, ` +
               `Memory: ${memoryStats?.currentMB.toFixed(1)}MB`,
      metrics: {
        startup: startupStats,
        render: renderStats,
        solver: solverStats,
        totalStartupMs: totalStartupTime,
      },
      memory: memoryStats || { avgMB: 0, maxMB: 0 },
    };
  }

  /**
   * Clear metrics
   */
  clear(): void {
    this.metrics = [];
    this.memorySnapshots = [];
    this.startTimes.clear();
    this.markerStartTimes.clear();
  }

  /**
   * Export metrics as JSON
   */
  exportMetrics(): string {
    return JSON.stringify({
      metrics: this.metrics,
      memory: this.memorySnapshots,
      report: this.generateReport(),
    }, null, 2);
  }
}

/**
 * Global performance monitor instance
 */
export const perfMonitor = PerformanceMonitor.getInstance();

/**
 * Hook to measure React component render time
 */
export function usePerfMonitor(componentName: string) {
  return {
    startMeasure: () => perfMonitor.startTimer(`render-${componentName}`),
    endMeasure: () => perfMonitor.endTimer(`render-${componentName}`, 'render'),
  };
}
