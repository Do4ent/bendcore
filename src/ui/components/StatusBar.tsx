/**
 * BendCore UI Component: StatusBar
 * Bottom status bar with coordinates and progress
 *
 * @component
 * @example
 * <StatusBar
 *   message="Готово"
 *   coordinates={{ x: 100, y: 200, z: 50 }}
 *   mode="design"
 * />
 */

import React, { useMemo } from 'react';
import './StatusBar.css';

// ============ Types ============

export interface Coordinates {
  x: number;
  y: number;
  z: number;
}

export interface StatusBarProps {
  message?: string;
  coordinates?: Coordinates;
  mode?: 'design' | 'sketch' | 'idle' | 'loading' | 'error';
  progress?: number;
  isLoading?: boolean;
  isDarkMode?: boolean;
}

// ============ Component ============

export const StatusBar: React.FC<StatusBarProps> = ({
  message = 'Готово',
  coordinates,
  mode = 'idle',
  progress,
  isLoading = false,
  isDarkMode = false,
}) => {
  // Format coordinates
  const formattedCoords = useMemo(() => {
    if (!coordinates) return null;
    return `X: ${coordinates.x.toFixed(2)} Y: ${coordinates.y.toFixed(2)} Z: ${coordinates.z.toFixed(2)}`;
  }, [coordinates]);

  // Get mode label
  const modeLabel = useMemo(() => {
    const modes: Record<string, string> = {
      design: 'Проектирование',
      sketch: 'Эскиз',
      idle: 'Готово',
      loading: 'Загрузка...',
      error: 'Ошибка',
    };
    return modes[mode] || mode;
  }, [mode]);

  // Get mode icon
  const modeIcon = useMemo(() => {
    const icons: Record<string, string> = {
      design: '⚙️',
      sketch: '📐',
      idle: '✓',
      loading: '⟳',
      error: '✕',
    };
    return icons[mode] || '✓';
  }, [mode]);

  return (
    <div
      className={`status-bar ${isDarkMode ? 'status-bar--dark' : ''} status-bar--${mode}`}
      role="status"
      aria-live="polite"
      aria-label="Строка состояния"
    >
      {/* === Message Section === */}
      <div className="status-bar__section status-bar__message-section">
        <div className="status-bar__message">
          <span className="status-bar__message-icon">{isLoading ? '⟳' : ''}</span>
          <span className="status-bar__message-text">{message}</span>
        </div>
      </div>

      {/* === Coordinates Section === */}
      {formattedCoords && (
        <div className="status-bar__section status-bar__coords-section">
          <span className="status-bar__coords">{formattedCoords}</span>
        </div>
      )}

      {/* === Progress Bar === */}
      {progress !== undefined && progress > 0 && progress < 100 && (
        <div className="status-bar__section status-bar__progress-section">
          <div className="status-bar__progress-bar">
            <div
              className="status-bar__progress-fill"
              style={{ width: `${Math.min(progress, 100)}%` }}
              role="progressbar"
              aria-valuenow={Math.round(progress)}
              aria-valuemin={0}
              aria-valuemax={100}
            />
          </div>
          <span className="status-bar__progress-text">{Math.round(progress)}%</span>
        </div>
      )}

      {/* === Mode Indicator === */}
      <div className="status-bar__section status-bar__mode-section">
        <span
          className={`status-bar__mode status-bar__mode--${mode}`}
          title={modeLabel}
          aria-label={modeLabel}
        >
          <span className="status-bar__mode-icon">{modeIcon}</span>
          <span className="status-bar__mode-label">{modeLabel}</span>
        </span>
      </div>
    </div>
  );
};

export default StatusBar;
