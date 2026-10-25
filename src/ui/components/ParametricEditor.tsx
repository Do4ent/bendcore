import React, { useState, useCallback, useEffect, useMemo } from 'react';

/**
 * CAD-CORE-026: ParametricEditor
 * Parametric editor UI for real-time parameter updates and dependency management
 * ~900 LOC + tests
 */

export interface ParameterDefinition {
  /** Parameter unique identifier */
  id: string;
  /** Parameter display name */
  name: string;
  /** Parameter type */
  type: 'number' | 'string' | 'boolean' | 'select' | 'expression';
  /** Current parameter value */
  value: any;
  /** Minimum value (for numbers) */
  min?: number;
  /** Maximum value (for numbers) */
  max?: number;
  /** Step size (for numbers) */
  step?: number;
  /** Select options */
  options?: Array<{ label: string; value: any }>;
  /** Parameter unit */
  unit?: string;
  /** Parameter description */
  description?: string;
  /** Whether parameter is read-only */
  readOnly?: boolean;
  /** Expression for calculated parameters */
  expression?: string;
  /** Dependencies on other parameters */
  dependencies?: string[];
  /** Group this parameter belongs to */
  group?: string;
}

export interface ParametricEditorProps {
  /** Array of parameter definitions */
  parameters: ParameterDefinition[];
  /** Callback on parameter value change */
  onParameterChange?: (parameterId: string, value: any) => void;
  /** Callback on batch update */
  onBatchUpdate?: (updates: Record<string, any>) => void;
  /** Enable real-time updates */
  realTimeUpdate?: boolean;
  /** Update delay in ms */
  updateDelay?: number;
  /** Callback for expression evaluation */
  onEvaluateExpression?: (expression: string, context: Record<string, any>) => any;
  /** Trigger rebuild */
  onRebuild?: () => void;
  /** Enable undo/redo */
  enableUndoRedo?: boolean;
}

interface ParameterGroup {
  [groupName: string]: ParameterDefinition[];
}

/**
 * Expression Evaluator for parametric calculations
 */
class ExpressionEvaluator {
  private context: Record<string, any> = {};

  /**
   * Set evaluation context with parameter values
   */
  setContext(context: Record<string, any>): void {
    this.context = context;
  }

  /**
   * Safely evaluate expression with current context
   */
  evaluate(expression: string): any {
    try {
      // Sanitize expression: only allow alphanumeric, operators, and parentheses
      if (!/^[0-9+\-*/.()a-zA-Z_\s]+$/.test(expression)) {
        throw new Error('Invalid expression characters');
      }

      // Create safe evaluation function
      const fn = new Function(...Object.keys(this.context), `return ${expression}`);
      return fn(...Object.values(this.context));
    } catch (error) {
      console.error(`Failed to evaluate expression: ${expression}`, error);
      return null;
    }
  }
}

/**
 * Dependency resolver for parameter updates
 */
class DependencyResolver {
  private parameters: ParameterDefinition[];
  private evaluator: ExpressionEvaluator;

  constructor(parameters: ParameterDefinition[], evaluator: ExpressionEvaluator) {
    this.parameters = parameters;
    this.evaluator = evaluator;
  }

  /**
   * Get all parameters that depend on the given parameter
   */
  getDependents(parameterId: string): ParameterDefinition[] {
    return this.parameters.filter((p) => p.dependencies?.includes(parameterId));
  }

  /**
   * Recursively get all parameters that need updating
   */
  getUpdateChain(parameterId: string): string[] {
    const chain: string[] = [parameterId];
    const dependents = this.getDependents(parameterId);

    dependents.forEach((p) => {
      chain.push(...this.getUpdateChain(p.id));
    });

    return [...new Set(chain)]; // Remove duplicates
  }

  /**
   * Evaluate dependent parameters
   */
  evaluateDependents(
    parameterId: string,
    values: Record<string, any>
  ): Record<string, any> {
    const updates: Record<string, any> = {};
    const dependents = this.getDependents(parameterId);

    dependents.forEach((p) => {
      if (p.expression) {
        this.evaluator.setContext(values);
        const result = this.evaluator.evaluate(p.expression);
        if (result !== null) {
          updates[p.id] = result;
          values[p.id] = result;
        }
      }
    });

    return updates;
  }
}

/**
 * ParametricEditor Component
 * UI for managing parametric design with dependency resolution
 */
export const ParametricEditor: React.FC<ParametricEditorProps> = ({
  parameters,
  onParameterChange,
  onBatchUpdate,
  realTimeUpdate = true,
  updateDelay = 300,
  onEvaluateExpression,
  onRebuild,
  enableUndoRedo = true,
}) => {
  const [values, setValues] = useState<Record<string, any>>(() => {
    const initial: Record<string, any> = {};
    parameters.forEach((p) => {
      initial[p.id] = p.value;
    });
    return initial;
  });

  const [pendingUpdates, setPendingUpdates] = useState<Record<string, any>>({});
  const [history, setHistory] = useState<Array<Record<string, any>>>([values]);
  const [historyIndex, setHistoryIndex] = useState(0);
  const [expandedGroups, setExpandedGroups] = useState<Set<string>>(new Set());

  // Initialize managers
  const evaluator = useMemo(() => new ExpressionEvaluator(), []);
  const resolver = useMemo(() => new DependencyResolver(parameters, evaluator), [parameters, evaluator]);

  // Group parameters
  const groupedParameters = useMemo<ParameterGroup>(() => {
    const groups: ParameterGroup = {};
    parameters.forEach((p) => {
      const groupName = p.group || 'General';
      if (!groups[groupName]) groups[groupName] = [];
      groups[groupName].push(p);
    });
    return groups;
  }, [parameters]);

  /**
   * Handle parameter value change
   */
  const handleParameterChange = useCallback(
    (parameterId: string, newValue: any) => {
      const newValues = { ...values, [parameterId]: newValue };

      // Evaluate dependent parameters
      const dependentUpdates = resolver.evaluateDependents(parameterId, newValues);
      const allUpdates = { ...newValues, ...dependentUpdates };

      if (realTimeUpdate) {
        setPendingUpdates((prev) => ({ ...prev, [parameterId]: newValue }));
      }

      // Batch callback
      if (Object.keys(dependentUpdates).length > 0) {
        onBatchUpdate?.({ [parameterId]: newValue, ...dependentUpdates });
      } else {
        onParameterChange?.(parameterId, newValue);
      }

      // Update local state
      setValues(allUpdates);

      // Add to history
      if (enableUndoRedo) {
        const newHistory = history.slice(0, historyIndex + 1);
        newHistory.push(allUpdates);
        setHistory(newHistory);
        setHistoryIndex(newHistory.length - 1);
      }
    },
    [values, resolver, realTimeUpdate, onParameterChange, onBatchUpdate, enableUndoRedo, history, historyIndex]
  );

  /**
   * Handle undo
   */
  const handleUndo = useCallback(() => {
    if (historyIndex > 0) {
      const newIndex = historyIndex - 1;
      const prevValues = history[newIndex];
      setValues(prevValues);
      setHistoryIndex(newIndex);
      onBatchUpdate?.(prevValues);
    }
  }, [historyIndex, history, onBatchUpdate]);

  /**
   * Handle redo
   */
  const handleRedo = useCallback(() => {
    if (historyIndex < history.length - 1) {
      const newIndex = historyIndex + 1;
      const nextValues = history[newIndex];
      setValues(nextValues);
      setHistoryIndex(newIndex);
      onBatchUpdate?.(nextValues);
    }
  }, [historyIndex, history, onBatchUpdate]);

  /**
   * Toggle group expansion
   */
  const toggleGroup = useCallback((groupName: string) => {
    setExpandedGroups((prev) => {
      const newSet = new Set(prev);
      if (newSet.has(groupName)) {
        newSet.delete(groupName);
      } else {
        newSet.add(groupName);
      }
      return newSet;
    });
  }, []);

  /**
   * Render parameter input
   */
  const renderParameter = (param: ParameterDefinition) => {
    const value = values[param.id];
    const isReadOnly = param.readOnly || param.expression; // Calculated params are read-only

    switch (param.type) {
      case 'number':
        return (
          <input
            type="number"
            min={param.min}
            max={param.max}
            step={param.step || 0.1}
            value={value}
            onChange={(e) => handleParameterChange(param.id, parseFloat(e.target.value))}
            disabled={isReadOnly}
            style={{
              width: '100%',
              padding: '6px',
              fontSize: '12px',
              border: '1px solid #444',
              borderRadius: '4px',
              backgroundColor: isReadOnly ? '#2a2a2a' : '#1a1a1a',
              color: '#fff',
            }}
          />
        );

      case 'string':
        return (
          <input
            type="text"
            value={value}
            onChange={(e) => handleParameterChange(param.id, e.target.value)}
            disabled={isReadOnly}
            style={{
              width: '100%',
              padding: '6px',
              fontSize: '12px',
              border: '1px solid #444',
              borderRadius: '4px',
              backgroundColor: isReadOnly ? '#2a2a2a' : '#1a1a1a',
              color: '#fff',
            }}
          />
        );

      case 'boolean':
        return (
          <input
            type="checkbox"
            checked={value}
            onChange={(e) => handleParameterChange(param.id, e.target.checked)}
            disabled={isReadOnly}
            style={{ cursor: isReadOnly ? 'default' : 'pointer' }}
          />
        );

      case 'select':
        return (
          <select
            value={value}
            onChange={(e) => handleParameterChange(param.id, e.target.value)}
            disabled={isReadOnly}
            style={{
              width: '100%',
              padding: '6px',
              fontSize: '12px',
              border: '1px solid #444',
              borderRadius: '4px',
              backgroundColor: isReadOnly ? '#2a2a2a' : '#1a1a1a',
              color: '#fff',
            }}
          >
            {param.options?.map((opt) => (
              <option key={opt.value} value={opt.value}>
                {opt.label}
              </option>
            ))}
          </select>
        );

      case 'expression':
        return (
          <div
            style={{
              padding: '6px',
              fontSize: '11px',
              border: '1px solid #444',
              borderRadius: '4px',
              backgroundColor: '#2a2a2a',
              color: '#00ff00',
              fontFamily: 'monospace',
              userSelect: 'text',
            }}
          >
            {param.expression}
          </div>
        );

      default:
        return <span>Unsupported type: {param.type}</span>;
    }
  };

  return (
    <div
      style={{
        padding: '12px',
        backgroundColor: '#222',
        color: '#fff',
        fontSize: '12px',
        fontFamily: 'system-ui, -apple-system, sans-serif',
        borderRadius: '4px',
        overflow: 'auto',
        maxHeight: '600px',
      }}
    >
      {/* Header with controls */}
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          marginBottom: '12px',
          paddingBottom: '8px',
          borderBottom: '1px solid #444',
        }}
      >
        <h3 style={{ margin: 0, fontSize: '14px', fontWeight: 600 }}>Parameters</h3>
        <div style={{ display: 'flex', gap: '4px' }}>
          {enableUndoRedo && (
            <>
              <button
                onClick={handleUndo}
                disabled={historyIndex === 0}
                title="Undo"
                style={{
                  padding: '4px 8px',
                  fontSize: '11px',
                  backgroundColor: historyIndex === 0 ? '#333' : '#0066cc',
                  color: '#fff',
                  border: 'none',
                  borderRadius: '3px',
                  cursor: historyIndex === 0 ? 'default' : 'pointer',
                }}
              >
                ↶
              </button>
              <button
                onClick={handleRedo}
                disabled={historyIndex === history.length - 1}
                title="Redo"
                style={{
                  padding: '4px 8px',
                  fontSize: '11px',
                  backgroundColor: historyIndex === history.length - 1 ? '#333' : '#0066cc',
                  color: '#fff',
                  border: 'none',
                  borderRadius: '3px',
                  cursor: historyIndex === history.length - 1 ? 'default' : 'pointer',
                }}
              >
                ↷
              </button>
            </>
          )}
          <button
            onClick={onRebuild}
            title="Rebuild"
            style={{
              padding: '4px 8px',
              fontSize: '11px',
              backgroundColor: '#00aa00',
              color: '#fff',
              border: 'none',
              borderRadius: '3px',
              cursor: 'pointer',
            }}
          >
            Rebuild
          </button>
        </div>
      </div>

      {/* Parameter groups */}
      {Object.entries(groupedParameters).map(([groupName, groupParams]) => (
        <div key={groupName} style={{ marginBottom: '12px' }}>
          {/* Group header */}
          <div
            onClick={() => toggleGroup(groupName)}
            style={{
              padding: '6px 8px',
              backgroundColor: '#333',
              cursor: 'pointer',
              borderRadius: '3px',
              marginBottom: '6px',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
            }}
          >
            <span style={{ fontSize: '10px' }}>
              {expandedGroups.has(groupName) ? '▼' : '▶'}
            </span>
            <span style={{ fontWeight: 500 }}>{groupName}</span>
          </div>

          {/* Group parameters */}
          {expandedGroups.has(groupName) && (
            <div style={{ marginLeft: '12px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {groupParams.map((param) => (
                <div key={param.id}>
                  <label
                    style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                      marginBottom: '4px',
                      fontSize: '11px',
                    }}
                  >
                    <span style={{ fontWeight: 500 }}>
                      {param.name}
                      {param.unit && <span style={{ marginLeft: '4px', opacity: 0.7 }}>({param.unit})</span>}
                    </span>
                    {param.expression && (
                      <span style={{ fontSize: '9px', opacity: 0.6, fontStyle: 'italic' }}>
                        calculated
                      </span>
                    )}
                  </label>
                  {renderParameter(param)}
                  {param.description && (
                    <div style={{ fontSize: '10px', opacity: 0.6, marginTop: '2px', fontStyle: 'italic' }}>
                      {param.description}
                    </div>
                  )}
                </div>
              ))}
            </div>
          )}
        </div>
      ))}
    </div>
  );
};

export default ParametricEditor;
