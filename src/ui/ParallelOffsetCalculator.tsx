import React, { useState, useEffect, useCallback } from 'react';
import './ParallelOffsetCalculator.css';

// Интерфейсы для типизации
interface CalculationParameters {
  outer_diameter: number;
  wall_thickness: number;
  bend_angle: number;
  offset_distance: number;
  bend_radius: number;
  material: string;
}

interface CalculationResult {
  total_length: number;
  bend_1_length: number;
  straight_section: number;
  bend_2_length: number;
  bend_angles: [number, number];
  bend_1_pos: [number, number, number];
  bend_2_pos: [number, number, number];
  confidence: number;
  warnings: string;
  is_valid: boolean;
  offset_distance?: number;
}

interface ValidationError {
  code: string;
  message: string;
  parameter: string;
}

interface ParallelOffsetCalculatorProps {
  onResultsUpdate?: (results: CalculationResult) => void;
  onSaveTemplate?: (name: string) => void;
  initialParams?: Partial<CalculationParameters>;
}

// Стандартные значения диаметров (дюймы → мм)
const STANDARD_DIAMETERS = [
  { label: '1/4"', value: 6.35 },
  { label: '3/8"', value: 9.53 },
  { label: '1/2"', value: 12.7 },
  { label: '5/8"', value: 15.88 },
  { label: '3/4"', value: 19.05 },
  { label: '1"', value: 25.4 },
  { label: '1.25"', value: 31.75 },
  { label: '1.5"', value: 38.1 },
  { label: '2"', value: 50.8 },
];

const STANDARD_WALL_THICKNESS = [0.5, 0.71, 0.83, 1.0, 1.24, 1.5, 1.65, 2.0, 2.4];
const MATERIALS = ['Steel', 'Stainless Steel', 'Aluminum', 'Copper', 'Titanium', 'Brass'];

export const ParallelOffsetCalculator: React.FC<ParallelOffsetCalculatorProps> = ({
  onResultsUpdate,
  onSaveTemplate,
  initialParams,
}) => {
  // Состояние параметров
  const [params, setParams] = useState<CalculationParameters>({
    outer_diameter: initialParams?.outer_diameter || 20.0,
    wall_thickness: initialParams?.wall_thickness || 2.0,
    bend_angle: initialParams?.bend_angle || 90.0,
    offset_distance: initialParams?.offset_distance || 30.0,
    bend_radius: initialParams?.bend_radius || 50.0,
    material: initialParams?.material || 'Steel',
  });

  // Состояние результатов и ошибок
  const [result, setResult] = useState<CalculationResult | null>(null);
  const [errors, setErrors] = useState<ValidationError[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const [isDarkMode, setIsDarkMode] = useState(false);
  const [templateName, setTemplateName] = useState('');
  const [showPreview2D, setShowPreview2D] = useState(true);
  const [exportFormat, setExportFormat] = useState<'pdf' | 'csv' | 'dxf'>('pdf');

  // Проверка тёмного режима
  useEffect(() => {
    const isDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
    setIsDarkMode(isDark);
  }, []);

  // Обработчик изменения параметров
  const handleParameterChange = useCallback(
    (key: keyof CalculationParameters, value: string | number) => {
      setParams(prev => ({
        ...prev,
        [key]: typeof value === 'string' ? parseFloat(value) || 0 : value,
      }));
      // Очистить ошибки при изменении
      setErrors([]);
    },
    []
  );

  // Валидация на клиенте
  const validateLocally = (): boolean => {
    const localErrors: ValidationError[] = [];

    if (params.outer_diameter < 3.0 || params.outer_diameter > 500.0) {
      localErrors.push({
        code: 'INVALID_OD',
        message: 'Диаметр должен быть в диапазоне 3-500 мм',
        parameter: 'outer_diameter',
      });
    }

    if (params.wall_thickness <= 0 || params.wall_thickness >= params.outer_diameter / 2) {
      localErrors.push({
        code: 'INVALID_WALL',
        message: 'Некорректная толщина стенки',
        parameter: 'wall_thickness',
      });
    }

    if (params.bend_angle < 5.0 || params.bend_angle > 180.0) {
      localErrors.push({
        code: 'INVALID_ANGLE',
        message: 'Угол должен быть в диапазоне 5-180°',
        parameter: 'bend_angle',
      });
    }

    if (params.offset_distance <= 0 || params.offset_distance > params.outer_diameter * 10) {
      localErrors.push({
        code: 'INVALID_OFFSET',
        message: 'Смещение должно быть положительным и не более 10*OD',
        parameter: 'offset_distance',
      });
    }

    if (params.bend_radius <= 0) {
      localErrors.push({
        code: 'INVALID_RADIUS',
        message: 'Радиус гибки должен быть положительным',
        parameter: 'bend_radius',
      });
    }

    setErrors(localErrors);
    return localErrors.length === 0;
  };

  // Расчёт
  const handleCalculate = useCallback(async () => {
    if (!validateLocally()) {
      return;
    }

    setIsLoading(true);
    try {
      const response = await fetch('/api/bendcore/parallel-offset/calculate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(params),
      });

      if (!response.ok) {
        throw new Error('Ошибка при расчёте');
      }

      const data: CalculationResult = await response.json();
      setResult(data);
      onResultsUpdate?.(data);
    } catch (error) {
      console.error('Ошибка:', error);
      setErrors([{
        code: 'API_ERROR',
        message: 'Ошибка при обращении к серверу',
        parameter: '',
      }]);
    } finally {
      setIsLoading(false);
    }
  }, [params, onResultsUpdate]);

  // Обработчик горячей клавиши (Enter)
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Enter' && !isLoading && errors.length === 0) {
        handleCalculate();
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [handleCalculate, isLoading, errors]);

  // Экспорт результатов
  const handleExport = async () => {
    if (!result) return;

    setIsLoading(true);
    try {
      const response = await fetch('/api/bendcore/parallel-offset/export', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          calculation_result: result,
          parameters: params,
          format: exportFormat,
        }),
      });

      if (!response.ok) {
        throw new Error('Ошибка при экспорте');
      }

      // Скачиваем файл
      const blob = await response.blob();
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `parallel-offset-${new Date().toISOString().slice(0, 10)}.${exportFormat}`;
      document.body.appendChild(a);
      a.click();
      window.URL.revokeObjectURL(url);
      document.body.removeChild(a);
    } catch (error) {
      console.error('Ошибка при экспорте:', error);
    } finally {
      setIsLoading(false);
    }
  };

  // Сохранение как шаблон
  const handleSaveTemplate = async () => {
    if (!templateName.trim()) {
      alert('Введите имя шаблона');
      return;
    }

    onSaveTemplate?.(templateName);
    setTemplateName('');
    alert('Шаблон сохранён');
  };

  // Рисование 2D диаграммы
  const draw2DPreview = () => {
    if (!result) return null;

    const width = 400;
    const height = 300;
    const padding = 40;
    const scale = 1; // пиксели на мм

    const x1 = padding;
    const x2 = padding + result.bend_1_length * scale;
    const x3 = x2 + result.straight_section * scale;
    const x4 = x3 + result.bend_2_length * scale;

    const y_center = height / 2;

    return (
      <svg
        width={width}
        height={height}
        className="preview-2d"
        viewBox={`0 0 ${width} ${height}`}
      >
        {/* Сетка */}
        <defs>
          <pattern id="grid" width="50" height="50" patternUnits="userSpaceOnUse">
            <path d="M 50 0 L 0 0 0 50" fill="none" stroke={isDarkMode ? '#333' : '#eee'} strokeWidth="0.5" />
          </pattern>
        </defs>
        <rect width={width} height={height} fill="url(#grid)" />

        {/* Первая гибка */}
        <path
          d={`M ${x1} ${y_center} A ${result.bend_1_length * scale} ${result.bend_1_length * scale} 0 0 1 ${x2} ${y_center - (result.offset_distance ?? 0) * scale * 0.3}`}
          stroke={isDarkMode ? '#4da6ff' : '#0066cc'}
          strokeWidth="3"
          fill="none"
        />

        {/* Прямой участок */}
        <line
          x1={x2}
          y1={y_center - (result.offset_distance ?? 0) * scale * 0.3}
          x2={x3}
          y2={y_center - (result.offset_distance ?? 0) * scale * 0.3}
          stroke={isDarkMode ? '#66ff66' : '#00cc00'}
          strokeWidth="3"
        />

        {/* Вторая гибка */}
        <path
          d={`M ${x3} ${y_center - (result.offset_distance ?? 0) * scale * 0.3} A ${result.bend_2_length * scale} ${result.bend_2_length * scale} 0 0 1 ${x4} ${y_center}`}
          stroke={isDarkMode ? '#ff99ff' : '#cc00cc'}
          strokeWidth="3"
          fill="none"
        />

        {/* Размерные линии */}
        <text x={x1} y={y_center + 25} fontSize="10" fill={isDarkMode ? '#aaa' : '#666'}>
          Гибка 1
        </text>
        <text x={(x2 + x3) / 2} y={y_center + 25} fontSize="10" fill={isDarkMode ? '#aaa' : '#666'}>
          Прямой
        </text>
        <text x={x3} y={y_center + 25} fontSize="10" fill={isDarkMode ? '#aaa' : '#666'}>
          Гибка 2
        </text>
      </svg>
    );
  };

  const hasErrors = errors.length > 0;
  const getFieldError = (param: keyof CalculationParameters) =>
    errors.find(e => e.parameter === param);

  return (
    <div className={`parallel-offset-calculator ${isDarkMode ? 'dark-mode' : 'light-mode'}`}>
      <div className="calculator-header">
        <h2>Калькулятор параллельного смещения трубопровода</h2>
        <label className="dark-mode-toggle">
          <input
            type="checkbox"
            checked={isDarkMode}
            onChange={e => setIsDarkMode(e.target.checked)}
          />
          {isDarkMode ? '🌙 Тёмный режим' : '☀️ Светлый режим'}
        </label>
      </div>

      {/* Ошибки валидации */}
      {hasErrors && (
        <div className="error-section" role="alert">
          <h4>Ошибки валидации:</h4>
          <ul>
            {errors.map((err, idx) => (
              <li key={idx}>
                <strong>{err.parameter}:</strong> {err.message}
              </li>
            ))}
          </ul>
        </div>
      )}

      <div className="calculator-content">
        {/* РАЗДЕЛ 1: Параметры трубы */}
        <section className="parameters-section">
          <h3>Параметры трубы</h3>

          <div className="param-group">
            <label htmlFor="od">Внешний диаметр (мм)</label>
            <div className="input-with-dropdown">
              <input
                id="od"
                type="number"
                value={params.outer_diameter}
                onChange={e => handleParameterChange('outer_diameter', e.target.value)}
                className={getFieldError('outer_diameter') ? 'input-error' : ''}
                aria-label="Внешний диаметр трубы"
                aria-invalid={!!getFieldError('outer_diameter')}
              />
              <select
                onChange={e => handleParameterChange('outer_diameter', e.target.value)}
                title="Выбрать стандартный размер"
              >
                <option value="">Стандартные</option>
                {STANDARD_DIAMETERS.map(d => (
                  <option key={d.value} value={d.value}>
                    {d.label} ({d.value.toFixed(2)} мм)
                  </option>
                ))}
              </select>
            </div>
            {getFieldError('outer_diameter') && (
              <span className="error-text">{getFieldError('outer_diameter')?.message}</span>
            )}
          </div>

          <div className="param-group">
            <label htmlFor="wall">Толщина стенки (мм)</label>
            <div className="input-with-dropdown">
              <input
                id="wall"
                type="number"
                step="0.1"
                value={params.wall_thickness}
                onChange={e => handleParameterChange('wall_thickness', e.target.value)}
                className={getFieldError('wall_thickness') ? 'input-error' : ''}
                aria-label="Толщина стенки трубы"
              />
              <select
                onChange={e => handleParameterChange('wall_thickness', e.target.value)}
                title="Выбрать стандартную толщину"
              >
                <option value="">Стандартные</option>
                {STANDARD_WALL_THICKNESS.map(w => (
                  <option key={w} value={w}>
                    {w.toFixed(2)} мм
                  </option>
                ))}
              </select>
            </div>
          </div>

          <div className="param-group">
            <label htmlFor="material">Материал</label>
            <select
              id="material"
              value={params.material}
              onChange={e => handleParameterChange('material', e.target.value)}
              aria-label="Материал трубы"
            >
              {MATERIALS.map(m => (
                <option key={m} value={m}>
                  {m}
                </option>
              ))}
            </select>
          </div>
        </section>

        {/* РАЗДЕЛ 2: Параметры смещения */}
        <section className="offset-parameters-section">
          <h3>Параметры смещения</h3>

          <div className="param-group">
            <label htmlFor="angle">Угол гибки (градусы)</label>
            <div className="slider-group">
              <input
                type="range"
                id="angle"
                min="5"
                max="180"
                step="1"
                value={params.bend_angle}
                onChange={e => handleParameterChange('bend_angle', e.target.value)}
                className={getFieldError('bend_angle') ? 'input-error' : ''}
                aria-label="Угол гибки (5-180°)"
              />
              <input
                type="number"
                value={params.bend_angle}
                onChange={e => handleParameterChange('bend_angle', e.target.value)}
                min="5"
                max="180"
                className={getFieldError('bend_angle') ? 'input-error' : ''}
              />
              <span className="unit">°</span>
            </div>
          </div>

          <div className="param-group">
            <label htmlFor="offset">Расстояние смещения (мм)</label>
            <input
              id="offset"
              type="number"
              step="1"
              value={params.offset_distance}
              onChange={e => handleParameterChange('offset_distance', e.target.value)}
              className={getFieldError('offset_distance') ? 'input-error' : ''}
              aria-label="Расстояние параллельного смещения"
            />
            {getFieldError('offset_distance') && (
              <span className="error-text">{getFieldError('offset_distance')?.message}</span>
            )}
          </div>

          <div className="param-group">
            <label htmlFor="radius">Радиус гибки (мм)</label>
            <input
              id="radius"
              type="number"
              step="1"
              value={params.bend_radius}
              onChange={e => handleParameterChange('bend_radius', e.target.value)}
              className={getFieldError('bend_radius') ? 'input-error' : ''}
              aria-label="Радиус гибки трубы"
            />
            {getFieldError('bend_radius') && (
              <span className="error-text">{getFieldError('bend_radius')?.message}</span>
            )}
          </div>
        </section>

        {/* Кнопка Calculate */}
        <button
          onClick={handleCalculate}
          disabled={isLoading || hasErrors}
          className="calculate-button"
          aria-busy={isLoading}
        >
          {isLoading ? 'Расчёт...' : 'Рассчитать'}
        </button>

        {/* РАЗДЕЛ 3: Результаты */}
        {result && (
          <section className="results-section">
            <h3>Результаты расчёта</h3>

            {/* Основные параметры */}
            <div className="results-grid">
              <div className="result-card">
                <div className="result-label">Общая длина трубы</div>
                <div className="result-value">{result.total_length.toFixed(2)} мм</div>
              </div>

              <div className="result-card">
                <div className="result-label">Длина первой гибки</div>
                <div className="result-value">{result.bend_1_length.toFixed(2)} мм</div>
              </div>

              <div className="result-card">
                <div className="result-label">Прямой участок</div>
                <div className="result-value">{result.straight_section.toFixed(2)} мм</div>
              </div>

              <div className="result-card">
                <div className="result-label">Длина второй гибки</div>
                <div className="result-value">{result.bend_2_length.toFixed(2)} мм</div>
              </div>

              <div className="result-card">
                <div className="result-label">Угол гибки 1</div>
                <div className="result-value">{result.bend_angles[0].toFixed(2)}°</div>
              </div>

              <div className="result-card">
                <div className="result-label">Угол гибки 2</div>
                <div className="result-value">{result.bend_angles[1].toFixed(2)}°</div>
              </div>

              <div className="result-card confidence">
                <div className="result-label">Уверенность расчёта</div>
                <div className="result-value">{result.confidence.toFixed(1)}%</div>
                <div className="confidence-bar">
                  <div
                    className="confidence-fill"
                    style={{
                      width: `${result.confidence}%`,
                      backgroundColor: result.confidence > 85 ? '#00cc00' : '#ffaa00',
                    }}
                  />
                </div>
              </div>
            </div>

            {/* Предупреждения */}
            {result.warnings && (
              <div className="warnings-box">
                <strong>⚠️ Предупреждения:</strong>
                <p>{result.warnings}</p>
              </div>
            )}

            {/* 2D Preview */}
            {showPreview2D && (
              <div className="preview-section">
                <h4>2D диаграмма смещения</h4>
                {draw2DPreview()}
                <button
                  onClick={() => setShowPreview2D(false)}
                  className="toggle-preview-btn"
                >
                  Скрыть диаграмму
                </button>
              </div>
            )}
          </section>
        )}

        {/* РАЗДЕЛ 4: Экспорт и сохранение */}
        {result && (
          <section className="export-section">
            <h3>Экспорт и сохранение</h3>

            <div className="export-controls">
              <div className="param-group">
                <label htmlFor="export-format">Формат экспорта</label>
                <select
                  id="export-format"
                  value={exportFormat}
                  onChange={e => setExportFormat(e.target.value as 'pdf' | 'csv' | 'dxf')}
                >
                  <option value="pdf">PDF документ</option>
                  <option value="csv">CSV таблица</option>
                  <option value="dxf">DXF рисунок</option>
                </select>
              </div>

              <button onClick={handleExport} disabled={isLoading} className="export-button">
                📥 Экспортировать
              </button>
            </div>

            <div className="template-controls">
              <div className="param-group">
                <label htmlFor="template-name">Сохранить как шаблон</label>
                <div className="template-input-group">
                  <input
                    id="template-name"
                    type="text"
                    value={templateName}
                    onChange={e => setTemplateName(e.target.value)}
                    placeholder="Введите имя шаблона"
                  />
                  <button
                    onClick={handleSaveTemplate}
                    disabled={!templateName.trim()}
                    className="save-template-btn"
                  >
                    💾 Сохранить
                  </button>
                </div>
              </div>
            </div>
          </section>
        )}
      </div>
    </div>
  );
};

export default ParallelOffsetCalculator;
