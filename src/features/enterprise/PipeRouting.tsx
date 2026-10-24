/**
 * Pipe Routing Automation - Автоматизированное проектирование маршрутов труб
 */

import React, { useState } from 'react';
import { PipeRoute, PipeFitting } from './types';

interface PipeRoutingProps {
  onGenerate: (route: PipeRoute) => void;
  onCancel: () => void;
}

export const PipeRouting: React.FC<PipeRoutingProps> = ({ onGenerate, onCancel }) => {
  const [startX, setStartX] = useState<number>(0);
  const [startY, setStartY] = useState<number>(0);
  const [startZ, setStartZ] = useState<number>(0);
  const [endX, setEndX] = useState<number>(100);
  const [endY, setEndY] = useState<number>(100);
  const [endZ, setEndZ] = useState<number>(100);
  const [diameter, setDiameter] = useState<number>(25.4);
  const [material, setMaterial] = useState<string>('steel');
  const [avoidanceMode, setAvoidanceMode] = useState<string>('automatic');
  const [isGenerating, setIsGenerating] = useState<boolean>(false);

  const standardDiameters = [
    { value: 6.35, label: '1/4" (6.35mm)' },
    { value: 12.7, label: '1/2" (12.7mm)' },
    { value: 19.05, label: '3/4" (19.05mm)' },
    { value: 25.4, label: '1" (25.4mm)' },
    { value: 31.75, label: '1-1/4" (31.75mm)' },
    { value: 38.1, label: '1-1/2" (38.1mm)' },
    { value: 50.8, label: '2" (50.8mm)' },
  ];

  const calculateRoute = () => {
    setIsGenerating(true);

    // ИМИТАЦИЯ расчёта маршрута
    setTimeout(() => {
      const startPoint: [number, number, number] = [startX, startY, startZ];
      const endPoint: [number, number, number] = [endX, endY, endZ];

      // Вычисляем прямое расстояние
      const dx = endX - startX;
      const dy = endY - startY;
      const dz = endZ - startZ;
      const directDistance = Math.sqrt(dx * dx + dy * dy + dz * dz);

      // ИМИТАЦИЯ: генерируем фиттинги
      const mockFittings: PipeFitting[] = [
        {
          id: 'fit-1',
          type: 'coupling',
          angle: 0,
          size: diameter,
          cost: diameter > 20 ? 15 : 10,
        },
        {
          id: 'fit-2',
          type: 'elbow',
          angle: 90,
          size: diameter,
          cost: diameter > 20 ? 25 : 15,
        },
        {
          id: 'fit-3',
          type: 'coupling',
          angle: 0,
          size: diameter,
          cost: diameter > 20 ? 15 : 10,
        },
      ];

      const mockRoute: PipeRoute = {
        id: `route-${Date.now()}`,
        name: `Маршрут труба ${diameter}мм`,
        startPoint,
        endPoint,
        diameter,
        material,
        fittings: mockFittings,
        length: directDistance * 1.2, // Добавляем 20% на изгибы
        bendCount: 2,
        cost: directDistance * 2 + mockFittings.reduce((sum, f) => sum + f.cost, 0),
      };

      onGenerate(mockRoute);
      setIsGenerating(false);
    }, 1500);
  };

  return (
    <div className="pipe-routing-dialog">
      <h3>Автоматизированное проектирование маршрутов труб</h3>

      <div className="routing-form">
        <div className="coordinates-section">
          <h4>Начальная точка (A)</h4>
          <div className="coords-grid">
            <div className="coord-input">
              <label>X (мм):</label>
              <input
                type="number"
                value={startX}
                onChange={(e) => setStartX(parseFloat(e.target.value))}
                disabled={isGenerating}
              />
            </div>
            <div className="coord-input">
              <label>Y (мм):</label>
              <input
                type="number"
                value={startY}
                onChange={(e) => setStartY(parseFloat(e.target.value))}
                disabled={isGenerating}
              />
            </div>
            <div className="coord-input">
              <label>Z (мм):</label>
              <input
                type="number"
                value={startZ}
                onChange={(e) => setStartZ(parseFloat(e.target.value))}
                disabled={isGenerating}
              />
            </div>
          </div>
        </div>

        <div className="coordinates-section">
          <h4>Конечная точка (B)</h4>
          <div className="coords-grid">
            <div className="coord-input">
              <label>X (мм):</label>
              <input
                type="number"
                value={endX}
                onChange={(e) => setEndX(parseFloat(e.target.value))}
                disabled={isGenerating}
              />
            </div>
            <div className="coord-input">
              <label>Y (мм):</label>
              <input
                type="number"
                value={endY}
                onChange={(e) => setEndY(parseFloat(e.target.value))}
                disabled={isGenerating}
              />
            </div>
            <div className="coord-input">
              <label>Z (мм):</label>
              <input
                type="number"
                value={endZ}
                onChange={(e) => setEndZ(parseFloat(e.target.value))}
                disabled={isGenerating}
              />
            </div>
          </div>
        </div>

        <div className="form-group">
          <label htmlFor="diameter">Стандартный диаметр:</label>
          <select
            id="diameter"
            value={diameter}
            onChange={(e) => setDiameter(parseFloat(e.target.value))}
            disabled={isGenerating}
          >
            {standardDiameters.map((d) => (
              <option key={d.value} value={d.value}>
                {d.label}
              </option>
            ))}
          </select>
        </div>

        <div className="form-group">
          <label htmlFor="material">Материал трубы:</label>
          <select
            id="material"
            value={material}
            onChange={(e) => setMaterial(e.target.value)}
            disabled={isGenerating}
          >
            <option value="steel">Сталь (DIN 2391)</option>
            <option value="stainless">Нержавеющая сталь (304)</option>
            <option value="aluminum">Алюминий (6063-T5)</option>
            <option value="copper">Медь (DIN 1652)</option>
            <option value="brass">Латунь (CW614N)</option>
          </select>
        </div>

        <div className="form-group">
          <label htmlFor="avoidance">Режим избежания препятствий:</label>
          <select
            id="avoidance"
            value={avoidanceMode}
            onChange={(e) => setAvoidanceMode(e.target.value)}
            disabled={isGenerating}
          >
            <option value="automatic">Автоматический</option>
            <option value="conservative">Консервативный</option>
            <option value="aggressive">Агрессивный (минимум поворотов)</option>
            <option value="shortest">Кратчайший путь</option>
          </select>
        </div>

        <button
          className="generate-button"
          onClick={calculateRoute}
          disabled={isGenerating}
        >
          {isGenerating ? (
            <>
              <span className="spinner" />
              Проектирование маршрута...
            </>
          ) : (
            '🔄 Автоматическое проектирование'
          )}
        </button>
      </div>

      <div className="dialog-buttons">
        <button className="btn-secondary" onClick={onCancel}>
          Закрыть
        </button>
      </div>

      <style>{`
        .pipe-routing-dialog {
          padding: 16px;
          background: white;
          border-radius: 8px;
          border: 1px solid #ddd;
          max-width: 500px;
        }

        .routing-form {
          margin: 16px 0;
        }

        .coordinates-section {
          margin-bottom: 16px;
          border: 1px solid #eee;
          border-radius: 4px;
          padding: 12px;
          background: #fafafa;
        }

        .coordinates-section h4 {
          margin: 0 0 12px 0;
          color: #333;
          font-size: 13px;
        }

        .coords-grid {
          display: grid;
          grid-template-columns: repeat(3, 1fr);
          gap: 8px;
        }

        .coord-input {
          display: flex;
          flex-direction: column;
        }

        .coord-input label {
          font-weight: 500;
          font-size: 11px;
          margin-bottom: 2px;
          color: #666;
        }

        .coord-input input {
          padding: 6px;
          border: 1px solid #ddd;
          border-radius: 3px;
          font-size: 12px;
        }

        .form-group {
          margin-bottom: 12px;
        }

        .form-group label {
          display: block;
          font-weight: 500;
          font-size: 12px;
          margin-bottom: 4px;
        }

        .form-group select {
          width: 100%;
          padding: 6px;
          border: 1px solid #ddd;
          border-radius: 3px;
          font-size: 12px;
        }

        .generate-button {
          width: 100%;
          padding: 12px;
          background: #ff9800;
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
          margin-bottom: 16px;
        }

        .generate-button:hover:not(:disabled) {
          background: #f57c00;
        }

        .generate-button:disabled {
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

        .dialog-buttons {
          display: flex;
          gap: 8px;
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
