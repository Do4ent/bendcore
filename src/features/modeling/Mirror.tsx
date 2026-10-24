/**
 * Mirror Module - Зеркалирование геометрии
 */

import React, { useState } from 'react';
import { MirrorFeature } from './types';

interface MirrorProps {
  features: string[];
  onApply: (feature: MirrorFeature) => void;
  onCancel: () => void;
}

export const Mirror: React.FC<MirrorProps> = ({ features, onApply, onCancel }) => {
  const [mirrorPlane, setMirrorPlane] = useState<'xy' | 'xz' | 'yz'>('xy');
  const [selectedFeatures, setSelectedFeatures] = useState<string[]>(features);

  const handleApply = () => {
    const mirrorFeature: MirrorFeature = {
      id: `mirror-${Date.now()}`,
      name: `Mirror (${mirrorPlane})`,
      plane: mirrorPlane,
      features: selectedFeatures,
    };
    onApply(mirrorFeature);
  };

  const toggleFeature = (featureId: string) => {
    setSelectedFeatures(
      selectedFeatures.includes(featureId)
        ? selectedFeatures.filter((f) => f !== featureId)
        : [...selectedFeatures, featureId]
    );
  };

  return (
    <div className="feature-dialog mirror-dialog">
      <h3>Параметры зеркалирования (Mirror)</h3>

      <div className="param-group">
        <label htmlFor="plane">Плоскость зеркалирования:</label>
        <select
          id="plane"
          value={mirrorPlane}
          onChange={(e) => setMirrorPlane(e.target.value as any)}
        >
          <option value="xy">XY плоскость</option>
          <option value="xz">XZ плоскость</option>
          <option value="yz">YZ плоскость</option>
        </select>
      </div>

      <div className="features-list">
        <label>Функции для зеркалирования: {selectedFeatures.length}</label>
        <div className="features-info">
          {selectedFeatures.length > 0 ? (
            <span>{selectedFeatures.length} функция(й) выбрано</span>
          ) : (
            <span>Выберите функции</span>
          )}
        </div>
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
