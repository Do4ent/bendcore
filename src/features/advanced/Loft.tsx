/**
 * Loft Module - Плавный переход между профилями
 */

import React, { useState } from 'react';
import { LoftFeature } from './types';

interface LoftProps {
  sketches: string[];
  onApply: (feature: LoftFeature) => void;
  onCancel: () => void;
}

export const Loft: React.FC<LoftProps> = ({ sketches, onApply, onCancel }) => {
  const [profiles, setProfiles] = useState<string[]>(sketches);
  const [surfaceType, setSurfaceType] = useState<'smooth' | 'linear' | 'ruled'>('smooth');
  const [startTangentPlane, setStartTangentPlane] = useState<'xy' | 'xz' | 'yz'>('xy');
  const [startTangentAngle, setStartTangentAngle] = useState<number>(0);
  const [useStartTangent, setUseStartTangent] = useState<boolean>(false);
  const [useEndTangent, setUseEndTangent] = useState<boolean>(false);

  const handleApply = () => {
    const loftFeature: LoftFeature = {
      id: `loft-${Date.now()}`,
      name: `Loft (${surfaceType})`,
      profiles,
      surfaceType,
      ...(useStartTangent && {
        startTangent: { plane: startTangentPlane, angle: startTangentAngle },
      }),
    };
    onApply(loftFeature);
  };

  return (
    <div className="feature-dialog loft-dialog">
      <h3>Параметры переходной поверхности (Loft)</h3>

      <div className="param-group">
        <label>Выбранные профили: {profiles.length}</label>
        <p className="note">
          Убедитесь, что профили упорядочены последовательно. Минимум 2 профиля.
        </p>
        <div className="profiles-count">
          {profiles.length >= 2 ? (
            <span className="success">✓ {profiles.length} профилей готово</span>
          ) : (
            <span className="error">✗ Необходимо минимум 2 профиля</span>
          )}
        </div>
      </div>

      <div className="param-group">
        <label htmlFor="surfaceType">Тип поверхности:</label>
        <select
          id="surfaceType"
          value={surfaceType}
          onChange={(e) => setSurfaceType(e.target.value as any)}
        >
          <option value="smooth">Гладкая (G2 непрерывность)</option>
          <option value="linear">Линейная</option>
          <option value="ruled">Линейчатая</option>
        </select>
      </div>

      <div className="param-group">
        <label>
          <input
            type="checkbox"
            checked={useStartTangent}
            onChange={(e) => setUseStartTangent(e.target.checked)}
          />
          Начальное касание (Start Tangent)
        </label>
      </div>

      {useStartTangent && (
        <>
          <div className="param-group">
            <label htmlFor="startPlane">Плоскость касания:</label>
            <select
              id="startPlane"
              value={startTangentPlane}
              onChange={(e) => setStartTangentPlane(e.target.value as any)}
            >
              <option value="xy">XY плоскость</option>
              <option value="xz">XZ плоскость</option>
              <option value="yz">YZ плоскость</option>
            </select>
          </div>

          <div className="param-group">
            <label htmlFor="startAngle">Угол касания (°):</label>
            <input
              id="startAngle"
              type="number"
              value={startTangentAngle}
              onChange={(e) => setStartTangentAngle(parseFloat(e.target.value))}
              min="0"
              max="360"
              step="1"
            />
          </div>
        </>
      )}

      <div className="param-group">
        <label>
          <input
            type="checkbox"
            checked={useEndTangent}
            onChange={(e) => setUseEndTangent(e.target.checked)}
          />
          Конечное касание (End Tangent)
        </label>
      </div>

      <div className="dialog-buttons">
        <button
          className="btn-primary"
          onClick={handleApply}
          disabled={profiles.length < 2}
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
