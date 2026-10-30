import React, { useState } from 'react';

/**
 * CloudSettings Component - GAP 090
 * Cloud integration configuration
 */

export interface CloudSettingsProps {
  isOpen?: boolean;
  onClose?: () => void;
  onSave?: (settings: any) => void;
}

const CloudSettings: React.FC<CloudSettingsProps> = ({
  isOpen = true,
  onClose,
  onSave
}) => {
  const [settings, setSettings] = useState({
    cloudEnabled: false,
    accountEmail: '',
    autoSync: true,
    syncInterval: 300,
    offlineMode: false,
    storageUsage: '2.5 GB / 100 GB'
  });

  const handleChange = (key: string, value: any) => {
    setSettings(prev => ({ ...prev, [key]: value }));
  };

  const handleSave = () => {
    if (onSave) onSave(settings);
    if (onClose) onClose();
  };

  return (
    <div className="cloud-settings">
      <div className="settings-header">
        <h3>Cloud Settings</h3>
        {onClose && <button className="close-btn" onClick={onClose}>×</button>}
      </div>

      <div className="settings-content">
        <div className="setting-item">
          <label><input type="checkbox" checked={settings.cloudEnabled} onChange={(e) => handleChange('cloudEnabled', e.target.checked)} /> Enable Cloud Integration</label>
        </div>

        {settings.cloudEnabled && (
          <>
            <div className="setting-item">
              <label>Cloud Account:</label>
              <input type="email" value={settings.accountEmail} onChange={(e) => handleChange('accountEmail', e.target.value)} placeholder="your@email.com" />
            </div>
            <div className="setting-item">
              <label><input type="checkbox" checked={settings.autoSync} onChange={(e) => handleChange('autoSync', e.target.checked)} /> Auto-sync Projects</label>
            </div>
            <div className="setting-item">
              <label>Sync Interval (seconds):</label>
              <input type="number" value={settings.syncInterval} onChange={(e) => handleChange('syncInterval', parseInt(e.target.value))} min="60" max="3600" />
            </div>
            <div className="setting-item">
              <label><input type="checkbox" checked={settings.offlineMode} onChange={(e) => handleChange('offlineMode', e.target.checked)} /> Allow Offline Mode</label>
            </div>
            <div className="storage-info">
              <span className="label">Storage Usage:</span>
              <span className="value">{settings.storageUsage}</span>
            </div>
          </>
        )}
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

export default CloudSettings;
