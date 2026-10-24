/**
 * Chamfer Module - Фаски на рёбрах
 */

import React, { useState } from 'react';
import { ChamferFeature } from './types';

interface ChamferProps {
  edges: string[];
  onApply: (feature: ChamferFeature) => void;
  onCancel: () => void;
}

export const Chamfer: React.FC<ChamferProps> = ({ edges, onApply, onCancel }) => {
  const [size, setSize] = useState<number>(0.5);
  const [chamferType, setChamferType] = useState<'distance' | 'angle'>('distance');
  const [angle, setAngle] = useState<number>(45);
  const [selectedEdges] = useState<string[]>(edges);

  const handleApply = () => {
    const chamferFeature: ChamferFeature = {
      id: `chamfer-${Date.now()}`,
      name: `Chamfer (${size.toFixed(2)}mm)`,
      size,
      edges: selectedEdges,
      chamferType,
      angle: chamferType === 'angle' ? angle : undefined,
    };
    onApply(chamferFeature);
  };

  return (
    <div className="feature-dialog chamfer-dialog">
      <h3>Параметры фаски (Chamfer)</h3>

      <div className="param-group">
        <label htmlFor="chamferType">Тип фаски:</label>
        <select
          id="chamferType"
          value={chamferType}
          onChange={(e) => setChamferType(e.target.value as any)}
        >
          <option value="distance">Расстояние</option>
          <option value="angle">Расстояние и угол</option>
        </select>
      </div>

      <div className="param-group">
        <label htmlFor="size">Размер (мм):</label>
        <input
          id="size"
          type="number"
          value={size}
          onChange={(e) => setSize(parseFloat(e.target.value))}
          min="0.1"
          max="100"
          step="0.1"
        />
      </div>

      {chamferType === 'angle' && (
        <div className="param-group">
          <label htmlFor="angle">Угол (°):</label>
          <input
            id="angle"
            type="number"
            value={angle}
            onChange={(e) => setAngle(parseFloat(e.target.value))}
            min="0"
            max="90"
            step="1"
          />
        </div>
      )}

      <div className="edges-list">
        <label>Выбранные рёбра: {selectedEdges.length}</label>
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
