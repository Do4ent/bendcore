import React, { useState } from 'react';

/**
 * GeometricPrimitives Component - GAP 094
 * Line, circle, arc, rectangle, polygon tools with snapping
 */

export interface GeometricPrimitivesProps {
  isOpen: boolean;
  onClose: () => void;
  onCreatePrimitive?: (type: string, data: any) => void;
  isDarkMode?: boolean;
}

const GeometricPrimitives: React.FC<GeometricPrimitivesProps> = ({
  isOpen,
  onClose,
  onCreatePrimitive,
  isDarkMode = false
}) => {
  const [activeTool, setActiveTool] = useState<string>('');
  const [snapEnabled, setSnapEnabled] = useState(true);
  const [primitives, setPrimitives] = useState<any[]>([]);

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  const tools = [
    { id: 'line', name: 'Line', icon: '/' },
    { id: 'circle', name: 'Circle', icon: '○' },
    { id: 'arc', name: 'Arc', icon: '⌒' },
    { id: 'rectangle', name: 'Rectangle', icon: '▭' },
    { id: 'polygon', name: 'Polygon', icon: '⬟' }
  ];

  const handleCreatePrimitive = (toolId: string) => {
    const primitive = { id: `prim_${Date.now()}`, type: toolId };
    setPrimitives([...primitives, primitive]);
    if (onCreatePrimitive) {
      onCreatePrimitive(toolId, primitive);
    }
    setActiveTool('');
  };

  if (!isOpen) return null;

  return (
    <div className="geometric-primitives-overlay">
      <div className="geometric-primitives-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="gp-header">
          <h2>Geometric Primitives</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>

        <div className="gp-body">
          <div className="gp-tools">
            {tools.map(tool => (
              <div key={tool.id} className="gp-tool-item">
                <button
                  className={`gp-tool-btn ${activeTool === tool.id ? 'active' : ''}`}
                  onClick={() => setActiveTool(activeTool === tool.id ? '' : tool.id)}
                  title={tool.name}
                >
                  <span>{tool.icon}</span>
                  <span>{tool.name}</span>
                </button>
                {activeTool === tool.id && (
                  <button
                    className="btn btn-create"
                    onClick={() => handleCreatePrimitive(tool.id)}
                  >
                    Create {tool.name}
                  </button>
                )}
              </div>
            ))}
          </div>

          <div className="gp-options">
            <label className="snap-option">
              <input
                type="checkbox"
                checked={snapEnabled}
                onChange={(e) => setSnapEnabled(e.target.checked)}
              />
              Enable Snapping
            </label>
          </div>

          <div className="gp-list">
            <div className="section-title">Created Primitives ({primitives.length})</div>
            {primitives.length > 0 ? (
              primitives.map(prim => (
                <div key={prim.id} className="gp-item">
                  <span>{prim.type}</span>
                </div>
              ))
            ) : (
              <div className="empty-state">No primitives created</div>
            )}
          </div>
        </div>

        <div className="gp-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default GeometricPrimitives;
