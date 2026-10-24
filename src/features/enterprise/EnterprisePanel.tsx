/**
 * Панель управления Enterprise функциями
 */

import React, { useState } from 'react';
import { PipeRoute, BillOfMaterials } from './types';
import { PipeRouting } from './PipeRouting';
import { BOM } from './BOM';

interface EnterprisePanelProps {
  onRouteGenerated: (route: PipeRoute) => void;
  onBOMGenerated: (bom: BillOfMaterials) => void;
  routes: PipeRoute[];
  boms: BillOfMaterials[];
}

type DialogType = 'routing' | 'bom' | null;

export const EnterprisePanel: React.FC<EnterprisePanelProps> = ({
  onRouteGenerated,
  onBOMGenerated,
  routes,
  boms,
}) => {
  const [activeDialog, setActiveDialog] = useState<DialogType>(null);

  return (
    <div className="enterprise-panel">
      <h2>Enterprise функции (ЭТАП 4)</h2>

      <div className="enterprise-toolbar">
        <button
          className="tool-btn"
          onClick={() => setActiveDialog('routing')}
          title="Автоматизация маршрутизации труб"
        >
          🔄 Маршруты
        </button>
        <button
          className="tool-btn"
          onClick={() => setActiveDialog('bom')}
          title="Ведомость материалов"
        >
          📋 BOM
        </button>
      </div>

      <div className="routes-tree">
        <h3>Спроектированные маршруты ({routes.length})</h3>
        <div className="routes-list">
          {routes.length === 0 ? (
            <p className="empty-state">Маршруты не спроектированы</p>
          ) : (
            routes.map((route) => (
              <div key={route.id} className="route-item">
                <span className="route-name">{route.name}</span>
                <span className="route-length">{route.length.toFixed(1)}мм</span>
                <span className="route-cost">${route.cost.toFixed(2)}</span>
              </div>
            ))
          )}
        </div>
      </div>

      <div className="boms-tree">
        <h3>Созданные BOM ({boms.length})</h3>
        <div className="boms-list">
          {boms.length === 0 ? (
            <p className="empty-state">BOM не созданы</p>
          ) : (
            boms.map((bom) => (
              <div key={bom.id} className="bom-item">
                <span className="bom-name">{bom.projectName}</span>
                <span className="items-count">{bom.items.length} поз.</span>
                <span className="bom-cost">${bom.totalCost.toFixed(2)}</span>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Диалоги */}
      {activeDialog === 'routing' && (
        <PipeRouting
          onGenerate={(route) => {
            onRouteGenerated(route);
            setActiveDialog(null);
          }}
          onCancel={() => setActiveDialog(null)}
        />
      )}

      {activeDialog === 'bom' && (
        <BOM
          onGenerate={(bom) => {
            onBOMGenerated(bom);
            setActiveDialog(null);
          }}
          onCancel={() => setActiveDialog(null)}
        />
      )}

      <style>{`
        .enterprise-panel {
          padding: 16px;
          border: 1px solid #ddd;
          border-radius: 8px;
          background: #f9f9f9;
        }

        .enterprise-toolbar {
          display: grid;
          grid-template-columns: repeat(2, 1fr);
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
          background: #f0f5ff;
          border-color: #2196f3;
        }

        .routes-tree,
        .boms-tree {
          margin-top: 16px;
        }

        .routes-list,
        .boms-list {
          background: white;
          border: 1px solid #eee;
          border-radius: 4px;
          max-height: 200px;
          overflow-y: auto;
        }

        .route-item,
        .bom-item {
          padding: 8px 12px;
          border-bottom: 1px solid #f0f0f0;
          display: flex;
          gap: 12px;
          align-items: center;
          font-size: 12px;
        }

        .route-name,
        .bom-name {
          flex: 1;
          font-weight: 500;
        }

        .route-length,
        .route-cost,
        .items-count,
        .bom-cost {
          background: #e8f5e9;
          padding: 2px 6px;
          border-radius: 3px;
          font-size: 11px;
          color: #2e7d32;
        }

        .empty-state {
          padding: 12px;
          color: #999;
          text-align: center;
          font-size: 12px;
        }
      `}</style>
    </div>
  );
};
