/**
 * Offset Visualization
 * Визуализация результатов offset-расчётов в 2D/3D
 */

import React from 'react';

interface VisualizationProps {
  offsetType: 'parallel' | '3d-combined';
  result: any;
  isDarkMode?: boolean;
}

export const OffsetVisualization: React.FC<VisualizationProps> = ({
  offsetType,
  result,
  isDarkMode = false,
}) => {
  if (!result) return null;

  const draw2DParallel = () => {
    const width = 400;
    const height = 250;
    const padding = 30;

    return (
      <svg
        width={width}
        height={height}
        viewBox={`0 0 ${width} ${height}`}
        className="visualization-2d"
      >
        <defs>
          <pattern
            id="grid-parallel"
            width="20"
            height="20"
            patternUnits="userSpaceOnUse"
          >
            <path
              d="M 20 0 L 0 0 0 20"
              fill="none"
              stroke={isDarkMode ? '#333' : '#eee'}
              strokeWidth="0.5"
            />
          </pattern>
        </defs>

        {/* Сетка */}
        <rect width={width} height={height} fill="url(#grid-parallel)" />

        {/* Первая гибка */}
        <circle
          cx={padding + (result.bend_1_length || 0) / 2}
          cy={height / 2}
          r={Math.min((result.bend_1_length || 30) / 2, 40)}
          fill="none"
          stroke={isDarkMode ? '#4da6ff' : '#0066cc'}
          strokeWidth="2"
        />

        {/* Прямой участок */}
        <line
          x1={padding + (result.bend_1_length || 0)}
          y1={height / 2 - 20}
          x2={padding + (result.bend_1_length || 0) + (result.straight_section || 0)}
          y2={height / 2 - 20}
          stroke={isDarkMode ? '#66ff66' : '#00cc00'}
          strokeWidth="3"
        />

        {/* Вторая гибка */}
        <circle
          cx={padding + (result.bend_1_length || 0) + (result.straight_section || 0) + (result.bend_2_length || 0) / 2}
          cy={height / 2}
          r={Math.min((result.bend_2_length || 30) / 2, 40)}
          fill="none"
          stroke={isDarkMode ? '#ff99ff' : '#cc00cc'}
          strokeWidth="2"
        />

        {/* Labels */}
        <text x={padding + 10} y={height - 20} fontSize="12" fill={isDarkMode ? '#aaa' : '#666'}>
          Гибка 1
        </text>
        <text
          x={padding + (result.bend_1_length || 0) + (result.straight_section || 0) / 2}
          y={height - 20}
          fontSize="12"
          fill={isDarkMode ? '#aaa' : '#666'}
        >
          Прямой
        </text>
        <text
          x={padding + (result.bend_1_length || 0) + (result.straight_section || 0) + 10}
          y={height - 20}
          fontSize="12"
          fill={isDarkMode ? '#aaa' : '#666'}
        >
          Гибка 2
        </text>
      </svg>
    );
  };

  const draw3D = () => {
    // Упрощённая 3D визуализация как изометрия
    const width = 400;
    const height = 300;

    return (
      <svg
        width={width}
        height={height}
        viewBox={`0 0 ${width} ${height}`}
        className="visualization-3d"
      >
        {/* Оси координат */}
        <line x1="50" y1="250" x2="150" y2="250" stroke={isDarkMode ? '#ff6666' : '#ff0000'} strokeWidth="2" />
        <line x1="50" y1="250" x2="50" y2="150" stroke={isDarkMode ? '#66ff66' : '#00ff00'} strokeWidth="2" />
        <line x1="50" y1="250" x2="100" y2="200" stroke={isDarkMode ? '#6666ff' : '#0000ff'} strokeWidth="2" />

        <text x="160" y="260" fontSize="12" fill={isDarkMode ? '#ff6666' : '#ff0000'}>
          X
        </text>
        <text x="40" y="140" fontSize="12" fill={isDarkMode ? '#66ff66' : '#00ff00'}>
          Y
        </text>
        <text x="110" y="200" fontSize="12" fill={isDarkMode ? '#6666ff' : '#0000ff'}>
          Z
        </text>

        {/* Траектория (упрощённо) */}
        <polyline
          points="50,250 100,200 150,150 200,200 250,250"
          fill="none"
          stroke={isDarkMode ? '#ffaa00' : '#ff9900'}
          strokeWidth="2"
        />

        {/* Точки траектории */}
        {[
          { x: 50, y: 250 },
          { x: 100, y: 200 },
          { x: 150, y: 150 },
          { x: 200, y: 200 },
          { x: 250, y: 250 },
        ].map((point, idx) => (
          <circle key={idx} cx={point.x} cy={point.y} r="4" fill={isDarkMode ? '#ffaa00' : '#ff9900'} />
        ))}

        <text x="150" y="20" fontSize="14" fontWeight="bold" fill={isDarkMode ? '#aaa' : '#666'}>
          3D Траектория смещения
        </text>
      </svg>
    );
  };

  return (
    <div className="offset-visualization">
      <h4>Визуализация</h4>
      {offsetType === 'parallel' ? draw2DParallel() : draw3D()}
    </div>
  );
};

export default OffsetVisualization;
