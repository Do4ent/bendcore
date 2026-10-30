import React, { useState } from 'react';

export interface AssemblyConstraintEditorProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const AssemblyConstraintEditor: React.FC<AssemblyConstraintEditorProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [constraints, setConstraints] = useState<any[]>([]);
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  if (!isOpen) return null;

  return (
    <div className="assembly-constraint-overlay">
      <div className="assembly-constraint-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="ace-header">
          <h2>Assembly Constraints</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="ace-body">
          <div className="constraints-list">
            {constraints.length === 0 && <div className="empty-state">No constraints</div>}
          </div>
        </div>
        <div className="ace-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default AssemblyConstraintEditor;
