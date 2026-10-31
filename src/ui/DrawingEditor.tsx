// Компонент редактора чертежа (Q4 Task 1 Subtask 2)
// Позволяет редактировать параметры трубы в реальном времени с 3D просмотром
import React, { useState, useCallback, useEffect } from 'react';
import styles from './DrawingEditor.module.css';

export interface EditableParameter {
  id: string;
  name: string;
  current_value: number;
  min_value: number;
  max_value: number;
  unit: string;
  is_formula: boolean;
  formula_text: string;
  description?: string;
}

export interface DrawingState {
  drawing_id: string;
  parameters: EditableParameter[];
  last_modified_at: string;
  modified_by: string;
}

interface DrawingEditorProps {
  drawingId: string;
  onSave: (state: DrawingState) => Promise<void>;
  onCancel?: () => void;
  readOnly?: boolean;
}

export const DrawingEditor: React.FC<DrawingEditorProps> = ({
  drawingId,
  onSave,
  onCancel,
  readOnly = false,
}) => {
  const [drawingState, setDrawingState] = useState<DrawingState>({
    drawing_id: drawingId,
    parameters: [],
    last_modified_at: '',
    modified_by: '',
  });

  const [hasChanges, setHasChanges] = useState(false);
  const [isSaving, setIsSaving] = useState(false);
  const [activeTab, setActiveTab] = useState<'parameters' | 'formula' | 'history'>('parameters');
  const [editingParamId, setEditingParamId] = useState<string | null>(null);
  const [history, setHistory] = useState<Array<{ action: string; timestamp: string }>>([]);
  const [undoStack, setUndoStack] = useState<DrawingState[]>([]);
  const [redoStack, setRedoStack] = useState<DrawingState[]>([]);

  // Обновить параметр
  const updateParameter = useCallback((paramId: string, newValue: number) => {
    setDrawingState(prev => {
      const updated = { ...prev };
      const param = updated.parameters.find(p => p.id === paramId);
      if (param) {
        // Сохранить в undo stack перед изменением
        setUndoStack(s => [...s, JSON.parse(JSON.stringify(prev))]);
        setRedoStack([]);

        param.current_value = newValue;
        // Проверить диапазон
        if (newValue < param.min_value) {
          param.current_value = param.min_value;
        } else if (newValue > param.max_value) {
          param.current_value = param.max_value;
        }

        const now = new Date().toISOString();
        setHistory(h => [...h, {
          action: `Отредактирован параметр "${param.name}"`,
          timestamp: now,
        }]);

        setHasChanges(true);
      }
      return updated;
    });
  }, []);

  // Отменить последнее действие
  const handleUndo = useCallback(() => {
    if (undoStack.length > 0) {
      const lastState = undoStack[undoStack.length - 1];
      setRedoStack(s => [...s, drawingState]);
      setDrawingState(lastState);
      setUndoStack(s => s.slice(0, -1));
      setHistory(h => [...h, {
        action: 'Отмена действия',
        timestamp: new Date().toISOString(),
      }]);
    }
  }, [undoStack, drawingState]);

  // Повторить последнее отменённое действие
  const handleRedo = useCallback(() => {
    if (redoStack.length > 0) {
      const nextState = redoStack[redoStack.length - 1];
      setUndoStack(s => [...s, drawingState]);
      setDrawingState(nextState);
      setRedoStack(s => s.slice(0, -1));
      setHistory(h => [...h, {
        action: 'Повтор действия',
        timestamp: new Date().toISOString(),
      }]);
    }
  }, [redoStack, drawingState]);

  // Сохранить изменения
  const handleSave = useCallback(async () => {
    setIsSaving(true);
    try {
      const now = new Date().toISOString();
      const stateToSave = {
        ...drawingState,
        last_modified_at: now,
        modified_by: 'current_user',
      };
      await onSave(stateToSave);
      setHasChanges(false);
      setHistory(h => [...h, {
        action: 'Чертёж сохранён',
        timestamp: now,
      }]);
    } finally {
      setIsSaving(false);
    }
  }, [drawingState, onSave]);

  // Отменить все изменения
  const handleCancel = useCallback(() => {
    setHasChanges(false);
    setUndoStack([]);
    setRedoStack([]);
    if (onCancel) {
      onCancel();
    }
  }, [onCancel]);

  return (
    <div className={styles.drawingEditor}>
      {/* Заголовок */}
      <div className={styles.header}>
        <h2>Редактор чертежа: {drawingId}</h2>
        <div className={styles.toolbar}>
          <button
            onClick={handleUndo}
            disabled={undoStack.length === 0 || readOnly}
            className={styles.toolbarBtn}
            title="Отменить (Ctrl+Z)"
          >
            ↶ Отменить
          </button>
          <button
            onClick={handleRedo}
            disabled={redoStack.length === 0 || readOnly}
            className={styles.toolbarBtn}
            title="Повторить (Ctrl+Y)"
          >
            ↷ Повторить
          </button>
          <div className={styles.spacer} />
          <button
            onClick={handleSave}
            disabled={!hasChanges || isSaving || readOnly}
            className={styles.saveBtn}
          >
            {isSaving ? '⏳ Сохраняется...' : '💾 Сохранить'}
          </button>
          <button
            onClick={handleCancel}
            disabled={readOnly}
            className={styles.cancelBtn}
          >
            ✕ Отменить
          </button>
        </div>
      </div>

      {/* Вкладки */}
      <div className={styles.tabs} role="tablist" aria-label="Разделы редактора">
        <button
          role="tab"
          aria-selected={activeTab === 'parameters'}
          className={`${styles.tab} ${activeTab === 'parameters' ? styles.active : ''}`}
          onClick={() => setActiveTab('parameters')}
        >
          Параметры
        </button>
        <button
          role="tab"
          aria-selected={activeTab === 'formula'}
          className={`${styles.tab} ${activeTab === 'formula' ? styles.active : ''}`}
          onClick={() => setActiveTab('formula')}
        >
          Формулы
        </button>
        <button
          role="tab"
          aria-selected={activeTab === 'history'}
          className={`${styles.tab} ${activeTab === 'history' ? styles.active : ''}`}
          onClick={() => setActiveTab('history')}
        >
          История ({history.length})
        </button>
      </div>

      {/* Содержимое */}
      <div className={styles.content}>
        {activeTab === 'parameters' && (
          <div className={styles.parametersPanel}>
            <h3>Параметры чертежа</h3>
            {drawingState.parameters.length === 0 ? (
              <p className={styles.empty}>Нет параметров для отображения</p>
            ) : (
              <table className={styles.parametersTable}>
                <thead>
                  <tr>
                    <th>Имя</th>
                    <th>Значение</th>
                    <th>Диапазон</th>
                    <th>Единица</th>
                    <th>Действие</th>
                  </tr>
                </thead>
                <tbody>
                  {drawingState.parameters.map(param => (
                    <tr key={param.id} className={param.is_formula ? styles.formulaRow : ''}>
                      <td>{param.name}</td>
                      <td>
                        {editingParamId === param.id ? (
                          <input
                            type="number"
                            value={param.current_value}
                            min={param.min_value}
                            max={param.max_value}
                            onChange={e => updateParameter(param.id, parseFloat(e.target.value))}
                            onBlur={() => setEditingParamId(null)}
                            autoFocus
                            disabled={readOnly}
                          />
                        ) : (
                          <span
                            onClick={() => !readOnly && setEditingParamId(param.id)}
                            className={styles.editableValue}
                          >
                            {param.current_value.toFixed(2)}
                          </span>
                        )}
                      </td>
                      <td>{param.min_value} - {param.max_value}</td>
                      <td>{param.unit}</td>
                      <td>
                        {param.is_formula && <span className={styles.badge}>Формула</span>}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
            {hasChanges && (
              <div className={styles.warning}>
                ⚠ Есть несохранённые изменения
              </div>
            )}
          </div>
        )}

        {activeTab === 'formula' && (
          <div className={styles.formulaPanel}>
            <h3>Редактор формул</h3>
            <p className={styles.info}>
              Поддерживаемые переменные: $OD (внешний диаметр), $Wall (толщина стенки)
            </p>
            {drawingState.parameters
              .filter(p => p.is_formula)
              .map(param => (
                <div key={param.id} className={styles.formulaItem}>
                  <label>{param.name}</label>
                  <input
                    type="text"
                    value={param.formula_text}
                    onChange={e => {
                      setDrawingState(prev => ({
                        ...prev,
                        parameters: prev.parameters.map(p =>
                          p.id === param.id
                            ? { ...p, formula_text: e.target.value }
                            : p
                        ),
                      }));
                      setHasChanges(true);
                    }}
                    disabled={readOnly}
                  />
                </div>
              ))}
            {drawingState.parameters.filter(p => p.is_formula).length === 0 && (
              <p className={styles.empty}>Формулы отсутствуют</p>
            )}
          </div>
        )}

        {activeTab === 'history' && (
          <div className={styles.historyPanel}>
            <h3>История изменений</h3>
            {history.length === 0 ? (
              <p className={styles.empty}>История пуста</p>
            ) : (
              <ul className={styles.historyList}>
                {[...history].reverse().map((item, idx) => (
                  <li key={idx}>
                    <span className={styles.timestamp}>{new Date(item.timestamp).toLocaleTimeString()}</span>
                    <span className={styles.action}>{item.action}</span>
                  </li>
                ))}
              </ul>
            )}
          </div>
        )}
      </div>

      {/* Информация о последней модификации */}
      <div className={styles.footer}>
        <small>
          Последняя модификация: {drawingState.last_modified_at ? new Date(drawingState.last_modified_at).toLocaleString() : 'Не сохранено'}
          {' | '} Редактор: {drawingState.modified_by}
        </small>
      </div>
    </div>
  );
};

export default DrawingEditor;
