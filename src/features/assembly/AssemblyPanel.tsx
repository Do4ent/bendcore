/**
 * Панель управления сборкой и симуляцией
 */

import React, { useState } from 'react';
import { BendCoreAssembly, AssemblyPart, FEAResult } from './types';
import { Assembly } from './Assembly';
import { FEASimulator } from '../simulation';

interface AssemblyPanelProps {
  onAssemblyCreated: (assembly: BendCoreAssembly) => void;
  assemblies: BendCoreAssembly[];
}

type DialogType = 'assembly' | 'simulation' | null;

export const AssemblyPanel: React.FC<AssemblyPanelProps> = ({ onAssemblyCreated, assemblies }) => {
  const [activeDialog, setActiveDialog] = useState<DialogType>(null);
  const [results, setResults] = useState<FEAResult[]>([]);

  const mockParts: AssemblyPart[] = [
    {
      id: 'part-1',
      name: 'Основание',
      partId: 'part-1',
      placement: { position: [0, 0, 0], rotation: [0, 0, 0] },
      visibility: true,
    },
    {
      id: 'part-2',
      name: 'Крышка',
      partId: 'part-2',
      placement: { position: [0, 0, 10], rotation: [0, 0, 0] },
      visibility: true,
    },
    {
      id: 'part-3',
      name: 'Внутренняя трубка',
      partId: 'part-3',
      placement: { position: [5, 5, 5], rotation: [0, 0, 0] },
      visibility: true,
    },
  ];

  const handleAssemblyApply = (assembly: BendCoreAssembly) => {
    onAssemblyCreated(assembly);
    setActiveDialog(null);
  };

  const handleAnalyze = (result: FEAResult) => {
    setResults([...results, result]);
  };

  return (
    <div className="assembly-panel">
      <h2>Сборка и симуляция (ЭТАП 3)</h2>

      <div className="assembly-toolbar">
        <button
          className="tool-btn"
          onClick={() => setActiveDialog('assembly')}
          title="Создать сборку"
        >
          ⚙️ Сборка
        </button>
        <button
          className="tool-btn"
          onClick={() => setActiveDialog('simulation')}
          title="FEA Симуляция"
        >
          📊 FEA (Mock)
        </button>
      </div>

      <div className="assemblies-tree">
        <h3>Созданные сборки ({assemblies.length})</h3>
        <div className="assemblies-list">
          {assemblies.length === 0 ? (
            <p className="empty-state">Сборок нет</p>
          ) : (
            assemblies.map((asm) => (
              <div key={asm.id} className="assembly-item">
                <span className="assembly-name">{asm.name}</span>
                <span className="parts-count">{asm.parts.length} деталей</span>
                <span className="constraints-count">{asm.constraints.length} огр.</span>
              </div>
            ))
          )}
        </div>
      </div>

      <div className="results-tree">
        <h3>Результаты анализа ({results.length})</h3>
        <div className="results-list">
          {results.length === 0 ? (
            <p className="empty-state">Результатов нет</p>
          ) : (
            results.map((result) => (
              <div key={result.id} className="result-item">
                <span className="result-type">{result.type}</span>
                <span className="result-max">Max: {result.maxValue.toFixed(1)}</span>
                <span className="result-unit">{result.unit}</span>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Диалоги */}
      {activeDialog === 'assembly' && (
        <Assembly
          parts={mockParts}
          onApply={handleAssemblyApply}
          onCancel={() => setActiveDialog(null)}
        />
      )}

      {activeDialog === 'simulation' && (
        <FEASimulator
          onAnalyze={handleAnalyze}
          onCancel={() => setActiveDialog(null)}
        />
      )}

      <style>{`
        .assembly-panel {
          padding: 16px;
          border: 1px solid #ddd;
          border-radius: 8px;
          background: #f9f9f9;
        }

        .assembly-toolbar {
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
          background: #fff3e0;
          border-color: #ff9800;
        }

        .assemblies-tree,
        .results-tree {
          margin-top: 16px;
        }

        .assemblies-list,
        .results-list {
          background: white;
          border: 1px solid #eee;
          border-radius: 4px;
          max-height: 200px;
          overflow-y: auto;
        }

        .assembly-item,
        .result-item {
          padding: 8px 12px;
          border-bottom: 1px solid #f0f0f0;
          display: flex;
          gap: 12px;
          align-items: center;
          font-size: 12px;
        }

        .assembly-name,
        .result-type {
          flex: 1;
          font-weight: 500;
        }

        .parts-count,
        .constraints-count,
        .result-max {
          background: #e3f2fd;
          padding: 2px 6px;
          border-radius: 3px;
          font-size: 11px;
          color: #1976d2;
        }

        .result-unit {
          color: #999;
          font-size: 11px;
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
