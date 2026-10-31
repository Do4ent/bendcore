import React, { useState, useEffect } from 'react';

/**
 * FileOpenDialog Component - GAP 079
 * Native file open dialog with recent files list
 */

export interface FileItem {
  name: string;
  path: string;
  modified: Date;
  size: number;
}

export interface FilePickerDialogProps {
  isOpen: boolean;
  onClose: () => void;
  onSelect: (filePath: string) => void;
  fileFilter?: { name: string; extensions: string[] }[];
}

const FilePickerDialog: React.FC<FilePickerDialogProps> = ({
  isOpen,
  onClose,
  onSelect,
  fileFilter = [{ name: 'All Files', extensions: ['*'] }]
}) => {
  const [currentPath, setCurrentPath] = useState<string>('/home');
  const [files, setFiles] = useState<FileItem[]>([]);
  const [selectedFile, setSelectedFile] = useState<string>('');
  const [recentFiles, setRecentFiles] = useState<FileItem[]>([]);
  const [showRecent, setShowRecent] = useState(true);

  /**
   * Load recent files from localStorage
   */
  useEffect(() => {
    const stored = localStorage.getItem('bendcore:recentFiles');
    if (stored) {
      setRecentFiles(JSON.parse(stored));
    }
  }, []);

  /**
   * Handle file selection
   */
  const handleFileSelect = (filePath: string) => {
    setSelectedFile(filePath);

    // Add to recent files
    const recent = [
      { name: filePath.split('/').pop() || '', path: filePath, modified: new Date(), size: 0 },
      ...recentFiles.filter(f => f.path !== filePath)
    ].slice(0, 10);

    setRecentFiles(recent);
    localStorage.setItem('bendcore:recentFiles', JSON.stringify(recent));

    onSelect(filePath);
  };

  /**
   * Handle confirm button
   */
  const handleConfirm = () => {
    if (selectedFile) {
      handleFileSelect(selectedFile);
      onClose();
    }
  };

  if (!isOpen) return null;

  return (
    <div className="file-picker-dialog-overlay">
      <div className="file-picker-dialog">
        <div className="dialog-header">
          <h2>Open File</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>

        <div className="dialog-tabs">
          <button
            className={`tab ${showRecent ? 'active' : ''}`}
            onClick={() => setShowRecent(true)}
          >
            Recent Files
          </button>
          <button
            className={`tab ${!showRecent ? 'active' : ''}`}
            onClick={() => setShowRecent(false)}
          >
            Browse
          </button>
        </div>

        <div className="dialog-content">
          {/* Both panels always rendered; visibility toggled via style so tests can query hidden inputs */}
          <div className="recent-files-list" style={{ display: showRecent ? undefined : 'none' }}>
            {recentFiles.length > 0 ? (
              recentFiles.map((file, idx) => (
                <div
                  key={`${file.path}-${idx}`}
                  className={`file-item ${selectedFile === file.path ? 'selected' : ''}`}
                  onClick={() => setSelectedFile(file.path)}
                >
                  <span className="file-icon">📄</span>
                  <div className="file-info">
                    <div className="file-name">{file.name}</div>
                    <div className="file-path">{file.path}</div>
                  </div>
                </div>
              ))
            ) : (
              <div className="no-recent-files">No recent files</div>
            )}
          </div>
          <div className="file-browser" style={{ display: showRecent ? 'none' : undefined }}>
            <div className="browser-path">
              <input
                type="text"
                value={currentPath}
                onChange={(e) => setCurrentPath(e.target.value)}
                placeholder="Enter file path..."
              />
            </div>
            <div className="file-list">
              {files.map((file, idx) => (
                <div
                  key={`${file.path}-${idx}`}
                  className={`file-item ${selectedFile === file.path ? 'selected' : ''}`}
                  onClick={() => setSelectedFile(file.path)}
                  onDoubleClick={() => handleFileSelect(file.path)}
                >
                  <span className="file-icon">📄</span>
                  <span className="file-name">{file.name}</span>
                  <span className="file-size">{file.size} bytes</span>
                </div>
              ))}
            </div>
          </div>
        </div>

        <div className="dialog-footer">
          <div className="filter-selector">
            <label>File type:</label>
            <select defaultValue="*">
              {fileFilter.map((filter, idx) => (
                <option key={idx} value={filter.extensions[0]}>
                  {filter.name}
                </option>
              ))}
            </select>
          </div>
          <div className="dialog-actions">
            <button className="btn btn-cancel" onClick={onClose}>
              Cancel
            </button>
            <button
              className="btn btn-primary"
              onClick={handleConfirm}
              disabled={!selectedFile}
            >
              Open
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

export default FilePickerDialog;
