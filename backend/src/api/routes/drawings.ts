// API маршруты для управления чертежами (Q4 Task 1 Subtask 6)
// GET/PUT/POST операции для параметров, слоёв, аннотаций и Undo/Redo

import { Router, Request, Response } from 'express';

const router = Router();

// Типы для чертежей
interface EditableParameter {
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

interface DrawingState {
  drawing_id: string;
  parameters: EditableParameter[];
  last_modified_at: string;
  modified_by: string;
}

interface Layer {
  id: string;
  name: string;
  visible: boolean;
  editable: boolean;
  locked: boolean;
  element_ids: string[];
}

interface Annotation {
  id: string;
  type: 'dimension' | 'note' | 'label' | 'arrow';
  text: string;
  x: number;
  y: number;
  layer_id: string;
}

// In-memory storage for demo (в реальной системе используется БД)
const drawingsStore = new Map<string, DrawingState>();
const layersStore = new Map<string, Layer[]>();
const annotationsStore = new Map<string, Annotation[]>();
const historyStore = new Map<string, Array<{ action: string; timestamp: string }>>();

// ===== Drawing Endpoints =====

/**
 * GET /api/drawings/:id
 * Получить чертёж с параметрами и слоями
 */
router.get('/:id', (req: Request, res: Response) => {
  const drawingId = req.params.id;

  const state = drawingsStore.get(drawingId);
  if (!state) {
    return res.status(404).json({
      error: 'Drawing not found',
      drawing_id: drawingId,
    });
  }

  const layers = layersStore.get(drawingId) || [];
  const annotations = annotationsStore.get(drawingId) || [];

  res.json({
    drawing: state,
    layers,
    annotations,
  });
});

/**
 * POST /api/drawings/:id/load
 * Загрузить чертёж (инициализировать)
 */
router.post('/:id/load', (req: Request, res: Response) => {
  const drawingId = req.params.id;

  const state: DrawingState = {
    drawing_id: drawingId,
    parameters: [
      {
        id: 'length_1',
        name: 'Длина 1',
        current_value: 100,
        min_value: 10,
        max_value: 500,
        unit: 'mm',
        is_formula: false,
        formula_text: '',
      },
      {
        id: 'angle_1',
        name: 'Угол 1',
        current_value: 90,
        min_value: 0,
        max_value: 180,
        unit: 'deg',
        is_formula: false,
        formula_text: '',
      },
    ],
    last_modified_at: new Date().toISOString(),
    modified_by: 'system',
  };

  drawingsStore.set(drawingId, state);
  historyStore.set(drawingId, []);

  // Инициализировать слои
  const defaultLayers: Layer[] = [
    { id: 'layer_contours', name: 'Контуры', visible: true, editable: true, locked: false, element_ids: [] },
    { id: 'layer_dimensions', name: 'Размеры', visible: true, editable: true, locked: false, element_ids: [] },
    { id: 'layer_annotations', name: 'Аннотации', visible: true, editable: true, locked: false, element_ids: [] },
  ];
  layersStore.set(drawingId, defaultLayers);
  annotationsStore.set(drawingId, []);

  res.json({
    status: 'loaded',
    drawing: state,
    layers: defaultLayers,
  });
});

// ===== Parameter Endpoints =====

/**
 * PUT /api/drawings/:id/parameters/:param_id
 * Обновить единственный параметр
 */
router.put('/:id/parameters/:param_id', (req: Request, res: Response) => {
  const { id, param_id } = req.params;
  const { current_value, formula_text, is_formula } = req.body;

  const state = drawingsStore.get(id);
  if (!state) {
    return res.status(404).json({ error: 'Drawing not found' });
  }

  const param = state.parameters.find(p => p.id === param_id);
  if (!param) {
    return res.status(404).json({ error: 'Parameter not found' });
  }

  // Валидация диапазона
  if (current_value !== undefined) {
    if (current_value < param.min_value || current_value > param.max_value) {
      return res.status(400).json({
        error: 'Value out of range',
        min: param.min_value,
        max: param.max_value,
      });
    }
    param.current_value = current_value;
  }

  if (formula_text !== undefined && is_formula) {
    param.formula_text = formula_text;
    param.is_formula = true;
  }

  state.last_modified_at = new Date().toISOString();
  state.modified_by = req.get('X-User-ID') || 'unknown';

  // Добавить в историю
  const history = historyStore.get(id) || [];
  history.push({
    action: `Обновлён параметр "${param.name}"`,
    timestamp: state.last_modified_at,
  });
  historyStore.set(id, history);

  res.json({
    parameter: param,
    drawing_state: state,
  });
});

/**
 * GET /api/drawings/:id/parameters
 * Получить все параметры
 */
router.get('/:id/parameters', (req: Request, res: Response) => {
  const state = drawingsStore.get(req.params.id);
  if (!state) {
    return res.status(404).json({ error: 'Drawing not found' });
  }

  res.json({
    parameters: state.parameters,
  });
});

// ===== Layer Endpoints =====

/**
 * POST /api/drawings/:id/layers
 * Создать новый слой
 */
router.post('/:id/layers', (req: Request, res: Response) => {
  const { id } = req.params;
  const { name } = req.body;

  if (!name) {
    return res.status(400).json({ error: 'Layer name required' });
  }

  const layers = layersStore.get(id) || [];
  const newLayer: Layer = {
    id: `layer_${Date.now()}`,
    name,
    visible: true,
    editable: true,
    locked: false,
    element_ids: [],
  };

  layers.push(newLayer);
  layersStore.set(id, layers);

  const history = historyStore.get(id) || [];
  history.push({
    action: `Создан слой "${name}"`,
    timestamp: new Date().toISOString(),
  });
  historyStore.set(id, history);

  res.json({
    layer: newLayer,
    layers,
  });
});

/**
 * GET /api/drawings/:id/layers
 * Получить все слои
 */
router.get('/:id/layers', (req: Request, res: Response) => {
  const layers = layersStore.get(req.params.id) || [];
  res.json({ layers });
});

/**
 * PUT /api/drawings/:id/layers/:layer_id
 * Обновить слой
 */
router.put('/:id/layers/:layer_id', (req: Request, res: Response) => {
  const { id, layer_id } = req.params;
  const { visible, locked, name } = req.body;

  const layers = layersStore.get(id);
  if (!layers) {
    return res.status(404).json({ error: 'Layers not found' });
  }

  const layer = layers.find(l => l.id === layer_id);
  if (!layer) {
    return res.status(404).json({ error: 'Layer not found' });
  }

  if (visible !== undefined) layer.visible = visible;
  if (locked !== undefined) layer.locked = locked;
  if (name !== undefined) layer.name = name;

  res.json({ layer, layers });
});

/**
 * DELETE /api/drawings/:id/layers/:layer_id
 * Удалить слой
 */
router.delete('/:id/layers/:layer_id', (req: Request, res: Response) => {
  const { id, layer_id } = req.params;

  const layers = layersStore.get(id);
  if (!layers) {
    return res.status(404).json({ error: 'Layers not found' });
  }

  const index = layers.findIndex(l => l.id === layer_id);
  if (index === -1) {
    return res.status(404).json({ error: 'Layer not found' });
  }

  layers.splice(index, 1);
  layersStore.set(id, layers);

  res.json({
    message: 'Layer deleted',
    layers,
  });
});

// ===== Annotation Endpoints =====

/**
 * POST /api/drawings/:id/annotations
 * Добавить аннотацию
 */
router.post('/:id/annotations', (req: Request, res: Response) => {
  const { id } = req.params;
  const { type, text, x, y, layer_id } = req.body;

  if (!type || !text) {
    return res.status(400).json({ error: 'Type and text required' });
  }

  const annotations = annotationsStore.get(id) || [];
  const newAnnotation: Annotation = {
    id: `annot_${Date.now()}`,
    type,
    text,
    x: x || 0,
    y: y || 0,
    layer_id: layer_id || 'layer_annotations',
  };

  annotations.push(newAnnotation);
  annotationsStore.set(id, annotations);

  res.json({
    annotation: newAnnotation,
    annotations,
  });
});

/**
 * GET /api/drawings/:id/annotations
 * Получить все аннотации
 */
router.get('/:id/annotations', (req: Request, res: Response) => {
  const annotations = annotationsStore.get(req.params.id) || [];
  res.json({ annotations });
});

/**
 * DELETE /api/drawings/:id/annotations/:annot_id
 * Удалить аннотацию
 */
router.delete('/:id/annotations/:annot_id', (req: Request, res: Response) => {
  const { id, annot_id } = req.params;

  const annotations = annotationsStore.get(id);
  if (!annotations) {
    return res.status(404).json({ error: 'Annotations not found' });
  }

  const index = annotations.findIndex(a => a.id === annot_id);
  if (index === -1) {
    return res.status(404).json({ error: 'Annotation not found' });
  }

  annotations.splice(index, 1);
  annotationsStore.set(id, annotations);

  res.json({
    message: 'Annotation deleted',
    annotations,
  });
});

// ===== Undo/Redo Endpoints =====

/**
 * POST /api/drawings/:id/undo
 * Отменить последнее действие
 */
router.post('/:id/undo', (req: Request, res: Response) => {
  const history = historyStore.get(req.params.id) || [];

  if (history.length === 0) {
    return res.status(400).json({ error: 'Nothing to undo' });
  }

  const lastAction = history.pop();
  historyStore.set(req.params.id, history);

  res.json({
    message: 'Action undone',
    last_action: lastAction,
    history_length: history.length,
  });
});

/**
 * GET /api/drawings/:id/history
 * Получить историю действий
 */
router.get('/:id/history', (req: Request, res: Response) => {
  const history = historyStore.get(req.params.id) || [];
  res.json({
    history,
    count: history.length,
  });
});

/**
 * DELETE /api/drawings/:id/history
 * Очистить историю
 */
router.delete('/:id/history', (req: Request, res: Response) => {
  historyStore.set(req.params.id, []);
  res.json({
    message: 'History cleared',
  });
});

export default router;
