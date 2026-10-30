import React, { useState } from 'react';

/**
 * PerformanceSettings Component - GAP 088
 * Performance tuning and optimization settings
 */

export interface PerformanceSettingsProps {
  isOpen?: boolean;
  onClose?: () => void;
  onSave?: (settings: any) => void;
}

const PerformanceSettings: React.FC<PerformanceSettingsProps> = ({
  isOpen = true,
  onClose,
  onSave
}) => {
  const [settings, setSettings] = useState({
    cacheSize: 500,
    maxMeshResolution: 1000000,
    enableGPU: true,
    renderQuality: 'high',
    shadowQuality: 'medium',
    antiAliasing: true,
    enableOptimization: true
  });

  const handleChange = (key: string, value: any) => {
    setSettings(prev => ({ ...prev, [key]: value }));
  };

  const handleSave = () => {
    if (onSave) onSave(settings);
    if (onClose) onClose();
  };

  return (
    <div className="performance-settings">
      <div className="settings-header">
        <h3>Performance Settings</h3>
        {onClose && <button className="close-btn" onClick={onClose}>×</button>}
      </div>

      <div className="settings-content">
        <div className="setting-item">
          <label>Cache Size (MB):</label>
          <input type="number" value={settings.cacheSize} onChange={(e) => handleChange('cacheSize', parseInt(e.target.value))} min="100" max="2000" />
        </div>
        <div className="setting-item">
          <label>Max Mesh Resolution:</label>
          <input type="number" value={settings.maxMeshResolution} onChange={(e) => handleChange('maxMeshResolution', parseInt(e.target.value))} min="100000" max="5000000" />
        </div>
        <div className="setting-item">
          <label><input type="checkbox" checked={settings.enableGPU} onChange={(e) => handleChange('enableGPU', e.target.checked)} /> Enable GPU Acceleration</label>
        </div>
        <div className="setting-item">
          <label>Render Quality:</label>
          <select value={settings.renderQuality} onChange={(e) => handleChange('renderQuality', e.target.value)}>
            <option value="low">Low</option>
            <option value="medium">Medium</option>
            <option value="high">High</option>
            <option value="ultra">Ultra</option>
          </select>
        </div>
        <div className="setting-item">
          <label>Shadow Quality:</label>
          <select value={settings.shadowQuality} onChange={(e) => handleChange('shadowQuality', e.target.value)}>
            <option value="off">Off</option>
            <option value="low">Low</option>
            <option value="medium">Medium</option>
            <option value="high">High</option>
          </select>
        </div>
        <div className="setting-item">
          <label><input type="checkbox" checked={settings.antiAliasing} onChange={(e) => handleChange('antiAliasing', e.target.checked)} /> Anti-Aliasing</label>
        </div>
      </div>

      {(onSave || onClose) && (
        <div className="settings-footer">
          {onClose && <button className="btn btn-cancel" onClick={onClose}>Cancel</button>}
          {onSave && <button className="btn btn-primary" onClick={handleSave}>Save</button>}
        </div>
      )}
    </div>
  );
};

export default PerformanceSettings;
