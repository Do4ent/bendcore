/**
 * React Component Performance Optimization Utilities
 * PERF-001: Application Startup Profiling - Lazy Loading & Code Splitting
 *
 * Provides optimized component wrapper and lazy loading utilities
 */

import React, { lazy, Suspense, ComponentType, ReactNode } from 'react';

/**
 * Optimized component wrapper with automatic memoization
 * Prevents unnecessary re-renders
 */
export function withMemo<P extends object>(
  Component: React.ComponentType<P>,
  displayName?: string
): React.MemoExoticComponent<React.ComponentType<P>> {
  const memoized = React.memo(Component);
  if (displayName) {
    memoized.displayName = displayName;
  }
  return memoized;
}

/**
 * Lazy load component with fallback UI
 */
export function lazyLoad<P extends object>(
  importFunc: () => Promise<{ default: ComponentType<P> }>,
  displayName?: string
): React.LazyExoticComponent<ComponentType<P>> {
  const LazyComponent = lazy(importFunc);
  if (displayName) {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    (LazyComponent as any).displayName = `Lazy(${displayName})`;
  }
  return LazyComponent;
}

/**
 * Component loading fallback
 */
export const ComponentLoadingFallback: React.FC = () => (
  <div
    style={{
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      padding: '20px',
      color: '#999',
      fontSize: '14px',
    }}
  >
    Loading...
  </div>
);

/**
 * Suspense wrapper for lazy-loaded components
 */
export function SuspenseBoundary({
  children,
  fallback,
  name,
}: {
  children: ReactNode;
  fallback?: ReactNode;
  name?: string;
}): JSX.Element {
  return (
    <Suspense
      fallback={
        fallback || (
          <div style={{ padding: '20px', textAlign: 'center', color: '#ccc' }}>
            Loading {name || 'component'}...
          </div>
        )
      }
    >
      {children}
    </Suspense>
  );
}

/**
 * Higher-order component for optimized performance
 */
export function withPerformanceOptimization<P extends object>(
  Component: React.ComponentType<P>,
  options?: {
    lazy?: boolean;
    memo?: boolean;
    displayName?: string;
  }
): React.ComponentType<P> {
  const { lazy: shouldLazy = false, memo: shouldMemo = true, displayName } = options || {};

  let OptimizedComponent: React.ComponentType<P> = Component;

  if (shouldMemo) {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    OptimizedComponent = React.memo(OptimizedComponent) as any as React.ComponentType<P>;
  }

  if (shouldLazy) {
    // Can't directly lazy load a component like this
    // This is a placeholder for the concept
    console.warn('Lazy loading via HOC requires dynamic imports');
  }

  if (displayName) {
    OptimizedComponent.displayName = displayName;
  }

  return OptimizedComponent;
}

/**
 * Batch state updates for better performance
 */
export function useBatchUpdate<T>(defaultValue: T) {
  const [value, setValue] = React.useState<T>(defaultValue);
  const updateQueue = React.useRef<T[]>([]);
  const timeoutRef = React.useRef<NodeJS.Timeout>();

  const batchUpdate = React.useCallback((newValue: T | ((prev: T) => T)) => {
    const actualValue = typeof newValue === 'function' ? (newValue as (prev: T) => T)(value) : newValue;
    updateQueue.current.push(actualValue);

    // Clear existing timeout
    if (timeoutRef.current) {
      clearTimeout(timeoutRef.current);
    }

    // Batch updates within a frame
    timeoutRef.current = setTimeout(() => {
      if (updateQueue.current.length > 0) {
        // Apply the last value in the queue
        setValue(updateQueue.current[updateQueue.current.length - 1]);
        updateQueue.current = [];
      }
    }, 0);
  }, [value]);

  React.useEffect(() => {
    return () => {
      if (timeoutRef.current) {
        clearTimeout(timeoutRef.current);
      }
    };
  }, []);

  return [value, batchUpdate] as const;
}

/**
 * Debounced state update
 */
export function useDebouncedState<T>(
  defaultValue: T,
  delay: number = 300
): [T, (value: T) => void] {
  const [value, setValue] = React.useState<T>(defaultValue);
  const timeoutRef = React.useRef<NodeJS.Timeout>();

  const setDebouncedValue = React.useCallback((newValue: T) => {
    if (timeoutRef.current) {
      clearTimeout(timeoutRef.current);
    }
    timeoutRef.current = setTimeout(() => {
      setValue(newValue);
    }, delay);
  }, [delay]);

  React.useEffect(() => {
    return () => {
      if (timeoutRef.current) {
        clearTimeout(timeoutRef.current);
      }
    };
  }, []);

  return [value, setDebouncedValue];
}

/**
 * Throttled callback
 */
export function useThrottledCallback<T extends any[], R>(
  callback: (...args: T) => R,
  delay: number = 300
): (...args: T) => void {
  const timeoutRef = React.useRef<NodeJS.Timeout>();
  const lastRunRef = React.useRef<number>(0);

  return React.useCallback(
    (...args: T) => {
      const now = Date.now();
      const timeSinceLastRun = now - lastRunRef.current;

      if (timeSinceLastRun >= delay) {
        callback(...args);
        lastRunRef.current = now;
      } else {
        if (timeoutRef.current) {
          clearTimeout(timeoutRef.current);
        }
        timeoutRef.current = setTimeout(() => {
          callback(...args);
          lastRunRef.current = Date.now();
        }, delay - timeSinceLastRun);
      }
    },
    [callback, delay]
  );
}

/**
 * Intersection Observer hook for lazy rendering
 */
export function useIntersectionObserver(
  ref: React.RefObject<HTMLElement>,
  options?: IntersectionObserverInit
): boolean {
  const [isVisible, setIsVisible] = React.useState(false);

  React.useEffect(() => {
    if (!ref.current) return;

    const observer = new IntersectionObserver(([entry]) => {
      if (entry.isIntersecting) {
        setIsVisible(true);
        observer.unobserve(entry.target);
      }
    }, options);

    observer.observe(ref.current);

    return () => {
      observer.disconnect();
    };
  }, [ref, options]);

  return isVisible;
}
