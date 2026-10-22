import React, { useState } from 'react';
import { useReporting } from '../../hooks/useReporting';

interface ExportDialogProps {
  reportId: string;
  isOpen: boolean;
  onClose: () => void;
  onSuccess?: (path: string) => void;
}

/**
 * Export Report Dialog
 * Allows selecting export formats and output location
 */
export const ExportDialog: React.FC<ExportDialogProps> = ({
  reportId,
  isOpen,
  onClose,
  onSuccess,
}) => {
  const { exportReport, ipcConnected } = useReporting();
  const [selectedFormats, setSelectedFormats] = useState<string[]>(['PDF']);
  const [outputPath, setOutputPath] = useState('~/bendcore-reports/');
  const [isExporting, setIsExporting] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const availableFormats = ['PDF', 'JSON', 'CSV', 'HTML', 'DXF', 'STEP'];

  const handleFormatToggle = (format: string) => {
    setSelectedFormats((prev) => {
      if (prev.includes(format)) {
        return prev.filter((f) => f !== format);
      } else {
        return [...prev, format];
      }
    });
  };

  const handleExport = async () => {
    if (selectedFormats.length === 0) {
      setError('Please select at least one format');
      return;
    }

    setError(null);
    setIsExporting(true);

    try {
      const path = await exportReport({
        reportId,
        formats: selectedFormats,
        outputPath: outputPath || '~/bendcore-reports/',
      });

      if (onSuccess) {
        onSuccess(path);
      }

      // Show success message
      alert(`Report exported to:\n${path}`);
      onClose();
    } catch (err) {
      const message = err instanceof Error ? err.message : 'Export failed';
      setError(message);
    } finally {
      setIsExporting(false);
    }
  };

  if (!isOpen) return null;

  return (
    <div className="export-dialog-overlay">
      <div className="export-dialog-box">
        <div className="dialog-header">
          <h2>Export Report {reportId}</h2>
          <button className="close-btn" onClick={onClose}>
            ✕
          </button>
        </div>

        <div className="dialog-body">
          {/* Format Selection */}
          <div className="export-section">
            <h3>Export Formats</h3>
            <div className="format-grid">
              {availableFormats.map((format) => (
                <label key={format} className="format-checkbox">
                  <input
                    type="checkbox"
                    checked={selectedFormats.includes(format)}
                    onChange={() => handleFormatToggle(format)}
                    disabled={isExporting}
                  />
                  <span className="format-label">{format}</span>
                </label>
              ))}
            </div>
          </div>

          {/* Output Path */}
          <div className="export-section">
            <label htmlFor="output-path">Output Directory:</label>
            <div className="path-input-group">
              <input
                id="output-path"
                type="text"
                value={outputPath}
                onChange={(e) => setOutputPath(e.target.value)}
                placeholder="~/bendcore-reports/"
                className="path-input"
                disabled={isExporting}
              />
              <button className="browse-btn" disabled={isExporting}>
                Browse...
              </button>
            </div>
          </div>

          {/* Options */}
          <div className="export-section">
            <h3>Options</h3>
            <div className="option-item">
              <label>
                <input
                  type="checkbox"
                  defaultChecked
                  disabled={isExporting}
                />
                Include analysis results
              </label>
            </div>
            <div className="option-item">
              <label>
                <input
                  type="checkbox"
                  defaultChecked
                  disabled={isExporting}
                />
                Include standards compliance
              </label>
            </div>
            <div className="option-item">
              <label>
                <input
                  type="checkbox"
                  defaultChecked={false}
                  disabled={isExporting}
                />
                Include collaboration comments
              </label>
            </div>
          </div>

          {/* Connection Status */}
          {!ipcConnected && (
            <div className="status-warning">
              ⚠ Backend not connected. Export may fail.
            </div>
          )}

          {/* Error Message */}
          {error && (
            <div className="error-message">
              <span className="error-icon">✕</span>
              <span>{error}</span>
            </div>
          )}
        </div>

        <div className="dialog-footer">
          <button
            className="btn-secondary"
            onClick={onClose}
            disabled={isExporting}
          >
            Cancel
          </button>
          <button
            className="btn-primary"
            onClick={handleExport}
            disabled={isExporting || selectedFormats.length === 0}
          >
            {isExporting ? 'Exporting...' : 'Export'}
          </button>
        </div>
      </div>

      <style jsx>{`
        .export-dialog-overlay {
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

        .export-dialog-box {
          background: white;
          border-radius: 8px;
          box-shadow: 0 4px 20px rgba(0, 0, 0, 0.15);
          width: 500px;
          max-height: 90vh;
          overflow-y: auto;
        }

        .dialog-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 20px;
          border-bottom: 1px solid #e0e0e0;
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

        .close-btn:hover {
          color: #000;
        }

        .dialog-body {
          padding: 20px;
        }

        .export-section {
          margin-bottom: 20px;
        }

        .export-section h3 {
          margin: 0 0 12px 0;
          font-size: 14px;
          font-weight: 600;
          color: #333;
        }

        .export-section > label {
          display: block;
          font-size: 13px;
          font-weight: 500;
          margin-bottom: 8px;
          color: #666;
        }

        .format-grid {
          display: grid;
          grid-template-columns: repeat(3, 1fr);
          gap: 10px;
        }

        .format-checkbox {
          display: flex;
          align-items: center;
          gap: 6px;
          padding: 8px;
          border: 1px solid #ddd;
          border-radius: 4px;
          cursor: pointer;
          font-size: 13px;
          transition: all 0.2s;
        }

        .format-checkbox:hover {
          background: #f9f9f9;
        }

        .format-checkbox input {
          cursor: pointer;
        }

        .format-label {
          cursor: pointer;
        }

        .path-input-group {
          display: flex;
          gap: 8px;
        }

        .path-input {
          flex: 1;
          padding: 8px 10px;
          border: 1px solid #ddd;
          border-radius: 4px;
          font-size: 13px;
        }

        .path-input:focus {
          outline: none;
          border-color: #2196F3;
          box-shadow: 0 0 0 2px rgba(33, 150, 243, 0.1);
        }

        .browse-btn {
          padding: 8px 14px;
          background: #f0f0f0;
          border: 1px solid #ddd;
          border-radius: 4px;
          font-size: 13px;
          cursor: pointer;
          transition: all 0.2s;
        }

        .browse-btn:hover:not(:disabled) {
          background: #e0e0e0;
        }

        .browse-btn:disabled {
          opacity: 0.6;
          cursor: not-allowed;
        }

        .option-item {
          padding: 6px 0;
          font-size: 13px;
        }

        .option-item label {
          display: flex;
          align-items: center;
          gap: 6px;
          cursor: pointer;
        }

        .option-item input {
          cursor: pointer;
        }

        .status-warning {
          padding: 10px 12px;
          background: #fff3cd;
          border: 1px solid #ffc107;
          border-radius: 4px;
          color: #856404;
          font-size: 13px;
          margin-bottom: 12px;
        }

        .error-message {
          display: flex;
          gap: 8px;
          padding: 10px 12px;
          background: #ffebee;
          border: 1px solid #ef5350;
          border-radius: 4px;
          color: #c62828;
          font-size: 13px;
          margin-bottom: 12px;
        }

        .error-icon {
          font-weight: bold;
        }

        .dialog-footer {
          display: flex;
          gap: 12px;
          justify-content: flex-end;
          padding: 20px;
          border-top: 1px solid #e0e0e0;
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
