import React, { useState, useCallback } from 'react';

/**
 * ConstraintTools Component - GAP 092
 * Geometric constraint tools for sketch elements
 * Supports: coincident, parallel, perpendicular, distance, angle, tangent, equality, symmetry, horizontal, vertical
 */

export interface ConstraintToolsProps {
  isOpen: boolean;
  onClose: () => void;
  onApplyConstraint?: (type: string, elements: string[], value?: number) => void;
  selectedElements?: string[];
  isDarkMode?: boolean;
}

const ConstraintTools: React.FC<ConstraintToolsProps> = ({
  isOpen,
  onClose,
  onApplyConstraint,
  selectedElements = [],
  isDarkMode = false
}) => {
  const [activeConstraint, setActiveConstraint] = useState<string>('');
  const [constraintValue, setConstraintValue] = useState<number>(0);
  const [constraints, setConstraints] = useState<Array<{ id: string; type: string; value?: number }>>([]);
  const [feedbackMessage, setFeedbackMessage] = useState<string>('');

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';
  const borderColor = isDarkMode ? '#444444' : '#cccccc';
  const accentColor = '#4caf50';

  const constraintTypes = [
    { id: 'coincident', name: 'Coincident', icon: '●', description: 'Make points coincident' },
    { id: 'parallel', name: 'Parallel', icon: '∥', description: 'Make lines parallel' },
    { id: 'perpendicular', name: 'Perpendicular', icon: '⊥', description: 'Make lines perpendicular' },
    { id: 'distance', name: 'Distance', icon: '📏', description: 'Set distance between elements', hasValue: true },
    { id: 'angle', name: 'Angle', icon: '∠', description: 'Set angle between lines', hasValue: true },
    { id: 'tangent', name: 'Tangent', icon: '⌒', description: 'Make elements tangent' },
    { id: 'equality', name: 'Equality', icon: '=', description: 'Make elements equal' },
    { id: 'symmetry', name: 'Symmetry', icon: '◆', description: 'Create symmetry constraint' },
    { id: 'horizontal', name: 'Horizontal', icon: '—', description: 'Make line horizontal' },
    { id: 'vertical', name: 'Vertical', icon: '|', description: 'Make line vertical' }
  ];

  /**
   * Apply constraint to selected elements
   */
  const handleApplyConstraint = useCallback((constraintId: string) => {
    if (selectedElements.length === 0) {
      setFeedbackMessage('Select elements first');
      setTimeout(() => setFeedbackMessage(''), 3000);
      return;
    }

    const constraintType = constraintTypes.find(c => c.id === constraintId);
    if (!constraintType) return;

    const newConstraint = {
      id: `constraint_${Date.now()}`,
      type: constraintId,
      value: constraintType.hasValue ? constraintValue : undefined
    };

    setConstraints([...constraints, newConstraint]);
    setActiveConstraint('');
    setConstraintValue(0);

    if (onApplyConstraint) {
      onApplyConstraint(constraintId, selectedElements, newConstraint.value);
    }

    setFeedbackMessage(`${constraintType.name} constraint applied`);
    setTimeout(() => setFeedbackMessage(''), 3000);
  }, [selectedElements, constraintTypes, constraintValue, onApplyConstraint]);

  /**
   * Remove constraint
   */
  const handleRemoveConstraint = (constraintId: string) => {
    setConstraints(constraints.filter(c => c.id !== constraintId));
    setFeedbackMessage('Constraint removed');
    setTimeout(() => setFeedbackMessage(''), 3000);
  };

  /**
   * Edit constraint value
   */
  const handleEditValue = (constraintId: string, newValue: number) => {
    setConstraints(constraints.map(c =>
      c.id === constraintId ? { ...c, value: newValue } : c
    ));
  };

  /**
   * Clear all constraints
   */
  const handleClearAll = () => {
    setConstraints([]);
    setFeedbackMessage('All constraints cleared');
    setTimeout(() => setFeedbackMessage(''), 3000);
  };

  if (!isOpen) return null;

  return (
    <div className="constraint-tools-overlay">
      <div className="constraint-tools-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="constraint-header">
          <h2>Constraint Tools</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>

        <div className="constraint-body">
          <div className="constraint-section">
            <div className="section-title">Available Constraints</div>
            <div className="constraint-grid">
              {constraintTypes.map(constraint => (
                <div key={constraint.id} className="constraint-group">
                  <button
                    className={`constraint-btn ${activeConstraint === constraint.id ? 'active' : ''}`}
                    onClick={() => setActiveConstraint(activeConstraint === constraint.id ? '' : constraint.id)}
                    title={constraint.description}
                    style={{
                      backgroundColor: activeConstraint === constraint.id ? accentColor : 'transparent',
                      borderColor: borderColor
                    }}
                  >
                    <span className="constraint-icon">{constraint.icon}</span>
                    <span className="constraint-name">{constraint.name}</span>
                  </button>

                  {activeConstraint === constraint.id && (
                    <div className="constraint-controls" style={{ borderColor: borderColor }}>
                      <div className="constraint-description">{constraint.description}</div>

                      {constraint.hasValue && (
                        <div className="constraint-input-group">
                          <label>Value:</label>
                          <input
                            type="number"
                            value={constraintValue}
                            onChange={(e) => setConstraintValue(parseFloat(e.target.value))}
                            placeholder="Enter value"
                            style={{ backgroundColor: isDarkMode ? '#3a3a3a' : '#f5f5f5', color: textColor, borderColor }}
                          />
                        </div>
                      )}

                      <button
                        className="btn btn-apply"
                        onClick={() => handleApplyConstraint(constraint.id)}
                        style={{ backgroundColor: accentColor }}
                      >
                        Apply Constraint
                      </button>
                    </div>
                  )}
                </div>
              ))}
            </div>
          </div>

          <div className="constraint-section">
            <div className="section-title">Applied Constraints ({constraints.length})</div>
            <div className="constraint-list">
              {constraints.length > 0 ? (
                constraints.map(constraint => {
                  const type = constraintTypes.find(c => c.id === constraint.type);
                  return (
                    <div
                      key={constraint.id}
                      className="constraint-item"
                      style={{
                        backgroundColor: isDarkMode ? '#333333' : '#f9f9f9',
                        borderColor: borderColor
                      }}
                    >
                      <div className="constraint-item-header">
                        <span className="constraint-item-icon">{type?.icon}</span>
                        <span className="constraint-item-name">{type?.name}</span>
                        {constraint.value !== undefined && (
                          <span className="constraint-item-value">{constraint.value.toFixed(2)}</span>
                        )}
                      </div>

                      {constraint.value !== undefined && (
                        <div className="constraint-item-edit">
                          <input
                            type="number"
                            value={constraint.value}
                            onChange={(e) => handleEditValue(constraint.id, parseFloat(e.target.value))}
                            style={{
                              backgroundColor: isDarkMode ? '#3a3a3a' : '#ffffff',
                              color: textColor,
                              borderColor
                            }}
                          />
                        </div>
                      )}

                      <button
                        className="constraint-item-delete"
                        onClick={() => handleRemoveConstraint(constraint.id)}
                        title="Delete constraint"
                      >
                        ×
                      </button>
                    </div>
                  );
                })
              ) : (
                <div className="empty-state">No constraints applied yet</div>
              )}
            </div>
          </div>

          {feedbackMessage && (
            <div className="constraint-feedback" style={{ backgroundColor: accentColor }}>
              {feedbackMessage}
            </div>
          )}
        </div>

        <div className="constraint-footer">
          {constraints.length > 0 && (
            <button className="btn btn-danger" onClick={handleClearAll}>
              Clear All
            </button>
          )}
          <button className="btn btn-cancel" onClick={onClose}>
            Close
          </button>
        </div>
      </div>
    </div>
  );
};

export default ConstraintTools;
