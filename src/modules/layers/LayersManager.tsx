import React, { useState } from 'react';
import './LayersManager.css';

export interface Layer {
  id: string;
  name: string;
  visible: boolean;
  locked: boolean;
  opacity: number;
  color: string;
  elementCount: number;
}

interface LayersManagerProps {
  layers: Layer[];
  selectedLayerId?: string;
  onSelectLayer?: (layerId: string) => void;
  onToggleVisibility?: (layerId: string) => void;
  onToggleLock?: (layerId: string) => void;
  onAddLayer?: () => void;
  onDeleteLayer?: (layerId: string) => void;
  onRenameLayer?: (layerId: string, newName: string) => void;
  onChangeOpacity?: (layerId: string, opacity: number) => void;
}

export const LayersManager: React.FC<LayersManagerProps> = ({
  layers,
  selectedLayerId,
  onSelectLayer,
  onToggleVisibility,
  onToggleLock,
  onAddLayer,
  onDeleteLayer,
  onRenameLayer,
  onChangeOpacity,
}) => {
  const [renamingId, setRenamingId] = useState<string | null>(null);
  const [newName, setNewName] = useState('');

  const handleStartRename = (layer: Layer) => {
    setRenamingId(layer.id);
    setNewName(layer.name);
  };

  const handleFinishRename = (layerId: string) => {
    if (newName && newName !== layers.find((l) => l.id === layerId)?.name) {
      onRenameLayer?.(layerId, newName);
    }
    setRenamingId(null);
  };

  return (
    <div className="layers-manager">
      {/* Header */}
      <div className="layers-header">
        <h3>Layers</h3>
        <button
          className="add-layer-btn"
          onClick={onAddLayer}
          title="Add new layer"
        >
          +
        </button>
      </div>

      {/* Layers List */}
      <div className="layers-list">
        {layers.length > 0 ? (
          layers.map((layer) => (
            <div
              key={layer.id}
              className={`layer-item ${
                selectedLayerId === layer.id ? 'selected' : ''
              }`}
              onClick={() => onSelectLayer?.(layer.id)}
            >
              {/* Visibility Toggle */}
              <button
                className="layer-visibility"
                onClick={(e) => {
                  e.stopPropagation();
                  onToggleVisibility?.(layer.id);
                }}
                title={layer.visible ? 'Hide' : 'Show'}
              >
                {layer.visible ? '👁' : '👁‍🗨'}
              </button>

              {/* Layer Color */}
              <div
                className="layer-color"
                style={{ backgroundColor: layer.color }}
              />

              {/* Layer Info */}
              <div className="layer-info">
                {renamingId === layer.id ? (
                  <input
                    type="text"
                    className="layer-input"
                    value={newName}
                    onChange={(e) => setNewName(e.target.value)}
                    onBlur={() => handleFinishRename(layer.id)}
                    onKeyDown={(e) => {
                      if (e.key === 'Enter') {
                        handleFinishRename(layer.id);
                      } else if (e.key === 'Escape') {
                        setRenamingId(null);
                      }
                    }}
                    autoFocus
                  />
                ) : (
                  <>
                    <div className="layer-name" onDoubleClick={() => handleStartRename(layer)}>
                      {layer.name}
                    </div>
                    <div className="layer-meta">
                      {layer.elementCount > 0 && (
                        <span className="badge">{layer.elementCount} items</span>
                      )}
                    </div>
                  </>
                )}
              </div>

              {/* Opacity */}
              <div className="layer-opacity">
                <input
                  type="range"
                  min="0"
                  max="100"
                  value={layer.opacity}
                  onChange={(e) =>
                    onChangeOpacity?.(layer.id, parseInt(e.target.value))
                  }
                  className="opacity-slider"
                  onClick={(e) => e.stopPropagation()}
                />
              </div>

              {/* Lock Toggle */}
              <button
                className="layer-lock"
                onClick={(e) => {
                  e.stopPropagation();
                  onToggleLock?.(layer.id);
                }}
                title={layer.locked ? 'Unlock' : 'Lock'}
              >
                {layer.locked ? '🔒' : '🔓'}
              </button>

              {/* Delete */}
              <button
                className="layer-delete"
                onClick={(e) => {
                  e.stopPropagation();
                  onDeleteLayer?.(layer.id);
                }}
                title="Delete layer"
              >
                ×
              </button>
            </div>
          ))
        ) : (
          <div className="empty-layers">No layers</div>
        )}
      </div>

      {/* Layer Count */}
      <div className="layers-footer">
        <span className="layer-count">{layers.length} layers</span>
      </div>
    </div>
  );
};

// Hook для управления слоями
export const useLayersManager = (initialLayers: Layer[] = []) => {
  const [layers, setLayers] = useState<Layer[]>(
    initialLayers.length > 0
      ? initialLayers
      : [
          {
            id: 'layer-1',
            name: 'Default',
            visible: true,
            locked: false,
            opacity: 100,
            color: '#0066cc',
            elementCount: 0,
          },
        ]
  );

  const [selectedLayerId, setSelectedLayerId] = useState<string>(
    layers[0]?.id || ''
  );

  const addLayer = () => {
    const newLayer: Layer = {
      id: `layer-${Date.now()}`,
      name: `Layer ${layers.length + 1}`,
      visible: true,
      locked: false,
      opacity: 100,
      color: `hsl(${Math.random() * 360}, 70%, 50%)`,
      elementCount: 0,
    };
    setLayers([...layers, newLayer]);
    setSelectedLayerId(newLayer.id);
  };

  const deleteLayer = (layerId: string) => {
    setLayers((prev) => prev.filter((l) => l.id !== layerId));
    if (selectedLayerId === layerId && layers.length > 1) {
      setSelectedLayerId(layers.find((l) => l.id !== layerId)?.id || '');
    }
  };

  const toggleVisibility = (layerId: string) => {
    setLayers((prev) =>
      prev.map((l) =>
        l.id === layerId ? { ...l, visible: !l.visible } : l
      )
    );
  };

  const toggleLock = (layerId: string) => {
    setLayers((prev) =>
      prev.map((l) =>
        l.id === layerId ? { ...l, locked: !l.locked } : l
      )
    );
  };

  const renameLayer = (layerId: string, newName: string) => {
    setLayers((prev) =>
      prev.map((l) => (l.id === layerId ? { ...l, name: newName } : l))
    );
  };

  const changeOpacity = (layerId: string, opacity: number) => {
    setLayers((prev) =>
      prev.map((l) =>
        l.id === layerId ? { ...l, opacity: Math.max(0, Math.min(100, opacity)) } : l
      )
    );
  };

  return {
    layers,
    selectedLayerId,
    setSelectedLayerId,
    addLayer,
    deleteLayer,
    toggleVisibility,
    toggleLock,
    renameLayer,
    changeOpacity,
  };
};
