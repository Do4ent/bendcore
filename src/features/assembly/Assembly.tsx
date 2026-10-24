/**
 * Assembly Module - Управление сборками
 */

import React, { useState } from 'react';
import { BendCoreAssembly, AssemblyPart, AssemblyConstraint } from './types';

interface AssemblyProps {
  parts: AssemblyPart[];
  onApply: (assembly: BendCoreAssembly) => void;
  onCancel: () => void;
}

export const Assembly: React.FC<AssemblyProps> = ({ parts, onApply, onCancel }) => {
  const [assembly, setAssembly] = useState<BendCoreAssembly>({
    id: `asm-${Date.now()}`,
    name: 'Сборка',
    parts: parts.map((p) => ({
      id: p.id,
      name: p.name,
      partId: p.id,
      placement: { position: [0, 0, 0], rotation: [0, 0, 0] },
      visibility: true,
    })),
    constraints: [],
    explodedView: { active: false, distance: 50, direction: [1, 1, 1] },
  });

  const [selectedParts, setSelectedParts] = useState<string[]>([]);
  const [constraintType, setConstraintType] = useState<AssemblyConstraint['type']>('coincident');

  const handleAddConstraint = () => {
    if (selectedParts.length >= 2) {
      const newConstraint: AssemblyConstraint = {
        id: `constraint-${Date.now()}`,
        type: constraintType,
        part1: selectedParts[0],
        face1: 'face-0',
        part2: selectedParts[1],
        face2: 'face-0',
        status: 'satisfied',
      };
      setAssembly({
        ...assembly,
        constraints: [...assembly.constraints, newConstraint],
      });
      setSelectedParts([]);
    }
  };

  const handleApply = () => {
    onApply(assembly);
  };

  const handleRemoveConstraint = (constraintId: string) => {
    setAssembly({
      ...assembly,
      constraints: assembly.constraints.filter((c) => c.id !== constraintId),
    });
  };

  return (
    <div className="assembly-dialog">
      <h3>Параметры сборки (Assembly)</h3>

      <div className="assembly-container">
        <div className="parts-section">
          <h4>Детали ({assembly.parts.length})</h4>
          <div className="parts-list">
            {assembly.parts.map((part) => (
              <div
                key={part.id}
                className={`part-item ${selectedParts.includes(part.id) ? 'selected' : ''}`}
                onClick={() =>
                  setSelectedParts(
                    selectedParts.includes(part.id)
                      ? selectedParts.filter((p) => p !== part.id)
                      : [...selectedParts, part.id]
                  )
                }
              >
                <span className="part-name">{part.name}</span>
                <button
                  className="visibility-btn"
                  onClick={(e) => {
                    e.stopPropagation();
                    setAssembly({
                      ...assembly,
                      parts: assembly.parts.map((p) =>
                        p.id === part.id ? { ...p, visibility: !p.visibility } : p
                      ),
                    });
                  }}
                >
                  {part.visibility ? '👁️' : '🚫'}
                </button>
              </div>
            ))}
          </div>
        </div>

        <div className="constraints-section">
          <h4>Ограничения ({assembly.constraints.length})</h4>
          <div className="constraint-controls">
            <select
              value={constraintType}
              onChange={(e) => setConstraintType(e.target.value as any)}
              className="constraint-select"
            >
              <option value="coincident">Совпадение (Coincident)</option>
              <option value="parallel">Параллель (Parallel)</option>
              <option value="perpendicular">Перпендикуляр (Perpendicular)</option>
              <option value="distance">Расстояние (Distance)</option>
              <option value="concentric">Концентричность (Concentric)</option>
              <option value="coplanar">Компланарность (Coplanar)</option>
            </select>
            <button
              className="add-constraint-btn"
              onClick={handleAddConstraint}
              disabled={selectedParts.length < 2}
            >
              Добавить ограничение ({selectedParts.length}/2)
            </button>
          </div>

          <div className="constraints-list">
            {assembly.constraints.length === 0 ? (
              <p className="empty-state">Ограничений нет</p>
            ) : (
              assembly.constraints.map((constraint) => (
                <div key={constraint.id} className="constraint-item">
                  <span className="constraint-type">{constraint.type}</span>
                  <span className={`constraint-status ${constraint.status}`}>
                    {constraint.status === 'satisfied' && '✓'}
                    {constraint.status === 'conflicting' && '⚠️'}
                    {constraint.status === 'redundant' && 'ℹ️'}
                  </span>
                  <button
                    className="remove-btn"
                    onClick={() => handleRemoveConstraint(constraint.id)}
                  >
                    ×
                  </button>
                </div>
              ))
            )}
          </div>
        </div>

        <div className="exploded-view-section">
          <h4>Взрывной вид (Exploded View)</h4>
          <label className="checkbox-label">
            <input
              type="checkbox"
              checked={assembly.explodedView?.active || false}
              onChange={(e) =>
                setAssembly({
                  ...assembly,
                  explodedView: {
                    ...(assembly.explodedView || { distance: 50, direction: [1, 1, 1] }),
                    active: e.target.checked,
                  },
                })
              }
            />
            Включить взрывной вид
          </label>

          {assembly.explodedView?.active && (
            <div className="param-group">
              <label>Расстояние (мм): {assembly.explodedView?.distance || 50}</label>
              <input
                type="range"
                min="10"
                max="200"
                value={assembly.explodedView?.distance || 50}
                onChange={(e) =>
                  setAssembly({
                    ...assembly,
                    explodedView: {
                      ...assembly.explodedView!,
                      distance: parseFloat(e.target.value),
                    },
                  })
                }
              />
            </div>
          )}
        </div>
      </div>

      <div className="dialog-buttons">
        <button className="btn-primary" onClick={handleApply}>
          Создать сборку
        </button>
        <button className="btn-secondary" onClick={onCancel}>
          Отмена
        </button>
      </div>

      <style>{`
        .assembly-dialog {
          padding: 16px;
          background: white;
          border-radius: 8px;
          border: 1px solid #ddd;
        }

        .assembly-container {
          display: grid;
          grid-template-columns: 1fr 1fr;
          gap: 16px;
          margin: 16px 0;
        }

        .parts-section,
        .constraints-section {
          border: 1px solid #eee;
          border-radius: 4px;
          padding: 12px;
          background: #fafafa;
        }

        .parts-list,
        .constraints-list {
          max-height: 250px;
          overflow-y: auto;
          border: 1px solid #e0e0e0;
          border-radius: 3px;
          background: white;
        }

        .part-item,
        .constraint-item {
          padding: 8px 12px;
          border-bottom: 1px solid #f0f0f0;
          display: flex;
          justify-content: space-between;
          align-items: center;
          cursor: pointer;
          transition: background 0.2s;
        }

        .part-item:hover {
          background: #f5f5f5;
        }

        .part-item.selected {
          background: #e3f2fd;
          border-left: 3px solid #1976d2;
        }

        .visibility-btn {
          background: none;
          border: none;
          cursor: pointer;
          padding: 4px 8px;
        }

        .constraint-controls {
          display: flex;
          gap: 8px;
          margin-bottom: 12px;
        }

        .constraint-select {
          flex: 1;
          padding: 6px;
          border: 1px solid #ddd;
          border-radius: 3px;
          font-size: 12px;
        }

        .add-constraint-btn {
          padding: 6px 12px;
          background: #4caf50;
          color: white;
          border: none;
          border-radius: 3px;
          cursor: pointer;
          font-size: 12px;
        }

        .add-constraint-btn:hover:not(:disabled) {
          background: #45a049;
        }

        .add-constraint-btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }

        .constraint-type {
          flex: 1;
          font-weight: 500;
          font-size: 12px;
        }

        .constraint-status {
          margin: 0 8px;
          font-size: 14px;
        }

        .constraint-status.satisfied {
          color: #4caf50;
        }

        .constraint-status.conflicting {
          color: #ff9800;
        }

        .constraint-status.redundant {
          color: #2196f3;
        }

        .remove-btn {
          background: #f44336;
          color: white;
          border: none;
          border-radius: 3px;
          width: 24px;
          height: 24px;
          cursor: pointer;
          font-weight: bold;
        }

        .remove-btn:hover {
          background: #da190b;
        }

        .exploded-view-section {
          grid-column: 1 / -1;
          border: 1px solid #eee;
          border-radius: 4px;
          padding: 12px;
          background: #fafafa;
        }

        .checkbox-label {
          display: flex;
          align-items: center;
          gap: 8px;
          cursor: pointer;
          margin-bottom: 8px;
        }

        .checkbox-label input {
          cursor: pointer;
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
