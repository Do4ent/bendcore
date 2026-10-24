/**
 * Surface Module - Создание произвольных поверхностей
 */

import React, { useState } from 'react';
import { SurfaceFeature } from './types';

interface SurfaceProps {
  inputs: string[];
  onApply: (feature: SurfaceFeature) => void;
  onCancel: () => void;
}

export const Surface: React.FC<SurfaceProps> = ({ inputs, onApply, onCancel }) => {
  const [surfaceType, setSurfaceType] = useState<'face' | 'surface' | 'blend' | 'fill'>('surface');
  const [continuity, setContinuity] = useState<'positional' | 'tangent' | 'curvature'>('tangent');
  const [selectedInputs, setSelectedInputs] = useState<string[]>(inputs);

  const handleApply = () => {
    if (selectedInputs.length < 2) {
      alert('Выберите минимум 2 входных элемента');
      return;
    }

    const surfaceFeature: SurfaceFeature = {
      id: `surface-${Date.now()}`,
      name: `Surface (${surfaceType})`,
      surfaceType,
      inputs: selectedInputs,
      tangentContinuity: continuity,
    };
    onApply(surfaceFeature);
  };

  return (
    <div className="feature-dialog surface-dialog">
      <h3>Параметры поверхности (Surface)</h3>

      <div className="param-group">
        <label htmlFor="surfaceType">Тип поверхности:</label>
        <select
          id="surfaceType"
          value={surfaceType}
          onChange={(e) => setSurfaceType(e.target.value as any)}
        >
          <option value="face">Грань (Face)</option>
          <option value="surface">Поверхность</option>
          <option value="blend">Смешивание (Blend)</option>
          <option value="fill">Заполнение (Fill)</option>
        </select>
      </div>

      <div className="param-group">
        <label htmlFor="continuity">Непрерывность касания:</label>
        <select
          id="continuity"
          value={continuity}
          onChange={(e) => setContinuity(e.target.value as any)}
        >
          <option value="positional">Позиционная (G0)</option>
          <option value="tangent">Касательная (G1)</option>
          <option value="curvature">Кривизна (G2)</option>
        </select>
      </div>

      <div className="inputs-section">
        <label>Входные элементы: {selectedInputs.length}</label>
        <div className="inputs-info">
          {selectedInputs.length >= 2 ? (
            <span className="success">✓ Достаточно элементов</span>
          ) : (
            <span className="error">✗ Требуется минимум 2 элемента</span>
          )}
        </div>
      </div>

      <div className="param-group info-box">
        <p><strong>Описание типов:</strong></p>
        <ul>
          <li><strong>Face:</strong> Создание поверхности из грани</li>
          <li><strong>Surface:</strong> Произвольная поверхность</li>
          <li><strong>Blend:</strong> Смешивание между двумя гранями</li>
          <li><strong>Fill:</strong> Заполнение замкнутого контура</li>
        </ul>
      </div>

      <div className="dialog-buttons">
        <button
          className="btn-primary"
          onClick={handleApply}
          disabled={selectedInputs.length < 2}
        >
          Применить
        </button>
        <button className="btn-secondary" onClick={onCancel}>
          Отмена
        </button>
      </div>
    </div>
  );
};
