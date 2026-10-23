/**
 * SketchEditor (Task 1.5): Canvas-based 2D sketch editor for drawing annotations
 * on technical drawings. Supports lines, arcs, text, dimensions with undo/redo,
 * selection, editing, dark mode, and localStorage persistence.
 */
import React, { useCallback, useEffect, useRef, useState } from 'react';
import type { SketchElement, SketchDimension } from './core/types';
import { t } from './core/i18n';
import './panels.css';

export type SketchTool = 'line' | 'arc' | 'text' | 'dimension' | 'select' | 'delete';

export interface SketchEditorProps {
  /** Sheet number for saving sketches */
  sheetNumber: number;
  /** Sketch name/title */
  sketchName: string;
  /** Initial elements (for editing existing sketch) */
  initialElements?: SketchElement[];
  /** Initial dimensions */
  initialDimensions?: SketchDimension[];
  /** Called when sketch is saved */
  onSave?: (elements: SketchElement[], dimensions: SketchDimension[]) => void;
  /** Called when sketch is cancelled */
  onCancel?: () => void;
  /** Dark mode flag */
  isDarkMode?: boolean;
}

interface UndoState {
  elements: SketchElement[];
  dimensions: SketchDimension[];
}

interface SelectionBox {
  startX: number;
  startY: number;
  endX: number;
  endY: number;
}

const CANVAS_PADDING = 20;
const GRID_SIZE = 10;
const MIN_DIMENSION = 20;

export const SketchEditor: React.FC<SketchEditorProps> = ({
  sheetNumber,
  sketchName,
  initialElements = [],
  initialDimensions = [],
  onSave,
  onCancel,
  isDarkMode = false,
}) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [elements, setElements] = useState<SketchElement[]>(initialElements);
  const [dimensions, setDimensions] = useState<SketchDimension[]>(initialDimensions);
  const [tool, setTool] = useState<SketchTool>('select');
  const [selected, setSelected] = useState<Set<string>>(new Set());
  const [isDrawing, setIsDrawing] = useState(false);
  const [startPoint, setStartPoint] = useState<{ x: number; y: number } | null>(null);
  const [currentPoint, setCurrentPoint] = useState<{ x: number; y: number } | null>(null);
  const [textInput, setTextInput] = useState('');
  const [showTextInput, setShowTextInput] = useState(false);
  const [textPosition, setTextPosition] = useState<{ x: number; y: number } | null>(null);
  const [undoStack, setUndoStack] = useState<UndoState[]>([{ elements, dimensions }]);
  const [redoStack, setRedoStack] = useState<UndoState[]>([]);
  const [selectionBox, setSelectionBox] = useState<SelectionBox | null>(null);
  const [feedback, setFeedback] = useState<string>('');

  // Colors based on theme
  const bgColor = isDarkMode ? '#2a2a2a' : '#ffffff';
  const gridColor = isDarkMode ? '#3a3a3a' : '#e0e0e0';
  const textColor = isDarkMode ? '#ffffff' : '#000000';
  const selectColor = '#2196f3';
  const toolColor = '#ff9800';

  // Save to localStorage
  const saveToLocalStorage = useCallback(() => {
    const sketch = { sheetNumber, name: sketchName, elements, dimensions };
    localStorage.setItem(`sketch_${sheetNumber}_${sketchName}`, JSON.stringify(sketch));
  }, [elements, dimensions, sheetNumber, sketchName]);

  // Undo/Redo handlers
  const pushUndoState = useCallback(() => {
    setUndoStack((prev) => [...prev, { elements, dimensions }]);
    setRedoStack([]);
  }, [elements, dimensions]);

  const undo = useCallback(() => {
    if (undoStack.length > 1) {
      const newStack = [...undoStack];
      const current = newStack.pop();
      if (current) {
        const previous = newStack[newStack.length - 1];
        setElements(previous.elements);
        setDimensions(previous.dimensions);
        setUndoStack(newStack);
        setRedoStack((prev) => [current, ...prev]);
        setFeedback('Undone');
      }
    }
  }, [undoStack]);

  const redo = useCallback(() => {
    if (redoStack.length > 0) {
      const next = redoStack[0];
      setElements(next.elements);
      setDimensions(next.dimensions);
      setUndoStack((prev) => [...prev, next]);
      setRedoStack((prev) => prev.slice(1));
      setFeedback('Redone');
    }
  }, [redoStack]);

  // Distance calculation
  const distance = (x1: number, y1: number, x2: number, y2: number) => {
    return Math.sqrt((x2 - x1) ** 2 + (y2 - y1) ** 2);
  };

  // Point-in-box test
  const pointInBox = (x: number, y: number, x1: number, y1: number, x2: number, y2: number) => {
    const minX = Math.min(x1, x2);
    const maxX = Math.max(x1, x2);
    const minY = Math.min(y1, y2);
    const maxY = Math.max(y1, y2);
    return x >= minX && x <= maxX && y >= minY && y <= maxY;
  };

  // Hit test for elements
  const hitTestElement = (x: number, y: number, element: SketchElement): boolean => {
    const tolerance = 5;
    if (element.type === 'line') {
      const d = distance(element.p0_x, element.p0_y, element.p1_x, element.p1_y);
      const d1 = distance(x, y, element.p0_x, element.p0_y);
      const d2 = distance(x, y, element.p1_x, element.p1_y);
      return d1 + d2 - d < tolerance;
    } else if (element.type === 'arc') {
      // Simplified arc hit test
      const arcDist = distance(x, y, element.p0_x, element.p0_y);
      const radius = element.radius ?? 0;
      return Math.abs(arcDist - radius) < tolerance;
    } else if (element.type === 'text') {
      return pointInBox(x, y, element.p0_x - 50, element.p0_y - 10, element.p0_x + 50, element.p0_y + 10);
    }
    return false;
  };

  // Hit test for dimensions
  const hitTestDimension = (x: number, y: number, dim: SketchDimension): boolean => {
    const tolerance = 5;
    return distance(x, y, dim.p0_x, dim.p0_y) < tolerance || distance(x, y, dim.p1_x, dim.p1_y) < tolerance;
  };

  // Draw grid
  const drawGrid = (ctx: CanvasRenderingContext2D, width: number, height: number) => {
    ctx.strokeStyle = gridColor;
    ctx.lineWidth = 0.5;
    for (let x = 0; x < width; x += GRID_SIZE) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, height);
      ctx.stroke();
    }
    for (let y = 0; y < height; y += GRID_SIZE) {
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(width, y);
      ctx.stroke();
    }
  };

  // Draw element
  const drawElement = (ctx: CanvasRenderingContext2D, elem: SketchElement, isSelected: boolean) => {
    ctx.lineWidth = isSelected ? 2 : 1.5;
    ctx.strokeStyle = isSelected ? selectColor : (elem.is_construction ? '#999999' : '#000000');
    ctx.fillStyle = textColor;
    ctx.font = `${elem.text_height ?? 12}px monospace`;

    switch (elem.type) {
      case 'line':
        ctx.beginPath();
        ctx.moveTo(elem.p0_x, elem.p0_y);
        ctx.lineTo(elem.p1_x, elem.p1_y);
        ctx.stroke();
        if (isSelected) {
          // Draw control points
          ctx.fillStyle = selectColor;
          ctx.fillRect(elem.p0_x - 3, elem.p0_y - 3, 6, 6);
          ctx.fillRect(elem.p1_x - 3, elem.p1_y - 3, 6, 6);
        }
        break;
      case 'arc':
        {
          const radius = elem.radius ?? 50;
          const startAngle = Math.atan2(elem.p0_y - elem.p1_y, elem.p0_x - elem.p1_x);
          const endAngle = startAngle + Math.PI;
          ctx.beginPath();
          ctx.arc(elem.p1_x, elem.p1_y, radius, startAngle, endAngle);
          ctx.stroke();
        }
        break;
      case 'text':
        ctx.fillText(elem.text ?? 'Text', elem.p0_x, elem.p0_y);
        break;
      case 'dimension':
        // Dimensions are drawn separately
        break;
    }
  };

  // Draw dimension
  const drawDimension = (ctx: CanvasRenderingContext2D, dim: SketchDimension, isSelected: boolean) => {
    ctx.lineWidth = isSelected ? 2 : 1.5;
    ctx.strokeStyle = isSelected ? selectColor : '#ff0000';
    ctx.fillStyle = isSelected ? selectColor : '#ff0000';
    ctx.font = '11px monospace';

    // Dimension line
    ctx.beginPath();
    ctx.moveTo(dim.p0_x, dim.p0_y);
    ctx.lineTo(dim.p1_x, dim.p1_y);
    ctx.stroke();

    // Text with offset
    const offsetX = dim.offset_x ?? 10;
    const offsetY = dim.offset_y ?? 10;
    ctx.fillText(dim.text, dim.p0_x + offsetX, dim.p0_y + offsetY);

    // Control points
    if (isSelected) {
      ctx.fillRect(dim.p0_x - 3, dim.p0_y - 3, 6, 6);
      ctx.fillRect(dim.p1_x - 3, dim.p1_y - 3, 6, 6);
    }
  };

  // Render canvas
  const render = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    // Clear
    ctx.fillStyle = bgColor;
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    // Draw grid
    drawGrid(ctx, canvas.width, canvas.height);

    // Draw elements
    elements.forEach((elem) => {
      drawElement(ctx, elem, selected.has(elem.id));
    });

    // Draw dimensions
    dimensions.forEach((dim) => {
      drawDimension(ctx, dim, selected.has(dim.id));
    });

    // Draw current drawing (preview)
    if (isDrawing && startPoint && currentPoint) {
      ctx.strokeStyle = toolColor;
      ctx.lineWidth = 2;
      if (tool === 'line') {
        ctx.beginPath();
        ctx.moveTo(startPoint.x, startPoint.y);
        ctx.lineTo(currentPoint.x, currentPoint.y);
        ctx.stroke();
      } else if (tool === 'arc') {
        const radius = distance(startPoint.x, startPoint.y, currentPoint.x, currentPoint.y);
        ctx.beginPath();
        ctx.arc(startPoint.x, startPoint.y, radius, 0, Math.PI * 2);
        ctx.stroke();
      } else if (tool === 'dimension') {
        ctx.beginPath();
        ctx.moveTo(startPoint.x, startPoint.y);
        ctx.lineTo(currentPoint.x, currentPoint.y);
        ctx.stroke();
      }
    }

    // Draw selection box
    if (selectionBox) {
      ctx.strokeStyle = selectColor;
      ctx.lineWidth = 1;
      ctx.setLineDash([4, 4]);
      ctx.strokeRect(
        Math.min(selectionBox.startX, selectionBox.endX),
        Math.min(selectionBox.startY, selectionBox.endY),
        Math.abs(selectionBox.endX - selectionBox.startX),
        Math.abs(selectionBox.endY - selectionBox.startY),
      );
      ctx.setLineDash([]);
    }
  }, [elements, dimensions, selected, isDrawing, startPoint, currentPoint, tool, selectionBox, bgColor, gridColor, textColor, toolColor, selectColor]);

  // Canvas event handlers
  const handleCanvasMouseDown = useCallback(
    (e: React.MouseEvent<HTMLCanvasElement>) => {
      const canvas = canvasRef.current;
      if (!canvas) return;

      const rect = canvas.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;

      if (tool === 'select') {
        // Check if clicking on element
        let found = false;
        for (const elem of elements) {
          if (hitTestElement(x, y, elem)) {
            if (!e.ctrlKey) setSelected(new Set());
            setSelected((prev) => new Set(prev).add(elem.id));
            found = true;
            break;
          }
        }
        for (const dim of dimensions) {
          if (hitTestDimension(x, y, dim)) {
            if (!e.ctrlKey) setSelected(new Set());
            setSelected((prev) => new Set(prev).add(dim.id));
            found = true;
            break;
          }
        }
        if (!found) {
          if (!e.ctrlKey) setSelected(new Set());
          setSelectionBox({ startX: x, startY: y, endX: x, endY: y });
        }
      } else if (tool === 'delete') {
        for (const elem of elements) {
          if (hitTestElement(x, y, elem)) {
            pushUndoState();
            setElements((prev) => prev.filter((e) => e.id !== elem.id));
            setFeedback('Element deleted');
            return;
          }
        }
        for (const dim of dimensions) {
          if (hitTestDimension(x, y, dim)) {
            pushUndoState();
            setDimensions((prev) => prev.filter((d) => d.id !== dim.id));
            setFeedback('Dimension deleted');
            return;
          }
        }
      } else {
        setIsDrawing(true);
        setStartPoint({ x, y });
      }
    },
    [tool, elements, dimensions, selected, pushUndoState],
  );

  const handleCanvasMouseMove = useCallback(
    (e: React.MouseEvent<HTMLCanvasElement>) => {
      const canvas = canvasRef.current;
      if (!canvas) return;

      const rect = canvas.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;

      setCurrentPoint({ x, y });

      if (selectionBox && isDrawing) {
        setSelectionBox((prev) => (prev ? { ...prev, endX: x, endY: y } : null));
      }
    },
    [isDrawing, selectionBox],
  );

  const handleCanvasMouseUp = useCallback(
    (e: React.MouseEvent<HTMLCanvasElement>) => {
      const canvas = canvasRef.current;
      if (!canvas) return;

      const rect = canvas.getBoundingClientRect();
      const x = e.clientX - rect.left;
      const y = e.clientY - rect.top;

      if (tool === 'select' && selectionBox) {
        // Select all elements in box
        const minX = Math.min(selectionBox.startX, x);
        const maxX = Math.max(selectionBox.startX, x);
        const minY = Math.min(selectionBox.startY, y);
        const maxY = Math.max(selectionBox.startY, y);

        const newSelected = new Set<string>();
        elements.forEach((elem) => {
          if (pointInBox(elem.p0_x, elem.p0_y, minX, minY, maxX, maxY) ||
              pointInBox(elem.p1_x, elem.p1_y, minX, minY, maxX, maxY)) {
            newSelected.add(elem.id);
          }
        });
        dimensions.forEach((dim) => {
          if (pointInBox(dim.p0_x, dim.p0_y, minX, minY, maxX, maxY) ||
              pointInBox(dim.p1_x, dim.p1_y, minX, minY, maxX, maxY)) {
            newSelected.add(dim.id);
          }
        });
        setSelected(newSelected);
        setSelectionBox(null);
      } else if (isDrawing && startPoint && distance(startPoint.x, startPoint.y, x, y) > MIN_DIMENSION) {
        pushUndoState();

        if (tool === 'line') {
          const elem: SketchElement = {
            id: `line_${Date.now()}`,
            type: 'line',
            p0_x: startPoint.x,
            p0_y: startPoint.y,
            p1_x: x,
            p1_y: y,
            is_construction: false,
          };
          setElements((prev) => [...prev, elem]);
          setFeedback('Line created');
        } else if (tool === 'arc') {
          const elem: SketchElement = {
            id: `arc_${Date.now()}`,
            type: 'arc',
            p0_x: startPoint.x,
            p0_y: startPoint.y,
            p1_x: x,
            p1_y: y,
            radius: distance(startPoint.x, startPoint.y, x, y) / 2,
            is_construction: false,
          };
          setElements((prev) => [...prev, elem]);
          setFeedback('Arc created');
        } else if (tool === 'dimension') {
          const dim: SketchDimension = {
            id: `dim_${Date.now()}`,
            p0_x: startPoint.x,
            p0_y: startPoint.y,
            p1_x: x,
            p1_y: y,
            text: distance(startPoint.x, startPoint.y, x, y).toFixed(2),
          };
          setDimensions((prev) => [...prev, dim]);
          setFeedback('Dimension created');
        }
      }

      setIsDrawing(false);
      setStartPoint(null);
      setSelectionBox(null);
    },
    [tool, isDrawing, startPoint, elements, dimensions, selected, selectionBox, pushUndoState],
  );

  // Text input handler
  const handleTextInput = useCallback(() => {
    if (textPosition && textInput.trim()) {
      pushUndoState();
      const elem: SketchElement = {
        id: `text_${Date.now()}`,
        type: 'text',
        p0_x: textPosition.x,
        p0_y: textPosition.y,
        p1_x: textPosition.x + 100,
        p1_y: textPosition.y + 20,
        text: textInput,
        text_height: 12,
        is_construction: false,
      };
      setElements((prev) => [...prev, elem]);
      setFeedback('Text created');
    }
    setShowTextInput(false);
    setTextInput('');
    setTextPosition(null);
  }, [textPosition, textInput, pushUndoState]);

  // Keyboard shortcuts
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 'z') {
        e.preventDefault();
        undo();
      } else if ((e.ctrlKey || e.metaKey) && e.key === 'y') {
        e.preventDefault();
        redo();
      } else if (e.key === 'Delete' && selected.size > 0) {
        pushUndoState();
        setElements((prev) => prev.filter((e) => !selected.has(e.id)));
        setDimensions((prev) => prev.filter((d) => !selected.has(d.id)));
        setSelected(new Set());
        setFeedback('Selected elements deleted');
      } else if (e.key === 'Escape') {
        setSelected(new Set());
        setShowTextInput(false);
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [undo, redo, selected, pushUndoState]);

  // Render
  useEffect(() => {
    render();
  }, [render]);

  return (
    <div className="sketch-editor" style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Toolbar */}
      <div className="bc-header" style={{ padding: '8px 12px', borderBottom: '1px solid #ccc', gap: '8px' }}>
        <h3 style={{ margin: 0, flex: 1 }}>Sketch: {sketchName}</h3>
        <div style={{ display: 'flex', gap: '4px' }}>
          <button
            className={tool === 'select' ? 'bc-btn primary' : 'bc-btn'}
            onClick={() => setTool('select')}
            title="Select (S)"
          >
            ◀▶
          </button>
          <button
            className={tool === 'line' ? 'bc-btn primary' : 'bc-btn'}
            onClick={() => setTool('line')}
            title="Line (L)"
          >
            ╱
          </button>
          <button
            className={tool === 'arc' ? 'bc-btn primary' : 'bc-btn'}
            onClick={() => setTool('arc')}
            title="Arc (A)"
          >
            ⌒
          </button>
          <button
            className={tool === 'text' ? 'bc-btn primary' : 'bc-btn'}
            onClick={() => setTool('text')}
            title="Text (T)"
          >
            A
          </button>
          <button
            className={tool === 'dimension' ? 'bc-btn primary' : 'bc-btn'}
            onClick={() => setTool('dimension')}
            title="Dimension (D)"
          >
            ⊢
          </button>
          <button
            className={tool === 'delete' ? 'bc-btn primary' : 'bc-btn'}
            onClick={() => setTool('delete')}
            title="Delete (Del)"
          >
            ✕
          </button>
          <div style={{ width: '1px', height: '24px', backgroundColor: '#ccc', margin: '0 4px' }} />
          <button className="bc-btn" onClick={undo} disabled={undoStack.length <= 1} title="Undo (Ctrl+Z)">
            ↶
          </button>
          <button className="bc-btn" onClick={redo} disabled={redoStack.length === 0} title="Redo (Ctrl+Y)">
            ↷
          </button>
        </div>
      </div>

      {/* Status bar */}
      {feedback && (
        <div style={{ padding: '4px 12px', fontSize: '12px', color: '#666', backgroundColor: '#f5f5f5' }}>
          {feedback}
        </div>
      )}

      {/* Canvas */}
      <div style={{ flex: 1, overflow: 'hidden', backgroundColor: bgColor }}>
        <canvas
          ref={canvasRef}
          width={800}
          height={600}
          style={{
            display: 'block',
            cursor: tool === 'delete' ? 'not-allowed' : tool === 'select' ? 'default' : 'crosshair',
            backgroundColor: bgColor,
          }}
          onMouseDown={handleCanvasMouseDown}
          onMouseMove={handleCanvasMouseMove}
          onMouseUp={handleCanvasMouseUp}
          onMouseLeave={() => setIsDrawing(false)}
        />
      </div>

      {/* Footer with stats */}
      <div style={{ padding: '8px 12px', borderTop: '1px solid #ccc', fontSize: '12px', color: '#666' }}>
        Elements: {elements.length} | Dimensions: {dimensions.length} | Selected: {selected.size} | Undo: {undoStack.length - 1} | Redo: {redoStack.length}
      </div>

      {/* Save/Cancel buttons */}
      <div className="bc-footer" style={{ padding: '8px 12px', borderTop: '1px solid #ccc', gap: '8px' }}>
        <button className="bc-btn" onClick={onCancel}>
          {t('common.cancel')}
        </button>
        <button
          className="bc-btn primary"
          onClick={() => {
            pushUndoState();
            saveToLocalStorage();
            onSave?.(elements, dimensions);
            setFeedback('Sketch saved');
          }}
        >
          {t('common.save')}
        </button>
      </div>

      {/* Text input modal */}
      {showTextInput && textPosition && (
        <div
          style={{
            position: 'fixed',
            left: textPosition.x,
            top: textPosition.y,
            backgroundColor: bgColor,
            border: '1px solid #ccc',
            padding: '4px',
            zIndex: 1000,
          }}
        >
          <input
            autoFocus
            type="text"
            value={textInput}
            onChange={(e) => setTextInput(e.target.value)}
            onKeyPress={(e) => {
              if (e.key === 'Enter') handleTextInput();
            }}
            onBlur={handleTextInput}
            style={{ width: '150px', padding: '4px' }}
          />
        </div>
      )}
    </div>
  );
};

export default SketchEditor;
