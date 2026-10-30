import React, { useState, useRef, useEffect, useCallback } from 'react';

/**
 * SketchUI Component - GAP 091
 * 2D sketch editing interface with canvas rendering and tools panel
 * Main orchestrator for sketch operations
 */

export interface SketchElement {
  id: string;
  type: 'line' | 'circle' | 'arc' | 'rectangle' | 'polygon' | 'point' | 'text';
  points?: Array<{ x: number; y: number }>;
  center?: { x: number; y: number };
  radius?: number;
  angle?: number;
  text?: string;
  constraints?: string[];
  construction?: boolean;
  selected?: boolean;
  color?: string;
  lineWidth?: number;
}

export interface SketchConstraint {
  id: string;
  type: 'coincident' | 'parallel' | 'perpendicular' | 'distance' | 'angle' | 'tangent' | 'equality' | 'symmetry' | 'horizontal' | 'vertical';
  elements: string[];
  value?: number;
  satisfied?: boolean;
}

export interface SketchUIProps {
  isOpen: boolean;
  onClose: () => void;
  onSave?: (elements: SketchElement[], constraints: SketchConstraint[]) => void;
  initialElements?: SketchElement[];
  initialConstraints?: SketchConstraint[];
  isDarkMode?: boolean;
}

const GRID_SIZE = 10;
const CANVAS_WIDTH = 800;
const CANVAS_HEIGHT = 600;

const SketchUI: React.FC<SketchUIProps> = ({
  isOpen,
  onClose,
  onSave,
  initialElements = [],
  initialConstraints = [],
  isDarkMode = false
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [elements, setElements] = useState<SketchElement[]>(initialElements);
  const [constraints, setConstraints] = useState<SketchConstraint[]>(initialConstraints);
  const [selectedElements, setSelectedElements] = useState<Set<string>>(new Set());
  const [activeTool, setActiveTool] = useState<string>('select');
  const [isDrawing, setIsDrawing] = useState(false);
  const [startPoint, setStartPoint] = useState<{ x: number; y: number } | null>(null);
  const [currentPoint, setCurrentPoint] = useState<{ x: number; y: number } | null>(null);
  const [undoStack, setUndoStack] = useState<SketchElement[][]>([initialElements]);
  const [redoStack, setRedoStack] = useState<SketchElement[][]>([]);

  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const gridColor = isDarkMode ? '#3a3a3a' : '#e0e0e0';
  const textColor = isDarkMode ? '#ffffff' : '#000000';
  const selectColor = '#2196f3';
  const toolColor = '#ff9800';
  const constraintColor = '#4caf50';

  /**
   * Draw grid on canvas
   */
  const drawGrid = useCallback((ctx: CanvasRenderingContext2D) => {
    ctx.strokeStyle = gridColor;
    ctx.lineWidth = 0.5;
    for (let i = 0; i < CANVAS_WIDTH; i += GRID_SIZE) {
      ctx.beginPath();
      ctx.moveTo(i, 0);
      ctx.lineTo(i, CANVAS_HEIGHT);
      ctx.stroke();
    }
    for (let i = 0; i < CANVAS_HEIGHT; i += GRID_SIZE) {
      ctx.beginPath();
      ctx.moveTo(0, i);
      ctx.lineTo(CANVAS_WIDTH, i);
      ctx.stroke();
    }
  }, [gridColor]);

  /**
   * Draw sketch elements
   */
  const drawElements = useCallback((ctx: CanvasRenderingContext2D) => {
    elements.forEach(elem => {
      const isSelected = selectedElements.has(elem.id);
      ctx.strokeStyle = isSelected ? selectColor : (elem.color || toolColor);
      ctx.lineWidth = elem.lineWidth || 2;
      ctx.fillStyle = ctx.strokeStyle + '33';

      switch (elem.type) {
        case 'line':
          if (elem.points && elem.points.length === 2) {
            ctx.beginPath();
            ctx.moveTo(elem.points[0].x, elem.points[0].y);
            ctx.lineTo(elem.points[1].x, elem.points[1].y);
            ctx.stroke();
          }
          break;
        case 'circle':
          if (elem.center && elem.radius) {
            ctx.beginPath();
            ctx.arc(elem.center.x, elem.center.y, elem.radius, 0, Math.PI * 2);
            ctx.stroke();
          }
          break;
        case 'rectangle':
          if (elem.points && elem.points.length >= 2) {
            const width = elem.points[1].x - elem.points[0].x;
            const height = elem.points[1].y - elem.points[0].y;
            ctx.strokeRect(elem.points[0].x, elem.points[0].y, width, height);
          }
          break;
        case 'text':
          if (elem.points && elem.text) {
            ctx.fillStyle = textColor;
            ctx.font = '12px sans-serif';
            ctx.fillText(elem.text, elem.points[0].x, elem.points[0].y);
          }
          break;
        case 'point':
          if (elem.points) {
            ctx.fillStyle = elem.color || toolColor;
            ctx.fillRect(elem.points[0].x - 3, elem.points[0].y - 3, 6, 6);
          }
          break;
      }
    });
  }, [elements, selectedElements, selectColor, toolColor, textColor]);

  /**
   * Render canvas
   */
  useEffect(() => {
    if (!canvasRef.current) return;
    const ctx = canvasRef.current.getContext('2d');
    if (!ctx) return;

    ctx.fillStyle = bgColor;
    ctx.fillRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);
    drawGrid(ctx);
    drawElements(ctx);

    // Draw current drawing
    if (isDrawing && startPoint && currentPoint && activeTool === 'line') {
      ctx.strokeStyle = toolColor;
      ctx.lineWidth = 2;
      ctx.setLineDash([5, 5]);
      ctx.beginPath();
      ctx.moveTo(startPoint.x, startPoint.y);
      ctx.lineTo(currentPoint.x, currentPoint.y);
      ctx.stroke();
      ctx.setLineDash([]);
    }
  }, [elements, selectedElements, isDrawing, startPoint, currentPoint, activeTool, bgColor, gridColor, textColor, selectColor, toolColor, drawGrid, drawElements]);

  /**
   * Handle canvas mouse down
   */
  const handleCanvasMouseDown = (e: React.MouseEvent<HTMLCanvasElement>) => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    if (activeTool === 'select') {
      // Selection logic
      let found = false;
      for (const elem of elements) {
        if (elem.points && elem.points[0]) {
          const dx = elem.points[0].x - x;
          const dy = elem.points[0].y - y;
          if (Math.sqrt(dx * dx + dy * dy) < 10) {
            setSelectedElements(new Set([elem.id]));
            found = true;
            break;
          }
        }
      }
      if (!found) {
        setSelectedElements(new Set());
      }
    } else if (activeTool === 'line') {
      if (!isDrawing) {
        setStartPoint({ x, y });
        setIsDrawing(true);
      } else {
        // Complete line
        const newElement: SketchElement = {
          id: `elem_${Date.now()}`,
          type: 'line',
          points: [startPoint!, { x, y }],
          color: toolColor
        };
        setElements([...elements, newElement]);
        pushUndoState([...elements, newElement]);
        setIsDrawing(false);
        setStartPoint(null);
      }
    } else if (activeTool === 'circle') {
      if (!isDrawing) {
        setStartPoint({ x, y });
        setIsDrawing(true);
      } else {
        const radius = Math.sqrt((x - startPoint!.x) ** 2 + (y - startPoint!.y) ** 2);
        const newElement: SketchElement = {
          id: `elem_${Date.now()}`,
          type: 'circle',
          center: startPoint ?? undefined,
          radius,
          color: toolColor
        };
        setElements([...elements, newElement]);
        pushUndoState([...elements, newElement]);
        setIsDrawing(false);
        setStartPoint(null);
      }
    }
  };

  /**
   * Handle canvas mouse move
   */
  const handleCanvasMouseMove = (e: React.MouseEvent<HTMLCanvasElement>) => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    setCurrentPoint({ x, y });
  };

  /**
   * Push undo state
   */
  const pushUndoState = (newElements: SketchElement[]) => {
    setUndoStack([...undoStack, newElements]);
    setRedoStack([]);
  };

  /**
   * Handle undo
   */
  const handleUndo = () => {
    if (undoStack.length > 1) {
      const newStack = [...undoStack];
      newStack.pop();
      const previous = newStack[newStack.length - 1];
      setElements(previous);
      setUndoStack(newStack);
      setRedoStack([...redoStack, elements]);
    }
  };

  /**
   * Handle redo
   */
  const handleRedo = () => {
    if (redoStack.length > 0) {
      const newStack = [...redoStack];
      const next = newStack.pop();
      if (next) {
        setElements(next);
        setUndoStack([...undoStack, next]);
        setRedoStack(newStack);
      }
    }
  };

  /**
   * Handle save
   */
  const handleSave = () => {
    if (onSave) {
      onSave(elements, constraints);
    }
    onClose();
  };

  if (!isOpen) return null;

  return (
    <div className="sketch-ui-overlay">
      <div className="sketch-ui-dialog" style={{ backgroundColor: bgColor, color: textColor }}>
        <div className="sketch-header">
          <h2>Sketch Editor</h2>
          <button className="close-btn" onClick={onClose}>×</button>
        </div>

        <div className="sketch-body">
          <div className="sketch-tools">
            <button
              className={`tool-btn ${activeTool === 'select' ? 'active' : ''}`}
              onClick={() => setActiveTool('select')}
              title="Select Tool"
            >
              ⬌
            </button>
            <button
              className={`tool-btn ${activeTool === 'line' ? 'active' : ''}`}
              onClick={() => setActiveTool('line')}
              title="Line Tool"
            >
              /
            </button>
            <button
              className={`tool-btn ${activeTool === 'circle' ? 'active' : ''}`}
              onClick={() => setActiveTool('circle')}
              title="Circle Tool"
            >
              ○
            </button>
            <button
              className={`tool-btn ${activeTool === 'rectangle' ? 'active' : ''}`}
              onClick={() => setActiveTool('rectangle')}
              title="Rectangle Tool"
            >
              ▭
            </button>
            <div className="tool-separator"></div>
            <button
              className="tool-btn"
              onClick={handleUndo}
              disabled={undoStack.length <= 1}
              title="Undo"
            >
              ↶
            </button>
            <button
              className="tool-btn"
              onClick={handleRedo}
              disabled={redoStack.length === 0}
              title="Redo"
            >
              ↷
            </button>
          </div>

          <div className="sketch-canvas-container">
            <canvas
              ref={canvasRef}
              width={CANVAS_WIDTH}
              height={CANVAS_HEIGHT}
              onMouseDown={handleCanvasMouseDown}
              onMouseMove={handleCanvasMouseMove}
              style={{ border: `1px solid ${textColor}33`, cursor: 'crosshair' }}
            />
          </div>

          <div className="sketch-info">
            <span>Elements: {elements.length} | Constraints: {constraints.length}</span>
            {selectedElements.size > 0 && <span>Selected: {selectedElements.size}</span>}
          </div>
        </div>

        <div className="sketch-footer">
          <button className="btn btn-cancel" onClick={onClose}>
            Cancel
          </button>
          <button className="btn btn-primary" onClick={handleSave}>
            Save Sketch
          </button>
        </div>
      </div>
    </div>
  );
};

export default SketchUI;
