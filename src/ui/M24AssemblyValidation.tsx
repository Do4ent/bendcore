import React, { useState } from 'react';

export interface AssemblyValidationProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const AssemblyValidation: React.FC<AssemblyValidationProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [issues, setIssues] = useState<any[]>([]);
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  if (!isOpen) return null;

  return (
    <div className="assembly-validation-overlay">
      <div className="assembly-validation-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="av-header">
          <h2>Assembly Validation</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="av-body">
          <div className="validation-issues">
            {issues.length === 0 && <div className="empty-state">No issues detected</div>}
          </div>
        </div>
        <div className="av-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default AssemblyValidation;
