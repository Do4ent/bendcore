/**
 * BendCore Formula Editor Component - Q3 2027 Task 1 Phase 2
 * React component for parametric dimension editing with real-time preview
 *
 * Features:
 * - Syntax-highlighted formula input
 * - Variable palette (click to insert)
 * - Real-time preview with calculated value
 * - Error display with helpful hints
 * - localStorage draft auto-save
 * - Dark/light mode support
 * - Keyboard shortcuts (Ctrl+Enter to apply, Esc to cancel)
 */

import React, { useState, useEffect, useRef } from 'react';
import './FormulaEditor.css';

interface VariableDefinition {
  name: string;
  value: number;
  unit: string;
  description: string;
}

interface FormulaEditorProps {
  dimensionId: string;
  initialFormula?: string;
  variables: VariableDefinition[];
  onApply: (formula: string, result: number) => void;
  onCancel: () => void;
  darkMode?: boolean;
}

interface EvaluationResult {
  success: boolean;
  result?: number;
  error?: string;
  suggestions?: string[];
}

const DRAFT_STORAGE_KEY = 'bendcore_formula_draft';

const COMMON_FUNCTIONS = [
  { name: 'sqrt()', description: 'Square root' },
  { name: 'abs()', description: 'Absolute value' },
  { name: 'min()', description: 'Minimum value' },
  { name: 'max()', description: 'Maximum value' },
];

const OPERATORS = [
  { symbol: '+', name: 'Add' },
  { symbol: '-', name: 'Subtract' },
  { symbol: '*', name: 'Multiply' },
  { symbol: '/', name: 'Divide' },
  { symbol: '^', name: 'Power' },
];

export const FormulaEditor: React.FC<FormulaEditorProps> = ({
  dimensionId,
  initialFormula = '',
  variables,
  onApply,
  onCancel,
  darkMode = false,
}) => {
  const [formula, setFormula] = useState<string>(initialFormula);
  const [result, setResult] = useState<EvaluationResult | null>(null);
  const [isDrafted, setIsDrafted] = useState(false);
  const inputRef = useRef<HTMLTextAreaElement>(null);

  // Load draft from localStorage on mount
  useEffect(() => {
    const draft = localStorage.getItem(DRAFT_STORAGE_KEY);
    if (draft && !initialFormula) {
      setFormula(draft);
      setIsDrafted(true);
    }
  }, [initialFormula]);

  // Save draft on formula change
  useEffect(() => {
    if (formula) {
      localStorage.setItem(DRAFT_STORAGE_KEY, formula);
      setIsDrafted(true);
    }
  }, [formula]);

  // Evaluate formula in real-time (debounced to avoid firing on every keystroke)
  useEffect(() => {
    const timer = setTimeout(() => {
      evaluateFormula(formula);
    }, 300);
    return () => clearTimeout(timer);
  }, [formula, variables]);

  const evaluateFormula = async (formulaText: string) => {
    if (!formulaText.trim()) {
      setResult(null);
      return;
    }

    try {
      // Create variable map for evaluation
      const variableMap: Record<string, number> = {};
      variables.forEach((v) => {
        variableMap[v.name] = v.value;
      });

      // Call backend API to evaluate formula
      const response = await fetch('/api/bendcore/evaluate-formula', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          formula: formulaText,
          variables: variableMap,
        }),
      });

      if (!response || !response.ok) {
        let errorMsg = 'Evaluation failed';
        let suggestions: string[] = [];
        if (response) {
          try {
            const error = await response.json();
            errorMsg = error.message || errorMsg;
            suggestions = error.suggestions || [];
          } catch {}
        }
        setResult({ success: false, error: errorMsg, suggestions });
        return;
      }

      const data = await response.json();
      setResult({
        success: true,
        result: data.result,
      });
    } catch (error) {
      setResult({
        success: false,
        error: error instanceof Error ? error.message : 'Unknown error',
        suggestions: [
          'Check variable names (use $OD, $Wall, etc.)',
          'Ensure operators are properly spaced',
          'Verify function syntax: sqrt(), abs(), min(), max()',
        ],
      });
    }
  };

  const handleInsertVariable = (varName: string) => {
    if (inputRef.current) {
      const textarea = inputRef.current;
      const start = textarea.selectionStart;
      const end = textarea.selectionEnd;
      const newFormula =
        formula.substring(0, start) +
        varName +
        formula.substring(end);
      setFormula(newFormula);

      // Move cursor after inserted variable
      setTimeout(() => {
        textarea.selectionStart = textarea.selectionEnd = start + varName.length;
        textarea.focus();
      }, 0);
    }
  };

  const handleInsertFunction = (funcName: string) => {
    handleInsertVariable(funcName);
  };

  const handleInsertOperator = (op: string) => {
    if (inputRef.current) {
      const textarea = inputRef.current;
      const start = textarea.selectionStart;
      const space = ' ';
      const newFormula =
        formula.substring(0, start) +
        space +
        op +
        space +
        formula.substring(start);
      setFormula(newFormula);

      setTimeout(() => {
        textarea.focus();
      }, 0);
    }
  };

  const handleApply = () => {
    if (result?.success && result.result !== undefined) {
      onApply(formula, result.result);
      localStorage.removeItem(DRAFT_STORAGE_KEY);
      setIsDrafted(false);
    }
  };

  const handleCancel = () => {
    localStorage.removeItem(DRAFT_STORAGE_KEY);
    setIsDrafted(false);
    onCancel();
  };

  const handleKeyDown = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.ctrlKey && e.key === 'Enter') {
      e.preventDefault();
      handleApply();
    } else if (e.key === 'Escape') {
      e.preventDefault();
      handleCancel();
    }
  };

  const containerClass = `formula-editor ${darkMode ? 'dark-mode' : 'light-mode'}`;

  return (
    <div className={containerClass}>
      <div className="formula-editor__header">
        <h3>Parametric Dimension Formula</h3>
        {isDrafted && <span className="draft-indicator">Draft</span>}
      </div>

      <div className="formula-editor__input-section">
        <label>Formula Expression</label>
        <textarea
          ref={inputRef}
          className={`formula-editor__input ${
            result?.success ? 'valid' : result?.error ? 'error' : ''
          }`}
          value={formula}
          onChange={(e) => setFormula(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder="e.g., $OD * 2 + 5"
          rows={3}
        />
        <div className="formula-editor__help-text">
          Ctrl+Enter to apply, Esc to cancel
        </div>
      </div>

      {/* Variable Palette */}
      <div className="formula-editor__variables">
        <label>Variables</label>
        <div className="variable-list">
          {variables.map((v) => (
            <button
              key={v.name}
              className="variable-button"
              onClick={() => handleInsertVariable(v.name)}
              title={`${v.description}: ${v.value}${v.unit}`}
              data-value={`${v.value}${v.unit}`}
            >
              {v.name}
            </button>
          ))}
        </div>
      </div>

      {/* Operators and Functions */}
      <div className="formula-editor__tools">
        <div className="tool-section">
          <label>Operators</label>
          <div className="tool-buttons">
            {OPERATORS.map((op) => (
              <button
                key={op.symbol}
                className="operator-button"
                onClick={() => handleInsertOperator(op.symbol)}
                title={op.name}
              >
                {op.symbol}
              </button>
            ))}
          </div>
        </div>

        <div className="tool-section">
          <label>Functions</label>
          <div className="tool-buttons">
            {COMMON_FUNCTIONS.map((func) => (
              <button
                key={func.name}
                className="function-button"
                onClick={() => handleInsertFunction(func.name)}
                title={func.description}
              >
                {func.name}
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* Preview and Errors */}
      {formula && (
        <div className="formula-editor__preview">
          {result?.success && result.result !== undefined ? (
            <div className="preview-success">
              <span className="preview-label">Result:</span>
              <span className="preview-value">{result.result.toFixed(3)}</span>
            </div>
          ) : result?.error ? (
            <div className="preview-error">
              <div className="error-message">⚠️ {result.error}</div>
              {result.suggestions && result.suggestions.length > 0 && (
                <div className="error-suggestions">
                  <p>Suggestions:</p>
                  <ul>
                    {result.suggestions.map((s, i) => (
                      <li key={i}>{s}</li>
                    ))}
                  </ul>
                </div>
              )}
            </div>
          ) : null}
        </div>
      )}

      {/* Action Buttons */}
      <div className="formula-editor__actions">
        <button
          className="action-button apply"
          onClick={handleApply}
          disabled={!result?.success}
          title="Apply formula (Ctrl+Enter)"
        >
          Apply
        </button>
        <button
          className="action-button cancel"
          onClick={handleCancel}
          title="Cancel (Esc)"
        >
          Cancel
        </button>
      </div>
    </div>
  );
};

export default FormulaEditor;
