/**
 * Memory Management and Garbage Collection
 * PERF-006: Memory Management
 *
 * Profiles memory usage and implements smart garbage collection
 * Target: <500MB for typical 100-part assembly
 */

export interface MemoryProfile {
  timestamp: number;
  heapUsed: number;
  heapTotal: number;
  heapLimit: number;
  external: number;
}

export interface CacheStats {
  solverCacheSize: number;
  geometryCacheSize: number;
  renderCacheSize: number;
  totalCacheSize: number;
}

export class MemoryManager {
  private memoryProfiles: MemoryProfile[] = [];
  private maxProfiles: number = 100;
  private caches: Map<string, Map<string, any>> = new Map();
  private lastGCTime: number = 0;
  private gcInterval: number = 60000; // 1 minute
  private targetHeapMB: number = 500;

  constructor() {
    this.initializeCaches();
    this.startMemoryMonitoring();
  }

  /**
   * Initialize cache maps
   */
  private initializeCaches(): void {
    this.caches.set('solver', new Map());
    this.caches.set('geometry', new Map());
    this.caches.set('render', new Map());
    this.caches.set('assets', new Map());
  }

  /**
   * Capture current memory profile
   */
  captureMemory(): MemoryProfile {
    const memory = (performance as any).memory || {
      usedJSHeapSize: 0,
      jsHeapSizeLimit: 0,
      totalJSHeapSize: 0,
    };

    const profile: MemoryProfile = {
      timestamp: Date.now(),
      heapUsed: memory.usedJSHeapSize,
      heapTotal: memory.totalJSHeapSize,
      heapLimit: memory.jsHeapSizeLimit,
      external: (memory as any).externalMemoryUsage || 0,
    };

    this.memoryProfiles.push(profile);
    if (this.memoryProfiles.length > this.maxProfiles) {
      this.memoryProfiles.shift();
    }

    return profile;
  }

  /**
   * Get memory usage in MB
   */
  getMemoryUsageMB(): number {
    const profile = this.captureMemory();
    return profile.heapUsed / (1024 * 1024);
  }

  /**
   * Check if memory usage is critical
   */
  isMemoryCritical(): boolean {
    const usageMB = this.getMemoryUsageMB();
    return usageMB > this.targetHeapMB * 0.9;
  }

  /**
   * Get cache statistics
   */
  getCacheStats(): CacheStats {
    let totalSize = 0;
    const stats: Record<string, number> = {};

    for (const [name, cache] of this.caches) {
      const size = this.estimateCacheSize(cache);
      stats[`${name}CacheSize`] = size;
      totalSize += size;
    }

    return {
      solverCacheSize: stats.solverCacheSize || 0,
      geometryCacheSize: stats.geometryCacheSize || 0,
      renderCacheSize: stats.renderCacheSize || 0,
      totalCacheSize: totalSize,
    };
  }

  /**
   * Estimate cache size in bytes (rough estimate)
   */
  private estimateCacheSize(cache: Map<string, any>): number {
    let bytes = 0;
    for (const [key, value] of cache) {
      bytes += key.length * 2; // String overhead
      bytes += JSON.stringify(value).length * 2; // Value overhead
      bytes += 56; // Node overhead
    }
    return bytes;
  }

  /**
   * Clear cache by name
   */
  clearCache(cacheName: string): number {
    const cache = this.caches.get(cacheName);
    if (!cache) return 0;

    const beforeSize = cache.size;
    cache.clear();
    return beforeSize;
  }

  /**
   * Evict cache entries based on LRU or size
   */
  evictCache(cacheName: string, targetSize: number): void {
    const cache = this.caches.get(cacheName);
    if (!cache) return;

    const entries = Array.from(cache.entries());
    const sizePerEntry = this.estimateCacheSize(cache) / cache.size;
    const entriesToRemove = Math.ceil((cache.size - targetSize) / sizePerEntry);

    for (let i = 0; i < entriesToRemove && cache.size > 0; i++) {
      const [key] = entries[i];
      cache.delete(key);
    }
  }

  /**
   * Perform garbage collection if needed
   */
  performGarbageCollection(): boolean {
    const now = Date.now();
    if (now - this.lastGCTime < this.gcInterval) {
      return false;
    }

    console.log('[PERF] Performing garbage collection...');
    this.lastGCTime = now;

    // Check memory usage
    const usageMB = this.getMemoryUsageMB();
    console.log(`[PERF] Memory usage: ${usageMB.toFixed(1)}MB`);

    if (usageMB > this.targetHeapMB) {
      // Evict caches
      const cacheStats = this.getCacheStats();
      console.log('[PERF] Cache stats:', cacheStats);

      // Evict oldest entries from caches
      for (const [name] of this.caches) {
        this.evictCache(name, 100); // Keep ~100 entries
      }
    }

    // Suggest garbage collection if available
    if ((globalThis as any).gc) {
      try {
        (globalThis as any).gc();
        console.log('[PERF] Garbage collection completed');
      } catch (e) {
        console.warn('[PERF] GC not available:', e);
      }
    }

    return true;
  }

  /**
   * Start periodic memory monitoring
   */
  private startMemoryMonitoring(): void {
    setInterval(() => {
      if (this.isMemoryCritical()) {
        console.warn('[PERF] Memory usage is critical!');
        this.performGarbageCollection();
      }
    }, 5000); // Check every 5 seconds
  }

  /**
   * Get memory statistics over time
   */
  getMemoryTrend() {
    if (this.memoryProfiles.length === 0) {
      return null;
    }

    const profiles = this.memoryProfiles;
    const usages = profiles.map(p => p.heapUsed / (1024 * 1024));

    const avg = usages.reduce((a, b) => a + b, 0) / usages.length;
    const min = Math.min(...usages);
    const max = Math.max(...usages);

    return {
      avgMB: avg.toFixed(1),
      minMB: min.toFixed(1),
      maxMB: max.toFixed(1),
      currentMB: usages[usages.length - 1].toFixed(1),
      sampleCount: profiles.length,
    };
  }

  /**
   * Register a cache
   */
  registerCache(name: string, cache: Map<string, any>): void {
    this.caches.set(name, cache);
  }

  /**
   * Get all profiles
   */
  getProfiles(): MemoryProfile[] {
    return [...this.memoryProfiles];
  }

  /**
   * Export memory report
   */
  exportReport() {
    const trend = this.getMemoryTrend();
    const cacheStats = this.getCacheStats();

    return {
      trend,
      caches: cacheStats,
      profiles: this.memoryProfiles.slice(-10), // Last 10 samples
      isCritical: this.isMemoryCritical(),
      targetMB: this.targetHeapMB,
    };
  }
}

/**
 * Global memory manager
 */
export const memoryManager = new MemoryManager();

/**
 * Hook to monitor component memory usage
 */
export function useMemoryMonitor(componentName: string) {
  React.useEffect(() => {
    const initialMemory = memoryManager.getMemoryUsageMB();

    return () => {
      const finalMemory = memoryManager.getMemoryUsageMB();
      const delta = (finalMemory - initialMemory).toFixed(2);
      console.log(`[PERF] Component ${componentName} memory delta: ${delta}MB`);
    };
  }, [componentName]);
}

// Placeholder for React import
const React = require('react');
