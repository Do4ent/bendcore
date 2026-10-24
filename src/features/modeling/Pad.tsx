/**
 * Pad/Extrude Module - Выдавливание профиля
 * Основная операция 3D моделирования
 */

import React, { useState } from 'react';
import { PadFeature } from './types';

interface PadProps {
  sketchId: string;
  onApply: (feature: PadFeature) => void;
  onCancel: () => void;
}

export const Pad: React.FC<PadProps> = ({ sketchId, onApply, onCancel }) => {
  const [length, setLength] = useState<number>(10);
  const [direction, setDirection] = useState<'normal' | 'reverse' | 'symmetric'>('normal');
  const [offset, setOffset] = useState<number>(0);
  const [draftAngle, setDraftAngle] = useState<number>(0);
  const [taper, setTaper] = useState<number>(0);

  const handleApply = () => {
    const padFeature: PadFeature = {
      id: `pad-${Date.now()}`,
      name: `Pad (${length}mm)`,
      sketchId,
      length,
      direction,
      offset,
      draftAngle,
      taper,
    };
    onApply(padFeature);
  };

  return (
    <div className="feature-dialog pad-dialog">
      <h3>Параметры выдавливания (Pad)</h3>

      <div className="param-group">
        <label htmlFor="length">Длина (мм):</label>
        <input
          id="length"
          type="number"
          value={length}
          onChange={(e) => setLength(parseFloat(e.target.value))}
          min="0.1"
          max="1000"
          step="0.1"
        />
      </div>

      <div className="param-group">
        <label htmlFor="direction">Направление:</label>
        <select
          id="direction"
          value={direction}
          onChange={(e) => setDirection(e.target.value as any)}
        >
          <option value="normal">Нормаль (вверх)</option>
          <option value="reverse">Обратное (вниз)</option>
          <option value="symmetric">Симметричное</option>
        </select>
      </div>

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

      <div className="param-group">
        <label htmlFor="taper">Конусность (мм):</label>
        <input
          id="taper"
          type="number"
          value={taper}
          onChange={(e) => setTaper(parseFloat(e.target.value))}
          min="-10"
          max="10"
          step="0.1"
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
