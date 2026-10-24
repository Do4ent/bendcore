/**
 * Pattern/Array Module - Массив копий функций
 */

import React, { useState } from 'react';
import { PatternFeature } from './types';

interface PatternProps {
  featureId: string;
  onApply: (feature: PatternFeature) => void;
  onCancel: () => void;
}

export const Pattern: React.FC<PatternProps> = ({ featureId, onApply, onCancel }) => {
  const [patternType, setPatternType] = useState<'linear' | 'circular'>('linear');
  const [spacing, setSpacing] = useState<number>(10);
  const [count, setCount] = useState<number>(3);
  const [direction, setDirection] = useState<'x' | 'y' | 'z'>('x');
  const [axis, setAxis] = useState<'x' | 'y' | 'z'>('z');
  const [angleStep, setAngleStep] = useState<number>(90);
  const [instances, setInstances] = useState<number>(4);

  const handleApply = () => {
    const patternFeature: PatternFeature = {
      id: `pattern-${Date.now()}`,
      name: `Pattern (${patternType})`,
      patternType,
      feature: featureId,
      ...(patternType === 'linear' && {
        direction,
        spacing,
        count,
      }),
      ...(patternType === 'circular' && {
        axis,
        angleStep,
        instances,
      }),
    };
    onApply(patternFeature);
  };

  return (
    <div className="feature-dialog pattern-dialog">
      <h3>Параметры массива (Pattern)</h3>

      <div className="param-group">
        <label htmlFor="patternType">Тип массива:</label>
        <select
          id="patternType"
          value={patternType}
          onChange={(e) => setPatternType(e.target.value as any)}
        >
          <option value="linear">Линейный</option>
          <option value="circular">Круговой</option>
        </select>
      </div>

      {patternType === 'linear' && (
        <>
          <div className="param-group">
            <label htmlFor="direction">Направление:</label>
            <select
              id="direction"
              value={direction}
              onChange={(e) => setDirection(e.target.value as any)}
            >
              <option value="x">Ось X</option>
              <option value="y">Ось Y</option>
              <option value="z">Ось Z</option>
            </select>
          </div>

          <div className="param-group">
            <label htmlFor="spacing">Шаг (мм): {spacing}</label>
            <input
              id="spacing"
              type="range"
              min="1"
              max="100"
              step="1"
              value={spacing}
              onChange={(e) => setSpacing(parseFloat(e.target.value))}
            />
            <input
              type="number"
              value={spacing}
              onChange={(e) => setSpacing(parseFloat(e.target.value))}
              min="0.1"
              max="1000"
              step="0.1"
            />
          </div>

          <div className="param-group">
            <label htmlFor="count">Количество копий: {count}</label>
            <input
              id="count"
              type="number"
              value={count}
              onChange={(e) => setCount(parseInt(e.target.value))}
              min="2"
              max="100"
            />
          </div>
        </>
      )}

      {patternType === 'circular' && (
        <>
          <div className="param-group">
            <label htmlFor="axis">Ось вращения:</label>
            <select
              id="axis"
              value={axis}
              onChange={(e) => setAxis(e.target.value as any)}
            >
              <option value="x">Ось X</option>
              <option value="y">Ось Y</option>
              <option value="z">Ось Z</option>
            </select>
          </div>

          <div className="param-group">
            <label htmlFor="angleStep">Угол между копиями (°): {angleStep}</label>
            <input
              id="angleStep"
              type="range"
              min="1"
              max="360"
              step="1"
              value={angleStep}
              onChange={(e) => setAngleStep(parseFloat(e.target.value))}
            />
            <input
              type="number"
              value={angleStep}
              onChange={(e) => setAngleStep(parseFloat(e.target.value))}
              min="1"
              max="360"
              step="0.5"
            />
          </div>

          <div className="param-group">
            <label htmlFor="instances">Количество копий: {instances}</label>
            <input
              id="instances"
              type="number"
              value={instances}
              onChange={(e) => setInstances(parseInt(e.target.value))}
              min="2"
              max="360"
            />
          </div>
        </>
      )}

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
