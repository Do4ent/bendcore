import React, { useState } from 'react';
import { useIpc } from '../../hooks/useIpc';

interface ImportDialogProps {
  isOpen: boolean;
  onClose: () => void;
  onSuccess?: (tubeId: string) => void;
  onMeshLoaded?: (mesh: ViewportMesh) => void;
}

interface FileInfo {
  name: string;
  size: number;
  format: 'step' | 'stp' | 'iges' | 'igs' | null;
}

interface MeshData {
  vertices: number[];
  indices: number[];
}

interface ViewportMesh {
  id: string;
  name: string;
  mesh: MeshData;
  color?: number;
}

interface AnalysisResult {
  ok?: boolean;
  tube?: {
    od?: number;
    wall?: number;
    bends?: Array<{ angle: number; radius: number }>;
    length?: number;
    confidence?: number;
  };
  mesh?: MeshData;
  viewportMesh?: ViewportMesh;
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
  onMeshLoaded,
}) => {
  const ipc = useIpc();

  // State
  const [selectedFile, setSelectedFile] = useState<File | null>(null);
  const [fileInfo, setFileInfo] = useState<FileInfo | null>(null);
  const [isImporting, setIsImporting] = useState(false);
  const [progress, setProgress] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [preview, setPreview] = useState<string>('');
  const [analysisResult, setAnalysisResult] = useState<AnalysisResult | null>(null);
  const [options, setOptions] = useState({
    deflection: 0.1,
  });

  const handleFileSelect = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;

    // Determine format
    const ext = file.name.split('.').pop()?.toLowerCase();
    const format = (ext === 'step' || ext === 'stp' || ext === 'iges' || ext === 'igs')
      ? (ext as 'step' | 'stp' | 'iges' | 'igs')
      : null;

    if (!format) {
      setError('Please select a STEP or IGES file (.step, .stp, .iges, .igs)');
      return;
    }

    setSelectedFile(file);
    setFileInfo({
      name: file.name,
      size: file.size,
      format,
    });
    setError(null);
    setAnalysisResult(null);
    setPreview(`File: ${file.name}\nSize: ${(file.size / 1024).toFixed(1)} KB\nFormat: ${format.toUpperCase()}`);
  };

  const handleImport = async () => {
    if (!selectedFile || !fileInfo?.format) {
      setError('No file selected');
      return;
    }

    setIsImporting(true);
    setProgress(0);
    setError(null);
    setAnalysisResult(null);

    try {
      // Read file as binary
      const arrayBuffer = await selectedFile.arrayBuffer();
      const base64 = btoa(String.fromCharCode(...new Uint8Array(arrayBuffer)));

      setProgress(30);

      // Send to backend for analysis
      const response = await fetch('/api/import/analyze', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          file_content: base64,
          deflection: options.deflection,
        }),
      });

      setProgress(70);

      const result = await response.json();

      if (result.success) {
        setProgress(100);
        setAnalysisResult(result.data);
        if (result.data.viewportMesh && onMeshLoaded) {
          onMeshLoaded(result.data.viewportMesh);
        }
        if (onSuccess) {
          onSuccess(result.data.viewportMesh?.id || `import_${Date.now()}`);
        }
      } else {
        setError(result.error || 'Import failed');
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
                accept=".step,.stp,.iges,.igs"
                onChange={handleFileSelect}
                disabled={isImporting}
                className="file-input"
                id="file-input"
              />
              <label htmlFor="file-input" className="file-label">
                {fileInfo ? (
                  <>
                    <span className="file-icon">📦</span>
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
                      <div className="file-prompt">Click to select STEP or IGES file</div>
                      <div className="file-hint">Supported formats: .step, .stp, .iges, .igs</div>
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

          {/* Analysis Options */}
          <div className="section">
            <h3>2. Options</h3>
            <div className="options-group">
              <label className="number-label">
                <span>Deflection (mm):</span>
                <input
                  type="number"
                  min="0.01"
                  max="1.0"
                  step="0.05"
                  value={options.deflection}
                  onChange={(e) =>
                    setOptions({ ...options, deflection: parseFloat(e.target.value) })
                  }
                  disabled={isImporting}
                />
              </label>
            </div>
          </div>

          {/* Progress */}
          {isImporting && (
            <div className="section">
              <h3>Analyzing...</h3>
              <div className="progress-bar">
                <div className="progress-fill" style={{ width: `${progress}%` }} />
              </div>
              <div className="progress-text">{progress}%</div>
            </div>
          )}

          {/* Analysis Results */}
          {analysisResult && analysisResult.tube && (
            <div className="section">
              <h3>3. Analysis Results</h3>
              <div className="results-grid">
                {analysisResult.tube.od && (
                  <div className="result-item">
                    <span className="result-label">OD:</span>
                    <span className="result-value">{analysisResult.tube.od.toFixed(2)} mm</span>
                  </div>
                )}
                {analysisResult.tube.wall && (
                  <div className="result-item">
                    <span className="result-label">Wall:</span>
                    <span className="result-value">{analysisResult.tube.wall.toFixed(2)} mm</span>
                  </div>
                )}
                {analysisResult.tube.length && (
                  <div className="result-item">
                    <span className="result-label">Length:</span>
                    <span className="result-value">{analysisResult.tube.length.toFixed(2)} mm</span>
                  </div>
                )}
                {analysisResult.tube.confidence !== undefined && (
                  <div className="result-item">
                    <span className="result-label">Confidence:</span>
                    <span className="result-value">{(analysisResult.tube.confidence * 100).toFixed(0)}%</span>
                  </div>
                )}
              </div>
              {analysisResult.tube.bends && analysisResult.tube.bends.length > 0 && (
                <div className="bends-list">
                  <h4>Bends</h4>
                  {analysisResult.tube.bends.map((bend, i) => (
                    <div key={i} className="bend-item">
                      {bend.angle.toFixed(0)}° @ R{bend.radius.toFixed(1)}
                    </div>
                  ))}
                </div>
              )}
              {analysisResult.mesh && (
                <div className="mesh-info">
                  ✓ 3D mesh loaded ({analysisResult.mesh.vertices.length / 3} vertices, {analysisResult.mesh.indices.length / 3} triangles)
                </div>
              )}
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

        /* Results */
        .results-grid {
          display: grid;
          grid-template-columns: 1fr 1fr;
          gap: 12px;
          margin-bottom: 16px;
        }

        .result-item {
          display: flex;
          justify-content: space-between;
          padding: 10px;
          background: #f9f9f9;
          border: 1px solid #e0e0e0;
          border-radius: 4px;
          font-size: 13px;
        }

        .result-label {
          font-weight: 600;
          color: #666;
        }

        .result-value {
          color: #333;
          font-weight: 500;
        }

        .bends-list {
          margin-top: 12px;
          padding: 10px;
          background: #f9f9f9;
          border: 1px solid #e0e0e0;
          border-radius: 4px;
        }

        .bends-list h4 {
          margin: 0 0 8px 0;
          font-size: 12px;
          font-weight: 600;
          color: #666;
        }

        .bend-item {
          padding: 6px 0;
          font-size: 12px;
          color: #333;
        }

        .mesh-info {
          margin-top: 12px;
          padding: 10px;
          background: #e8f5e9;
          border: 1px solid #4caf50;
          border-radius: 4px;
          font-size: 12px;
          color: #2e7d32;
        }
      `}</style>
    </div>
  );
};
