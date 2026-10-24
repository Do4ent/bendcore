import React, { useState, useCallback, useRef, useEffect } from 'react';
import './Combined3DOffsetCalculator.css';
import { Three3DRenderer } from './services/Three3DRenderer';

// ============================================================================
// Типы данных
// ============================================================================

interface Point3D {
  x: number;
  y: number;
  z: number;
}

interface BendDetail {
  position: Point3D;
  angle_xy: number;
  angle_z: number;
  length: number;
  radius: number;
  springback_correction: number;
  bend_index: number;
}

interface SectionDetail {
  start: Point3D;
  end: Point3D;
  length: number;
  slope_percent: number;
  section_index: number;
}

interface CalculationResult {
  success: boolean;
  error_message: string;
  total_length: number;
  total_distance_3d: number;
  bends: BendDetail[];
  sections: SectionDetail[];
  trajectory_3d: Point3D[];
  cnc_code: string;
  feasibility_score: number;
  warnings: string[];
}

// ============================================================================
// Props интерфейс
// ============================================================================

interface Combined3DOffsetCalculatorProps {
  onResultsUpdate?: (results: CalculationResult) => void;
  onExportCNC?: (code: string) => void;
  darkMode?: boolean;
}

// ============================================================================
// React компонент
// ============================================================================

export const Combined3DOffsetCalculator: React.FC<Combined3DOffsetCalculatorProps> = ({
  onResultsUpdate,
  onExportCNC,
  darkMode = false,
}) => {
  // ========================================================================
  // State: Точки начала и конца
  // ========================================================================
  const [startPoint, setStartPoint] = useState<Point3D>({ x: 0, y: 0, z: 0 });
  const [endPoint, setEndPoint] = useState<Point3D>({ x: 100, y: 100, z: 50 });

  // ========================================================================
  // State: Параметры трубы
  // ========================================================================
  const [outerDiameter, setOuterDiameter] = useState<number>(50);
  const [wallThickness, setWallThickness] = useState<number>(2.0);
  const [material, setMaterial] = useState<string>('steel');

  // ========================================================================
  // State: Параметры смещения/гибки
  // ========================================================================
  const [numBends, setNumBends] = useState<number>(2);
  const [distribution, setDistribution] = useState<string>('equal');
  const [bendRadius, setBendRadius] = useState<number>(30);
  const [inclinationAngle, setInclinationAngle] = useState<number>(30);
  const [springbackCorrection, setSpringbackCorrection] = useState<boolean>(true);

  // ========================================================================
  // State: Custom распределение гибок
  // ========================================================================
  const [customPositions, setCustomPositions] = useState<number[]>([0.33, 0.66]);

  // ========================================================================
  // State: Результаты и UI
  // ========================================================================
  const [result, setResult] = useState<CalculationResult | null>(null);
  const [isLoading, setIsLoading] = useState<boolean>(false);
  const [activeTab, setActiveTab] = useState<'results' | 'preview' | 'cnc'>('results');
  const [showWarnings, setShowWarnings] = useState<boolean>(true);

  // ========================================================================
  // Refs для 3D рендеринга
  // ========================================================================
  const canvasRef = useRef<HTMLDivElement>(null);
  const rendererRef = useRef<Three3DRenderer | null>(null);

  // ========================================================================
  // Обработчики для Точки старта
  // ========================================================================
  const handleStartPointChange = useCallback((axis: 'x' | 'y' | 'z', value: number) => {
    setStartPoint((prev) => ({ ...prev, [axis]: value }));
  }, []);

  // ========================================================================
  // Обработчики для Точки финиша
  // ========================================================================
  const handleEndPointChange = useCallback((axis: 'x' | 'y' | 'z', value: number) => {
    setEndPoint((prev) => ({ ...prev, [axis]: value }));
  }, []);

  // ========================================================================
  // Обработчики для параметров трубы
  // ========================================================================
  const handleOuterDiameterChange = useCallback((value: number) => {
    setOuterDiameter(Math.max(1, Math.min(500, value)));
  }, []);

  const handleWallThicknessChange = useCallback((value: number) => {
    setWallThickness(Math.max(0.1, Math.min(50, value)));
  }, []);

  // ========================================================================
  // Обработчики для параметров гибки
  // ========================================================================
  const handleNumBendsChange = useCallback((value: number) => {
    const newNum = Math.max(2, Math.min(5, value));
    setNumBends(newNum);

    // Auto-generate new custom positions if needed
    if (distribution === 'custom') {
      const newPositions = [];
      for (let i = 1; i <= newNum; ++i) {
        newPositions.push(i / (newNum + 1));
      }
      setCustomPositions(newPositions);
    }
  }, [distribution]);

  const handleBendRadiusChange = useCallback((value: number) => {
    setBendRadius(Math.max(5, Math.min(1000, value)));
  }, []);

  const handleInclinationAngleChange = useCallback((value: number) => {
    setInclinationAngle(Math.max(-85, Math.min(85, value)));
  }, []);

  // ========================================================================
  // Обработчик для custom позиций
  // ========================================================================
  const handleCustomPositionChange = useCallback((index: number, value: number) => {
    const newPositions = [...customPositions];
    newPositions[index] = Math.max(0, Math.min(1, value));
    setCustomPositions(newPositions);
  }, [customPositions]);

  // ========================================================================
  // Основной метод расчёта
  // ========================================================================
  const handleCalculate = useCallback(async () => {
    setIsLoading(true);
    try {
      // API call к backend для расчёта
      const params = {
        start_point: startPoint,
        end_point: endPoint,
        outer_diameter: outerDiameter,
        wall_thickness: wallThickness,
        bend_radius: bendRadius,
        num_bends: numBends,
        distribution: distribution,
        inclination_angle: inclinationAngle,
        springback_correction: springbackCorrection,
        material: material,
        custom_bend_positions: distribution === 'custom' ? customPositions : [],
      };

      const response = await fetch('/api/bendcore/combined-3d-offset/calculate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(params),
      });

      if (!response.ok) {
        throw new Error(`API error: ${response.statusText}`);
      }

      const data: CalculationResult = await response.json();
      setResult(data);

      if (onResultsUpdate) {
        onResultsUpdate(data);
      }

      // Обновление 3D визуализации
      if (data.success && data.trajectory_3d.length > 0) {
        if (canvasRef.current) {
          if (!rendererRef.current) {
            rendererRef.current = new Three3DRenderer(canvasRef.current);
          }
          rendererRef.current.renderTube(
            startPoint,
            endPoint,
            data.bends,
            outerDiameter,
            wallThickness
          );
        }
      }

      // Автоматическое переключение на вкладку результатов
      setActiveTab('preview');
    } catch (error) {
      console.error('Calculation error:', error);
      setResult({
        success: false,
        error_message: `Ошибка расчёта: ${error instanceof Error ? error.message : 'Unknown error'}`,
        total_length: 0,
        total_distance_3d: 0,
        bends: [],
        sections: [],
        trajectory_3d: [],
        cnc_code: '',
        feasibility_score: 0,
        warnings: [],
      });
    } finally {
      setIsLoading(false);
    }
  }, [
    startPoint,
    endPoint,
    outerDiameter,
    wallThickness,
    bendRadius,
    numBends,
    distribution,
    inclinationAngle,
    springbackCorrection,
    material,
    customPositions,
    onResultsUpdate,
  ]);

  // ========================================================================
  // Экспорт CNC кода
  // ========================================================================
  const handleExportCNC = useCallback(() => {
    if (result?.cnc_code) {
      const element = document.createElement('a');
      element.setAttribute('href', `data:text/plain;charset=utf-8,${encodeURIComponent(result.cnc_code)}`);
      element.setAttribute('download', 'tube_bend.nc');
      element.style.display = 'none';
      document.body.appendChild(element);
      element.click();
      document.body.removeChild(element);

      if (onExportCNC) {
        onExportCNC(result.cnc_code);
      }
    }
  }, [result?.cnc_code, onExportCNC]);

  // ========================================================================
  // Экспорт как PDF (заглушка)
  // ========================================================================
  const handleExportPDF = useCallback(() => {
    if (result?.success) {
      console.log('PDF export would be implemented here');
      alert('PDF экспорт будет реализован');
    }
  }, [result]);

  // ========================================================================
  // Сохранение как шаблон
  // ========================================================================
  const handleSaveTemplate = useCallback(() => {
    const template = {
      startPoint,
      endPoint,
      outerDiameter,
      wallThickness,
      material,
      numBends,
      distribution,
      bendRadius,
      inclinationAngle,
      springbackCorrection,
      customPositions,
    };

    const element = document.createElement('a');
    element.setAttribute(
      'href',
      `data:application/json;charset=utf-8,${encodeURIComponent(JSON.stringify(template, null, 2))}`
    );
    element.setAttribute('download', 'bend_template.json');
    element.style.display = 'none';
    document.body.appendChild(element);
    element.click();
    document.body.removeChild(element);
  }, [
    startPoint,
    endPoint,
    outerDiameter,
    wallThickness,
    material,
    numBends,
    distribution,
    bendRadius,
    inclinationAngle,
    springbackCorrection,
    customPositions,
  ]);

  // ========================================================================
  // Очистка при размонтировании компонента
  // ========================================================================
  useEffect(() => {
    return () => {
      if (rendererRef.current) {
        rendererRef.current.dispose();
      }
    };
  }, []);

  // ========================================================================
  // Рендеринг компонента
  // ========================================================================
  return (
    <div className={`combined-3d-offset-calculator ${darkMode ? 'dark-mode' : ''}`}>
      {/* ====================================================================
          ВЕРХНЯЯ ПАНЕЛЬ: Заголовок и управление
          ==================================================================== */}
      <div className="header-section">
        <h1>3D Offset Calculator</h1>
        <p className="subtitle">Комбинированный расчёт смещения трубопровода в 3D</p>
      </div>

      {/* ====================================================================
          ОСНОВНОЙ КОНТЕНТ: Два столбца
          ==================================================================== */}
      <div className="main-content">
        {/* Левый столбец: Входные параметры */}
        <div className="input-panel">
          {/* Раздел 1: Точка старта */}
          <div className="input-section">
            <h3>Точка старта (Start Point)</h3>
            <div className="point-input-group">
              <div className="coordinate-input">
                <label htmlFor="start-x">X (мм)</label>
                <input
                  id="start-x"
                  type="number"
                  value={startPoint.x}
                  onChange={(e) => handleStartPointChange('x', parseFloat(e.target.value) || 0)}
                  placeholder="0"
                />
              </div>
              <div className="coordinate-input">
                <label htmlFor="start-y">Y (мм)</label>
                <input
                  id="start-y"
                  type="number"
                  value={startPoint.y}
                  onChange={(e) => handleStartPointChange('y', parseFloat(e.target.value) || 0)}
                  placeholder="0"
                />
              </div>
              <div className="coordinate-input">
                <label htmlFor="start-z">Z (мм)</label>
                <input
                  id="start-z"
                  type="number"
                  value={startPoint.z}
                  onChange={(e) => handleStartPointChange('z', parseFloat(e.target.value) || 0)}
                  placeholder="0"
                />
              </div>
            </div>
          </div>

          {/* Раздел 2: Точка финиша */}
          <div className="input-section">
            <h3>Точка финиша (End Point)</h3>
            <div className="point-input-group">
              <div className="coordinate-input">
                <label htmlFor="end-x">X (мм)</label>
                <input
                  id="end-x"
                  type="number"
                  value={endPoint.x}
                  onChange={(e) => handleEndPointChange('x', parseFloat(e.target.value) || 0)}
                  placeholder="100"
                />
              </div>
              <div className="coordinate-input">
                <label htmlFor="end-y">Y (мм)</label>
                <input
                  id="end-y"
                  type="number"
                  value={endPoint.y}
                  onChange={(e) => handleEndPointChange('y', parseFloat(e.target.value) || 0)}
                  placeholder="100"
                />
              </div>
              <div className="coordinate-input">
                <label htmlFor="end-z">Z (мм)</label>
                <input
                  id="end-z"
                  type="number"
                  value={endPoint.z}
                  onChange={(e) => handleEndPointChange('z', parseFloat(e.target.value) || 0)}
                  placeholder="50"
                />
              </div>
            </div>
          </div>

          {/* Раздел 3: Параметры трубы */}
          <div className="input-section">
            <h3>Параметры трубы</h3>
            <div className="form-group">
              <label htmlFor="od">Наружный диаметр (мм)</label>
              <input
                id="od"
                type="number"
                min="1"
                max="500"
                value={outerDiameter}
                onChange={(e) => handleOuterDiameterChange(parseFloat(e.target.value) || 50)}
              />
              <span className="unit">мм</span>
            </div>

            <div className="form-group">
              <label htmlFor="wt">Толщина стенки (мм)</label>
              <input
                id="wt"
                type="number"
                min="0.1"
                max="50"
                step="0.1"
                value={wallThickness}
                onChange={(e) => handleWallThicknessChange(parseFloat(e.target.value) || 2)}
              />
              <span className="unit">мм</span>
            </div>

            <div className="form-group">
              <label htmlFor="material">Материал</label>
              <select value={material} onChange={(e) => setMaterial(e.target.value)}>
                <option value="steel">Сталь (Steel)</option>
                <option value="aluminum">Алюминий (Aluminum)</option>
                <option value="copper">Медь (Copper)</option>
              </select>
            </div>
          </div>

          {/* Раздел 4: Параметры смещения */}
          <div className="input-section">
            <h3>Параметры смещения</h3>
            <div className="form-group">
              <label htmlFor="num-bends">Количество гибок</label>
              <div className="slider-group">
                <input
                  id="num-bends"
                  type="range"
                  min="2"
                  max="5"
                  value={numBends}
                  onChange={(e) => handleNumBendsChange(parseInt(e.target.value))}
                />
                <span className="slider-value">{numBends}</span>
              </div>
            </div>

            <div className="form-group">
              <label htmlFor="bend-radius">Радиус гибки (мм)</label>
              <input
                id="bend-radius"
                type="number"
                min="5"
                max="1000"
                value={bendRadius}
                onChange={(e) => handleBendRadiusChange(parseFloat(e.target.value) || 30)}
              />
              <span className="unit">мм</span>
            </div>

            <div className="form-group">
              <label htmlFor="inclination">Угол наклона</label>
              <div className="slider-group">
                <input
                  id="inclination"
                  type="range"
                  min="-85"
                  max="85"
                  value={inclinationAngle}
                  onChange={(e) => handleInclinationAngleChange(parseFloat(e.target.value) || 0)}
                />
                <span className="slider-value">{inclinationAngle.toFixed(1)}°</span>
              </div>
            </div>

            <div className="form-group checkbox">
              <input
                id="springback"
                type="checkbox"
                checked={springbackCorrection}
                onChange={(e) => setSpringbackCorrection(e.target.checked)}
              />
              <label htmlFor="springback">Коррекция пружинения</label>
            </div>

            <div className="form-group">
              <label htmlFor="distribution">Распределение гибок</label>
              <div className="radio-group">
                <label>
                  <input
                    type="radio"
                    value="equal"
                    checked={distribution === 'equal'}
                    onChange={(e) => setDistribution(e.target.value)}
                  />
                  Равномерное (Equal)
                </label>
                <label>
                  <input
                    type="radio"
                    value="custom"
                    checked={distribution === 'custom'}
                    onChange={(e) => setDistribution(e.target.value)}
                  />
                  Пользовательское (Custom)
                </label>
              </div>
            </div>

            {distribution === 'custom' && (
              <div className="custom-positions">
                <h4>Позиции гибок (0.0 - 1.0)</h4>
                {customPositions.map((pos, idx) => (
                  <div key={idx} className="position-input">
                    <label>Гибка {idx + 1}</label>
                    <input
                      type="number"
                      min="0"
                      max="1"
                      step="0.01"
                      value={pos.toFixed(2)}
                      onChange={(e) => handleCustomPositionChange(idx, parseFloat(e.target.value) || 0)}
                    />
                  </div>
                ))}
              </div>
            )}
          </div>

          {/* Кнопка Calculate */}
          <div className="action-buttons">
            <button
              className="btn-primary"
              onClick={handleCalculate}
              disabled={isLoading}
            >
              {isLoading ? 'Расчёт...' : 'Рассчитать'}
            </button>
            <button
              className="btn-secondary"
              onClick={handleSaveTemplate}
            >
              Сохранить как шаблон
            </button>
          </div>
        </div>

        {/* Правый столбец: Результаты и визуализация */}
        <div className="output-panel">
          {result ? (
            <>
              {/* Вкладки результатов */}
              <div className="tabs">
                <button
                  className={`tab-button ${activeTab === 'results' ? 'active' : ''}`}
                  onClick={() => setActiveTab('results')}
                >
                  Результаты
                </button>
                <button
                  className={`tab-button ${activeTab === 'preview' ? 'active' : ''}`}
                  onClick={() => setActiveTab('preview')}
                >
                  3D Предпросмотр
                </button>
                <button
                  className={`tab-button ${activeTab === 'cnc' ? 'active' : ''}`}
                  onClick={() => setActiveTab('cnc')}
                >
                  G-код
                </button>
              </div>

              {/* Содержимое вкладок */}
              <div className="tab-content">
                {/* Вкладка: Результаты */}
                {activeTab === 'results' && (
                  <div className="results-tab">
                    {result.success ? (
                      <>
                        {/* Основные значения */}
                        <div className="result-summary">
                          <div className="summary-item">
                            <span className="label">Общая длина:</span>
                            <span className="value">{result.total_length.toFixed(2)} мм</span>
                          </div>
                          <div className="summary-item">
                            <span className="label">3D расстояние:</span>
                            <span className="value">{result.total_distance_3d.toFixed(2)} мм</span>
                          </div>
                          <div className="summary-item">
                            <span className="label">Feasibility:</span>
                            <span className={`value feasibility-${Math.round(result.feasibility_score / 20)}`}>
                              {result.feasibility_score.toFixed(1)}%
                            </span>
                          </div>
                          <div className="summary-item">
                            <span className="label">Гибок:</span>
                            <span className="value">{result.bends.length}</span>
                          </div>
                        </div>

                        {/* Предупреждения */}
                        {result.warnings.length > 0 && showWarnings && (
                          <div className="warnings-section">
                            <h4>⚠ Предупреждения</h4>
                            <ul>
                              {result.warnings.map((warning, idx) => (
                                <li key={idx}>{warning}</li>
                              ))}
                            </ul>
                          </div>
                        )}

                        {/* Таблица гибок */}
                        {result.bends.length > 0 && (
                          <div className="bends-table">
                            <h4>Детали гибок</h4>
                            <table>
                              <thead>
                                <tr>
                                  <th>#</th>
                                  <th>X (мм)</th>
                                  <th>Y (мм)</th>
                                  <th>Z (мм)</th>
                                  <th>Радиус (мм)</th>
                                  <th>Угол XY (°)</th>
                                </tr>
                              </thead>
                              <tbody>
                                {result.bends.map((bend) => (
                                  <tr key={bend.bend_index}>
                                    <td>{bend.bend_index}</td>
                                    <td>{bend.position.x.toFixed(1)}</td>
                                    <td>{bend.position.y.toFixed(1)}</td>
                                    <td>{bend.position.z.toFixed(1)}</td>
                                    <td>{bend.radius.toFixed(2)}</td>
                                    <td>{bend.angle_xy.toFixed(1)}</td>
                                  </tr>
                                ))}
                              </tbody>
                            </table>
                          </div>
                        )}

                        {/* Кнопки экспорта */}
                        <div className="export-buttons">
                          <button className="btn-export" onClick={handleExportCNC}>
                            📥 G-код
                          </button>
                          <button className="btn-export" onClick={handleExportPDF}>
                            📄 PDF
                          </button>
                        </div>
                      </>
                    ) : (
                      <div className="error-message">
                        <strong>❌ Ошибка:</strong> {result.error_message}
                      </div>
                    )}
                  </div>
                )}

                {/* Вкладка: 3D Предпросмотр */}
                {activeTab === 'preview' && result.success && (
                  <div className="preview-tab">
                    <div className="canvas-container" ref={canvasRef} />
                    <div className="preview-info">
                      <p>Используйте мышь для вращения и масштабирования</p>
                      <p>Left Click + Drag = Вращение</p>
                      <p>Scroll = Масштабирование</p>
                    </div>
                  </div>
                )}

                {/* Вкладка: G-код */}
                {activeTab === 'cnc' && result.success && (
                  <div className="cnc-tab">
                    <textarea
                      className="cnc-code-textarea"
                      value={result.cnc_code}
                      readOnly
                    />
                    <button className="btn-copy" onClick={() => {
                      navigator.clipboard.writeText(result.cnc_code);
                      alert('G-код скопирован в буфер обмена');
                    }}>
                      📋 Копировать
                    </button>
                  </div>
                )}
              </div>
            </>
          ) : (
            <div className="empty-state">
              <p>Нажмите "Рассчитать" для отображения результатов</p>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default Combined3DOffsetCalculator;
