/**
 * Fillet Module - Скругление рёбер
 */

import React, { useState } from 'react';
import { FilletFeature } from './types';

interface FilletProps {
  edges: string[];
  onApply: (feature: FilletFeature) => void;
  onCancel: () => void;
}

export const Fillet: React.FC<FilletProps> = ({ edges, onApply, onCancel }) => {
  const [radius, setRadius] = useState<number>(1);
  const [selectedEdges, setSelectedEdges] = useState<string[]>(edges);
  const [preserveEdges, setPreserveEdges] = useState<boolean>(true);

  const handleApply = () => {
    const filletFeature: FilletFeature = {
      id: `fillet-${Date.now()}`,
      name: `Fillet (${radius.toFixed(2)}mm)`,
      radius,
      edges: selectedEdges,
      preserveEdges,
    };
    onApply(filletFeature);
  };

  return (
    <div className="feature-dialog fillet-dialog">
      <h3>Параметры скругления (Fillet)</h3>

      <div className="param-group">
        <label htmlFor="radius">Радиус (мм): {radius.toFixed(2)}</label>
        <input
          id="radius"
          type="range"
          min="0.1"
          max="50"
          step="0.1"
          value={radius}
          onChange={(e) => setRadius(parseFloat(e.target.value))}
        />
        <input
          type="number"
          value={radius}
          onChange={(e) => setRadius(parseFloat(e.target.value))}
          min="0.1"
          max="50"
          step="0.1"
        />
      </div>

      <div className="edges-list">
        <label>Выбранные рёбра: {selectedEdges.length}</label>
        <div className="edges-count">{selectedEdges.length} рёбер выбрано</div>
      </div>

      <div className="param-group">
        <label htmlFor="preserve">
          <input
            id="preserve"
            type="checkbox"
            checked={preserveEdges}
            onChange={(e) => setPreserveEdges(e.target.checked)}
          />
          Сохранять исходные рёбра
        </label>
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
