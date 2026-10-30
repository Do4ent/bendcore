import React, { useState, useEffect } from 'react';

/**
 * ThemeSelector Component - GAP 084
 * Light/dark theme and color scheme customizer
 */

export interface ThemeSelectorProps {
  isOpen?: boolean;
  onClose?: () => void;
  onThemeChange?: (theme: string, colors: Record<string, string>) => void;
}

const THEME_PRESETS = {
  light: { name: 'Light', colors: { bg: '#FFFFFF', text: '#000000', accent: '#1F6FC5' } },
  dark: { name: 'Dark', colors: { bg: '#1E1E1E', text: '#FFFFFF', accent: '#2E9E5B' } },
  auto: { name: 'Auto', colors: { bg: 'auto', text: 'auto', accent: '#1F6FC5' } }
};

const ThemeSelector: React.FC<ThemeSelectorProps> = ({
  isOpen = true,
  onClose,
  onThemeChange
}) => {
  const [activeTheme, setActiveTheme] = useState('dark');
  const [customColors, setCustomColors] = useState({
    primary: '#1F6FC5',
    success: '#2E9E5B',
    danger: '#D93A3A',
    warning: '#F5B517',
    info: '#0E97A3'
  });

  useEffect(() => {
    const stored = localStorage.getItem('bendcore:theme');
    if (stored) setActiveTheme(stored);
  }, []);

  const handleThemeSelect = (theme: string) => {
    setActiveTheme(theme);
    localStorage.setItem('bendcore:theme', theme);
    if (onThemeChange) {
      onThemeChange(theme, THEME_PRESETS[theme as keyof typeof THEME_PRESETS]?.colors || {});
    }
  };

  const handleColorChange = (key: string, value: string) => {
    setCustomColors(prev => ({ ...prev, [key]: value }));
  };

  return (
    <div className="theme-selector">
      <div className="selector-header">
        <h3>Theme & Colors</h3>
        {onClose && <button className="close-btn" onClick={onClose}>×</button>}
      </div>

      <div className="theme-selector-content">
        {/* Theme Presets */}
        <div className="theme-section">
          <h4>Theme Preset</h4>
          <div className="theme-buttons">
            {Object.entries(THEME_PRESETS).map(([key, preset]) => (
              <button
                key={key}
                className={`theme-btn ${activeTheme === key ? 'active' : ''}`}
                onClick={() => handleThemeSelect(key)}
                title={preset.name}
              >
                {preset.name}
              </button>
            ))}
          </div>
        </div>

        {/* Color Customization */}
        <div className="theme-section">
          <h4>Custom Colors</h4>
          <div className="color-picker-group">
            {Object.entries(customColors).map(([key, value]) => (
              <div key={key} className="color-item">
                <label>{key.charAt(0).toUpperCase() + key.slice(1)}:</label>
                <div className="color-input-group">
                  <input
                    type="color"
                    value={value}
                    onChange={(e) => handleColorChange(key, e.target.value)}
                  />
                  <input
                    type="text"
                    value={value}
                    onChange={(e) => handleColorChange(key, e.target.value)}
                    placeholder="#000000"
                  />
                </div>
              </div>
            ))}
          </div>
        </div>

        {/* Preview */}
        <div className="theme-section">
          <h4>Preview</h4>
          <div className="theme-preview">
            {Object.entries(customColors).map(([key, color]) => (
              <div
                key={key}
                className="preview-box"
                style={{ backgroundColor: color }}
                title={key}
              />
            ))}
          </div>
        </div>
      </div>

      {onClose && (
        <div className="selector-footer">
          <button className="btn btn-primary" onClick={onClose}>
            Apply
          </button>
        </div>
      )}
    </div>
  );
};

export default ThemeSelector;
