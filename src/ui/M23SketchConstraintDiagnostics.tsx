import React, { useState } from 'react';

export interface SketchConstraintDiagnosticsProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const SketchConstraintDiagnostics: React.FC<SketchConstraintDiagnosticsProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [diagnostics, setDiagnostics] = useState<Array<{ id: string; type: string; message: string }>>([]);

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  if (!isOpen) return null;

  return (
    <div className="diagnostics-overlay">
      <div className="diagnostics-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="diag-header">
          <h2>Constraint Diagnostics</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="diag-body">
          <div className="diagnostics-list">
            {diagnostics.length > 0 ? (
              diagnostics.map(d => (
                <div key={d.id} className="diagnostic-item">
                  <span className="diag-type">{d.type}</span>
                  <span className="diag-message">{d.message}</span>
                </div>
              ))
            ) : (
              <div className="empty-state">No issues detected</div>
            )}
          </div>
        </div>
        <div className="diag-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default SketchConstraintDiagnostics;
