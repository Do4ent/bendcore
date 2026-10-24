/**
 * Combined 3D Offset Calculator
 * Для сложных гибок в 3D пространстве
 */

import React, { useState, useCallback } from 'react';

interface Combined3DParams {
  outer_diameter: number;
  wall_thickness: number;
  bend_angle_1: number;
  bend_angle_2: number;
  offset_distance: number;
  bend_radius: number;
  material: string;
}

interface Combined3DResult {
  total_length: number;
  segment_1: number;
  segment_2: number;
  segment_3: number;
  segment_4: number;
  coordinates_3d: Array<[number, number, number]>;
  bend_angles: [number, number];
  confidence: number;
  warnings: string;
  is_valid: boolean;
}

interface Combined3DProps {
  params: Combined3DParams;
  onParamsChange: (params: Combined3DParams) => void;
  result: Combined3DResult | null;
  onCalculate: () => Promise<void>;
  isLoading?: boolean;
}

export const Combined3DOffsetCalculator: React.FC<Combined3DProps> = ({
  params,
  onParamsChange,
  result,
  onCalculate,
  isLoading = false,
}) => {
  const handleParamChange = useCallback(
    (key: keyof Combined3DParams, value: number | string) => {
      onParamsChange({
        ...params,
        [key]: typeof value === 'string' ? parseFloat(value) : value,
      });
    },
    [params, onParamsChange]
  );

  return (
    <div className="combined-3d-calculator">
      <div className="params-grid">
        {/* Первый угол гибки */}
        <div className="param-control">
          <label>Угол гибки 1 (°)</label>
          <input
            type="number"
            min="5"
            max="180"
            step="1"
            value={params.bend_angle_1}
            onChange={(e) => handleParamChange('bend_angle_1', e.target.value)}
          />
        </div>

        {/* Второй угол гибки */}
        <div className="param-control">
          <label>Угол гибки 2 (°)</label>
          <input
            type="number"
            min="5"
            max="180"
            step="1"
            value={params.bend_angle_2}
            onChange={(e) => handleParamChange('bend_angle_2', e.target.value)}
          />
        </div>
      </div>

      {/* Результаты 3D */}
      {result && (
        <div className="results-3d">
          <h4>3D Координаты траектории</h4>
          <div className="coordinates-table">
            <table>
              <thead>
                <tr>
                  <th>№</th>
                  <th>X (мм)</th>
                  <th>Y (мм)</th>
                  <th>Z (мм)</th>
                </tr>
              </thead>
              <tbody>
                {result.coordinates_3d?.map((coord, idx) => (
                  <tr key={idx}>
                    <td>{idx}</td>
                    <td>{coord[0].toFixed(2)}</td>
                    <td>{coord[1].toFixed(2)}</td>
                    <td>{coord[2].toFixed(2)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>

          <div className="segment-info">
            <div className="segment">Сегмент 1: {result.segment_1.toFixed(2)} мм</div>
            <div className="segment">Сегмент 2: {result.segment_2.toFixed(2)} мм</div>
            <div className="segment">Сегмент 3: {result.segment_3.toFixed(2)} мм</div>
            <div className="segment">Сегмент 4: {result.segment_4.toFixed(2)} мм</div>
          </div>
        </div>
      )}
    </div>
  );
};

export default Combined3DOffsetCalculator;
