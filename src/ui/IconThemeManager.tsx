import React, { useState } from 'react';

/**
 * IconThemeManager Component - GAP 085
 * Icon theme selection and customization
 */

interface IconTheme {
  id: string;
  name: string;
  iconSize: number;
  iconColor: string;
  iconStyle: 'outline' | 'filled' | 'rounded';
}

export interface IconThemeManagerProps {
  isOpen?: boolean;
  onClose?: () => void;
  onSave?: (theme: IconTheme) => void;
}

const ICON_THEMES: IconTheme[] = [
  { id: 'default', name: 'Default', iconSize: 24, iconColor: '#000000', iconStyle: 'outline' },
  { id: 'modern', name: 'Modern', iconSize: 20, iconColor: '#2E9E5B', iconStyle: 'filled' },
  { id: 'classic', name: 'Classic', iconSize: 32, iconColor: '#333333', iconStyle: 'outline' }
];

const IconThemeManager: React.FC<IconThemeManagerProps> = ({
  isOpen = true,
  onClose,
  onSave
}) => {
  const [selectedTheme, setSelectedTheme] = useState(ICON_THEMES[0]);
  const [iconSize, setIconSize] = useState(24);
  const [iconColor, setIconColor] = useState('#000000');
  const [iconStyle, setIconStyle] = useState<'outline' | 'filled' | 'rounded'>('outline');

  const handleSelectTheme = (theme: IconTheme) => {
    setSelectedTheme(theme);
    setIconSize(theme.iconSize);
    setIconColor(theme.iconColor);
    setIconStyle(theme.iconStyle);
  };

  const handleSave = () => {
    const theme: IconTheme = {
      id: selectedTheme.id,
      name: selectedTheme.name,
      iconSize,
      iconColor,
      iconStyle
    };
    if (onSave) onSave(theme);
    if (onClose) onClose();
  };

  // Sample icons for preview
  const sampleIcons = ['📁', '💾', '📋', '🔧', '⚙️', '📊'];

  return (
    <div className="icon-theme-manager">
      <div className="manager-header">
        <h3>Icon Theme Manager</h3>
        {onClose && <button className="close-btn" onClick={onClose}>×</button>}
      </div>

      <div className="manager-content">
        {/* Theme Presets */}
        <div className="theme-section">
          <h4>Icon Themes</h4>
          <div className="theme-buttons">
            {ICON_THEMES.map(theme => (
              <button
                key={theme.id}
                className={`theme-btn ${selectedTheme.id === theme.id ? 'active' : ''}`}
                onClick={() => handleSelectTheme(theme)}
              >
                {theme.name}
              </button>
            ))}
          </div>
        </div>

        {/* Customization */}
        <div className="customization-section">
          <h4>Customization</h4>
          <div className="custom-item">
            <label>Icon Size: {iconSize}px</label>
            <input
              type="range"
              min="16"
              max="48"
              value={iconSize}
              onChange={(e) => setIconSize(parseInt(e.target.value))}
            />
          </div>
          <div className="custom-item">
            <label>Icon Color:</label>
            <div className="color-input-group">
              <input
                type="color"
                value={iconColor}
                onChange={(e) => setIconColor(e.target.value)}
              />
              <input
                type="text"
                value={iconColor}
                onChange={(e) => setIconColor(e.target.value)}
              />
            </div>
          </div>
          <div className="custom-item">
            <label>Icon Style:</label>
            <select value={iconStyle} onChange={(e) => setIconStyle(e.target.value as any)}>
              <option value="outline">Outline</option>
              <option value="filled">Filled</option>
              <option value="rounded">Rounded</option>
            </select>
          </div>
        </div>

        {/* Preview */}
        <div className="preview-section">
          <h4>Preview</h4>
          <div className="icon-preview">
            {sampleIcons.map((icon, idx) => (
              <div
                key={idx}
                className="preview-icon"
                style={{
                  fontSize: `${iconSize}px`,
                  color: iconColor
                }}
              >
                {icon}
              </div>
            ))}
          </div>
        </div>
      </div>

      {(onSave || onClose) && (
        <div className="manager-footer">
          {onClose && <button className="btn btn-cancel" onClick={onClose}>Cancel</button>}
          {onSave && <button className="btn btn-primary" onClick={handleSave}>Save Theme</button>}
        </div>
      )}
    </div>
  );
};

export default IconThemeManager;
