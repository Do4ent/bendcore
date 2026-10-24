import fs from 'fs';
import path from 'path';

/**
 * PHASE 2: Production-grade Logger Service
 * Замена для console.log/console.error/console.warn
 *
 * Возможности:
 * - Структурированное логирование с timestamp, level, component
 * - Circular buffer (10K логов в памяти)
 * - Файловая запись для WARN/ERROR/CRITICAL
 * - Lazy evaluation (только dev выводит в console)
 * - JSON-структура для парсинга
 */

export enum LogLevel {
  DEBUG = 0,
  INFO = 1,
  WARN = 2,
  ERROR = 3,
  CRITICAL = 4
}

export interface LogEntry {
  timestamp: Date;
  level: LogLevel;
  component: string;
  message: string;
  data?: Record<string, unknown>;
  stack?: string;
}

/**
 * Singleton Logger Service
 */
export class Logger {
  private static instance: Logger;
  private logLevel = LogLevel.INFO;
  private logFile: string | null = null;
  private logs: LogEntry[] = [];
  private readonly maxLogs = 10000; // Circular buffer size

  /**
   * Get singleton instance
   */
  static getInstance(): Logger {
    if (!Logger.instance) {
      Logger.instance = new Logger();
    }
    return Logger.instance;
  }

  /**
   * Debug level logging
   */
  debug(component: string, message: string, data?: Record<string, unknown>): void {
    this.log(LogLevel.DEBUG, component, message, data);
  }

  /**
   * Info level logging
   */
  info(component: string, message: string, data?: Record<string, unknown>): void {
    this.log(LogLevel.INFO, component, message, data);
  }

  /**
   * Warning level logging
   */
  warn(component: string, message: string, data?: Record<string, unknown>): void {
    this.log(LogLevel.WARN, component, message, data);
  }

  /**
   * Error level logging with optional Error object
   */
  error(component: string, message: string, data?: Record<string, unknown>, error?: Error): void {
    this.log(LogLevel.ERROR, component, message, data, error?.stack);
  }

  /**
   * Critical level logging (highest priority)
   */
  critical(component: string, message: string, data?: Record<string, unknown>, error?: Error): void {
    this.log(LogLevel.CRITICAL, component, message, data, error?.stack);
  }

  /**
   * Internal logging implementation
   */
  private log(
    level: LogLevel,
    component: string,
    message: string,
    data?: Record<string, unknown>,
    stack?: string
  ): void {
    // Skip if below minimum log level
    if (level < this.logLevel) return;

    const entry: LogEntry = {
      timestamp: new Date(),
      level,
      component,
      message,
      data,
      stack
    };

    // Add to circular buffer
    this.logs.push(entry);
    if (this.logs.length > this.maxLogs) {
      this.logs.shift();
    }

    // Write to file if configured and level >= WARN
    if (this.logFile && level >= LogLevel.WARN) {
      this.writeToFile(entry);
    }

    // Output to console only in development
    if (process.env.NODE_ENV === 'development') {
      this.outputToConsole(entry);
    }
  }

  /**
   * Output formatted log to console (dev only)
   */
  private outputToConsole(entry: LogEntry): void {
    const levelStr = LogLevel[entry.level];
    const timestamp = entry.timestamp.toISOString();
    const prefix = `[${timestamp}] ${levelStr} [${entry.component}]`;

    switch (entry.level) {
      case LogLevel.DEBUG:
        console.debug(prefix, entry.message, entry.data || '');
        break;
      case LogLevel.INFO:
        console.log(prefix, entry.message, entry.data || '');
        break;
      case LogLevel.WARN:
        console.warn(prefix, entry.message, entry.data || '');
        break;
      case LogLevel.ERROR:
        console.error(prefix, entry.message, entry.data || '', entry.stack ? `\nStack:\n${entry.stack}` : '');
        break;
      case LogLevel.CRITICAL:
        console.error(prefix, '⚠️ CRITICAL:', entry.message, entry.data || '', entry.stack ? `\nStack:\n${entry.stack}` : '');
        break;
    }
  }

  /**
   * Write to log file (async non-blocking)
   */
  private writeToFile(entry: LogEntry): void {
    if (!this.logFile) return;

    const logLine = JSON.stringify({
      timestamp: entry.timestamp.toISOString(),
      level: LogLevel[entry.level],
      component: entry.component,
      message: entry.message,
      data: entry.data,
      stack: entry.stack
    }) + '\n';

    // Async append (non-blocking)
    fs.appendFile(this.logFile, logLine, (err) => {
      if (err && process.env.NODE_ENV === 'development') {
        console.error('Failed to write to log file:', err);
      }
    });
  }

  /**
   * Get logs with optional filtering
   */
  getLogs(filter?: { level?: LogLevel; component?: string }): LogEntry[] {
    return this.logs.filter(log => {
      if (filter?.level !== undefined && log.level !== filter.level) return false;
      if (filter?.component && log.component !== filter.component) return false;
      return true;
    });
  }

  /**
   * Clear all logs from memory
   */
  clearLogs(): void {
    this.logs = [];
  }

  /**
   * Set log file path for persistent logging
   */
  setLogFile(filePath: string): void {
    this.logFile = filePath;
    // Ensure directory exists
    const dir = path.dirname(filePath);
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }
  }

  /**
   * Set minimum log level to output
   */
  setLogLevel(level: LogLevel): void {
    this.logLevel = level;
  }

  /**
   * Export all logs as JSON array
   */
  exportLogs(): string {
    return JSON.stringify(this.logs.map(log => ({
      timestamp: log.timestamp.toISOString(),
      level: LogLevel[log.level],
      component: log.component,
      message: log.message,
      data: log.data,
      stack: log.stack
    })), null, 2);
  }

  /**
   * Get log statistics
   */
  getStats(): {
    totalLogs: number;
    byLevel: Record<string, number>;
    byComponent: Record<string, number>;
  } {
    const stats = {
      totalLogs: this.logs.length,
      byLevel: {} as Record<string, number>,
      byComponent: {} as Record<string, number>
    };

    for (const log of this.logs) {
      const levelName = LogLevel[log.level];
      stats.byLevel[levelName] = (stats.byLevel[levelName] || 0) + 1;
      stats.byComponent[log.component] = (stats.byComponent[log.component] || 0) + 1;
    }

    return stats;
  }
}

/**
 * Export singleton instance for convenience
 */
export const logger = Logger.getInstance();
