import React, { useState, useRef } from 'react';
import './SketchesModule.css';

export interface Sketch2D {
  id: string;
  name: string;
  plane: 'xy' | 'xz' | 'yz';
  elements: SketchElement[];
  constraints: SketchConstraint[];
  createdAt: Date;
  updatedAt: Date;
}

export interface SketchElement {
  id: string;
  type: 'line' | 'circle' | 'arc' | 'rectangle' | 'polygon' | 'point';
  points: [number, number][];
  properties: {
    strokeWidth?: number;
    color?: string;
    fillColor?: string;
  };
}

export interface SketchConstraint {
  id: string;
  type: 'horizontal' | 'vertical' | 'parallel' | 'perpendicular' | 'distance' | 'radius';
  elements: string[];
  value?: number;
}

interface SketchesModuleProps {
  tubeId: string;
}

interface SketchTool {
  id: string;
  name: string;
  icon: string;
  cursor: string;
}

export const SketchesModule: React.FC<SketchesModuleProps> = ({ tubeId }) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [sketches, setSketches] = useState<Sketch2D[]>([
    {
      id: 'sketch-001',
      name: 'Front View',
      plane: 'xy',
      elements: [],
      constraints: [],
      createdAt: new Date(),
      updatedAt: new Date(),
    },
    {
      id: 'sketch-002',
      name: 'Side View',
      plane: 'xz',
      elements: [],
      constraints: [],
      createdAt: new Date(),
      updatedAt: new Date(),
    },
  ]);

  const [selectedSketchId, setSelectedSketchId] = useState<string>('sketch-001');
  const [selectedTool, setSelectedTool] = useState<string>('line');
  const [statusMessage, setStatusMessage] = useState('2D Sketches ready');

  const tools: SketchTool[] = [
    { id: 'line', name: 'Line', icon: '─', cursor: 'crosshair' },
    { id: 'circle', name: 'Circle', icon: '◯', cursor: 'crosshair' },
    { id: 'rectangle', name: 'Rectangle', icon: '▭', cursor: 'crosshair' },
    { id: 'arc', name: 'Arc', icon: '⌒', cursor: 'crosshair' },
    { id: 'polygon', name: 'Polygon', icon: '⬠', cursor: 'crosshair' },
  ];

  const constraintTools = [
    { id: 'horizontal', name: 'Horizontal', icon: '─' },
    { id: 'vertical', name: 'Vertical', icon: '│' },
    { id: 'parallel', name: 'Parallel', icon: '∥' },
    { id: 'perpendicular', name: 'Perpendicular', icon: '⊥' },
    { id: 'distance', name: 'Distance', icon: '⟷' },
    { id: 'radius', name: 'Radius', icon: 'R' },
  ];

  const selectedSketch = sketches.find((s) => s.id === selectedSketchId);

  const handleAddSketch = () => {
    const newSketch: Sketch2D = {
      id: `sketch-${Date.now()}`,
      name: `New Sketch ${sketches.length + 1}`,
      plane: 'xy',
      elements: [],
      constraints: [],
      createdAt: new Date(),
      updatedAt: new Date(),
    };
    setSketches([...sketches, newSketch]);
    setSelectedSketchId(newSketch.id);
    setStatusMessage(`✅ Created new sketch: ${newSketch.name}`);
  };

  const handleDeleteSketch = (sketchId: string) => {
    setSketches((prev) => prev.filter((s) => s.id !== sketchId));
    if (selectedSketchId === sketchId) {
      const remaining = sketches.filter((s) => s.id !== sketchId);
      if (remaining.length > 0) {
        setSelectedSketchId(remaining[0].id);
      }
    }
    setStatusMessage('Sketch deleted');
  };

  const handleCanvasClick = (e: React.MouseEvent<HTMLCanvasElement>) => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    setStatusMessage(`Point placed: (${Math.round(x)}, ${Math.round(y)})`);
  };

  return (
    <div className="sketches-module">
      {/* Left: Sketch List & Tools */}
      <div className="sketches-panel">
        {/* Sketch List */}
        <div className="sketches-list">
          <div className="list-header">
            <h4>Sketches</h4>
            <button className="new-btn" onClick={handleAddSketch} title="New sketch">
              +
            </button>
          </div>

          {sketches.map((sketch) => (
            <div
              key={sketch.id}
              className={`sketch-item ${
                selectedSketchId === sketch.id ? 'selected' : ''
              }`}
              onClick={() => setSelectedSketchId(sketch.id)}
            >
              <div className="sketch-info">
                <div className="sketch-name">{sketch.name}</div>
                <div className="sketch-plane">{sketch.plane.toUpperCase()}</div>
              </div>
              <button
                className="sketch-delete"
                onClick={(e) => {
                  e.stopPropagation();
                  handleDeleteSketch(sketch.id);
                }}
              >
                ×
              </button>
            </div>
          ))}
        </div>

        {/* Drawing Tools */}
        <div className="tools-section">
          <h4>Draw Tools</h4>
          <div className="tools-grid">
            {tools.map((tool) => (
              <button
                key={tool.id}
                className={`tool-btn ${selectedTool === tool.id ? 'active' : ''}`}
                onClick={() => {
                  setSelectedTool(tool.id);
                  setStatusMessage(`${tool.name} tool active`);
                }}
                style={{ cursor: tool.cursor }}
                title={tool.name}
              >
                {tool.icon}
              </button>
            ))}
          </div>
        </div>

        {/* Constraint Tools */}
        <div className="tools-section">
          <h4>Constraints</h4>
          <div className="tools-grid">
            {constraintTools.map((tool) => (
              <button
                key={tool.id}
                className="tool-btn"
                onClick={() => setStatusMessage(`${tool.name} constraint`)}
                title={tool.name}
              >
                {tool.icon}
              </button>
            ))}
          </div>
        </div>

        {/* Sketch Info */}
        {selectedSketch && (
          <div className="sketch-details">
            <h4>Sketch Info</h4>
            <div className="detail-row">
              <span>Name:</span>
              <input
                type="text"
                value={selectedSketch.name}
                onChange={(e) => {
                  setSketches((prev) =>
                    prev.map((s) =>
                      s.id === selectedSketchId
                        ? { ...s, name: e.target.value }
                        : s
                    )
                  );
                }}
                className="detail-input"
              />
            </div>
            <div className="detail-row">
              <span>Elements:</span>
              <span className="detail-value">{selectedSketch.elements.length}</span>
            </div>
            <div className="detail-row">
              <span>Constraints:</span>
              <span className="detail-value">
                {selectedSketch.constraints.length}
              </span>
            </div>
            <div className="detail-row">
              <span>Modified:</span>
              <span className="detail-value">
                {selectedSketch.updatedAt.toLocaleDateString()}
              </span>
            </div>
          </div>
        )}
      </div>

      {/* Center: Canvas */}
      <div className="sketches-canvas">
        <canvas
          ref={canvasRef}
          className="sketch-canvas"
          onClick={handleCanvasClick}
          style={{ cursor: 'crosshair' }}
        />
        {selectedSketch && (
          <div className="canvas-info">
            <h3>{selectedSketch.name}</h3>
            <p>Plane: {selectedSketch.plane.toUpperCase()}</p>
            <p>Tool: {selectedTool}</p>
          </div>
        )}
      </div>

      {/* Right: Properties */}
      <div className="sketches-properties">
        <h4>Properties</h4>
        {selectedSketch ? (
          <div className="properties-content">
            <div className="prop-section">
              <label>Stroke Width:</label>
              <input type="number" defaultValue="2" min="1" max="10" />
            </div>
            <div className="prop-section">
              <label>Stroke Color:</label>
              <input type="color" defaultValue="#0066cc" />
            </div>
            <div className="prop-section">
              <label>Fill Color:</label>
              <input type="color" defaultValue="#ffffff" />
            </div>
            <div className="prop-section">
              <label>Show Grid:</label>
              <input type="checkbox" defaultChecked />
            </div>
            <div className="prop-section">
              <label>Grid Size (mm):</label>
              <input type="number" defaultValue="10" />
            </div>
          </div>
        ) : (
          <div className="no-selection">Select a sketch to edit</div>
        )}
      </div>

      {/* Status Bar */}
      <div className="status-bar">
        <span className="status-message">{statusMessage}</span>
        <span className="status-mode">Mode: Sketch</span>
      </div>
    </div>
  );
};
