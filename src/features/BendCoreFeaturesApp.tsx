/**
 * BendCore CAD Features App
 * ПОЛНАЯ РЕАЛИЗАЦИЯ ВСЕХ 4 ЭТАПОВ
 */

import React, { useState } from 'react';
import { ModelingPanel } from './modeling';
import { AdvancedPanel } from './advanced';
import { AssemblyPanel } from './assembly';
import { EnterprisePanel } from './enterprise';
import { FEASimulator } from './simulation';
import {
  ModelingFeature,
  AdvancedFeature,
  BendCoreAssembly,
  PipeRoute,
  BillOfMaterials,
  FEAResult,
} from './index';

type TabType = 'modeling' | 'advanced' | 'assembly' | 'enterprise';

interface BendCoreFeaturesAppProps {
  onSave?: (state: AppState) => void;
}

interface AppState {
  modelingFeatures: ModelingFeature[];
  advancedFeatures: AdvancedFeature[];
  assemblies: BendCoreAssembly[];
  routes: PipeRoute[];
  boms: BillOfMaterials[];
  results: FEAResult[];
}

export const BendCoreFeaturesApp: React.FC<BendCoreFeaturesAppProps> = ({ onSave }) => {
  const [activeTab, setActiveTab] = useState<TabType>('modeling');
  const [state, setState] = useState<AppState>({
    modelingFeatures: [],
    advancedFeatures: [],
    assemblies: [],
    routes: [],
    boms: [],
    results: [],
  });

  const handleAddModelingFeature = (feature: ModelingFeature) => {
    const newState = {
      ...state,
      modelingFeatures: [...state.modelingFeatures, feature],
    };
    setState(newState);
    onSave?.(newState);
  };

  const handleAddAdvancedFeature = (feature: AdvancedFeature) => {
    const newState = {
      ...state,
      advancedFeatures: [...state.advancedFeatures, feature],
    };
    setState(newState);
    onSave?.(newState);
  };

  const handleCreateAssembly = (assembly: BendCoreAssembly) => {
    const newState = {
      ...state,
      assemblies: [...state.assemblies, assembly],
    };
    setState(newState);
    onSave?.(newState);
  };

  const handleGenerateRoute = (route: PipeRoute) => {
    const newState = {
      ...state,
      routes: [...state.routes, route],
    };
    setState(newState);
    onSave?.(newState);
  };

  const handleGenerateBOM = (bom: BillOfMaterials) => {
    const newState = {
      ...state,
      boms: [...state.boms, bom],
    };
    setState(newState);
    onSave?.(newState);
  };

  const handleAnalyze = (result: FEAResult) => {
    const newState = {
      ...state,
      results: [...state.results, result],
    };
    setState(newState);
    onSave?.(newState);
  };

  const stats = {
    modeling: state.modelingFeatures.length,
    advanced: state.advancedFeatures.length,
    assemblies: state.assemblies.length,
    routes: state.routes.length,
    boms: state.boms.length,
    analyses: state.results.length,
  };

  return (
    <div className="bendcore-features-app">
      <header className="app-header">
        <h1>BendCore CAD - Полная реализация (4 этапа)</h1>
        <p className="subtitle">
          Полнофункциональная система проектирования гнутых труб и сборок
        </p>
      </header>

      <div className="tabs-container">
        <div className="tabs-list">
          <button
            className={`tab-button ${activeTab === 'modeling' ? 'active' : ''}`}
            onClick={() => setActiveTab('modeling')}
          >
            <span className="tab-icon">📦</span>
            <span className="tab-label">ЭТАП 1: Базовое</span>
            <span className="tab-badge">{stats.modeling}</span>
          </button>

          <button
            className={`tab-button ${activeTab === 'advanced' ? 'active' : ''}`}
            onClick={() => setActiveTab('advanced')}
          >
            <span className="tab-icon">🌊</span>
            <span className="tab-label">ЭТАП 2: Продвинутое</span>
            <span className="tab-badge">{stats.advanced}</span>
          </button>

          <button
            className={`tab-button ${activeTab === 'assembly' ? 'active' : ''}`}
            onClick={() => setActiveTab('assembly')}
          >
            <span className="tab-icon">⚙️</span>
            <span className="tab-label">ЭТАП 3: Сборка</span>
            <span className="tab-badge">{stats.assemblies}</span>
          </button>

          <button
            className={`tab-button ${activeTab === 'enterprise' ? 'active' : ''}`}
            onClick={() => setActiveTab('enterprise')}
          >
            <span className="tab-icon">🏭</span>
            <span className="tab-label">ЭТАП 4: Enterprise</span>
            <span className="tab-badge">
              {stats.routes + stats.boms}
            </span>
          </button>
        </div>
      </div>

      <div className="tabs-content">
        {activeTab === 'modeling' && (
          <ModelingPanel
            onFeatureAdded={handleAddModelingFeature}
            features={state.modelingFeatures}
          />
        )}

        {activeTab === 'advanced' && (
          <AdvancedPanel
            onFeatureAdded={handleAddAdvancedFeature}
            features={state.advancedFeatures}
          />
        )}

        {activeTab === 'assembly' && (
          <AssemblyPanel
            onAssemblyCreated={handleCreateAssembly}
            assemblies={state.assemblies}
          />
        )}

        {activeTab === 'enterprise' && (
          <EnterprisePanel
            onRouteGenerated={handleGenerateRoute}
            onBOMGenerated={handleGenerateBOM}
            routes={state.routes}
            boms={state.boms}
          />
        )}
      </div>

      <footer className="app-footer">
        <div className="stats-row">
          <div className="stat">
            <span className="stat-label">Всего функций:</span>
            <span className="stat-value">
              {stats.modeling + stats.advanced}
            </span>
          </div>
          <div className="stat">
            <span className="stat-label">Сборок:</span>
            <span className="stat-value">{stats.assemblies}</span>
          </div>
          <div className="stat">
            <span className="stat-label">Маршрутов:</span>
            <span className="stat-value">{stats.routes}</span>
          </div>
          <div className="stat">
            <span className="stat-label">BOM:</span>
            <span className="stat-value">{stats.boms}</span>
          </div>
          <div className="stat">
            <span className="stat-label">Анализов:</span>
            <span className="stat-value">{stats.analyses}</span>
          </div>
        </div>
      </footer>

      <style>{`
        .bendcore-features-app {
          width: 100%;
          max-width: 1400px;
          margin: 0 auto;
          background: white;
          border-radius: 8px;
          box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
          overflow: hidden;
        }

        .app-header {
          background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
          color: white;
          padding: 24px;
          text-align: center;
        }

        .app-header h1 {
          margin: 0 0 8px 0;
          font-size: 28px;
          font-weight: 600;
        }

        .subtitle {
          margin: 0;
          font-size: 14px;
          opacity: 0.9;
        }

        .tabs-container {
          background: #f5f5f5;
          border-bottom: 1px solid #ddd;
          padding: 0;
          overflow-x: auto;
        }

        .tabs-list {
          display: flex;
          gap: 0;
          padding: 0;
          min-width: min-content;
        }

        .tab-button {
          flex: 1;
          min-width: 200px;
          padding: 16px 12px;
          background: white;
          border: none;
          border-bottom: 3px solid transparent;
          cursor: pointer;
          font-size: 13px;
          font-weight: 500;
          color: #666;
          display: flex;
          align-items: center;
          justify-content: center;
          gap: 8px;
          transition: all 0.3s;
          position: relative;
        }

        .tab-button:hover {
          background: #fafafa;
          color: #333;
        }

        .tab-button.active {
          background: white;
          color: #667eea;
          border-bottom-color: #667eea;
        }

        .tab-icon {
          font-size: 16px;
        }

        .tab-label {
          white-space: nowrap;
        }

        .tab-badge {
          background: #667eea;
          color: white;
          border-radius: 12px;
          padding: 2px 8px;
          font-size: 11px;
          font-weight: 600;
          min-width: 24px;
          text-align: center;
        }

        .tab-button.active .tab-badge {
          background: #667eea;
        }

        .tabs-content {
          padding: 24px;
          min-height: 400px;
          background: white;
        }

        .app-footer {
          background: #f9f9f9;
          border-top: 1px solid #ddd;
          padding: 16px 24px;
        }

        .stats-row {
          display: grid;
          grid-template-columns: repeat(5, 1fr);
          gap: 16px;
        }

        .stat {
          text-align: center;
          padding: 12px;
          background: white;
          border-radius: 4px;
          border: 1px solid #eee;
        }

        .stat-label {
          display: block;
          font-size: 11px;
          color: #999;
          margin-bottom: 4px;
          text-transform: uppercase;
          letter-spacing: 0.5px;
        }

        .stat-value {
          display: block;
          font-size: 24px;
          font-weight: bold;
          color: #667eea;
        }

        @media (max-width: 1200px) {
          .stats-row {
            grid-template-columns: repeat(3, 1fr);
          }
        }

        @media (max-width: 768px) {
          .tab-button {
            min-width: 150px;
            padding: 12px 8px;
          }

          .tab-label {
            display: none;
          }

          .stats-row {
            grid-template-columns: repeat(2, 1fr);
          }
        }
      `}</style>
    </div>
  );
};
