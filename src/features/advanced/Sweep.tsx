/**
 * Sweep Module - Движение профиля по пути
 */

import React, { useState } from 'react';
import { SweepFeature } from './types';

interface SweepProps {
  profileId: string;
  onApply: (feature: SweepFeature) => void;
  onCancel: () => void;
}

export const Sweep: React.FC<SweepProps> = ({ profileId, onApply, onCancel }) => {
  const [pathId, setPathId] = useState<string>('');
  const [twist, setTwist] = useState<number>(0);
  const [keepNormal, setKeepNormal] = useState<boolean>(true);
  const [scaleProfile, setScaleProfile] = useState<boolean>(false);

  const handleApply = () => {
    if (!pathId) {
      alert('Выберите путь выметания');
      return;
    }

    const sweepFeature: SweepFeature = {
      id: `sweep-${Date.now()}`,
      name: `Sweep (twist: ${twist}°)`,
      profile: profileId,
      path: pathId,
      twist: twist !== 0 ? twist : undefined,
      scaleProfile,
      keepNormal,
    };
    onApply(sweepFeature);
  };

  return (
    <div className="feature-dialog sweep-dialog">
      <h3>Параметры выметания (Sweep)</h3>

      <div className="param-group">
        <label>Профиль для выметания:</label>
        <p className="info-text">✓ Выбран профиль</p>
      </div>

      <div className="param-group">
        <label htmlFor="path">Путь выметания:</label>
        <p className="note">Выберите кривую или ребро в качестве пути</p>
        <div className="path-selector">
          {pathId ? (
            <span className="selected-path">✓ Путь выбран: {pathId}</span>
          ) : (
            <span className="no-path">Путь не выбран</span>
          )}
        </div>
      </div>

      <div className="param-group">
        <label htmlFor="twist">Угол поворота (°): {twist}</label>
        <input
          id="twist"
          type="range"
          min="0"
          max="360"
          step="5"
          value={twist}
          onChange={(e) => setTwist(parseFloat(e.target.value))}
        />
        <input
          type="number"
          value={twist}
          onChange={(e) => setTwist(parseFloat(e.target.value))}
          min="0"
          max="360"
          step="1"
        />
      </div>

      <div className="param-group">
        <label>
          <input
            type="checkbox"
            checked={keepNormal}
            onChange={(e) => setKeepNormal(e.target.checked)}
          />
          Сохранять нормаль к пути
        </label>
      </div>

      <div className="param-group">
        <label>
          <input
            type="checkbox"
            checked={scaleProfile}
            onChange={(e) => setScaleProfile(e.target.checked)}
          />
          Масштабировать профиль вдоль пути
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
