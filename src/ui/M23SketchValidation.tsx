import React, { useState } from 'react';

export interface SketchValidationProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const SketchValidation: React.FC<SketchValidationProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [errors, setErrors] = useState<Array<{ id: string; message: string }>>([]);
  const [warnings, setWarnings] = useState<Array<{ id: string; message: string }>>([]);

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  if (!isOpen) return null;

  return (
    <div className="validation-overlay">
      <div className="validation-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="val-header">
          <h2>Sketch Validation</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="val-body">
          <div className="errors-section">
            <div className="section-title">Errors ({errors.length})</div>
            {errors.length === 0 && <div className="empty-state">No errors</div>}
          </div>
          <div className="warnings-section">
            <div className="section-title">Warnings ({warnings.length})</div>
            {warnings.length === 0 && <div className="empty-state">No warnings</div>}
          </div>
        </div>
        <div className="val-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default SketchValidation;
