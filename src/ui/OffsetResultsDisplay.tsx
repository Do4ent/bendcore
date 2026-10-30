/**
 * Offset Results Display
 * Отображение результатов расчётов offset-калькулятора
 */

import React, { useCallback } from 'react';

export interface OffsetResult {
  total_length: number;
  bend_angles?: [number, number];
  confidence: number;
  warnings?: string;
  is_valid: boolean;
  bend_1_length?: number;
  straight_section?: number;
  bend_2_length?: number;
  segment_1?: number;
  segment_2?: number;
  segment_3?: number;
  segment_4?: number;
}

interface OffsetResultsDisplayProps {
  result: OffsetResult | null;
  offsetType: 'parallel' | '3d-combined';
  onCopyToParams?: (result: OffsetResult) => void;
  onExport?: (format: 'pdf' | 'csv' | 'dxf') => void;
}

export const OffsetResultsDisplay: React.FC<OffsetResultsDisplayProps> = ({
  result,
  offsetType,
  onCopyToParams,
  onExport,
}) => {
  const handleCopyResults = useCallback(() => {
    if (result && onCopyToParams) {
      onCopyToParams(result);
    }
  }, [result, onCopyToParams]);

  const handleExport = useCallback(
    (format: 'pdf' | 'csv' | 'dxf') => {
      if (onExport) {
        onExport(format);
      }
    },
    [onExport]
  );

  if (!result) {
    return (
      <div className="results-empty">
        <p>Нет результатов. Выполните расчёт.</p>
      </div>
    );
  }

  return (
    <div className="offset-results-display">
      <div className="results-header">
        <h3>Результаты расчёта</h3>
        <div className="validity-indicator">
          {result.is_valid ? (
            <span className="valid">✓ Допустимо</span>
          ) : (
            <span className="invalid">✗ Ошибка</span>
          )}
        </div>
      </div>

      {/* Parallel Offset Results */}
      {offsetType === 'parallel' && (
        <div className="parallel-results">
          <div className="results-grid">
            <div className="result-card">
              <label>Общая длина</label>
              <span className="result-value">{result.total_length?.toFixed(2)} мм</span>
            </div>
            {result.bend_1_length && (
              <div className="result-card">
                <label>Длина гибки 1</label>
                <span className="result-value">{result.bend_1_length.toFixed(2)} мм</span>
              </div>
            )}
            {result.straight_section && (
              <div className="result-card">
                <label>Прямой участок</label>
                <span className="result-value">{result.straight_section.toFixed(2)} мм</span>
              </div>
            )}
            {result.bend_2_length && (
              <div className="result-card">
                <label>Длина гибки 2</label>
                <span className="result-value">{result.bend_2_length.toFixed(2)} мм</span>
              </div>
            )}
          </div>
        </div>
      )}

      {/* 3D Combined Results */}
      {offsetType === '3d-combined' && (
        <div className="combined-3d-results">
          <div className="segments-grid">
            {result.segment_1 && (
              <div className="segment-card">
                <label>Сегмент 1</label>
                <span className="result-value">{result.segment_1.toFixed(2)} мм</span>
              </div>
            )}
            {result.segment_2 && (
              <div className="segment-card">
                <label>Сегмент 2</label>
                <span className="result-value">{result.segment_2.toFixed(2)} мм</span>
              </div>
            )}
            {result.segment_3 && (
              <div className="segment-card">
                <label>Сегмент 3</label>
                <span className="result-value">{result.segment_3.toFixed(2)} мм</span>
              </div>
            )}
            {result.segment_4 && (
              <div className="segment-card">
                <label>Сегмент 4</label>
                <span className="result-value">{result.segment_4.toFixed(2)} мм</span>
              </div>
            )}
          </div>
          <div className="total-card">
            <label>Итоговая длина</label>
            <span className="result-value">{result.total_length.toFixed(2)} мм</span>
          </div>
        </div>
      )}

      {/* Confidence and Angles */}
      <div className="metadata">
        <div className="confidence-bar">
          <label>Уверенность:</label>
          <div className="bar">
            <div
              className="fill"
              style={{ width: `${result.confidence}%` }}
            />
          </div>
          <span>{result.confidence.toFixed(1)}%</span>
        </div>

        {result.bend_angles && (
          <div className="angles-info">
            <span>Углы: {result.bend_angles[0].toFixed(1)}°, {result.bend_angles[1].toFixed(1)}°</span>
          </div>
        )}
      </div>

      {/* Warnings */}
      {result.warnings && (
        <div className="warnings">
          <strong>⚠️ Предупреждения:</strong>
          <p>{result.warnings}</p>
        </div>
      )}

      {/* Actions */}
      <div className="results-actions">
        <button onClick={handleCopyResults} className="btn-primary">
          📋 Скопировать в параметры
        </button>
        <select
          onChange={(e) => handleExport(e.target.value as 'pdf' | 'csv' | 'dxf')}
          defaultValue=""
        >
          <option value="">Экспортировать как...</option>
          <option value="pdf">PDF</option>
          <option value="csv">CSV</option>
          <option value="dxf">DXF</option>
        </select>
      </div>
    </div>
  );
};

export default OffsetResultsDisplay;
