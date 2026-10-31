/**
 * Constraint Solver Caching System
 * PERF-003: Constraint Solver Caching
 *
 * Caches constraint solver results for unchanged sketches
 * LRU cache with parameter-change invalidation
 * Target: <50ms solving for 50-constraint sketches
 */

export interface SketchConstraint {
  id: string;
  type: string;
  entities: string[];
  value?: number;
  parameters?: Record<string, any>;
}

export interface SolverResult {
  sketchId: string;
  constraints: SketchConstraint[];
  solution: Record<string, number>;
  residuals: Record<string, number>;
  converged: boolean;
  iterations: number;
  solveTime: number;
  timestamp: number;
}

export interface CacheEntry {
  key: string;
  result: SolverResult;
  timestamp: number;
  accessCount: number;
  lastAccessed: number;
  hash: string;
}

/**
 * LRU Cache for constraint solver results
 */
export class ConstraintSolverCache {
  private cache: Map<string, CacheEntry> = new Map();
  private maxSize: number;
  private hitCount: number = 0;
  private missCount: number = 0;
  private maxAge: number; // milliseconds

  constructor(maxSize: number = 1000, maxAge: number = 300000) { // 5 min default
    this.maxSize = maxSize;
    this.maxAge = maxAge;
  }

  /**
   * Generate cache key from sketch and constraints
   */
  private generateKey(sketchId: string, constraints: SketchConstraint[]): string {
    return `${sketchId}:${this.hashConstraints(constraints)}`;
  }

  /**
   * Hash constraints for invalidation detection
   */
  private hashConstraints(constraints: SketchConstraint[]): string {
    // 32-bit FNV-1a over the canonical form of every constraint. The entity
    // list is copied before sorting so callers' arrays are not mutated.
    let hash = 0x811c9dc5;
    for (const constraint of constraints) {
      const str = JSON.stringify({
        type: constraint.type,
        entities: [...constraint.entities].sort(),
        value: constraint.value,
        parameters: constraint.parameters,
      });
      for (let i = 0; i < str.length; i++) {
        hash ^= str.charCodeAt(i);
        hash = Math.imul(hash, 0x01000193);
      }
    }
    return (hash >>> 0).toString(16);
  }

  /**
   * Store a solver result in cache
   */
  put(sketchId: string, constraints: SketchConstraint[], result: SolverResult): void {
    const key = this.generateKey(sketchId, constraints);
    const hash = this.hashConstraints(constraints);

    // Replacing an existing entry must not evict another one
    if (this.cache.has(key)) {
      this.cache.delete(key);
    } else if (this.cache.size >= this.maxSize) {
      this.evictLRU();
    }

    const entry: CacheEntry = {
      key,
      result,
      timestamp: Date.now(),
      accessCount: 0,
      lastAccessed: Date.now(),
      hash,
    };

    this.cache.set(key, entry);
  }

  /**
   * Retrieve a solver result from cache
   */
  get(sketchId: string, constraints: SketchConstraint[]): SolverResult | null {
    const key = this.generateKey(sketchId, constraints);
    const entry = this.cache.get(key);

    if (!entry) {
      this.missCount++;
      return null;
    }

    // Check if cache entry has expired
    if (Date.now() - entry.timestamp > this.maxAge) {
      this.cache.delete(key);
      this.missCount++;
      return null;
    }

    // Validate hash (parameters haven't changed)
    const currentHash = this.hashConstraints(constraints);
    if (entry.hash !== currentHash) {
      this.cache.delete(key);
      this.missCount++;
      return null;
    }

    // Update access info and move the entry to the most-recently-used end
    entry.accessCount++;
    entry.lastAccessed = Date.now();
    this.cache.delete(key);
    this.cache.set(key, entry);
    this.hitCount++;

    return entry.result;
  }

  /**
   * Invalidate cache entry for a sketch
   */
  invalidate(sketchId: string): void {
    const keysToDelete: string[] = [];
    for (const [key] of this.cache) {
      if (key.startsWith(`${sketchId}:`)) {
        keysToDelete.push(key);
      }
    }
    keysToDelete.forEach(key => this.cache.delete(key));
  }

  /**
   * Evict least recently used entry
   */
  private evictLRU(): void {
    // Map preserves insertion order and entries are re-inserted on access,
    // so the first key is always the least recently used one.
    const oldest = this.cache.keys().next();
    if (!oldest.done) {
      this.cache.delete(oldest.value);
    }
  }

  /**
   * Clear entire cache
   */
  clear(): void {
    this.cache.clear();
    this.hitCount = 0;
    this.missCount = 0;
  }

  /**
   * Get cache statistics
   */
  getStats() {
    const total = this.hitCount + this.missCount;
    const hitRate = total > 0 ? (this.hitCount / total) * 100 : 0;

    return {
      size: this.cache.size,
      maxSize: this.maxSize,
      hitCount: this.hitCount,
      missCount: this.missCount,
      hitRate: hitRate.toFixed(2),
      utilization: ((this.cache.size / this.maxSize) * 100).toFixed(2),
    };
  }

  /**
   * Get cache size in memory (rough estimate)
   */
  getMemoryUsageBytes(): number {
    let bytes = 0;
    for (const entry of this.cache.values()) {
      // Rough estimate: key + result data
      bytes += entry.key.length * 2; // JavaScript string overhead
      bytes += JSON.stringify(entry.result).length * 2;
      bytes += 128; // Entry metadata overhead
    }
    return bytes;
  }
}

/**
 * Global solver cache instance
 */
export const solverCache = new ConstraintSolverCache(1000, 300000);

/**
 * Mock solver function with cache support
 */
export async function solveConstraintsCached(
  sketchId: string,
  constraints: SketchConstraint[],
  solver: (c: SketchConstraint[]) => Promise<SolverResult>
): Promise<SolverResult> {
  // Try cache first
  const cached = solverCache.get(sketchId, constraints);
  if (cached) {
    console.log(`[PERF] Solver cache hit for ${sketchId}`);
    return cached;
  }

  // Solve and cache result
  console.log(`[PERF] Solving constraints for ${sketchId}`);
  const result = await solver(constraints);
  solverCache.put(sketchId, constraints, result);

  return result;
}
