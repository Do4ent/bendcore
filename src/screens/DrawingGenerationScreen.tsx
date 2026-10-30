import React, { useState } from 'react';
import {
  RibbonBar,
  StatusBar,
  Tooltip,
  Badge,
  Spinner,
  Icon,
} from '../ui/components';
import './DrawingGenerationScreen.css';

interface DrawingGenerationScreenProps {
  tubeId: string;
}

type DrawingView = 'front' | 'top' | 'isometric' | '3d';
type ExportFormat = 'pdf' | 'dwg' | 'png' | 'svg';

export const DrawingGenerationScreen: React.FC<DrawingGenerationScreenProps> = ({
  tubeId,
}) => {
  const [activeTab, setActiveTab] = useState<string>('view');
  const [selectedView, setSelectedView] = useState<DrawingView>('3d');
  const [showDimensions, setShowDimensions] = useState(true);
  const [showNotes, setShowNotes] = useState(true);
  const [statusMessage, setStatusMessage] = useState('Drawing ready for export');
  const [isExporting, setIsExporting] = useState(false);
  const [exportProgress, setExportProgress] = useState(0);

  const views: { name: DrawingView; label: string; icon: string }[] = [
    { name: 'front', label: 'Front', icon: '⊡' },
    { name: 'top', label: 'Top', icon: '▬' },
    { name: 'isometric', label: 'Isometric', icon: '◈' },
    { name: '3d', label: '3D', icon: '█' },
  ];

  const exportFormats: { format: ExportFormat; label: string; icon: string }[] = [
    { format: 'pdf', label: 'PDF', icon: '📄' },
    { format: 'dwg', label: 'DWG', icon: '📋' },
    { format: 'png', label: 'PNG', icon: '🖼' },
    { format: 'svg', label: 'SVG', icon: '✏' },
  ];

  const handleExport = async (format: ExportFormat) => {
    setIsExporting(true);
    setStatusMessage(`Exporting to ${format.toUpperCase()}...`);
    setExportProgress(0);

    // Simulate export progress
    const progressInterval = setInterval(() => {
      setExportProgress((prev) => {
        if (prev >= 100) {
          clearInterval(progressInterval);
          return 100;
        }
        return prev + Math.random() * 30;
      });
    }, 200);

    setTimeout(() => {
      clearInterval(progressInterval);
      setIsExporting(false);
      setExportProgress(100);
      setStatusMessage(`✅ Exported as ${format.toUpperCase()}`);
    }, 2000);
  };

  return (
    <div className="drawing-generation-screen">
      {/* Ribbon */}
      <RibbonBar
        activeTab={activeTab}
        onTabChange={setActiveTab}
        onCommand={(cmd) => console.log('Command:', cmd)}
      />

      {/* Main Content */}
      <div className="drawing-main">
        {/* Left Panel: View Options */}
        <div className="drawing-controls">
          <div className="controls-section">
            <h4>View</h4>
            <div className="view-grid">
              {views.map((view) => (
                <Tooltip key={view.name} content={view.label}>
                  <button
                    className={`view-btn ${selectedView === view.name ? 'active' : ''}`}
                    onClick={() => {
                      setSelectedView(view.name);
                      setStatusMessage(`View: ${view.label}`);
                    }}
                  >
                    <span className="view-icon">{view.icon}</span>
                    <span className="view-label">{view.label}</span>
                  </button>
                </Tooltip>
              ))}
            </div>
          </div>

          <div className="controls-section">
            <h4>Display Options</h4>
            <label className="checkbox-label">
              <input
                type="checkbox"
                checked={showDimensions}
                onChange={(e) => setShowDimensions(e.target.checked)}
              />
              <span>Show Dimensions</span>
            </label>
            <label className="checkbox-label">
              <input
                type="checkbox"
                checked={showNotes}
                onChange={(e) => setShowNotes(e.target.checked)}
              />
              <span>Show Notes</span>
            </label>
          </div>

          <div className="controls-section">
            <Badge variant="success">Ready to Export</Badge>
          </div>
        </div>

        {/* Center Panel: Drawing Canvas */}
        <div className="drawing-canvas">
          <canvas id="drawing-canvas" />
          <div className="canvas-overlay">
            <h3>{tubeId} - {selectedView.toUpperCase()} View</h3>
            {showDimensions && (
              <div className="dimensions-info">
                <span>OD: 25.4mm</span>
                <span>Wall: 2.0mm</span>
                <span>Length: 500mm</span>
              </div>
            )}
            {showNotes && (
              <div className="notes-info">
                <p>Material: Steel</p>
                <p>Scale: 1:1</p>
              </div>
            )}
          </div>
        </div>

        {/* Right Panel: Export Options */}
        <div className="export-panel">
          <div className="export-header">
            <h3>
              <Icon name="download" size="sm" />
              Export Drawing
            </h3>
          </div>

          <div className="export-formats">
            {exportFormats.map((fmt) => (
              <button
                key={fmt.format}
                className="export-btn"
                onClick={() => handleExport(fmt.format)}
                disabled={isExporting}
              >
                <span className="export-icon">{fmt.icon}</span>
                <span className="export-label">{fmt.label}</span>
              </button>
            ))}
          </div>

          {isExporting && (
            <div className="export-progress">
              <div className="progress-info">
                <Spinner size="sm" />
                <span>Exporting...</span>
              </div>
              <div className="progress-bar">
                <div
                  className="progress-fill"
                  style={{ width: `${exportProgress}%` }}
                />
              </div>
              <div className="progress-percent">{Math.round(exportProgress)}%</div>
            </div>
          )}

          <div className="export-info">
            <h4>Export Settings</h4>
            <div className="setting">
              <label>Resolution:</label>
              <select className="setting-select">
                <option>96 DPI (Screen)</option>
                <option>150 DPI (Print)</option>
                <option>300 DPI (High Quality)</option>
              </select>
            </div>
            <div className="setting">
              <label>Scale:</label>
              <select className="setting-select">
                <option>1:1 (Actual)</option>
                <option>1:2</option>
                <option>1:5</option>
                <option>1:10</option>
              </select>
            </div>
          </div>
        </div>
      </div>

      {/* Status Bar */}
      <StatusBar
        message={statusMessage}
        mode="Drawing"
        progress={isExporting ? exportProgress : undefined}
      />
    </div>
  );
};
