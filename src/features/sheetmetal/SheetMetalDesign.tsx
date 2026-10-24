/**
 * BendCore Sheet Metal Design Panel
 * Интерфейс для расчётов развёрток и припусков на изгиб
 */

import React, { useState } from 'react';
import {
  SheetMetalCalculator,
  SheetMetalFeature,
  UnfoldResult,
  MaterialType,
} from './SheetMetalCalculator';

interface SheetMetalDesignProps {
  onApply?: (feature: SheetMetalFeature, result: UnfoldResult) => void;
}

export const SheetMetalDesign: React.FC<SheetMetalDesignProps> = ({ onApply }) => {
  // Параметры листа
  const [thickness, setThickness] = useState(1.0);
  const [material, setMaterial] = useState<MaterialType>('steel');
  const [bendRadius, setBendRadius] = useState(2.0);
  const [bendAngle, setBendAngle] = useState(90);
  const [length1, setLength1] = useState(50);
  const [length2, setLength2] = useState(50);
  const [customKFactor, setCustomKFactor] = useState<number | null>(null);

  // Результаты
  const [unfoldResult, setUnfoldResult] = useState<UnfoldResult | null>(null);
  const [validation, setValidation] = useState<any>(null);
  const [activeTab, setActiveTab] = useState<'input' | 'results' | 'dfm'>('input');

  const handleCalculate = () => {
    const feature: SheetMetalFeature = {
      id: `sm-${Date.now()}`,
      name: `Лист ${thickness}мм ${material} изгиб ${bendAngle}°`,
      baseThickness: thickness,
      bendRadius,
      bendAngle,
      kFactor: customKFactor ?? undefined,
      material,
      createdAt: new Date(),
    };

    // Валидация
    const validation = SheetMetalCalculator.validateBend(feature);
    setValidation(validation);

    if (!validation.valid) {
      return;
    }

    // Расчёт
    const result = SheetMetalCalculator.unfoldPattern(feature, length1, length2);
    setUnfoldResult(result);
    setActiveTab('results');

    // Callback
    if (onApply) {
      onApply(feature, result);
    }
  };

  const minRadius = SheetMetalCalculator.getMinimumBendRadius(thickness, material);
  const kFactor = SheetMetalCalculator.getKFactor(thickness, material);
  const springbackAngle = SheetMetalCalculator.calculateSpringback(bendAngle, material);

  const materialLabels: Record<MaterialType, string> = {
    steel: 'Сталь (E=210 GPa)',
    aluminum: 'Алюминий (E=69 GPa)',
    stainless: 'Нержавейка (E=193 GPa)',
    copper: 'Медь (E=110 GPa)',
    titanium: 'Титан (E=103 GPa)',
  };

  return (
    <div className="sheet-metal-panel">
      <div className="panel-header">
        <h3>📋 Sheet Metal Design</h3>
        <span className="status-badge">✅ Production Ready</span>
      </div>

      <div className="tab-buttons">
        <button
          className={`tab-btn ${activeTab === 'input' ? 'active' : ''}`}
          onClick={() => setActiveTab('input')}
        >
          ⚙️ Параметры
        </button>
        <button
          className={`tab-btn ${activeTab === 'results' ? 'active' : ''}`}
          onClick={() => setActiveTab('results')}
          disabled={!unfoldResult}
        >
          📊 Результаты
        </button>
        <button
          className={`tab-btn ${activeTab === 'dfm' ? 'active' : ''}`}
          onClick={() => setActiveTab('dfm')}
        >
          🔍 DFM Check
        </button>
      </div>

      {activeTab === 'input' && (
        <div className="input-section">
          <div className="param-group">
            <label>Толщина материала (мм)</label>
            <div className="input-with-feedback">
              <input
                type="number"
                min="0.1"
                max="10"
                step="0.1"
                value={thickness}
                onChange={(e) => setThickness(parseFloat(e.target.value))}
              />
              <span className="unit">мм</span>
            </div>
          </div>

          <div className="param-group">
            <label>Материал</label>
            <select value={material} onChange={(e) => setMaterial(e.target.value as MaterialType)}>
              {Object.entries(materialLabels).map(([key, label]) => (
                <option key={key} value={key}>
                  {label}
                </option>
              ))}
            </select>
            <small className="hint">K-фактор: {kFactor.toFixed(4)}</small>
          </div>

          <div className="param-group">
            <label>Радиус изгиба (мм)</label>
            <div className="input-with-feedback">
              <input
                type="number"
                min="0.5"
                max="50"
                step="0.5"
                value={bendRadius}
                onChange={(e) => setBendRadius(parseFloat(e.target.value))}
              />
              <span className="unit">мм</span>
            </div>
            <small className="hint">
              Минимум: {minRadius.toFixed(2)} мм
              {bendRadius < minRadius && <span className="warning"> ❌ СЛИШКОМ МАЛЕНЬКИЙ</span>}
            </small>
          </div>

          <div className="param-group">
            <label>Угол изгиба (°)</label>
            <div className="slider-container">
              <input
                type="range"
                min="0"
                max="180"
                step="5"
                value={bendAngle}
                onChange={(e) => setBendAngle(parseFloat(e.target.value))}
              />
              <span className="value">{bendAngle}°</span>
            </div>
            <small className="hint">
              Пружинение: {springbackAngle.toFixed(2)}°
              {bendAngle > 170 && (
                <span className="warning"> ⚠️ БЛИЗКО К 180°</span>
              )}
            </small>
          </div>

          <div className="param-group">
            <label>Длина первой части (мм)</label>
            <input
              type="number"
              min="1"
              max="500"
              step="1"
              value={length1}
              onChange={(e) => setLength1(parseFloat(e.target.value))}
            />
          </div>

          <div className="param-group">
            <label>Длина второй части (мм)</label>
            <input
              type="number"
              min="1"
              max="500"
              step="1"
              value={length2}
              onChange={(e) => setLength2(parseFloat(e.target.value))}
            />
          </div>

          <div className="param-group">
            <label>Custom K-фактор (опционально)</label>
            <input
              type="number"
              min="0.1"
              max="0.5"
              step="0.01"
              placeholder="Оставить пустым для автоматического"
              value={customKFactor ?? ''}
              onChange={(e) => setCustomKFactor(e.target.value ? parseFloat(e.target.value) : null)}
            />
          </div>

          <button className="calculate-btn" onClick={handleCalculate}>
            📐 Рассчитать развёртку
          </button>

          {validation && !validation.valid && (
            <div className="error-box">
              <h4>❌ Ошибки валидации:</h4>
              <ul>
                {validation.warnings.map((w: string, i: number) => (
                  <li key={i}>{w}</li>
                ))}
              </ul>
            </div>
          )}

          {validation && validation.warnings.length > 0 && validation.valid && (
            <div className="warning-box">
              <h4>⚠️ Предупреждения:</h4>
              <ul>
                {validation.warnings.map((w: string, i: number) => (
                  <li key={i}>{w}</li>
                ))}
              </ul>
            </div>
          )}
        </div>
      )}

      {activeTab === 'results' && unfoldResult && (
        <div className="results-section">
          <h4>📊 Результаты расчёта</h4>

          <div className="results-grid">
            <div className="result-card primary">
              <span className="label">Развёрнутая длина</span>
              <span className="value">{unfoldResult.flatLength}</span>
              <span className="unit">мм</span>
            </div>

            <div className="result-card">
              <span className="label">Припуск на изгиб</span>
              <span className="value">{unfoldResult.bendAllowance}</span>
              <span className="unit">мм</span>
            </div>

            <div className="result-card">
              <span className="label">K-фактор</span>
              <span className="value">{unfoldResult.kFactorUsed}</span>
            </div>

            <div className="result-card">
              <span className="label">Угол пружинения</span>
              <span className="value">{unfoldResult.springbackAngle}</span>
              <span className="unit">°</span>
            </div>

            <div className="result-card">
              <span className="label">Мин. радиус изгиба</span>
              <span className="value">{unfoldResult.minimumBendRadius}</span>
              <span className="unit">мм</span>
            </div>

            <div className="result-card">
              <span className="label">Рекомендуемый радиус пуансона</span>
              <span className="value">{unfoldResult.recommendedPunchRadius}</span>
              <span className="unit">мм</span>
            </div>

            <div className="result-card">
              <span className="label">Потеря на рез (kerf)</span>
              <span className="value">{unfoldResult.materialLoss}</span>
              <span className="unit">мм</span>
            </div>

            <div className="result-card">
              <span className="label">Время обработки</span>
              <span className="value">{unfoldResult.processingTime}</span>
              <span className="unit">сек</span>
            </div>
          </div>

          <div className="space-info">
            <h5>💾 Требуемое пространство</h5>
            <p>
              {unfoldResult.spaceNeeded[0]} мм × {unfoldResult.spaceNeeded[1]} мм
            </p>
          </div>

          <button className="secondary-btn" onClick={() => setActiveTab('input')}>
            ← Назад к параметрам
          </button>
        </div>
      )}

      {activeTab === 'dfm' && (
        <div className="dfm-section">
          <h4>🔍 Design for Manufacturing Check</h4>

          <div className="dfm-checklist">
            <div className={`check-item ${bendRadius >= minRadius ? 'pass' : 'fail'}`}>
              <span className="status">{bendRadius >= minRadius ? '✅' : '❌'}</span>
              <span className="text">
                Радиус изгиба >= минимального ({bendRadius} >= {minRadius.toFixed(2)})
              </span>
            </div>

            <div className={`check-item ${bendAngle >= 0 && bendAngle <= 180 ? 'pass' : 'fail'}`}>
              <span className="status">{bendAngle >= 0 && bendAngle <= 180 ? '✅' : '❌'}</span>
              <span className="text">Угол изгиба в допустимом диапазоне (0-180°)</span>
            </div>

            <div className={`check-item ${bendAngle <= 170 ? 'pass' : 'warn'}`}>
              <span className="status">{bendAngle <= 170 ? '✅' : '⚠️'}</span>
              <span className="text">Угол не слишком близок к 180°</span>
            </div>

            <div className={`check-item ${bendRadius >= thickness ? 'pass' : 'warn'}`}>
              <span className="status">{bendRadius >= thickness ? '✅' : '⚠️'}</span>
              <span className="text">Радиус >= толщины материала</span>
            </div>

            <div className="check-item pass">
              <span className="status">✅</span>
              <span className="text">Материал поддерживается</span>
            </div>
          </div>

          <div className="dfm-recommendations">
            <h5>💡 Рекомендации</h5>
            <ul>
              <li>Использовать пуансон радиусом {SheetMetalCalculator.getRecommendedPunchRadius(thickness, material).toFixed(2)} мм</li>
              <li>Припусти ≈ {SheetMetalCalculator.calculateBendAllowance(bendRadius, thickness, bendAngle, kFactor).toFixed(2)} мм на изгиб</li>
              <li>Материал вернётся на {springbackAngle.toFixed(2)}° из-за пружинения</li>
              <li>Время обработки ≈ {SheetMetalCalculator.calculateProcessingTime(thickness, bendAngle, bendRadius).toFixed(1)} сек</li>
            </ul>
          </div>
        </div>
      )}

      <style>{`
        .sheet-metal-panel {
          background: #f5f5f5;
          border: 1px solid #ddd;
          border-radius: 6px;
          padding: 20px;
          margin: 10px 0;
          max-width: 900px;
        }

        .panel-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 20px;
          border-bottom: 2px solid #667eea;
          padding-bottom: 10px;
        }

        .panel-header h3 {
          margin: 0;
          color: #333;
        }

        .status-badge {
          background: #4caf50;
          color: white;
          padding: 4px 8px;
          border-radius: 4px;
          font-size: 0.85em;
        }

        .tab-buttons {
          display: flex;
          gap: 10px;
          margin-bottom: 20px;
        }

        .tab-btn {
          padding: 8px 16px;
          background: white;
          border: 2px solid #ddd;
          border-radius: 4px;
          cursor: pointer;
          font-weight: 500;
          transition: all 0.2s;
        }

        .tab-btn:hover:not(:disabled) {
          border-color: #667eea;
          color: #667eea;
        }

        .tab-btn.active {
          background: #667eea;
          color: white;
          border-color: #667eea;
        }

        .tab-btn:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }

        .input-section, .results-section, .dfm-section {
          display: flex;
          flex-direction: column;
          gap: 16px;
        }

        .param-group {
          display: flex;
          flex-direction: column;
          gap: 6px;
        }

        .param-group label {
          font-weight: 600;
          color: #333;
          font-size: 0.95em;
        }

        .param-group input[type="number"],
        .param-group select {
          padding: 8px 12px;
          border: 1px solid #ccc;
          border-radius: 4px;
          font-size: 1em;
        }

        .input-with-feedback {
          display: flex;
          align-items: center;
          gap: 8px;
        }

        .input-with-feedback input {
          flex: 1;
        }

        .unit {
          color: #666;
          font-weight: 500;
          min-width: 30px;
        }

        .hint {
          font-size: 0.85em;
          color: #666;
          margin-top: 4px;
        }

        .warning {
          color: #ff9800;
          font-weight: 600;
        }

        .slider-container {
          display: flex;
          align-items: center;
          gap: 12px;
        }

        .slider-container input[type="range"] {
          flex: 1;
        }

        .slider-container .value {
          min-width: 45px;
          font-weight: 600;
          color: #667eea;
        }

        .calculate-btn {
          padding: 12px 24px;
          background: #667eea;
          color: white;
          border: none;
          border-radius: 4px;
          font-size: 1em;
          font-weight: 600;
          cursor: pointer;
          transition: background 0.2s;
        }

        .calculate-btn:hover {
          background: #5568d3;
        }

        .secondary-btn {
          padding: 10px 16px;
          background: #f0f0f0;
          border: 1px solid #ccc;
          border-radius: 4px;
          cursor: pointer;
          font-weight: 500;
          transition: all 0.2s;
        }

        .secondary-btn:hover {
          background: #e0e0e0;
        }

        .error-box {
          background: #ffebee;
          border: 1px solid #ef5350;
          border-radius: 4px;
          padding: 12px;
          color: #c62828;
        }

        .warning-box {
          background: #fff3e0;
          border: 1px solid #ffb74d;
          border-radius: 4px;
          padding: 12px;
          color: #e65100;
        }

        .results-grid {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
          gap: 12px;
        }

        .result-card {
          background: white;
          border: 1px solid #ddd;
          border-radius: 6px;
          padding: 16px;
          display: flex;
          flex-direction: column;
          align-items: center;
          text-align: center;
        }

        .result-card.primary {
          background: #667eea;
          color: white;
          border-color: #667eea;
        }

        .result-card .label {
          font-size: 0.85em;
          opacity: 0.8;
          margin-bottom: 8px;
        }

        .result-card .value {
          font-size: 1.8em;
          font-weight: 700;
        }

        .result-card .unit {
          font-size: 0.9em;
          margin-top: 6px;
        }

        .space-info {
          background: white;
          border: 1px solid #ddd;
          border-radius: 6px;
          padding: 16px;
        }

        .space-info h5 {
          margin: 0 0 8px 0;
          color: #333;
        }

        .space-info p {
          margin: 0;
          font-size: 1.1em;
          font-weight: 600;
          color: #667eea;
        }

        .dfm-checklist {
          display: flex;
          flex-direction: column;
          gap: 10px;
        }

        .check-item {
          display: flex;
          align-items: center;
          gap: 12px;
          padding: 12px;
          background: white;
          border-left: 4px solid #ccc;
          border-radius: 4px;
        }

        .check-item.pass {
          border-left-color: #4caf50;
          background: #f1f8e9;
        }

        .check-item.warn {
          border-left-color: #ff9800;
          background: #fff3e0;
        }

        .check-item.fail {
          border-left-color: #f44336;
          background: #ffebee;
        }

        .check-item .status {
          font-size: 1.3em;
          min-width: 30px;
        }

        .dfm-recommendations {
          background: white;
          border: 1px solid #ddd;
          border-radius: 6px;
          padding: 16px;
        }

        .dfm-recommendations h5 {
          margin: 0 0 12px 0;
          color: #333;
        }

        .dfm-recommendations ul {
          margin: 0;
          padding-left: 20px;
        }

        .dfm-recommendations li {
          margin: 6px 0;
          color: #555;
        }
      `}</style>
    </div>
  );
};

export default SheetMetalDesign;
