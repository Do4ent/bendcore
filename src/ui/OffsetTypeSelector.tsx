/**
 * Offset Type Selector
 * Переключатель между Parallel и 3D Combined offset
 */

import React from 'react';

export type OffsetType = 'parallel' | '3d-combined';

interface OffsetTypeSelectorProps {
  selectedType: OffsetType;
  onTypeChange: (type: OffsetType) => void;
}

export const OffsetTypeSelector: React.FC<OffsetTypeSelectorProps> = ({
  selectedType,
  onTypeChange,
}) => {
  return (
    <div className="offset-type-selector">
      <div className="selector-header">
        <h3>Тип калькулятора смещения</h3>
      </div>

      <div className="selector-options">
        {/* Parallel Offset */}
        <label className="radio-option">
          <input
            type="radio"
            name="offset-type"
            value="parallel"
            checked={selectedType === 'parallel'}
            onChange={(e) => onTypeChange(e.target.value as OffsetType)}
          />
          <span className="radio-label">
            <span className="option-title">Параллельное смещение</span>
            <span className="option-desc">Простые гибки с параллельным смещением</span>
          </span>
        </label>

        {/* 3D Combined Offset */}
        <label className="radio-option">
          <input
            type="radio"
            name="offset-type"
            value="3d-combined"
            checked={selectedType === '3d-combined'}
            onChange={(e) => onTypeChange(e.target.value as OffsetType)}
          />
          <span className="radio-label">
            <span className="option-title">3D комбинированное смещение</span>
            <span className="option-desc">Сложные гибки в трёхмерном пространстве</span>
          </span>
        </label>
      </div>
    </div>
  );
};

export default OffsetTypeSelector;
