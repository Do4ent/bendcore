import React, { useState, useRef } from 'react';
import {
  RibbonBar,
  StatusBar,
  Tooltip,
  Badge,
  Spinner,
} from '../ui/components';
import './PathSketchScreen.css';

interface PathSketchScreenProps {
  tubeId: string;
}

type SketchPlane = 'xy' | 'xz' | 'yz';
type SketchView = 'front' | 'top' | 'isometric';

export const PathSketchScreen: React.FC<PathSketchScreenProps> = ({ tubeId }) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [activeTab, setActiveTab] = useState<string>('sketch');
  const [selectedPlane, setSelectedPlane] = useState<SketchPlane>('xy');
  const [selectedView, setSelectedView] = useState<SketchView>('front');
  const [statusMessage, setStatusMessage] = useState('Sketch editor ready');
  const [isLoading, setIsLoading] = useState(false);
  const [sketchMode, setSketchMode] = useState<'line' | 'circle' | 'arc' | 'point'>('line');
  const [pointCount, setPointCount] = useState(0);
  const [isSaved, setIsSaved] = useState(true);

  const handleRibbonCommand = (commandId: string) => {
    console.log('Command:', commandId);

    if (commandId === 'sketch-line') {
      setSketchMode('line');
      setStatusMessage('Draw line (click to place points)');
    } else if (commandId === 'sketch-circle') {
      setSketchMode('circle');
      setStatusMessage('Draw circle (click center, then radius)');
    } else if (commandId === 'sketch-arc') {
      setSketchMode('arc');
      setStatusMessage('Draw arc');
    }
  };

  const handleCanvasClick = () => {
    setPointCount(prev => prev + 1);
    setStatusMessage(`Point ${pointCount + 1} placed`);
    setIsSaved(false);
  };

  const handleSaveSketch = async () => {
    setIsLoading(true);
    setStatusMessage('Saving sketch...');

    setTimeout(() => {
      setIsLoading(false);
      setStatusMessage('✅ Sketch saved successfully');
      setIsSaved(true);
    }, 1500);
  };

  return (
    <div className="path-sketch-screen">
      {/* Ribbon */}
      <RibbonBar
        activeTab={activeTab}
        onTabChange={setActiveTab}
        onCommand={handleRibbonCommand}
      />

      {/* Main Content */}
      <div className="sketch-main">
        {/* Tools Panel */}
        <div className="sketch-tools">
          <div className="tools-section">
            <h4>Plane</h4>
            <select
              value={selectedPlane}
              onChange={(e) => setSelectedPlane(e.target.value as SketchPlane)}
              className="tool-select"
            >
              <option value="xy">XY Plane</option>
              <option value="xz">XZ Plane</option>
              <option value="yz">YZ Plane</option>
            </select>
          </div>

          <div className="tools-section">
            <h4>View</h4>
            <div className="view-buttons">
              {(['front', 'top', 'isometric'] as const).map((view) => (
                <button
                  key={view}
                  className={`view-btn ${selectedView === view ? 'active' : ''}`}
                  onClick={() => setSelectedView(view)}
                >
                  {view.charAt(0).toUpperCase() + view.slice(1)}
                </button>
              ))}
            </div>
          </div>

          <div className="tools-section">
            <h4>Sketch Tools</h4>
            <div className="sketch-buttons">
              <Tooltip content="Draw line">
                <button
                  className={`sketch-btn ${sketchMode === 'line' ? 'active' : ''}`}
                  onClick={() => {
                    setSketchMode('line');
                    setStatusMessage('Line tool active');
                  }}
                >
                  ─
                </button>
              </Tooltip>
              <Tooltip content="Draw circle">
                <button
                  className={`sketch-btn ${sketchMode === 'circle' ? 'active' : ''}`}
                  onClick={() => {
                    setSketchMode('circle');
                    setStatusMessage('Circle tool active');
                  }}
                >
                  ◯
                </button>
              </Tooltip>
              <Tooltip content="Draw arc">
                <button
                  className={`sketch-btn ${sketchMode === 'arc' ? 'active' : ''}`}
                  onClick={() => {
                    setSketchMode('arc');
                    setStatusMessage('Arc tool active');
                  }}
                >
                  ⌒
                </button>
              </Tooltip>
            </div>
          </div>

          <button
            className="save-sketch-btn"
            onClick={handleSaveSketch}
            disabled={isSaved || isLoading}
          >
            {isLoading ? <Spinner size="sm" /> : '💾'}
            {isLoading ? 'Saving...' : 'Save Sketch'}
          </button>
        </div>

        {/* Canvas */}
        <div className="sketch-canvas-container">
          <canvas
            ref={canvasRef}
            className="sketch-canvas"
            onClick={handleCanvasClick}
          />
          <div className="canvas-info">
            <div className="info-overlay">
              <h3>{tubeId} - {selectedPlane.toUpperCase()} Plane</h3>
              <p>View: {selectedView}</p>
              <p>Mode: {sketchMode}</p>
              {!isSaved && <Badge variant="warning">Unsaved changes</Badge>}
            </div>
          </div>
        </div>
      </div>

      {/* Status Bar */}
      <StatusBar
        message={statusMessage}
        mode="sketch"
        progress={isLoading ? 50 : undefined}
        isLoading={isLoading}
      />
    </div>
  );
};
