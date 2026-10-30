import React, { useState } from 'react';

/**
 * AssemblyUI Component - GAP 0105
 * Main assembly workbench interface
 */

export interface AssemblyUIProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const AssemblyUI: React.FC<AssemblyUIProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [components, setComponents] = useState<any[]>([]);
  const [selectedComponent, setSelectedComponent] = useState<string | null>(null);

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  if (!isOpen) return null;

  return (
    <div className="assembly-ui-overlay">
      <div className="assembly-ui-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="assembly-header">
          <h2>Assembly Workbench</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="assembly-body">
          <div className="assembly-tree">
            <div className="section-title">Components ({components.length})</div>
            {components.length === 0 && <div className="empty-state">No components</div>}
          </div>
          <div className="assembly-properties">
            <div className="section-title">Properties</div>
            {selectedComponent ? (
              <div className="properties-panel">
                <div>Component: {selectedComponent}</div>
              </div>
            ) : (
              <div className="empty-state">Select a component</div>
            )}
          </div>
        </div>
        <div className="assembly-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default AssemblyUI;
