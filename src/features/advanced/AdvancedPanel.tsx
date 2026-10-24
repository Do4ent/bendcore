/**
 * Панель управления продвинутым моделированием
 */

import React, { useState } from 'react';
import { AdvancedFeature } from './types';
import { Loft } from './Loft';
import { Sweep } from './Sweep';
import { Surface } from './Surface';

interface AdvancedPanelProps {
  onFeatureAdded: (feature: AdvancedFeature) => void;
  features: AdvancedFeature[];
}

type DialogType = 'loft' | 'sweep' | 'surface' | null;

export const AdvancedPanel: React.FC<AdvancedPanelProps> = ({ onFeatureAdded, features }) => {
  const [activeDialog, setActiveDialog] = useState<DialogType>(null);

  return (
    <div className="advanced-panel">
      <h2>Продвинутое моделирование (ЭТАП 2)</h2>

      <div className="advanced-toolbar">
        <button
          className="tool-btn"
          onClick={() => setActiveDialog('loft')}
          title="Переходная поверхность (Loft)"
        >
          🌊 Loft
        </button>
        <button
          className="tool-btn"
          onClick={() => setActiveDialog('sweep')}
          title="Выметание (Sweep)"
        >
          🔄 Sweep
        </button>
        <button
          className="tool-btn"
          onClick={() => setActiveDialog('surface')}
          title="Поверхность (Surface)"
        >
          🎨 Surface
        </button>
      </div>

      <div className="features-tree">
        <h3>Продвинутые функции ({features.length})</h3>
        <div className="features-list">
          {features.length === 0 ? (
            <p className="empty-state">Нет продвинутых функций</p>
          ) : (
            features.map((feature) => (
              <div key={feature.id} className="feature-item">
                <span className="feature-name">{feature.name}</span>
                <span className="feature-type">({feature.id.split('-')[0]})</span>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Диалоги */}
      {activeDialog === 'loft' && (
        <Loft
          sketches={[]}
          onApply={(feature) => {
            onFeatureAdded(feature);
            setActiveDialog(null);
          }}
          onCancel={() => setActiveDialog(null)}
        />
      )}

      {activeDialog === 'sweep' && (
        <Sweep
          profileId="profile-1"
          onApply={(feature) => {
            onFeatureAdded(feature);
            setActiveDialog(null);
          }}
          onCancel={() => setActiveDialog(null)}
        />
      )}

      {activeDialog === 'surface' && (
        <Surface
          inputs={[]}
          onApply={(feature) => {
            onFeatureAdded(feature);
            setActiveDialog(null);
          }}
          onCancel={() => setActiveDialog(null)}
        />
      )}

      <style>{`
        .advanced-panel {
          padding: 16px;
          border: 1px solid #ddd;
          border-radius: 8px;
          background: #f9f9f9;
        }

        .advanced-toolbar {
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

        .tool-btn:hover {
          background: #f0f7ff;
          border-color: #2196f3;
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
        }

        .empty-state {
          padding: 12px;
          color: #999;
          text-align: center;
        }

        .info-box {
          background: #e3f2fd;
          border-left: 3px solid #2196f3;
          padding: 8px;
          border-radius: 3px;
        }

        .info-box ul {
          margin: 0;
          padding-left: 20px;
          font-size: 11px;
        }

        .info-box li {
          margin: 4px 0;
        }

        .success {
          color: #4caf50;
          font-weight: 500;
        }

        .error {
          color: #f44336;
          font-weight: 500;
        }
      `}</style>
    </div>
  );
};
