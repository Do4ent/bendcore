import React, { useState } from 'react';

/**
 * SettingsDialog Component - GAP 083
 * Global application settings with categories
 */

export interface SettingsCategory {
  id: string;
  name: string;
  icon: string;
}

export interface SettingsDialogProps {
  isOpen: boolean;
  onClose: () => void;
  onSave: (settings: Record<string, any>) => void;
}

const DEFAULT_CATEGORIES: SettingsCategory[] = [
  { id: 'general', name: 'General', icon: '⚙️' },
  { id: 'display', name: 'Display', icon: '🖥️' },
  { id: 'advanced', name: 'Advanced', icon: '🔧' }
];

const SettingsDialog: React.FC<SettingsDialogProps> = ({
  isOpen,
  onClose,
  onSave
}) => {
  const [activeCategory, setActiveCategory] = useState('general');
  const [settings, setSettings] = useState<Record<string, any>>({
    autoSave: true,
    autoSaveInterval: 60,
    theme: 'dark',
    defaultUnits: 'metric',
    enableNotifications: true,
    cacheSize: 500,
    maxUndoSteps: 100,
    enableGPU: true
  });

  const handleSettingChange = (key: string, value: any) => {
    setSettings(prev => ({
      ...prev,
      [key]: value
    }));
  };

  const handleSave = () => {
    onSave(settings);
    onClose();
  };

  if (!isOpen) return null;

  return (
    <div className="settings-dialog-overlay">
      <div className="settings-dialog">
        <div className="dialog-header">
          <h2>Settings</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>

        <div className="settings-container">
          {/* Categories Sidebar */}
          <div className="settings-sidebar">
            {DEFAULT_CATEGORIES.map(cat => (
              <button
                key={cat.id}
                className={`settings-category ${activeCategory === cat.id ? 'active' : ''}`}
                onClick={() => setActiveCategory(cat.id)}
              >
                <span className="category-icon">{cat.icon}</span>
                <span className="category-name">{cat.name}</span>
              </button>
            ))}
          </div>

          {/* Settings Panel */}
          <div className="settings-panel">
            {activeCategory === 'general' && (
              <div className="settings-group">
                <h3>General Settings</h3>
                <div className="setting-item">
                  <label>
                    <input
                      type="checkbox"
                      checked={settings.autoSave}
                      onChange={(e) => handleSettingChange('autoSave', e.target.checked)}
                    />
                    {' '}Enable auto-save
                  </label>
                </div>
                <div className="setting-item">
                  <label>Auto-save interval (seconds):</label>
                  <input
                    type="number"
                    min="10"
                    max="300"
                    value={settings.autoSaveInterval}
                    onChange={(e) => handleSettingChange('autoSaveInterval', parseInt(e.target.value))}
                  />
                </div>
                <div className="setting-item">
                  <label>Default units:</label>
                  <select
                    value={settings.defaultUnits}
                    onChange={(e) => handleSettingChange('defaultUnits', e.target.value)}
                  >
                    <option value="metric">Metric (mm)</option>
                    <option value="imperial">Imperial (in)</option>
                    <option value="custom">Custom</option>
                  </select>
                </div>
                <div className="setting-item">
                  <label>
                    <input
                      type="checkbox"
                      checked={settings.enableNotifications}
                      onChange={(e) => handleSettingChange('enableNotifications', e.target.checked)}
                    />
                    {' '}Enable notifications
                  </label>
                </div>
              </div>
            )}

            {activeCategory === 'display' && (
              <div className="settings-group">
                <h3>Display Settings</h3>
                <div className="setting-item">
                  <label>Theme:</label>
                  <select
                    value={settings.theme}
                    onChange={(e) => handleSettingChange('theme', e.target.value)}
                  >
                    <option value="light">Light</option>
                    <option value="dark">Dark</option>
                    <option value="auto">Auto</option>
                  </select>
                </div>
              </div>
            )}

            {activeCategory === 'advanced' && (
              <div className="settings-group">
                <h3>Advanced Settings</h3>
                <div className="setting-item">
                  <label>Cache size (MB):</label>
                  <input
                    type="number"
                    min="100"
                    max="2000"
                    value={settings.cacheSize}
                    onChange={(e) => handleSettingChange('cacheSize', parseInt(e.target.value))}
                  />
                </div>
                <div className="setting-item">
                  <label>Max undo steps:</label>
                  <input
                    type="number"
                    min="10"
                    max="1000"
                    value={settings.maxUndoSteps}
                    onChange={(e) => handleSettingChange('maxUndoSteps', parseInt(e.target.value))}
                  />
                </div>
                <div className="setting-item">
                  <label>
                    <input
                      type="checkbox"
                      checked={settings.enableGPU}
                      onChange={(e) => handleSettingChange('enableGPU', e.target.checked)}
                    />
                    {' '}Enable GPU acceleration
                  </label>
                </div>
              </div>
            )}
          </div>
        </div>

        <div className="dialog-footer">
          <button className="btn btn-cancel" onClick={onClose}>
            Cancel
          </button>
          <button className="btn btn-primary" onClick={handleSave}>
            Save
          </button>
        </div>
      </div>
    </div>
  );
};

export default SettingsDialog;
