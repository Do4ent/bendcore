import React, { useState } from 'react';

/**
 * PluginSettings Component - GAP 089
 * Plugin management UI
 */

export interface Plugin {
  id: string;
  name: string;
  version: string;
  enabled: boolean;
  description: string;
}

export interface PluginSettingsProps {
  isOpen?: boolean;
  onClose?: () => void;
  onSave?: (plugins: Plugin[]) => void;
}

const PluginSettings: React.FC<PluginSettingsProps> = ({
  isOpen = true,
  onClose,
  onSave
}) => {
  const [plugins, setPlugins] = useState<Plugin[]>([
    { id: '1', name: 'MeshOptimizer', version: '1.0.0', enabled: true, description: 'Optimize mesh quality' },
    { id: '2', name: 'MaterialLibrary', version: '2.1.0', enabled: true, description: 'Material database' },
    { id: '3', name: 'CloudSync', version: '1.5.0', enabled: false, description: 'Cloud synchronization' }
  ]);

  const togglePlugin = (id: string) => {
    setPlugins(prev => prev.map(p => p.id === id ? { ...p, enabled: !p.enabled } : p));
  };

  const handleSave = () => {
    if (onSave) onSave(plugins);
    if (onClose) onClose();
  };

  return (
    <div className="plugin-settings">
      <div className="settings-header">
        <h3>Plugin Management</h3>
        {onClose && <button className="close-btn" onClick={onClose}>×</button>}
      </div>

      <div className="plugins-list">
        {plugins.map(plugin => (
          <div key={plugin.id} className="plugin-item">
            <label className="plugin-checkbox">
              <input type="checkbox" checked={plugin.enabled} onChange={() => togglePlugin(plugin.id)} />
              <div className="plugin-info">
                <div className="plugin-name">{plugin.name} v{plugin.version}</div>
                <div className="plugin-description">{plugin.description}</div>
              </div>
            </label>
          </div>
        ))}
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

export default PluginSettings;
