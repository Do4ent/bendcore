import React, { useState } from 'react';

/**
 * KeyboardShortcutEditor Component - GAP 086
 * Custom keyboard shortcut configuration
 */

export interface ShortcutProfile {
  name: string;
  shortcuts: Record<string, string>;
}

export interface KeyboardShortcutEditorProps {
  isOpen?: boolean;
  onClose?: () => void;
  onSave?: (profile: ShortcutProfile) => void;
}

const KeyboardShortcutEditor: React.FC<KeyboardShortcutEditorProps> = ({
  isOpen = true,
  onClose,
  onSave
}) => {
  const [profileName, setProfileName] = useState('Default');
  const [shortcuts, setShortcuts] = useState({
    'new-document': 'Ctrl+N',
    'open-file': 'Ctrl+O',
    'save-file': 'Ctrl+S',
    'save-as': 'Ctrl+Shift+S',
    'undo': 'Ctrl+Z',
    'redo': 'Ctrl+Y',
    'select-all': 'Ctrl+A',
    'delete': 'Delete',
    'copy': 'Ctrl+C',
    'paste': 'Ctrl+V'
  });
  const [editingKey, setEditingKey] = useState<string | null>(null);

  const handleShortcutChange = (action: string, keys: string) => {
    setShortcuts(prev => ({ ...prev, [action]: keys }));
  };

  const handleSave = () => {
    if (onSave) {
      onSave({ name: profileName, shortcuts });
    }
    if (onClose) onClose();
  };

  return (
    <div className="keyboard-shortcut-editor">
      <div className="editor-header">
        <h3>Keyboard Shortcuts</h3>
        {onClose && <button className="close-btn" onClick={onClose}>×</button>}
      </div>

      <div className="editor-content">
        <div className="profile-section">
          <label>Profile Name:</label>
          <input
            type="text"
            value={profileName}
            onChange={(e) => setProfileName(e.target.value)}
            placeholder="Enter profile name..."
          />
        </div>

        <div className="shortcuts-list">
          {Object.entries(shortcuts).map(([action, keys]) => (
            <div key={action} className="shortcut-row">
              <span className="shortcut-action">{action.replace(/-/g, ' ')}</span>
              <input
                type="text"
                value={keys}
                onChange={(e) => handleShortcutChange(action, e.target.value)}
                placeholder="Enter shortcut..."
                className="shortcut-input"
              />
              {editingKey === action && (
                <div className="shortcut-help">
                  <small>e.g., Ctrl+Alt+S, Shift+F5</small>
                </div>
              )}
            </div>
          ))}
        </div>
      </div>

      {(onSave || onClose) && (
        <div className="editor-footer">
          {onClose && <button className="btn btn-cancel" onClick={onClose}>Cancel</button>}
          {onSave && <button className="btn btn-primary" onClick={handleSave}>Save Profile</button>}
        </div>
      )}
    </div>
  );
};

export default KeyboardShortcutEditor;
