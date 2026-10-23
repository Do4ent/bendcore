/**
 * LayersPanel компонент (Q2 2027, Task 3)
 * Управление видимостью слоёв на чертежах:
 * - Список всех слоёв с чекбоксами видимости
 * - Иконки для каждого типа слоя
 * - Toggle все/скрыть все
 * - Количество элементов в каждом слое
 * - Светлый/тёмный режим поддержки
 */

import React, { useCallback, useMemo } from 'react';
import type { Layer, LayerType, LayerVisibility } from './core/types';
import { CoreClient } from './core/coreClient';
import './panels.css';

export interface LayersPanelProps {
  layers?: Layer[];
  layerVisibility?: LayerVisibility;
  onToggleLayer?: (layerType: number, visible: boolean) => void;
  onShowAll?: () => void;
  onHideAll?: () => void;
  client?: CoreClient;
  darkMode?: boolean;
}

interface LayerInfo {
  type: number;
  name: string;
  icon: string;
  color: string;
}

const LAYER_CONFIGS: Record<number, LayerInfo> = {
  0: {
    type: 0,
    name: 'Контур',
    icon: '◯',
    color: '#000000',
  },
  1: {
    type: 1,
    name: 'Размеры',
    icon: '↔',
    color: '#0000FF',
  },
  2: {
    type: 2,
    name: 'Оси',
    icon: '✚',
    color: '#FF0000',
  },
  3: {
    type: 3,
    name: 'Разметка',
    icon: '◆',
    color: '#008000',
  },
  4: {
    type: 4,
    name: 'Вспомогательное',
    icon: '·',
    color: '#A9A9A9',
  },
};

export const LayersPanel: React.FC<LayersPanelProps> = ({
  layers = [],
  layerVisibility = { layer_visibility: {} },
  onToggleLayer,
  onShowAll,
  onHideAll,
  client,
  darkMode = false,
}) => {
  const core = useMemo(() => client ?? new CoreClient(), [client]);

  // Получить состояние видимости для слоя
  const isLayerVisible = useCallback(
    (layerType: number): boolean => {
      const vis = layerVisibility.layer_visibility?.[layerType];
      return vis !== undefined ? vis : true;
    },
    [layerVisibility],
  );

  // Обработать клик по чекбоксу видимости
  const handleLayerToggle = useCallback(
    async (layerType: number, currentlyVisible: boolean) => {
      const newVisible = !currentlyVisible;

      // Вызвать API
      try {
        await core.call('drawing.set_layer_visibility', {
          layer_type: layerType,
          visible: newVisible,
        });
      } catch (error) {
        console.error('Error toggling layer visibility:', error);
      }

      // Уведомить родителя
      if (onToggleLayer) {
        onToggleLayer(layerType, newVisible);
      }
    },
    [core, onToggleLayer],
  );

  // Показать все слои
  const handleShowAll = useCallback(async () => {
    try {
      await core.call('drawing.show_all_layers', {});
    } catch (error) {
      console.error('Error showing all layers:', error);
    }
    if (onShowAll) {
      onShowAll();
    }
  }, [core, onShowAll]);

  // Скрыть все слои
  const handleHideAll = useCallback(async () => {
    try {
      await core.call('drawing.hide_all_layers', {});
    } catch (error) {
      console.error('Error hiding all layers:', error);
    }
    if (onHideAll) {
      onHideAll();
    }
  }, [core, onHideAll]);

  // Получить всех слои, используя конфиг если они не пришли
  const allLayers = useMemo(() => {
    if (layers && layers.length > 0) {
      return layers;
    }
    // Создать по умолчанию, если не пришли
    return Object.values(LAYER_CONFIGS).map((config) => ({
      type: config.type,
      name: config.name,
      visible: true,
      locked: false,
      color: config.color,
      element_count: 0,
    }));
  }, [layers]);

  return (
    <div className={`layers-panel ${darkMode ? 'dark-mode' : 'light-mode'}`}>
      <div className="panel-header">
        <h3>Слои</h3>
        <div className="layer-controls">
          <button
            title="Показать все слои"
            onClick={handleShowAll}
            className="layer-btn"
          >
            👁️
          </button>
          <button
            title="Скрыть все слои"
            onClick={handleHideAll}
            className="layer-btn"
          >
            🚫
          </button>
        </div>
      </div>

      <div className="layers-list">
        {allLayers.map((layer) => {
          const visible = isLayerVisible(layer.type);
          const config = LAYER_CONFIGS[layer.type];

          return (
            <div key={layer.type} className="layer-item">
              <label className="layer-label">
                <input
                  type="checkbox"
                  checked={visible}
                  onChange={() => handleLayerToggle(layer.type, visible)}
                  className="layer-checkbox"
                />
                <span
                  className="layer-icon"
                  style={{ color: layer.color }}
                  title={layer.name}
                >
                  {config?.icon || '◾'}
                </span>
                <span className="layer-name">{layer.name}</span>
              </label>
              {layer.element_count > 0 && (
                <span className="element-count">
                  {layer.element_count}
                </span>
              )}
            </div>
          );
        })}
      </div>

      <style>{`
        .layers-panel {
          display: flex;
          flex-direction: column;
          width: 220px;
          border: 1px solid #ddd;
          border-radius: 4px;
          overflow: hidden;
          font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
          font-size: 13px;
        }

        .layers-panel.dark-mode {
          background: #2d2d30;
          color: #cccccc;
          border-color: #3e3e42;
        }

        .layers-panel.light-mode {
          background: #ffffff;
          color: #333333;
        }

        .panel-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 10px 12px;
          border-bottom: 1px solid #ddd;
          background: #f5f5f5;
        }

        .layers-panel.dark-mode .panel-header {
          background: #3c3c42;
          border-color: #3e3e42;
        }

        .panel-header h3 {
          margin: 0;
          font-size: 13px;
          font-weight: 600;
        }

        .layer-controls {
          display: flex;
          gap: 4px;
        }

        .layer-btn {
          background: transparent;
          border: 1px solid transparent;
          padding: 4px 6px;
          cursor: pointer;
          border-radius: 3px;
          font-size: 12px;
          transition: background 0.2s;
        }

        .layer-btn:hover {
          background: #e0e0e0;
        }

        .layers-panel.dark-mode .layer-btn:hover {
          background: #505050;
        }

        .layers-list {
          flex: 1;
          overflow-y: auto;
          padding: 8px;
          display: flex;
          flex-direction: column;
          gap: 2px;
        }

        .layer-item {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 6px 8px;
          border-radius: 3px;
          transition: background 0.2s;
        }

        .layer-item:hover {
          background: #f0f0f0;
        }

        .layers-panel.dark-mode .layer-item:hover {
          background: #3e3e42;
        }

        .layer-label {
          display: flex;
          align-items: center;
          gap: 8px;
          cursor: pointer;
          flex: 1;
          user-select: none;
        }

        .layer-checkbox {
          cursor: pointer;
          width: 14px;
          height: 14px;
          margin: 0;
        }

        .layer-icon {
          font-weight: bold;
          font-size: 14px;
          display: inline-flex;
          align-items: center;
          justify-content: center;
          width: 16px;
        }

        .layer-name {
          flex: 1;
          font-size: 12px;
        }

        .element-count {
          font-size: 11px;
          color: #888;
          background: #f0f0f0;
          padding: 2px 6px;
          border-radius: 2px;
          margin-left: 4px;
        }

        .layers-panel.dark-mode .element-count {
          background: #3e3e42;
          color: #999;
        }
      `}</style>
    </div>
  );
};

export default LayersPanel;
