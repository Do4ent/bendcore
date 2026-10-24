/**
 * Pocket/Hole Module - Карман/Отверстие
 */

import React, { useState } from 'react';
import { PocketFeature } from './types';

interface PocketProps {
  sketchId: string;
  onApply: (feature: PocketFeature) => void;
  onCancel: () => void;
}

export const Pocket: React.FC<PocketProps> = ({ sketchId, onApply, onCancel }) => {
  const [depth, setDepth] = useState<number>(5);
  const [depthType, setDepthType] = useState<'blind' | 'through' | 'twoSided'>('blind');
  const [offset, setOffset] = useState<number>(0);
  const [draftAngle, setDraftAngle] = useState<number>(0);

  const handleApply = () => {
    const pocketFeature: PocketFeature = {
      id: `pocket-${Date.now()}`,
      name: `Pocket (${depthType})`,
      sketchId,
      depth,
      depthType,
      offset,
      draftAngle,
    };
    onApply(pocketFeature);
  };

  return (
    <div className="feature-dialog pocket-dialog">
      <h3>Параметры кармана (Pocket)</h3>

      <div className="param-group">
        <label htmlFor="depthType">Тип глубины:</label>
        <select
          id="depthType"
          value={depthType}
          onChange={(e) => setDepthType(e.target.value as any)}
        >
          <option value="blind">Слепое (ограниченное)</option>
          <option value="through">Сквозное (through all)</option>
          <option value="twoSided">Двусторонее</option>
        </select>
      </div>

      {depthType === 'blind' && (
        <div className="param-group">
          <label htmlFor="depth">Глубина (мм):</label>
          <input
            id="depth"
            type="number"
            value={depth}
            onChange={(e) => setDepth(parseFloat(e.target.value))}
            min="0.1"
            max="1000"
            step="0.1"
          />
        </div>
      )}

      {depthType === 'twoSided' && (
        <>
          <div className="param-group">
            <label htmlFor="depth1">Глубина 1 (мм):</label>
            <input
              id="depth1"
              type="number"
              value={depth}
              onChange={(e) => setDepth(parseFloat(e.target.value))}
              min="0.1"
              max="1000"
              step="0.1"
            />
          </div>
          <div className="param-group">
            <label htmlFor="depth2">Глубина 2 (мм):</label>
            <input
              id="depth2"
              type="number"
              defaultValue={depth}
              min="0.1"
              max="1000"
              step="0.1"
            />
          </div>
        </>
      )}

      <div className="param-group">
        <label htmlFor="offset">Смещение (мм):</label>
        <input
          id="offset"
          type="number"
          value={offset}
          onChange={(e) => setOffset(parseFloat(e.target.value))}
          min="-100"
          max="100"
          step="0.1"
        />
      </div>

      <div className="param-group">
        <label htmlFor="draft">Угол уклона (°):</label>
        <input
          id="draft"
          type="number"
          value={draftAngle}
          onChange={(e) => setDraftAngle(parseFloat(e.target.value))}
          min="-45"
          max="45"
          step="0.5"
        />
      </div>

      <div className="dialog-buttons">
        <button className="btn-primary" onClick={handleApply}>
          Применить
        </button>
        <button className="btn-secondary" onClick={onCancel}>
          Отмена
        </button>
      </div>
    </div>
  );
};
