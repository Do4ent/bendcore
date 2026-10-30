import React, { useState } from 'react';

export interface SketchLayerPanelProps {
  isOpen: boolean;
  onClose: () => void;
  isDarkMode?: boolean;
}

const SketchLayerPanel: React.FC<SketchLayerPanelProps> = ({
  isOpen,
  onClose,
  isDarkMode = false
}) => {
  const [layers, setLayers] = useState<Array<{ id: string; name: string; visible: boolean }>>([
    { id: 'layer1', name: 'Layer 1', visible: true }
  ]);

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const textColor = isDarkMode ? '#ffffff' : '#000000';

  const toggleVisibility = (id: string) => {
    setLayers(layers.map(l => l.id === id ? { ...l, visible: !l.visible } : l));
  };

  if (!isOpen) return null;

  return (
    <div className="layer-panel-overlay">
      <div className="layer-panel-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="layer-header">
          <h2>Layers</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>
        <div className="layer-body">
          <div className="layer-list">
            {layers.map(layer => (
              <div key={layer.id} className="layer-item">
                <input
                  type="checkbox"
                  checked={layer.visible}
                  onChange={() => toggleVisibility(layer.id)}
                />
                <span>{layer.name}</span>
              </div>
            ))}
          </div>
        </div>
        <div className="layer-footer">
          <button className="btn btn-cancel" onClick={onClose}>Close</button>
        </div>
      </div>
    </div>
  );
};

export default SketchLayerPanel;
