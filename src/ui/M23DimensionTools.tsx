import React, { useState } from 'react';

/**
 * DimensionTools Component - GAP 093
 * Dimension annotation and editing for sketch elements
 */

export interface DimensionToolsProps {
  isOpen: boolean;
  onClose: () => void;
  onAddDimension?: (type: string, value: number, elements: string[]) => void;
  selectedElements?: string[];
  isDarkMode?: boolean;
}

const DimensionTools: React.FC<DimensionToolsProps> = ({
  isOpen,
  onClose,
  onAddDimension,
  selectedElements = [],
  isDarkMode = false
}) => {
  const [dimensions, setDimensions] = useState<Array<{ id: string; type: string; value: number }>>([]);
  const [activeDimension, setActiveDimension] = useState<string>('');
  const [dimensionValue, setDimensionValue] = useState<number>(0);

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';
  const borderColor = isDarkMode ? '#444444' : '#cccccc';

  const dimensionTypes = [
    { id: 'linear', name: 'Linear Dimension', icon: '⟷' },
    { id: 'radius', name: 'Radius Dimension', icon: 'R' },
    { id: 'diameter', name: 'Diameter Dimension', icon: '⌀' },
    { id: 'angle', name: 'Angular Dimension', icon: '∠' }
  ];

  const handleAddDimension = (typeId: string) => {
    const newDim = {
      id: `dim_${Date.now()}`,
      type: typeId,
      value: dimensionValue
    };
    setDimensions([...dimensions, newDim]);
    if (onAddDimension) {
      onAddDimension(typeId, dimensionValue, selectedElements);
    }
    setDimensionValue(0);
    setActiveDimension('');
  };

  const handleRemoveDimension = (id: string) => {
    setDimensions(dimensions.filter(d => d.id !== id));
  };

  if (!isOpen) return null;

  return (
    <div className="dimension-tools-overlay">
      <div className="dimension-tools-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="dimension-header">
          <h2>Dimension Tools</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>

        <div className="dimension-body">
          <div className="dimension-types">
            {dimensionTypes.map(dim => (
              <div key={dim.id} className="dimension-type">
                <button
                  className={`dimension-btn ${activeDimension === dim.id ? 'active' : ''}`}
                  onClick={() => setActiveDimension(activeDimension === dim.id ? '' : dim.id)}
                  title={dim.name}
                  style={{ borderColor }}
                >
                  <span className="dimension-icon">{dim.icon}</span>
                  <span className="dimension-name">{dim.name}</span>
                </button>

                {activeDimension === dim.id && (
                  <div className="dimension-input-group" style={{ borderColor }}>
                    <label>Value:</label>
                    <input
                      type="number"
                      value={dimensionValue}
                      onChange={(e) => setDimensionValue(parseFloat(e.target.value))}
                      placeholder="Enter value"
                      style={{
                        backgroundColor: isDarkMode ? '#3a3a3a' : '#f5f5f5',
                        color: textColor,
                        borderColor
                      }}
                    />
                    <button
                      className="btn btn-add"
                      onClick={() => handleAddDimension(dim.id)}
                    >
                      Add Dimension
                    </button>
                  </div>
                )}
              </div>
            ))}
          </div>

          <div className="dimensions-list">
            <div className="section-title">Added Dimensions ({dimensions.length})</div>
            {dimensions.length > 0 ? (
              dimensions.map(dim => {
                const type = dimensionTypes.find(d => d.id === dim.type);
                return (
                  <div
                    key={dim.id}
                    className="dimension-item"
                    style={{
                      backgroundColor: isDarkMode ? '#333333' : '#f9f9f9',
                      borderColor
                    }}
                  >
                    <span>{type?.icon} {type?.name}: {dim.value}</span>
                    <button
                      className="dimension-delete"
                      onClick={() => handleRemoveDimension(dim.id)}
                    >
                      ×
                    </button>
                  </div>
                );
              })
            ) : (
              <div className="empty-state">No dimensions added yet</div>
            )}
          </div>
        </div>

        <div className="dimension-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default DimensionTools;
