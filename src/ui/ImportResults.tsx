// Import Results Component (Q3 2027, Task 1, Subtask 8)
import React from 'react';

interface ImportResultsProps {
  formulas: any[];
  warnings: string[];
  onApply: () => void;
  onCancel: () => void;
}

const ImportResults: React.FC<ImportResultsProps> = ({ formulas, warnings, onApply, onCancel }) => {
  return (
    <div className="import-results-container">
      <h3>Import Results</h3>
      <div className="formulas-list">
        {formulas.map((formula, idx) => (
          <div key={idx} className="formula-item">
            <span className="formula-name">{formula.id}</span>
            <span className="formula-value">{formula.formula_text}</span>
          </div>
        ))}
      </div>
      {warnings.length > 0 && (
        <div className="warnings-list">
          <h4>Warnings:</h4>
          {warnings.map((warning, idx) => (
            <p key={idx} className="warning-item">{warning}</p>
          ))}
        </div>
      )}
      <div className="results-actions">
        <button className="button-primary" onClick={onApply}>Apply</button>
        <button className="button-secondary" onClick={onCancel}>Cancel</button>
      </div>
    </div>
  );
};

export default ImportResults;
