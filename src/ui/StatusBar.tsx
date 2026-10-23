/**
 * BendCore Status Bar Component
 * Bottom bar showing real-time status information
 * Phase 32: Professional UI
 */

import React, { useState, useEffect, useCallback } from 'react';
import './StatusBar.css';

export interface StatusBarState {
  stepHint?: string;
  cursorX?: number;
  cursorY?: number;
  cursorZ?: number;
  activePlane?: string;
  snapMode?: boolean;
  snapType?: 'grid' | 'endpoint' | 'midpoint' | 'intersection';
  units?: 'mm' | 'in' | 'cm';
  standard?: 'GOST' | 'ISO' | 'DIN' | 'ANSI';
  violations?: {
    errors: number;
    warnings: number;
    info: number;
  };
  isCalculating?: boolean;
}

export interface StatusBarProps {
  state?: StatusBarState;
  onStatusClick?: (section: string) => void;
  onCommand?: (commandId: string) => void;
}

export const StatusBar: React.FC<StatusBarProps> = ({
  state = {},
  onStatusClick,
  onCommand,
}) => {
  const [isSpinning, setIsSpinning] = useState(false);

  useEffect(() => {
    setIsSpinning(state.isCalculating || false);
  }, [state.isCalculating]);

  const handleViolationsClick = useCallback(() => {
    if (onStatusClick) {
      onStatusClick('violations');
    }
    if (onCommand && (state.violations?.errors || state.violations?.warnings)) {
      onCommand('show-check-results');
    }
  }, [state.violations, onStatusClick, onCommand]);

  return (
    <div className="bendcore-status-bar">
      {/* Step hint section */}
      <div className="status-section step-hint-section">
        <span className="status-icon">📝</span>
        <span className="status-text">
          {state.stepHint || 'Ready'}
        </span>
      </div>

      {/* Coordinates section */}
      <div
        className="status-section coordinates-section"
        onClick={() => onStatusClick?.('coordinates')}
      >
        <span className="status-icon">🎯</span>
        <span className="status-text">
          X:{state.cursorX?.toFixed(1) || '0.0'} Y:
          {state.cursorY?.toFixed(1) || '0.0'} Z:
          {state.cursorZ?.toFixed(1) || '0.0'}
        </span>
      </div>

      {/* Active plane section */}
      <div
        className="status-section plane-section"
        onClick={() => onStatusClick?.('plane')}
      >
        <span className="status-text">
          {state.activePlane || 'XY Plane'}
        </span>
      </div>

      {/* Snap mode section */}
      <div
        className={`status-section snap-section ${
          state.snapMode ? 'active' : 'inactive'
        }`}
        onClick={() => onStatusClick?.('snap')}
      >
        <span className="status-icon">
          {state.snapMode ? '🔘' : '⚪'}
        </span>
        <span className="status-text">
          Snap: {state.snapMode ? 'ON' : 'OFF'}
          {state.snapMode && state.snapType && (
            <span className="snap-type">({state.snapType})</span>
          )}
        </span>
      </div>

      {/* Units and standards section */}
      <div
        className="status-section units-section"
        onClick={() => onStatusClick?.('units')}
      >
        <span className="status-text">
          {state.units || 'mm'} / {state.standard || 'ISO'}
        </span>
      </div>

      {/* Violations section */}
      {(state.violations?.errors || state.violations?.warnings || state.violations?.info) && (
        <div
          className={`status-section violations-section ${
            state.violations?.errors
              ? 'has-errors'
              : state.violations?.warnings
              ? 'has-warnings'
              : 'has-info'
          }`}
          onClick={handleViolationsClick}
        >
          {state.violations?.errors ? (
            <>
              <span className="status-icon violation-error">⚠️</span>
              <span className="status-text">
                Errors: {state.violations.errors}
              </span>
            </>
          ) : state.violations?.warnings ? (
            <>
              <span className="status-icon violation-warning">⚡</span>
              <span className="status-text">
                Warnings: {state.violations.warnings}
              </span>
            </>
          ) : (
            <>
              <span className="status-icon violation-info">ℹ️</span>
              <span className="status-text">
                Info: {state.violations.info}
              </span>
            </>
          )}
        </div>
      )}

      {/* Recalc status section */}
      <div className={`status-section recalc-section ${
        isSpinning ? 'calculating' : 'done'
      }`}>
        {isSpinning ? (
          <>
            <span className="status-icon spinner">⟳</span>
            <span className="status-text">Calculating...</span>
          </>
        ) : state.violations?.errors ? (
          <>
            <span className="status-icon">⚠️</span>
            <span className="status-text">Errors found</span>
          </>
        ) : (
          <>
            <span className="status-icon">✓</span>
            <span className="status-text">Recalc OK</span>
          </>
        )}
      </div>
    </div>
  );
};

export default StatusBar;
