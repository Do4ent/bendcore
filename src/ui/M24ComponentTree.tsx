import React, { useState } from 'react';

/**
 * ComponentTree Component - GAP 0106
 * Assembly component tree with hierarchy
 */

export interface ComponentTreeProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const ComponentTree: React.FC<ComponentTreeProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [components, setComponents] = useState<Array<{ id: string; name: string; children: any[] }>>([]);

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  if (!isOpen) return null;

  return (
    <div className="component-tree-overlay">
      <div className="component-tree-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="ct-header">
          <h2>Component Tree</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="ct-body">
          <div className="tree-view">
            {components.length === 0 && <div className="empty-state">No components</div>}
          </div>
        </div>
        <div className="ct-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default ComponentTree;
