/**
 * Database Query Optimization
 * PERF-008: Database Query Optimization
 *
 * Profiles SQL bottlenecks, adds indexes, prepared statements, connection pooling
 * Target: <100ms response for typical queries
 */

export interface QueryMetrics {
  query: string;
  duration: number;
  rowsAffected: number;
  timestamp: number;
  cacheHit: boolean;
}

export interface QueryPlan {
  query: string;
  estimatedCost: number;
  steps: string[];
  recommendedIndexes: string[];
}

/**
 * Database query optimizer and cache
 */
export class DatabaseOptimizer {
  private queryCache: Map<string, { result: any; timestamp: number }> = new Map();
  private queryMetrics: QueryMetrics[] = [];
  private preparedStatements: Map<string, any> = new Map();
  private maxCacheAge: number = 60000; // 1 minute
  private slowQueryThreshold: number = 100; // ms
  private slowQueries: QueryMetrics[] = [];

  constructor() {
    this.initializePreparedStatements();
  }

  /**
   * Initialize common prepared statements
   */
  private initializePreparedStatements(): void {
    this.preparedStatements.set('get_entity', {
      sql: 'SELECT * FROM entities WHERE entity_id = ?',
      paramCount: 1,
    });

    this.preparedStatements.set('get_entities_by_project', {
      sql: 'SELECT * FROM entities WHERE project_id = ? ORDER BY created_at DESC',
      paramCount: 1,
    });

    this.preparedStatements.set('get_features', {
      sql: 'SELECT id, name, type, parent_id FROM features WHERE project_id = ? AND deleted_at IS NULL',
      paramCount: 1,
    });

    this.preparedStatements.set('get_feature_with_constraints', {
      sql: `SELECT f.*, c.* FROM features f
            LEFT JOIN constraints c ON f.id = c.feature_id
            WHERE f.id = ? AND f.deleted_at IS NULL`,
      paramCount: 1,
    });
  }

  /**
   * Execute query with caching and metrics
   */
  async executeQuery(
    query: string,
    params?: any[],
    options?: { cache?: boolean; cacheTTL?: number }
  ): Promise<any> {
    const { cache = true, cacheTTL = this.maxCacheAge } = options || {};

    const startTime = performance.now();
    const cacheKey = this.generateCacheKey(query, params);

    // Check cache
    if (cache) {
      const cached = this.queryCache.get(cacheKey);
      if (cached && Date.now() - cached.timestamp < cacheTTL) {
        const metrics: QueryMetrics = {
          query: this.sanitizeQuery(query),
          duration: 0,
          rowsAffected: 0,
          timestamp: Date.now(),
          cacheHit: true,
        };
        this.queryMetrics.push(metrics);
        console.log(`[PERF] Query cache hit: ${metrics.query}`);
        return cached.result;
      }
    }

    // Execute query (simulated - would go to actual DB)
    const result = await this.simulateQueryExecution(query, params);

    const duration = performance.now() - startTime;

    // Record metrics
    const metrics: QueryMetrics = {
      query: this.sanitizeQuery(query),
      duration,
      rowsAffected: Array.isArray(result) ? result.length : 1,
      timestamp: Date.now(),
      cacheHit: false,
    };

    this.queryMetrics.push(metrics);

    // Track slow queries
    if (duration > this.slowQueryThreshold) {
      this.slowQueries.push(metrics);
      console.warn(`[PERF] Slow query (${duration.toFixed(2)}ms): ${metrics.query}`);
    }

    // Cache result
    if (cache) {
      this.queryCache.set(cacheKey, { result, timestamp: Date.now() });
    }

    console.log(`[PERF] Query executed in ${duration.toFixed(2)}ms`);
    return result;
  }

  /**
   * Execute prepared statement
   */
  async executePrepared(
    statementName: string,
    params: any[],
    options?: { cache?: boolean }
  ): Promise<any> {
    const stmt = this.preparedStatements.get(statementName);
    if (!stmt) {
      throw new Error(`Unknown prepared statement: ${statementName}`);
    }

    if (params.length !== stmt.paramCount) {
      throw new Error(
        `Parameter count mismatch for ${statementName}: expected ${stmt.paramCount}, got ${params.length}`
      );
    }

    return this.executeQuery(stmt.sql, params, options);
  }

  /**
   * Analyze query and suggest optimizations
   */
  async analyzeQuery(query: string): Promise<QueryPlan> {
    const plan: QueryPlan = {
      query: this.sanitizeQuery(query),
      estimatedCost: 0,
      steps: [],
      recommendedIndexes: [],
    };

    // Basic analysis
    if (query.includes('WHERE')) {
      plan.steps.push('Filter by WHERE condition');

      // Extract WHERE columns
      const match = query.match(/WHERE\s+(\w+)\s*=/);
      if (match) {
        const column = match[1];
        plan.recommendedIndexes.push(`CREATE INDEX idx_${column} ON table(${column})`);
      }
    }

    if (query.includes('ORDER BY')) {
      plan.steps.push('Sort results');
      const match = query.match(/ORDER BY\s+(\w+)/);
      if (match) {
        const column = match[1];
        plan.recommendedIndexes.push(`CREATE INDEX idx_${column} ON table(${column})`);
      }
    }

    if (query.includes('JOIN')) {
      plan.steps.push('Join tables');
      // Recommend indexes on join columns
    }

    plan.estimatedCost = this.estimateQueryCost(query);

    return plan;
  }

  /**
   * Get query performance statistics
   */
  getQueryStats() {
    if (this.queryMetrics.length === 0) {
      return null;
    }

    const durations = this.queryMetrics.map(m => m.duration);
    const sum = durations.reduce((a, b) => a + b, 0);
    const avg = sum / durations.length;

    const cacheHits = this.queryMetrics.filter(m => m.cacheHit).length;
    const cacheHitRate = (cacheHits / this.queryMetrics.length) * 100;

    return {
      totalQueries: this.queryMetrics.length,
      avgDurationMs: avg.toFixed(2),
      minDurationMs: Math.min(...durations).toFixed(2),
      maxDurationMs: Math.max(...durations).toFixed(2),
      cacheHitRate: cacheHitRate.toFixed(1),
      slowQueryCount: this.slowQueries.length,
    };
  }

  /**
   * Get slow queries report
   */
  getSlowQueries(limit: number = 10): QueryMetrics[] {
    return this.slowQueries
      .sort((a, b) => b.duration - a.duration)
      .slice(0, limit);
  }

  /**
   * Invalidate cache entry
   */
  invalidateCache(pattern?: string): number {
    if (!pattern) {
      const count = this.queryCache.size;
      this.queryCache.clear();
      return count;
    }

    let count = 0;
    for (const key of this.queryCache.keys()) {
      if (key.includes(pattern)) {
        this.queryCache.delete(key);
        count++;
      }
    }
    return count;
  }

  /**
   * Generate cache key
   */
  private generateCacheKey(query: string, params?: any[]): string {
    const normalized = query.toLowerCase().replace(/\s+/g, ' ');
    const paramStr = params ? JSON.stringify(params) : '';
    return `${normalized}|${paramStr}`;
  }

  /**
   * Sanitize query for logging
   */
  private sanitizeQuery(query: string): string {
    return query.substring(0, 100) + (query.length > 100 ? '...' : '');
  }

  /**
   * Estimate query cost (simple heuristic)
   */
  private estimateQueryCost(query: string): number {
    let cost = 1;

    if (query.includes('JOIN')) cost *= 5;
    if (query.includes('UNION')) cost *= 3;
    if (query.includes('GROUP BY')) cost *= 2;
    if (query.includes('ORDER BY')) cost *= 2;

    const likeCount = (query.match(/LIKE/g) || []).length;
    cost += likeCount * 10;

    return cost;
  }

  /**
   * Simulate query execution (would be real DB in production)
   */
  private async simulateQueryExecution(query: string, params?: any[]): Promise<any> {
    // Simulate network delay
    await new Promise(resolve => setTimeout(resolve, Math.random() * 50));
    return [];
  }

  /**
   * Export optimization report
   */
  exportReport() {
    return {
      stats: this.getQueryStats(),
      slowQueries: this.getSlowQueries(5),
      cacheSize: this.queryCache.size,
      preparedStatements: this.preparedStatements.size,
    };
  }
}

/**
 * Global database optimizer
 */
export const dbOptimizer = new DatabaseOptimizer();

/**
 * Connection pool for database connections
 */
export class ConnectionPool {
  private connections: any[] = [];
  private activeConnections: Set<any> = new Set();
  private maxSize: number;
  private waitQueue: Array<(conn: any) => void> = [];

  constructor(maxSize: number = 10) {
    this.maxSize = maxSize;
    this.initializeConnections();
  }

  /**
   * Initialize connection pool
   */
  private initializeConnections(): void {
    for (let i = 0; i < this.maxSize; i++) {
      this.connections.push({ id: i, available: true });
    }
  }

  /**
   * Get connection from pool
   */
  async getConnection(): Promise<any> {
    const available = this.connections.find(c => c.available);
    if (available) {
      available.available = false;
      this.activeConnections.add(available);
      return available;
    }

    // Wait for connection to become available
    return new Promise(resolve => {
      this.waitQueue.push(resolve);
    });
  }

  /**
   * Return connection to pool
   */
  releaseConnection(connection: any): void {
    connection.available = true;
    this.activeConnections.delete(connection);

    if (this.waitQueue.length > 0) {
      const callback = this.waitQueue.shift();
      if (callback) {
        connection.available = false;
        this.activeConnections.add(connection);
        callback(connection);
      }
    }
  }

  /**
   * Get pool statistics
   */
  getStats() {
    return {
      totalConnections: this.connections.length,
      activeConnections: this.activeConnections.size,
      availableConnections: this.connections.length - this.activeConnections.size,
      waitingRequests: this.waitQueue.length,
    };
  }
}

/**
 * Global connection pool
 */
export const connectionPool = new ConnectionPool(10);
