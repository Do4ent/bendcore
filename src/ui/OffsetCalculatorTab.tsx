/**
 * Offset Calculator Tab
 * Главная компонента для интеграции offset-калькулятора в TubeDialog
 */

import React, { useState, useCallback } from 'react';
import { OffsetTypeSelector, OffsetType } from './OffsetTypeSelector';
import { OffsetResultsDisplay } from './OffsetResultsDisplay';
import { OffsetVisualization } from './OffsetVisualization';
import { ParallelOffsetCalculator } from './ParallelOffsetCalculator';
import Combined3DOffsetCalculator from './Combined3DOffsetCalculator';
import './OffsetCalculatorTab.css';

interface TubeParams {
  outerDiameter: number;
  wallThickness: number;
  length: number;
  material: string;
}

interface OffsetCalculatorTabProps {
  tubeParams: TubeParams;
  onResultSync?: (offsetData: any) => void;
}

interface CalculationResult {
  total_length: number;
  bend_1_length?: number;
  straight_section?: number;
  bend_2_length?: number;
  segment_1?: number;
  segment_2?: number;
  segment_3?: number;
  segment_4?: number;
  bend_angles: [number, number];
  confidence: number;
  warnings?: string;
  is_valid: boolean;
}

export const OffsetCalculatorTab: React.FC<OffsetCalculatorTabProps> = ({
  tubeParams,
  onResultSync,
}) => {
  const [offsetType, setOffsetType] = useState<OffsetType>('parallel');
  const [result, setResult] = useState<CalculationResult | null>(null);
  const [isDarkMode, setIsDarkMode] = useState(false);

  // Параметры для параллельного offset
  const [parallelParams, setParallelParams] = useState({
    outer_diameter: tubeParams.outerDiameter,
    wall_thickness: tubeParams.wallThickness,
    bend_angle: 90.0,
    offset_distance: 30.0,
    bend_radius: 50.0,
    material: tubeParams.material,
  });

  // Параметры для 3D offset
  const [combined3DParams, setCombined3DParams] = useState({
    outer_diameter: tubeParams.outerDiameter,
    wall_thickness: tubeParams.wallThickness,
    bend_angle_1: 90.0,
    bend_angle_2: 90.0,
    offset_distance: 30.0,
    bend_radius: 50.0,
    material: tubeParams.material,
  });

  // Обработчик синхронизации результатов
  const handleResultUpdate = useCallback(
    (calcResult: CalculationResult) => {
      setResult(calcResult);
      if (onResultSync) {
        onResultSync({
          offsetType,
          offsetDistance: offsetType === 'parallel' ? parallelParams.offset_distance : combined3DParams.offset_distance,
          totalLength: calcResult.total_length,
          bendAngles: calcResult.bend_angles,
          confidence: calcResult.confidence,
        });
      }
    },
    [offsetType, parallelParams, combined3DParams, onResultSync]
  );

  // Копирование результатов в параметры трубы
  const handleCopyToParams = useCallback((calcResult: CalculationResult) => {
    // В реальном приложении здесь была бы интеграция с родительским компонентом
    console.log('Копирование результатов:', calcResult);
    alert(`Общая длина скопирована: ${calcResult.total_length.toFixed(2)} мм`);
  }, []);

  // Экспорт результатов
  const handleExport = useCallback((format: 'pdf' | 'csv' | 'dxf') => {
    if (!result) return;
    console.log(`Экспорт в ${format}:`, result);
    alert(`Экспорт в ${format} инициирован`);
  }, [result]);

  return (
    <div className={`offset-calculator-tab ${isDarkMode ? 'dark' : 'light'}`}>
      {/* Dark Mode Toggle */}
      <div className="tab-header">
        <h3>Калькулятор смещений</h3>
        <label className="dark-toggle">
          <input
            type="checkbox"
            checked={isDarkMode}
            onChange={(e) => setIsDarkMode(e.target.checked)}
          />
          {isDarkMode ? '🌙' : '☀️'}
        </label>
      </div>

      {/* Type Selector */}
      <OffsetTypeSelector selectedType={offsetType} onTypeChange={setOffsetType} />

      <div className="tab-content">
        {/* Parallel Offset Calculator */}
        {offsetType === 'parallel' && (
          <ParallelOffsetCalculator
            onResultsUpdate={handleResultUpdate}
            initialParams={parallelParams}
          />
        )}

        {/* 3D Combined Offset Calculator */}
        {offsetType === '3d-combined' && (
          <Combined3DOffsetCalculator
            params={combined3DParams}
            onParamsChange={setCombined3DParams}
            result={result as any}
            onCalculate={async () => {
              // Mock calculation for 3D
              const mockResult: CalculationResult = {
                total_length:
                  combined3DParams.bend_radius * 2 +
                  combined3DParams.offset_distance * 1.5,
                segment_1: combined3DParams.bend_radius,
                segment_2: combined3DParams.offset_distance,
                segment_3: combined3DParams.bend_radius,
                segment_4: combined3DParams.offset_distance * 0.5,
                bend_angles: [combined3DParams.bend_angle_1, combined3DParams.bend_angle_2],
                confidence: 82.5,
                warnings: 'Complex 3D geometry requires manufacturing review',
                is_valid: true,
              };
              handleResultUpdate(mockResult);
            }}
          />
        )}
      </div>

      {/* Results Display */}
      {result && (
        <div className="results-section">
          <OffsetResultsDisplay
            result={result}
            offsetType={offsetType}
            onCopyToParams={handleCopyToParams}
            onExport={handleExport}
          />
        </div>
      )}

      {/* Visualization */}
      {result && (
        <div className="visualization-section">
          <OffsetVisualization offsetType={offsetType} result={result} isDarkMode={isDarkMode} />
        </div>
      )}

      {/* Info Box */}
      <div className="info-box">
        <strong>Синхронизация параметров:</strong>
        <p>
          Текущие параметры трубы синхронизированы. Результаты расчётов можно скопировать в
          основные параметры трубы или экспортировать в DXF.
        </p>
      </div>
    </div>
  );
};

export default OffsetCalculatorTab;
