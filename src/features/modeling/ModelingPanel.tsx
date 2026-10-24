/**
 * Панель управления базовым моделированием
 */

import React, { useState } from 'react';
import { ModelingFeature, ModelingState, PadFeature } from './types';
import { Pad } from './Pad';
import { Pocket } from './Pocket';
import { Fillet } from './Fillet';
import { Chamfer } from './Chamfer';
import { Mirror } from './Mirror';
import { Pattern } from './Pattern';

interface ModelingPanelProps {
  onFeatureAdded: (feature: ModelingFeature) => void;
  features: ModelingFeature[];
}

type DialogType = 'pad' | 'pocket' | 'fillet' | 'chamfer' | 'mirror' | 'pattern' | null;

export const ModelingPanel: React.FC<ModelingPanelProps> = ({ onFeatureAdded, features }) => {
  const [activeDialog, setActiveDialog] = useState<DialogType>(null);
  const [selectedFeatures, setSelectedFeatures] = useState<string[]>([]);

  const handlePadApply = (feature: PadFeature) => {
    onFeatureAdded(feature);
    setActiveDialog(null);
  };

  const toggleFeatureSelection = (featureId: string) => {
    setSelectedFeatures(
      selectedFeatures.includes(featureId)
        ? selectedFeatures.filter((f) => f !== featureId)
        : [...selectedFeatures, featureId]
    );
  };

  return (
    <div className="modeling-panel">
      <h2>Базовое моделирование (ЭТАП 1)</h2>

      <div className="modeling-toolbar">
        <button
          className="tool-btn"
          onClick={() => setActiveDialog('pad')}
          title="Выдавливание (Pad)"
        >
          📦 Pad
        </button>
        <button
          className="tool-btn"
          onClick={() => setActiveDialog('pocket')}
          title="Карман (Pocket)"
        >
          🕳️ Pocket
        </button>
        <button
          className="tool-btn"
          onClick={() => setActiveDialog('fillet')}
          title="Скругление (Fillet)"
          disabled={features.length === 0}
        >
          🔘 Fillet
        </button>
        <button
          className="tool-btn"
          onClick={() => setActiveDialog('chamfer')}
          title="Фаска (Chamfer)"
          disabled={features.length === 0}
        >
          ✂️ Chamfer
        </button>
        <button
          className="tool-btn"
          onClick={() => setActiveDialog('mirror')}
          title="Зеркалирование (Mirror)"
          disabled={features.length === 0}
        >
          🪞 Mirror
        </button>
        <button
          className="tool-btn"
          onClick={() => setActiveDialog('pattern')}
          title="Массив (Pattern)"
          disabled={features.length === 0}
        >
          📊 Pattern
        </button>
      </div>

      <div className="features-tree">
        <h3>История функций ({features.length})</h3>
        <div className="features-list">
          {features.length === 0 ? (
            <p className="empty-state">Функций нет. Начните с Pad.</p>
          ) : (
            features.map((feature) => (
              <div
                key={feature.id}
                className={`feature-item ${selectedFeatures.includes(feature.id) ? 'selected' : ''}`}
                onClick={() => toggleFeatureSelection(feature.id)}
              >
                <span className="feature-name">{feature.name}</span>
                <span className="feature-type">({feature.id.split('-')[0]})</span>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Диалоги */}
      {activeDialog === 'pad' && (
        <Pad
          sketchId="sketch-1"
          onApply={handlePadApply}
          onCancel={() => setActiveDialog(null)}
        />
      )}

      {activeDialog === 'pocket' && (
        <Pocket
          sketchId="sketch-1"
          onApply={(feature) => {
            onFeatureAdded(feature);
            setActiveDialog(null);
          }}
          onCancel={() => setActiveDialog(null)}
        />
      )}

      {activeDialog === 'fillet' && (
        <Fillet
          edges={[]}
          onApply={(feature) => {
            onFeatureAdded(feature);
            setActiveDialog(null);
          }}
          onCancel={() => setActiveDialog(null)}
        />
      )}

      {activeDialog === 'chamfer' && (
        <Chamfer
          edges={[]}
          onApply={(feature) => {
            onFeatureAdded(feature);
            setActiveDialog(null);
          }}
          onCancel={() => setActiveDialog(null)}
        />
      )}

      {activeDialog === 'mirror' && (
        <Mirror
          features={selectedFeatures}
          onApply={(feature) => {
            onFeatureAdded(feature);
            setActiveDialog(null);
          }}
          onCancel={() => setActiveDialog(null)}
        />
      )}

      {activeDialog === 'pattern' && features.length > 0 && (
        <Pattern
          featureId={features[features.length - 1].id}
          onApply={(feature) => {
            onFeatureAdded(feature);
            setActiveDialog(null);
          }}
          onCancel={() => setActiveDialog(null)}
        />
      )}

      <style>{`
        .modeling-panel {
          padding: 16px;
          border: 1px solid #ddd;
          border-radius: 8px;
          background: #f9f9f9;
        }

        .modeling-toolbar {
          display: grid;
          grid-template-columns: repeat(3, 1fr);
          gap: 8px;
          margin: 16px 0;
        }

        .tool-btn {
          padding: 8px 12px;
          border: 1px solid #ccc;
          border-radius: 4px;
          background: white;
          cursor: pointer;
          font-size: 12px;
          transition: all 0.2s;
        }

        .tool-btn:hover:not(:disabled) {
          background: #e3f2fd;
          border-color: #1976d2;
        }

        .tool-btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }

        .features-tree {
          margin-top: 16px;
        }

        .features-list {
          background: white;
          border: 1px solid #eee;
          border-radius: 4px;
          max-height: 200px;
          overflow-y: auto;
        }

        .feature-item {
          padding: 8px 12px;
          border-bottom: 1px solid #f0f0f0;
          cursor: pointer;
          transition: background 0.2s;
        }

        .feature-item:hover {
          background: #f5f5f5;
        }

        .feature-item.selected {
          background: #e3f2fd;
          border-left: 3px solid #1976d2;
        }

        .empty-state {
          padding: 12px;
          color: #999;
          text-align: center;
        }

        .feature-dialog {
          margin-top: 16px;
          padding: 12px;
          background: white;
          border: 1px solid #ddd;
          border-radius: 4px;
        }

        .feature-dialog h3 {
          margin-top: 0;
          color: #333;
        }

        .param-group {
          margin: 12px 0;
        }

        .param-group label {
          display: block;
          font-weight: 500;
          margin-bottom: 4px;
          font-size: 12px;
        }

        .param-group input[type="text"],
        .param-group input[type="number"],
        .param-group select {
          width: 100%;
          padding: 6px;
          border: 1px solid #ddd;
          border-radius: 3px;
          font-size: 12px;
        }

        .param-group input[type="range"] {
          width: 100%;
          margin-bottom: 4px;
        }

        .dialog-buttons {
          display: flex;
          gap: 8px;
          margin-top: 16px;
        }

        .btn-primary,
        .btn-secondary {
          flex: 1;
          padding: 8px;
          border: none;
          border-radius: 4px;
          cursor: pointer;
          font-weight: 500;
          font-size: 12px;
        }

        .btn-primary {
          background: #1976d2;
          color: white;
        }

        .btn-primary:hover {
          background: #1565c0;
        }

        .btn-secondary {
          background: #f5f5f5;
          color: #333;
          border: 1px solid #ddd;
        }

        .btn-secondary:hover {
          background: #eeeeee;
        }
      `}</style>
    </div>
  );
};
