import React, { useState } from 'react';

/**
 * SaveDialog Component - GAP 080
 * Save dialog with format selection and preview
 */

export interface SaveDialogProps {
  isOpen: boolean;
  onClose: () => void;
  onSave: (filePath: string, format: string) => void;
  defaultFileName?: string;
  supportedFormats?: { format: string; extension: string; description: string }[];
}

const SaveDialog: React.FC<SaveDialogProps> = ({
  isOpen,
  onClose,
  onSave,
  defaultFileName = 'document',
  supportedFormats = [
    { format: 'BendCore Native', extension: '.bnd', description: 'BendCore project file' },
    { format: 'STEP', extension: '.step', description: 'STEP 3D model' },
    { format: 'IGES', extension: '.iges', description: 'IGES 3D model' },
    { format: 'STL', extension: '.stl', description: 'STL mesh format' }
  ]
}) => {
  const [fileName, setFileName] = useState(defaultFileName);
  const [selectedFormat, setSelectedFormat] = useState(supportedFormats[0].extension);
  const [savePath, setSavePath] = useState('/home');
  const [saveAsName, setSaveAsName] = useState(false);

  /**
   * Handle save button click
   */
  const handleSave = () => {
    const fullPath = `${savePath}/${fileName}${selectedFormat}`;
    onSave(fullPath, selectedFormat);
    onClose();
  };

  /**
   * Validate file name
   */
  const isValidFileName = () => {
    return fileName.trim().length > 0 && !fileName.includes('/');
  };

  if (!isOpen) return null;

  return (
    <div className="save-dialog-overlay">
      <div className="save-dialog">
        <div className="dialog-header">
          <h2>Save File</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>

        <div className="dialog-content">
          {/* Save Location */}
          <div className="dialog-section">
            <label htmlFor="save-path">Save location:</label>
            <div className="path-input-group">
              <input
                id="save-path"
                type="text"
                value={savePath}
                onChange={(e) => setSavePath(e.target.value)}
                placeholder="Enter save path..."
                className="path-input"
              />
              <button className="browse-btn" title="Browse...">📁</button>
            </div>
          </div>

          {/* File Name */}
          <div className="dialog-section">
            <label htmlFor="file-name">File name:</label>
            <div className="filename-input-group">
              <input
                id="file-name"
                type="text"
                value={fileName}
                onChange={(e) => setFileName(e.target.value)}
                placeholder="Enter file name..."
                className={`filename-input ${!isValidFileName() ? 'invalid' : ''}`}
              />
              <span className="file-extension">{selectedFormat}</span>
            </div>
            {!isValidFileName() && (
              <div className="error-message">Please enter a valid file name</div>
            )}
          </div>

          {/* Format Selection */}
          <div className="dialog-section">
            <label htmlFor="file-format">File format:</label>
            <select
              id="file-format"
              value={selectedFormat}
              onChange={(e) => setSelectedFormat(e.target.value)}
              className="format-select"
            >
              {supportedFormats.map((fmt) => (
                <option key={fmt.extension} value={fmt.extension}>
                  {fmt.format} ({fmt.extension}) - {fmt.description}
                </option>
              ))}
            </select>
          </div>

          {/* Save Options */}
          <div className="dialog-section">
            <label>
              <input
                type="checkbox"
                checked={saveAsName}
                onChange={(e) => setSaveAsName(e.target.checked)}
              />
              {' '}Save with new name
            </label>
          </div>

          {/* File Preview */}
          <div className="file-preview">
            <div className="preview-title">Preview:</div>
            <div className="preview-content">
              <div className="preview-item">
                <span className="preview-label">Full path:</span>
                <span className="preview-value">{savePath}/{fileName}{selectedFormat}</span>
              </div>
              <div className="preview-item">
                <span className="preview-label">Format:</span>
                <span className="preview-value">
                  {supportedFormats.find(f => f.extension === selectedFormat)?.description}
                </span>
              </div>
            </div>
          </div>
        </div>

        <div className="dialog-footer">
          <div style={{ flex: 1 }} />
          <button className="btn btn-cancel" onClick={onClose}>
            Cancel
          </button>
          <button
            className="btn btn-primary"
            onClick={handleSave}
            disabled={!isValidFileName()}
          >
            Save
          </button>
        </div>
      </div>
    </div>
  );
};

export default SaveDialog;
