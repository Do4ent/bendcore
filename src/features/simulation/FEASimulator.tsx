/**
 * FEA Simulator Module - ЗАГЛУШКА для демонстрации
 * Реальные FEA расчёты требуют специализированного ПО
 */

import React, { useState } from 'react';
import { FEAResult } from '../assembly/types';

interface FEASimulatorProps {
  onAnalyze: (result: FEAResult) => void;
  onCancel: () => void;
}

export const FEASimulator: React.FC<FEASimulatorProps> = ({ onAnalyze, onCancel }) => {
  const [simulationType, setSimulationType] = useState<'stress' | 'strain' | 'displacement'>('stress');
  const [meshSize, setMeshSize] = useState<number>(2);
  const [material, setMaterial] = useState<string>('steel');
  const [load, setLoad] = useState<number>(100);
  const [loadUnit, setLoadUnit] = useState<string>('N');
  const [results, setResults] = useState<FEAResult | null>(null);
  const [isRunning, setIsRunning] = useState<boolean>(false);
  const [progress, setProgress] = useState<number>(0);

  const simulationMaterials = {
    steel: { E: 210, density: 7850, name: 'Сталь (E=210GPa)' },
    aluminum: { E: 70, density: 2700, name: 'Алюминий (E=70GPa)' },
    copper: { E: 120, density: 8960, name: 'Медь (E=120GPa)' },
    titanium: { E: 100, density: 4500, name: 'Титан (E=100GPa)' },
  };

  const handleRunAnalysis = async () => {
    setIsRunning(true);
    setProgress(0);

    // ЗАГЛУШКА: Имитируем прогресс анализа
    for (let i = 0; i <= 100; i += 10) {
      await new Promise((resolve) => setTimeout(resolve, 200));
      setProgress(i);
    }

    // ЗАГЛУШКА: Генерируем случайные результаты
    const mockResult: FEAResult = {
      id: `fea-${Date.now()}`,
      name: `${simulationType.toUpperCase()} анализ`,
      type: simulationType,
      maxValue: Math.random() * 500 + 100,
      minValue: Math.random() * 50 + 5,
      avgValue: Math.random() * 200 + 50,
      unit: simulationType === 'displacement' ? 'мм' : 'МПа',
      colorMap: 'jet',
      meshSize,
      nodeCount: Math.floor(Math.random() * 50000 + 10000),
      elementCount: Math.floor(Math.random() * 30000 + 5000),
      executionTime: Math.random() * 2000 + 1000,
      status: 'completed',
      timestamp: new Date(),
      isStub: true,
    };

    setResults(mockResult);
    setIsRunning(false);
    setProgress(0);
    onAnalyze(mockResult);
  };

  return (
    <div className="fea-simulator-dialog">
      <h3>⚠️ FEA Симулятор (Демонстрационная заглушка)</h3>

      <div className="warning-banner">
        <strong>Внимание:</strong> Это демонстрационная заглушка. Реальные FEA расчёты требуют
        сложной численной математики. Результаты сгенерированы для демонстрации UI.
      </div>

      <div className="simulator-form">
        <div className="form-grid">
          <div className="form-group">
            <label htmlFor="simType">Тип анализа:</label>
            <select
              id="simType"
              value={simulationType}
              onChange={(e) => setSimulationType(e.target.value as any)}
              disabled={isRunning}
            >
              <option value="stress">Анализ напряжений (Stress)</option>
              <option value="strain">Анализ деформаций (Strain)</option>
              <option value="displacement">Перемещения (Displacement)</option>
            </select>
          </div>

          <div className="form-group">
            <label htmlFor="material">Материал:</label>
            <select
              id="material"
              value={material}
              onChange={(e) => setMaterial(e.target.value)}
              disabled={isRunning}
            >
              {Object.entries(simulationMaterials).map(([key, val]) => (
                <option key={key} value={key}>
                  {val.name}
                </option>
              ))}
            </select>
          </div>

          <div className="form-group">
            <label htmlFor="load">Нагрузка ({loadUnit}):</label>
            <input
              id="load"
              type="number"
              value={load}
              onChange={(e) => setLoad(parseFloat(e.target.value))}
              min="0"
              max="10000"
              step="10"
              disabled={isRunning}
            />
          </div>

          <div className="form-group">
            <label htmlFor="meshSize">Размер сетки (мм): {meshSize}</label>
            <input
              id="meshSize"
              type="range"
              min="0.5"
              max="10"
              step="0.5"
              value={meshSize}
              onChange={(e) => setMeshSize(parseFloat(e.target.value))}
              disabled={isRunning}
            />
          </div>
        </div>

        <button
          className="run-button"
          onClick={handleRunAnalysis}
          disabled={isRunning}
        >
          {isRunning ? (
            <>
              <span className="spinner" />
              Выполнение анализа... {progress}%
            </>
          ) : (
            '▶️ Запустить анализ'
          )}
        </button>

        {isRunning && (
          <div className="progress-bar">
            <div className="progress-fill" style={{ width: `${progress}%` }} />
          </div>
        )}
      </div>

      {results && (
        <div className="results-panel">
          <h4>Результаты анализа (Демо-данные)</h4>

          <div className="results-grid">
            <div className="result-card">
              <span className="label">Макс. {results.type}</span>
              <span className="value">{results.maxValue.toFixed(2)}</span>
              <span className="unit">{results.unit}</span>
            </div>

            <div className="result-card">
              <span className="label">Мин. значение</span>
              <span className="value">{results.minValue.toFixed(2)}</span>
              <span className="unit">{results.unit}</span>
            </div>

            <div className="result-card">
              <span className="label">Среднее</span>
              <span className="value">{results.avgValue.toFixed(2)}</span>
              <span className="unit">{results.unit}</span>
            </div>

            <div className="result-card">
              <span className="label">Узлов</span>
              <span className="value">{results.nodeCount.toLocaleString()}</span>
            </div>

            <div className="result-card">
              <span className="label">Элементов</span>
              <span className="value">{results.elementCount.toLocaleString()}</span>
            </div>

            <div className="result-card">
              <span className="label">Время расчёта</span>
              <span className="value">{results.executionTime.toFixed(0)}</span>
              <span className="unit">мс</span>
            </div>
          </div>

          <div className="warning-note">
            ⚠️ <strong>Важно:</strong> Эти результаты являются имитацией. Для производства
            используйте специализированное FEA ПО (ANSYS, ABAQUS, Nastran и т.д.)
          </div>
        </div>
      )}

      <div className="dialog-buttons">
        <button className="btn-secondary" onClick={onCancel}>
          Закрыть
        </button>
      </div>

      <style>{`
        .fea-simulator-dialog {
          padding: 16px;
          background: white;
          border-radius: 8px;
          border: 1px solid #ddd;
          max-width: 600px;
        }

        .warning-banner {
          background: #fff3cd;
          border: 1px solid #ffc107;
          border-radius: 4px;
          padding: 12px;
          margin: 12px 0;
          color: #856404;
          font-size: 12px;
        }

        .simulator-form {
          margin: 16px 0;
        }

        .form-grid {
          display: grid;
          grid-template-columns: repeat(2, 1fr);
          gap: 12px;
          margin-bottom: 16px;
        }

        .form-group {
          display: flex;
          flex-direction: column;
        }

        .form-group label {
          font-weight: 500;
          margin-bottom: 4px;
          font-size: 12px;
        }

        .form-group input,
        .form-group select {
          padding: 6px;
          border: 1px solid #ddd;
          border-radius: 3px;
          font-size: 12px;
        }

        .form-group input:disabled,
        .form-group select:disabled {
          background: #f5f5f5;
          cursor: not-allowed;
        }

        .run-button {
          width: 100%;
          padding: 12px;
          background: #2196f3;
          color: white;
          border: none;
          border-radius: 4px;
          cursor: pointer;
          font-weight: 500;
          font-size: 13px;
          display: flex;
          align-items: center;
          justify-content: center;
          gap: 8px;
          transition: background 0.2s;
        }

        .run-button:hover:not(:disabled) {
          background: #1976d2;
        }

        .run-button:disabled {
          opacity: 0.7;
          cursor: not-allowed;
        }

        .spinner {
          display: inline-block;
          width: 12px;
          height: 12px;
          border: 2px solid rgba(255, 255, 255, 0.3);
          border-radius: 50%;
          border-top-color: white;
          animation: spin 0.8s linear infinite;
        }

        @keyframes spin {
          to { transform: rotate(360deg); }
        }

        .progress-bar {
          width: 100%;
          height: 6px;
          background: #f0f0f0;
          border-radius: 3px;
          margin-top: 12px;
          overflow: hidden;
        }

        .progress-fill {
          height: 100%;
          background: #4caf50;
          transition: width 0.3s;
        }

        .results-panel {
          background: #f9f9f9;
          border: 1px solid #eee;
          border-radius: 4px;
          padding: 16px;
          margin: 16px 0;
        }

        .results-panel h4 {
          margin-top: 0;
          color: #333;
          margin-bottom: 12px;
        }

        .results-grid {
          display: grid;
          grid-template-columns: repeat(3, 1fr);
          gap: 12px;
          margin-bottom: 16px;
        }

        .result-card {
          background: white;
          border: 1px solid #ddd;
          border-radius: 4px;
          padding: 12px;
          text-align: center;
        }

        .result-card .label {
          display: block;
          font-size: 11px;
          color: #666;
          margin-bottom: 4px;
        }

        .result-card .value {
          display: block;
          font-size: 18px;
          font-weight: bold;
          color: #2196f3;
          margin-bottom: 2px;
        }

        .result-card .unit {
          display: block;
          font-size: 10px;
          color: #999;
        }

        .warning-note {
          background: #ffebee;
          border-left: 3px solid #f44336;
          padding: 12px;
          border-radius: 3px;
          font-size: 11px;
          color: #c62828;
          margin: 12px 0;
        }

        .dialog-buttons {
          display: flex;
          gap: 8px;
          margin-top: 16px;
        }

        .btn-secondary {
          flex: 1;
          padding: 8px;
          background: #f5f5f5;
          border: 1px solid #ddd;
          border-radius: 4px;
          cursor: pointer;
          font-weight: 500;
        }

        .btn-secondary:hover {
          background: #eeeeee;
        }
      `}</style>
    </div>
  );
};
