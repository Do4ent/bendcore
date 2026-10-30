import React, { useState } from 'react';

/**
 * UnitPreferences Component - GAP 087
 * Unit system selection (metric, imperial, custom)
 */

export interface UnitPreferencesProps {
  isOpen?: boolean;
  onClose?: () => void;
  onSave?: (units: any) => void;
}

const UnitPreferences: React.FC<UnitPreferencesProps> = ({
  isOpen = true,
  onClose,
  onSave
}) => {
  const [unitSystem, setUnitSystem] = useState('metric');
  const [customLength, setCustomLength] = useState('mm');
  const [customAngle, setCustomAngle] = useState('degrees');

  const handleSave = () => {
    const units = {
      system: unitSystem,
      length: unitSystem === 'metric' ? 'mm' : unitSystem === 'imperial' ? 'in' : customLength,
      angle: customAngle
    };
    if (onSave) onSave(units);
    if (onClose) onClose();
  };

  return (
    <div className="unit-preferences">
      <div className="preferences-header">
        <h3>Unit Preferences</h3>
        {onClose && <button className="close-btn" onClick={onClose}>×</button>}
      </div>

      <div className="preferences-content">
        <div className="unit-section">
          <h4>Unit System</h4>
          <div className="radio-group">
            <label>
              <input
                type="radio"
                value="metric"
                checked={unitSystem === 'metric'}
                onChange={(e) => setUnitSystem(e.target.value)}
              />
              {' '}Metric (mm, kg)
            </label>
            <label>
              <input
                type="radio"
                value="imperial"
                checked={unitSystem === 'imperial'}
                onChange={(e) => setUnitSystem(e.target.value)}
              />
              {' '}Imperial (in, lb)
            </label>
            <label>
              <input
                type="radio"
                value="custom"
                checked={unitSystem === 'custom'}
                onChange={(e) => setUnitSystem(e.target.value)}
              />
              {' '}Custom
            </label>
          </div>
        </div>

        {unitSystem === 'custom' && (
          <div className="custom-section">
            <div className="custom-item">
              <label>Length Unit:</label>
              <select value={customLength} onChange={(e) => setCustomLength(e.target.value)}>
                <option value="mm">Millimeters</option>
                <option value="cm">Centimeters</option>
                <option value="m">Meters</option>
                <option value="in">Inches</option>
                <option value="ft">Feet</option>
              </select>
            </div>
          </div>
        )}

        <div className="unit-section">
          <h4>Angle Unit</h4>
          <select value={customAngle} onChange={(e) => setCustomAngle(e.target.value)}>
            <option value="degrees">Degrees</option>
            <option value="radians">Radians</option>
          </select>
        </div>
      </div>

      {(onSave || onClose) && (
        <div className="preferences-footer">
          {onClose && <button className="btn btn-cancel" onClick={onClose}>Cancel</button>}
          {onSave && <button className="btn btn-primary" onClick={handleSave}>Save</button>}
        </div>
      )}
    </div>
  );
};

export default UnitPreferences;
