import { useCallback, useState } from 'react';
import { useIpc } from './useIpc';

interface PerformanceMetrics {
  analysisCacheHitRate: number;
  geometryCacheHitRate: number;
  standardsCacheHitRate: number;
  averageAnalysisTime: number;
  peakMemoryUsage: number;
  totalTubesProcessed: number;
  cacheHits: number;
  cacheMisses: number;
}

/**
 * Hook for Performance Monitoring
 * Communicates with C++ PerformanceMonitor via IPC
 *
 * Provides:
 * - Real-time cache hit rates
 * - Analysis timing metrics
 * - Memory usage tracking
 * - Performance metrics reset
 */
export const usePerformance = () => {
  const ipc = useIpc();
  const [metrics, setMetrics] = useState<PerformanceMetrics | null>(null);
  const [isLoading, setIsLoading] = useState(false);

  const getMetrics = useCallback(
    async (): Promise<PerformanceMetrics | null> => {
      setIsLoading(true);
      try {
        const result = await ipc.getPerformanceMetrics();
        if (result) {
          setMetrics(result);
          return result;
        }
        return null;
      } catch (error) {
        console.error('Failed to get performance metrics:', error);
        return null;
      } finally {
        setIsLoading(false);
      }
    },
    [ipc]
  );

  const resetMetrics = useCallback(
    async (): Promise<boolean> => {
      try {
        const success = await ipc.resetMetrics();
        if (success) {
          // Clear local state
          setMetrics(null);
        }
        return success;
      } catch (error) {
        console.error('Failed to reset metrics:', error);
        return false;
      }
    },
    [ipc]
  );

  /**
   * Get overall cache hit rate (average of all caches)
   */
  const getOverallCacheHitRate = useCallback((): number => {
    if (!metrics) return 0;
    return (
      (metrics.analysisCacheHitRate +
        metrics.geometryCacheHitRate +
        metrics.standardsCacheHitRate) /
      3
    );
  }, [metrics]);

  /**
   * Get total cache hits
   */
  const getTotalCacheHits = useCallback((): number => {
    if (!metrics) return 0;
    return metrics.cacheHits;
  }, [metrics]);

  /**
   * Get total cache misses
   */
  const getTotalCacheMisses = useCallback((): number => {
    if (!metrics) return 0;
    return metrics.cacheMisses;
  }, [metrics]);

  /**
   * Get average analysis time in milliseconds
   */
  const getAverageAnalysisTime = useCallback((): number => {
    if (!metrics) return 0;
    return Math.round(metrics.averageAnalysisTime * 100) / 100;
  }, [metrics]);

  /**
   * Get peak memory usage in MB
   */
  const getPeakMemoryUsage = useCallback((): number => {
    if (!metrics) return 0;
    return Math.round(metrics.peakMemoryUsage * 100) / 100;
  }, [metrics]);

  /**
   * Get memory usage as formatted string
   */
  const getMemoryFormatted = useCallback((): string => {
    const memory = getPeakMemoryUsage();
    return `${memory} MB`;
  }, [getPeakMemoryUsage]);

  /**
   * Determine performance health level
   */
  const getPerformanceHealth = useCallback((): 'good' | 'fair' | 'poor' => {
    const hitRate = getOverallCacheHitRate();
    const avgTime = getAverageAnalysisTime();

    // Health scoring
    let score = 0;

    // Cache hit rate scoring (0-40 points)
    if (hitRate >= 0.8) score += 40;
    else if (hitRate >= 0.6) score += 30;
    else if (hitRate >= 0.4) score += 20;
    else score += 10;

    // Analysis time scoring (0-60 points)
    if (avgTime < 50) score += 60;
    else if (avgTime < 100) score += 40;
    else if (avgTime < 200) score += 20;
    else score += 10;

    if (score >= 80) return 'good';
    if (score >= 50) return 'fair';
    return 'poor';
  }, [getOverallCacheHitRate, getAverageAnalysisTime]);

  /**
   * Get performance health color
   */
  const getHealthColor = useCallback((): string => {
    const health = getPerformanceHealth();
    switch (health) {
      case 'good':
        return '#4CAF50'; // Green
      case 'fair':
        return '#FF9800'; // Orange
      case 'poor':
        return '#F44336'; // Red
    }
  }, [getPerformanceHealth]);

  /**
   * Get cache statistics summary
   */
  const getCacheStats = useCallback(
    (): {
      hits: number;
      misses: number;
      total: number;
      hitRate: number;
    } => {
      const hits = getTotalCacheHits();
      const misses = getTotalCacheMisses();
      const total = hits + misses;

      return {
        hits,
        misses,
        total,
        hitRate: total > 0 ? Math.round((hits / total) * 100) : 0,
      };
    },
    [getTotalCacheHits, getTotalCacheMisses]
  );

  /**
   * Format metrics for display in UI
   */
  const getFormattedMetrics = useCallback(
    (): {
      cacheHitRate: string;
      averageAnalysisTime: string;
      peakMemory: string;
      health: string;
    } => {
      return {
        cacheHitRate: `${Math.round(getOverallCacheHitRate() * 100)}%`,
        averageAnalysisTime: `${getAverageAnalysisTime()} ms`,
        peakMemory: getMemoryFormatted(),
        health: getPerformanceHealth().toUpperCase(),
      };
    },
    [
      getOverallCacheHitRate,
      getAverageAnalysisTime,
      getMemoryFormatted,
      getPerformanceHealth,
    ]
  );

  return {
    // State
    metrics,
    isLoading,

    // Main operations
    getMetrics,
    resetMetrics,

    // Calculations
    getOverallCacheHitRate,
    getTotalCacheHits,
    getTotalCacheMisses,
    getAverageAnalysisTime,
    getPeakMemoryUsage,
    getMemoryFormatted,

    // Health assessment
    getPerformanceHealth,
    getHealthColor,

    // Summaries
    getCacheStats,
    getFormattedMetrics,

    // Connection
    ipcConnected: ipc.connected,
  };
};
