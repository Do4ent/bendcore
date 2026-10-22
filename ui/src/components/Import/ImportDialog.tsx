import React, { useState } from 'react';
import { useIpc } from '../../hooks/useIpc';

interface ImportDialogProps {
  isOpen: boolean;
  onClose: () => void;
  onSuccess?: (tubeId: string) => void;
}

interface FileInfo {
  name: string;
  size: number;
  format: 'csv' | 'json' | null;
}

/**
 * Import Dialog Component
 * Handles CSV/JSON import of tube models
 *
 * Features:
 * - File selection (CSV/JSON)
 * - Format preview
 * - Import options (validation, auto-fix)
 * - Progress tracking
 * - Error handling
 */
export const ImportDialog: React.FC<ImportDialogProps> = ({
  isOpen,
  onClose,
  onSuccess,
}) => {
  const ipc = useIpc();

  // State
  const [selectedFile, setSelectedFile] = useState<File | null>(null);
  const [fileInfo, setFileInfo] = useState<FileInfo | null>(null);
  const [isImporting, setIsImporting] = useState(false);
  const [progress, setProgress] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [preview, setPreview] = useState<string>('');
  const [options, setOptions] = useState({
    validateGeometry: true,
    autoFixErrors: false,
    mergeColinearSegments: true,
    toleranceMm: 0.1,
  });

  const handleFileSelect = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;

    // Determine format
    const format = file.name.endsWith('.csv')
      ? 'csv'
      : file.name.endsWith('.json')
      ? 'json'
      : null;

    if (!format) {
      setError('Please select a CSV or JSON file');
      return;
    }

    setSelectedFile(file);
    setFileInfo({
      name: file.name,
      size: file.size,
      format,
    });
    setError(null);

    // Generate preview
    const reader = new FileReader();
    reader.onload = (event) => {
      const content = event.target?.result as string;
      // Show first 500 chars
      setPreview(content.substring(0, 500));
    };
    reader.readAsText(file);
  };

  const handleImport = async () => {
    if (!selectedFile || !fileInfo?.format) {
      setError('No file selected');
      return;
    }

    setIsImporting(true);
    setProgress(0);
    setError(null);

    try {
      const fileContent = await selectedFile.text();

      // Extract tube parameters from file content
      // This is a simplified example - real implementation would parse properly
      const tubeParams = {
        outerDiameter: 20.0,
        wallThickness: 1.5,
        material: 'Copper',
      };

      // Call IPC to import
      setProgress(33);

      const result = await ipc.importTube({
        format: fileInfo.format,
        content: fileContent,
        ...tubeParams,
        options,
      });

      setProgress(66);

      if (result && result.ok && result.id) {
        setProgress(100);
        if (onSuccess) {
          onSuccess(result.id);
        }
        // Close after success
        setTimeout(() => {
          onClose();
        }, 500);
      } else {
        setError(result?.error || 'Import failed');
      }
    } catch (err) {
      const message = err instanceof Error ? err.message : 'Import failed';
      setError(message);
    } finally {
      setIsImporting(false);
    }
  };

  if (!isOpen) return null;

  return (
    <div className="import-dialog-overlay">
      <div className="import-dialog-box">
        <div className="dialog-header">
          <h2>Import Tube Model</h2>
          <button className="close-btn" onClick={onClose} disabled={isImporting}>
            ✕
          </button>
        </div>

        <div className="dialog-body">
          {/* File Selection */}
          <div className="section">
            <h3>1. Select File</h3>
            <div className="file-input-wrapper">
              <input
                type="file"
                accept=".csv,.json"
                onChange={handleFileSelect}
                disabled={isImporting}
                className="file-input"
                id="file-input"
              />
              <label htmlFor="file-input" className="file-label">
                {fileInfo ? (
                  <>
                    <span className="file-icon">📄</span>
                    <div>
                      <div className="file-name">{fileInfo.name}</div>
                      <div className="file-meta">
                        {fileInfo.format?.toUpperCase()} • {(fileInfo.size / 1024).toFixed(1)} KB
                      </div>
                    </div>
                  </>
                ) : (
                  <>
                    <span className="file-icon">📁</span>
                    <div>
                      <div className="file-prompt">Click to select CSV or JSON file</div>
                      <div className="file-hint">Supported formats: .csv, .json</div>
                    </div>
                  </>
                )}
              </label>
            </div>
          </div>

          {/* Preview */}
          {preview && (
            <div className="section">
              <h3>2. Preview</h3>
              <div className="preview-box">
                <pre>{preview}{preview.length === 500 ? '...' : ''}</pre>
              </div>
            </div>
          )}

          {/* Import Options */}
          <div className="section">
            <h3>3. Options</h3>
            <div className="options-group">
              <label className="checkbox-label">
                <input
                  type="checkbox"
                  checked={options.validateGeometry}
                  onChange={(e) =>
                    setOptions({ ...options, validateGeometry: e.target.checked })
                  }
                  disabled={isImporting}
                />
                <span>Validate geometry</span>
              </label>

              <label className="checkbox-label">
                <input
                  type="checkbox"
                  checked={options.autoFixErrors}
                  onChange={(e) =>
                    setOptions({ ...options, autoFixErrors: e.target.checked })
                  }
                  disabled={isImporting}
                />
                <span>Auto-fix minor errors</span>
              </label>

              <label className="checkbox-label">
                <input
                  type="checkbox"
                  checked={options.mergeColinearSegments}
                  onChange={(e) =>
                    setOptions({
                      ...options,
                      mergeColinearSegments: e.target.checked,
                    })
                  }
                  disabled={isImporting}
                />
                <span>Merge colinear segments</span>
              </label>

              <label className="number-label">
                <span>Tolerance (mm):</span>
                <input
                  type="number"
                  min="0.01"
                  max="1.0"
                  step="0.1"
                  value={options.toleranceMm}
                  onChange={(e) =>
                    setOptions({ ...options, toleranceMm: parseFloat(e.target.value) })
                  }
                  disabled={isImporting}
                />
              </label>
            </div>
          </div>

          {/* Progress */}
          {isImporting && (
            <div className="section">
              <h3>Importing...</h3>
              <div className="progress-bar">
                <div className="progress-fill" style={{ width: `${progress}%` }} />
              </div>
              <div className="progress-text">{progress}%</div>
            </div>
          )}

          {/* Error Message */}
          {error && (
            <div className="error-section">
              <div className="error-icon">⚠</div>
              <div className="error-text">{error}</div>
            </div>
          )}

          {/* IPC Status */}
          {!ipc.connected && (
            <div className="warning-section">
              <div className="warning-icon">⚠</div>
              <div className="warning-text">Backend not connected. Import may fail.</div>
            </div>
          )}
        </div>

        <div className="dialog-footer">
          <button
            className="btn-secondary"
            onClick={onClose}
            disabled={isImporting}
          >
            Cancel
          </button>
          <button
            className="btn-primary"
            onClick={handleImport}
            disabled={!selectedFile || isImporting}
          >
            {isImporting ? `Importing (${progress}%)...` : 'Import'}
          </button>
        </div>
      </div>

      <style jsx>{`
        .import-dialog-overlay {
          position: fixed;
          top: 0;
          left: 0;
          right: 0;
          bottom: 0;
          background: rgba(0, 0, 0, 0.5);
          display: flex;
          align-items: center;
          justify-content: center;
          z-index: 1000;
        }

        .import-dialog-box {
          background: white;
          border-radius: 8px;
          box-shadow: 0 4px 20px rgba(0, 0, 0, 0.15);
          width: 550px;
          max-height: 85vh;
          overflow-y: auto;
          display: flex;
          flex-direction: column;
        }

        .dialog-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 20px;
          border-bottom: 1px solid #e0e0e0;
          flex-shrink: 0;
        }

        .dialog-header h2 {
          margin: 0;
          font-size: 20px;
          font-weight: 600;
        }

        .close-btn {
          background: none;
          border: none;
          font-size: 24px;
          cursor: pointer;
          color: #666;
          padding: 0;
        }

        .close-btn:hover:not(:disabled) {
          color: #000;
        }

        .dialog-body {
          padding: 20px;
          flex: 1;
          overflow-y: auto;
        }

        .section {
          margin-bottom: 24px;
        }

        .section h3 {
          margin: 0 0 12px 0;
          font-size: 14px;
          font-weight: 600;
          color: #333;
        }

        /* File Input */
        .file-input {
          display: none;
        }

        .file-input-wrapper {
          position: relative;
        }

        .file-label {
          display: flex;
          align-items: center;
          gap: 12px;
          padding: 16px;
          border: 2px dashed #ddd;
          border-radius: 6px;
          cursor: pointer;
          transition: all 0.2s;
          background: #fafafa;
        }

        .file-label:hover {
          border-color: #2196F3;
          background: #e3f2fd;
        }

        .file-input:disabled ~ .file-label {
          opacity: 0.6;
          cursor: not-allowed;
        }

        .file-icon {
          font-size: 24px;
          flex-shrink: 0;
        }

        .file-name {
          font-weight: 500;
          color: #333;
          font-size: 13px;
        }

        .file-meta {
          font-size: 12px;
          color: #999;
          margin-top: 2px;
        }

        .file-prompt {
          font-weight: 500;
          color: #333;
          font-size: 13px;
        }

        .file-hint {
          font-size: 12px;
          color: #999;
          margin-top: 4px;
        }

        /* Preview */
        .preview-box {
          background: #f5f5f5;
          border: 1px solid #e0e0e0;
          border-radius: 4px;
          padding: 12px;
          max-height: 150px;
          overflow-y: auto;
        }

        .preview-box pre {
          margin: 0;
          font-size: 11px;
          font-family: 'Courier New', monospace;
          color: #666;
          line-height: 1.4;
        }

        /* Options */
        .options-group {
          display: flex;
          flex-direction: column;
          gap: 10px;
        }

        .checkbox-label,
        .number-label {
          display: flex;
          align-items: center;
          gap: 8px;
          font-size: 13px;
          color: #333;
        }

        .checkbox-label input,
        .number-label input {
          cursor: pointer;
        }

        .number-label {
          margin-top: 8px;
        }

        .number-label input {
          width: 60px;
          padding: 4px 6px;
          border: 1px solid #ddd;
          border-radius: 3px;
          font-size: 12px;
        }

        /* Progress */
        .progress-bar {
          height: 8px;
          background: #e0e0e0;
          border-radius: 4px;
          overflow: hidden;
        }

        .progress-fill {
          height: 100%;
          background: linear-gradient(90deg, #2196F3, #1976D2);
          transition: width 0.3s ease;
        }

        .progress-text {
          text-align: center;
          font-size: 12px;
          color: #666;
          margin-top: 8px;
        }

        /* Messages */
        .error-section,
        .warning-section {
          display: flex;
          gap: 12px;
          padding: 12px;
          border-radius: 4px;
          font-size: 13px;
          margin-bottom: 12px;
        }

        .error-section {
          background: #ffebee;
          border: 1px solid #ef5350;
          color: #c62828;
        }

        .warning-section {
          background: #fff3cd;
          border: 1px solid #ffc107;
          color: #856404;
        }

        .error-icon,
        .warning-icon {
          font-weight: bold;
          font-size: 16px;
          flex-shrink: 0;
        }

        .error-text,
        .warning-text {
          line-height: 1.4;
        }

        /* Footer */
        .dialog-footer {
          display: flex;
          gap: 12px;
          justify-content: flex-end;
          padding: 16px 20px;
          border-top: 1px solid #e0e0e0;
          flex-shrink: 0;
        }

        .btn-secondary,
        .btn-primary {
          padding: 8px 16px;
          border: none;
          border-radius: 4px;
          font-size: 14px;
          font-weight: 500;
          cursor: pointer;
          transition: all 0.2s;
        }

        .btn-secondary {
          background: #f0f0f0;
          color: #333;
        }

        .btn-secondary:hover:not(:disabled) {
          background: #e0e0e0;
        }

        .btn-primary {
          background: #2196F3;
          color: white;
        }

        .btn-primary:hover:not(:disabled) {
          background: #1976D2;
        }

        .btn-secondary:disabled,
        .btn-primary:disabled {
          opacity: 0.5;
          cursor: not-allowed;
        }
      `}</style>
    </div>
  );
};
