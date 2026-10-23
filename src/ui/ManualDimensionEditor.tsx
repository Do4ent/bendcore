/**
 * Редактор ручных размеров (Subtask 2.3)
 * Позволяет добавлять, редактировать и перемещать ручные размеры на развёртке.
 * Размеры сохраняются в черновик (localStorage) и не пересчитываются при изменении пути.
 */

import React, { useState, useRef, useEffect } from 'react';
import { DrawingData, ManualDimensionTable } from './core/types';
import './ManualDimensionEditor.css';

export interface ManualDimension {
  id: string;
  position: [number, number];
  text: string;
  reference: boolean;
  author: string;
  created_at: string;
}

export interface ManualDimensionEditorProps {
  /** Текущий лист чертежа */
  sheet: DrawingData;
  /** Таблица ручных размеров (может быть пустой) */
  dimensions?: ManualDimension[];
  /** Колбэк при добавлении размера */
  onAdd?: (dimension: ManualDimension) => void;
  /** Колбэк при обновлении размера */
  onUpdate?: (dimension: ManualDimension) => void;
  /** Колбэк при удалении размера */
  onRemove?: (id: string) => void;
  /** Включить drag-drop для перемещения размеров */
  enableDragDrop?: boolean;
  /** Сохранять размеры в localStorage (для черновика) */
  enableDraftSaving?: boolean;
}

const LOCAL_STORAGE_KEY = 'bendcore_manual_dimensions_draft';

export const ManualDimensionEditor: React.FC<ManualDimensionEditorProps> = ({
  sheet,
  dimensions = [],
  onAdd,
  onUpdate,
  onRemove,
  enableDragDrop = true,
  enableDraftSaving = true,
}) => {
  // Форма для добавления нового размера
  const [formX, setFormX] = useState<number>(100);
  const [formY, setFormY] = useState<number>(100);
  const [formText, setFormText] = useState<string>('');
  const [formReference, setFormReference] = useState<boolean>(false);
  const [formAuthor, setFormAuthor] = useState<string>('anonymous');
  const [validationError, setValidationError] = useState<string | null>(null);

  // Список размеров (локальный или переданный)
  const [localDimensions, setLocalDimensions] = useState<ManualDimension[]>(dimensions);
  const svgRef = useRef<SVGSVGElement>(null);
  const [draggingId, setDraggingId] = useState<string | null>(null);
  const [draggingOffset, setDraggingOffset] = useState<[number, number]>([0, 0]);

  // Загрузить размеры из черновика при монтировании
  useEffect(() => {
    if (enableDraftSaving) {
      const saved = localStorage.getItem(LOCAL_STORAGE_KEY);
      if (saved) {
        try {
          const parsed = JSON.parse(saved);
          setLocalDimensions(parsed);
        } catch (e) {
          console.warn('Failed to load draft dimensions:', e);
        }
      }
    }
  }, [enableDraftSaving]);

  // Сохранять размеры в черновик при изменении
  useEffect(() => {
    if (enableDraftSaving && localDimensions.length > 0) {
      localStorage.setItem(LOCAL_STORAGE_KEY, JSON.stringify(localDimensions));
    }
  }, [localDimensions, enableDraftSaving]);

  // Валидировать позицию размера
  const isPositionValid = (x: number, y: number): boolean => {
    const drawable = sheet.title_block;  // Примерная граница рабочей области
    const margin = 5;

    return (
      x >= drawable.x0 + margin &&
      x <= drawable.x1 - margin &&
      y >= drawable.y0 + margin &&
      y <= drawable.y1 - margin
    );
  };

  // Проверить расстояние между размерами
  const hasConflict = (x: number, y: number, excludeId?: string): boolean => {
    const minDistance = 15;  // в пиксельном масштабе SVG

    for (const dim of localDimensions) {
      if (excludeId && dim.id === excludeId) continue;

      const dx = dim.position[0] - x;
      const dy = dim.position[1] - y;
      const distance = Math.sqrt(dx * dx + dy * dy);

      if (distance < minDistance) {
        return true;
      }
    }

    return false;
  };

  // Добавить новый размер
  const handleAddDimension = (e: React.FormEvent) => {
    e.preventDefault();
    setValidationError(null);

    if (!formText.trim()) {
      setValidationError('Текст размера не может быть пустым');
      return;
    }

    if (!isPositionValid(formX, formY)) {
      setValidationError('Позиция находится вне рабочей области листа');
      return;
    }

    if (hasConflict(formX, formY)) {
      setValidationError('Размер слишком близко к другим размерам (минимум 15 мм)');
      return;
    }

    const newDimension: ManualDimension = {
      id: `manual_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`,
      position: [formX, formY],
      text: formText,
      reference: formReference,
      author: formAuthor,
      created_at: new Date().toISOString(),
    };

    setLocalDimensions([...localDimensions, newDimension]);
    onAdd?.(newDimension);

    // Очистить форму
    setFormText('');
    setFormX(100);
    setFormY(100);
    setFormReference(false);
  };

  // Обновить размер
  const handleUpdateDimension = (id: string, updates: Partial<ManualDimension>) => {
    const updated = localDimensions.map((dim) =>
      dim.id === id ? { ...dim, ...updates } : dim
    );
    setLocalDimensions(updated);
    const updatedDim = updated.find((d) => d.id === id);
    if (updatedDim) {
      onUpdate?.(updatedDim);
    }
  };

  // Удалить размер
  const handleRemoveDimension = (id: string) => {
    setLocalDimensions(localDimensions.filter((dim) => dim.id !== id));
    onRemove?.(id);
  };

  // Drag-drop: начало перетаскивания
  const handleMouseDown = (e: React.MouseEvent<SVGGElement>, id: string) => {
    if (!enableDragDrop) return;
    e.preventDefault();

    const svg = svgRef.current;
    if (!svg) return;

    const rect = svg.getBoundingClientRect();
    const dimension = localDimensions.find((d) => d.id === id);
    if (!dimension) return;

    const offsetX = e.clientX - rect.left - dimension.position[0];
    const offsetY = e.clientY - rect.top - dimension.position[1];

    setDraggingId(id);
    setDraggingOffset([offsetX, offsetY]);
  };

  // Drag-drop: перемещение
  const handleMouseMove = (e: React.MouseEvent<SVGSVGElement>) => {
    if (!draggingId || !enableDragDrop) return;

    const svg = svgRef.current;
    if (!svg) return;

    const rect = svg.getBoundingClientRect();
    const newX = e.clientX - rect.left - draggingOffset[0];
    const newY = e.clientY - rect.top - draggingOffset[1];

    if (isPositionValid(newX, newY) && !hasConflict(newX, newY, draggingId)) {
      handleUpdateDimension(draggingId, { position: [newX, newY] });
    }
  };

  // Drag-drop: конец перетаскивания
  const handleMouseUp = () => {
    setDraggingId(null);
  };

  // Масштаб SVG (предполагаем примерный масштаб листа)
  const svgScale = 0.5;
  const svgWidth = (sheet.format?.w ?? 420) * svgScale;
  const svgHeight = (sheet.format?.h ?? 297) * svgScale;

  return (
    <div className="manual-dimension-editor">
      <div className="editor-header">
        <h3>Ручные размеры</h3>
      </div>

      {/* Форма добавления размера */}
      <form className="add-dimension-form" onSubmit={handleAddDimension}>
        <div className="form-group">
          <label>X (мм):</label>
          <input
            type="number"
            value={formX}
            onChange={(e) => setFormX(parseFloat(e.target.value) || 0)}
            step="0.1"
            required
          />
        </div>

        <div className="form-group">
          <label>Y (мм):</label>
          <input
            type="number"
            value={formY}
            onChange={(e) => setFormY(parseFloat(e.target.value) || 0)}
            step="0.1"
            required
          />
        </div>

        <div className="form-group">
          <label>Текст:</label>
          <input
            type="text"
            value={formText}
            onChange={(e) => setFormText(e.target.value)}
            placeholder="Значение размера"
            required
          />
        </div>

        <div className="form-group">
          <label>
            <input
              type="checkbox"
              checked={formReference}
              onChange={(e) => setFormReference(e.target.checked)}
            />
            Справочный размер (со *)
          </label>
        </div>

        <div className="form-group">
          <label>Автор:</label>
          <input
            type="text"
            value={formAuthor}
            onChange={(e) => setFormAuthor(e.target.value)}
            placeholder="Имя автора"
          />
        </div>

        {validationError && <div className="validation-error">{validationError}</div>}

        <button type="submit" className="btn-add">
          Добавить размер
        </button>
      </form>

      {/* Preview на SVG */}
      <div className="svg-preview">
        <h4>Preview листа с размерами</h4>
        <svg
          ref={svgRef}
          width={svgWidth}
          height={svgHeight}
          viewBox={`0 0 ${svgWidth} ${svgHeight}`}
          className="drawing-svg"
          onMouseMove={handleMouseMove}
          onMouseUp={handleMouseUp}
          onMouseLeave={handleMouseUp}
        >
          {/* Граница листа */}
          <rect
            x={sheet.border?.x0 ?? 5}
            y={sheet.border?.y0 ?? 5}
            width={(sheet.border?.x1 ?? 415) - (sheet.border?.x0 ?? 5)}
            height={(sheet.border?.y1 ?? 292) - (sheet.border?.y0 ?? 5)}
            fill="none"
            stroke="black"
            strokeWidth="0.5"
          />

          {/* Отображаемые размеры */}
          {localDimensions.map((dim) => (
            <g
              key={dim.id}
              onMouseDown={(e) => handleMouseDown(e, dim.id)}
              className={`dimension-group ${draggingId === dim.id ? 'dragging' : ''}`}
              style={{ cursor: enableDragDrop ? 'grab' : 'default' }}
            >
              {/* Точка размера */}
              <circle cx={dim.position[0]} cy={dim.position[1]} r="3" fill="red" />

              {/* Текст размера */}
              <text
                x={dim.position[0] + 5}
                y={dim.position[1] - 5}
                fontSize="8"
                fill="red"
                className="dimension-text"
              >
                {dim.text}
                {dim.reference ? '*' : ''}
              </text>

              {/* Выделение при наведении */}
              <circle
                cx={dim.position[0]}
                cy={dim.position[1]}
                r="5"
                fill="none"
                stroke="orange"
                strokeWidth="0.5"
                opacity="0.5"
                className="dimension-highlight"
              />
            </g>
          ))}
        </svg>
      </div>

      {/* Список существующих размеров */}
      <div className="dimensions-list">
        <h4>Размеры ({localDimensions.length})</h4>
        {localDimensions.length === 0 ? (
          <p className="empty-message">Размеров нет. Добавьте первый размер выше.</p>
        ) : (
          <ul>
            {localDimensions.map((dim) => (
              <li key={dim.id} className="dimension-item">
                <div className="dimension-info">
                  <span className="dimension-text-value">
                    {dim.text}
                    {dim.reference ? '*' : ''}
                  </span>
                  <span className="dimension-position">
                    ({dim.position[0].toFixed(1)}, {dim.position[1].toFixed(1)})
                  </span>
                  <span className="dimension-author">{dim.author}</span>
                </div>

                <div className="dimension-actions">
                  <button
                    className="btn-edit"
                    onClick={() => {
                      const newText = prompt('Новый текст:', dim.text);
                      if (newText !== null) {
                        handleUpdateDimension(dim.id, { text: newText });
                      }
                    }}
                  >
                    Редактировать
                  </button>

                  <button
                    className="btn-remove"
                    onClick={() => {
                      if (window.confirm(`Удалить размер "${dim.text}"?`)) {
                        handleRemoveDimension(dim.id);
                      }
                    }}
                  >
                    Удалить
                  </button>
                </div>
              </li>
            ))}
          </ul>
        )}
      </div>

      {/* Информация о черновике */}
      {enableDraftSaving && localDimensions.length > 0 && (
        <div className="draft-info">
          💾 {localDimensions.length} размер{localDimensions.length === 1 ? '' : 'ов'} сохранен{localDimensions.length === 1 ? '' : 'о'} в черновик
        </div>
      )}
    </div>
  );
};

export default ManualDimensionEditor;
