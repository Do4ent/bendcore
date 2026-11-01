/**
 * Parameter Slider Panel Component
 * Provides interactive parameter controls for parametric design
 */

import React, { useState, useCallback, useEffect } from 'react';
import './ParameterSliderPanel.css';

interface ParameterSliderProps {
  id: string;
  name: string;
  type: 'float' | 'integer' | 'dimension' | 'angle';
  value: number;
  defaultValue: number;
  minValue?: number;
  maxValue?: number;
  step?: number;
  unit?: string;
  description?: string;
  onChange: (newValue: number) => void;
  onSensitivityAnalysis?: () => void;
}

interface SensitivityResult {
  parameterId: string;
  sensitivity: number;
}

/**
 * ParameterSlider - Individual parameter control
 */
const ParameterSlider: React.FC<ParameterSliderProps> = ({
  id,
  name,
  type,
  value,
  defaultValue,
  minValue = 0,
  maxValue = 100,
  step = 1,
  unit = '',
  description = '',
  onChange,
  onSensitivityAnalysis
}) => {
  const [isEditing, setIsEditing] = useState(false);
  const [inputValue, setInputValue] = useState(value.toString());
  const [isModified, setIsModified] = useState(false);

  useEffect(() => {
    setInputValue(value.toString());
    setIsModified(value !== defaultValue);
  }, [value, defaultValue]);

  const handleSliderChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const newValue = parseFloat(e.target.value);
    if (!isNaN(newValue)) {
      onChange(newValue);
      setInputValue(newValue.toString());
      setIsModified(newValue !== defaultValue);
    }
  };

  const handleInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    setInputValue(e.target.value);
  };

  const handleInputBlur = () => {
    const newValue = parseFloat(inputValue);
    if (!isNaN(newValue) && newValue >= minValue && newValue <= maxValue) {
      onChange(newValue);
      setIsModified(newValue !== defaultValue);
      setIsEditing(false);
    } else {
      setInputValue(value.toString());
      setIsEditing(false);
    }
  };

  const handleReset = () => {
    onChange(defaultValue);
    setInputValue(defaultValue.toString());
    setIsModified(false);
  };

  const formatValue = (val: number): string => {
    if (type === 'float' || type === 'dimension') {
      return val.toFixed(2);
    } else if (type === 'angle') {
      return val.toFixed(1);
    }
    return Math.round(val).toString();
  };

  const getPercentage = (): number => {
    const range = maxValue - minValue;
    return range > 0 ? ((value - minValue) / range) * 100 : 0;
  };

  return (
    <div className="parameter-slider-item">
      <div className="parameter-header">
        <label className="parameter-name">
          {name}
          {description && <span className="parameter-description" title={description}>?</span>}
        </label>
        <div className="parameter-controls">
          {isModified && (
            <button
              className="reset-button"
              onClick={handleReset}
              title="Reset to default value"
            >
              ↺
            </button>
          )}
          <button
            className="sensitivity-button"
            onClick={onSensitivityAnalysis}
            title="Analyze parameter sensitivity"
          >
            📊
          </button>
        </div>
      </div>

      <div className="parameter-body">
        <div className="slider-container">
          <input
            type="range"
            min={minValue}
            max={maxValue}
            step={step}
            value={value}
            onChange={handleSliderChange}
            className="slider-input"
          />
          <div className="slider-track">
            <div
              className="slider-fill"
              style={{ width: `${getPercentage()}%` }}
            ></div>
          </div>
        </div>

        <div className="value-display">
          {isEditing ? (
            <input
              type="number"
              value={inputValue}
              onChange={handleInputChange}
              onBlur={handleInputBlur}
              onKeyDown={(e) => {
                if (e.key === 'Enter') {
                  handleInputBlur();
                } else if (e.key === 'Escape') {
                  setInputValue(value.toString());
                  setIsEditing(false);
                }
              }}
              className="value-input"
              autoFocus
              min={minValue}
              max={maxValue}
            />
          ) : (
            <span
              className="value-text"
              onClick={() => setIsEditing(true)}
            >
              {formatValue(value)} {unit}
            </span>
          )}
        </div>
      </div>

      {isModified && (
        <div className="parameter-status">
          Modified (default: {formatValue(defaultValue)} {unit})
        </div>
      )}
    </div>
  );
};

/**
 * ParameterSliderPanel - Main panel containing all parameter sliders
 */
interface ParameterSliderPanelProps {
  parameters: ParameterSliderProps[];
  onParameterChange: (parameterId: string, newValue: number) => void;
  onAnalyzeSensitivity?: (parameterId: string) => Promise<SensitivityResult[]>;
  title?: string;
  showSensitivityAnalysis?: boolean;
}

export const ParameterSliderPanel: React.FC<ParameterSliderPanelProps> = ({
  parameters,
  onParameterChange,
  onAnalyzeSensitivity,
  title = 'Parametric Design',
  showSensitivityAnalysis = true
}) => {
  const [sensitivityResults, setSensitivityResults] = useState<SensitivityResult[]>([]);
  const [activeParameterId, setActiveParameterId] = useState<string | null>(null);
  const [isAnalyzing, setIsAnalyzing] = useState(false);

  const handleAnalyzeSensitivity = useCallback(async (parameterId: string) => {
    if (!onAnalyzeSensitivity) return;

    setIsAnalyzing(true);
    setActiveParameterId(parameterId);

    try {
      const results = await onAnalyzeSensitivity(parameterId);
      setSensitivityResults(results);
    } catch (error) {
      console.error('Sensitivity analysis failed:', error);
    } finally {
      setIsAnalyzing(false);
    }
  }, [onAnalyzeSensitivity]);

  const countModified = () => {
    return parameters.filter(p => p.value !== p.defaultValue).length;
  };

  const handleResetAll = () => {
    parameters.forEach(param => {
      onParameterChange(param.id, param.defaultValue);
    });
  };

  return (
    <div className="parameter-slider-panel">
      <div className="panel-header">
        <h2>{title}</h2>
        <div className="panel-status">
          {countModified() > 0 && (
            <span className="modified-count">
              {countModified()} parameter{countModified() !== 1 ? 's' : ''} modified
            </span>
          )}
        </div>
      </div>

      <div className="parameters-list">
        {parameters.map((param) => (
          <ParameterSlider
            key={param.id}
            {...param}
            onChange={(newValue) => onParameterChange(param.id, newValue)}
            onSensitivityAnalysis={() => {
              if (showSensitivityAnalysis) {
                handleAnalyzeSensitivity(param.id);
              }
            }}
          />
        ))}
      </div>

      {countModified() > 0 && (
        <div className="panel-footer">
          <button
            className="reset-all-button"
            onClick={handleResetAll}
          >
            Reset All Parameters
          </button>
        </div>
      )}

      {showSensitivityAnalysis && sensitivityResults.length > 0 && (
        <div className="sensitivity-results">
          <h3>Sensitivity Analysis Results</h3>
          <p className="analysis-target">
            Target: {parameters.find(p => p.id === activeParameterId)?.name}
          </p>
          <div className="sensitivity-list">
            {sensitivityResults.map((result) => (
              <div key={result.parameterId} className="sensitivity-item">
                <span className="param-name">
                  {parameters.find(p => p.id === result.parameterId)?.name}
                </span>
                <div className="sensitivity-bar">
                  <div
                    className="sensitivity-value"
                    style={{
                      width: `${Math.min(Math.abs(result.sensitivity) * 10, 100)}%`
                    }}
                  ></div>
                </div>
                <span className="sensitivity-number">
                  {result.sensitivity.toFixed(3)}
                </span>
              </div>
            ))}
          </div>
        </div>
      )}

      {isAnalyzing && (
        <div className="loading">
          Analyzing sensitivity...
        </div>
      )}
    </div>
  );
};

export default ParameterSliderPanel;
