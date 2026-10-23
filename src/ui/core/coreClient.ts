/**
 * Клиент команд ядра для интерфейса. Под Electron — через IPC главного процесса
 * (bendcore_cli), в браузере — через переданный транспорт (например, WebAssembly-модуль
 * с функцией dispatch). Ошибка ядра приходит как CoreClientError с текстом для пользователя.
 */
import type {
  AddSketchRequest,
  AddSketchResult,
  AssociativeDrawing,
  BendDataResult,
  CheckResult,
  CheckTubeInput,
  DrawingChanges,
  DrawingResult,
  DrawingVersion,
  Feed,
  Frame,
  InstancesState,
  LraRow,
  PathItem,
  PDFExportResult,
  ReleaseCheckResult,
  SketchBboxResult,
  Sketch2D,
  XyzRow,
} from './types';

export class CoreClientError extends Error {
  constructor(message: string, public readonly command: string) {
    super(message);
    this.name = 'CoreClientError';
  }
}

/** Транспорт: команда + аргументы → data или исключение. */
export type CoreTransport = (command: string, args: Record<string, unknown>) => Promise<unknown>;

declare global {
  interface Window {
    electron?: {
      ipc: {
        invoke: (channel: string, data: unknown) => Promise<{ status: 'ok' | 'error'; data?: unknown; message?: string }>;
      };
    };
  }
}

/** Транспорт через IPC Electron (канал bendcore:command). */
export const electronTransport: CoreTransport = async (command, args) => {
  if (typeof window === 'undefined' || !window.electron?.ipc) {
    throw new CoreClientError('ядро недоступно: приложение запущено не в Electron', command);
  }
  const res = await window.electron.ipc.invoke('bendcore:command', { command, args });
  if (res.status !== 'ok') throw new CoreClientError(res.message ?? 'ошибка ядра', command);
  return res.data;
};

/** Транспорт через функцию dispatch WebAssembly-модуля (строка JSON → строка JSON). */
export function wasmTransport(dispatch: (line: string) => string): CoreTransport {
  let id = 0;
  return async (command, args) => {
    const out = JSON.parse(dispatch(JSON.stringify({ id: ++id, command, args }))) as {
      ok: boolean;
      data?: unknown;
      error?: string;
    };
    if (!out.ok) throw new CoreClientError(out.error ?? 'ошибка ядра', command);
    return out.data;
  };
}

export interface BendDataArgs {
  path: PathItem[];
  feed?: Feed;
  E_A?: number;
  E_B?: number;
  frame?: Frame;
  precision?: number;
  delimiter?: string;
}

export class CoreClient {
  constructor(private readonly transport: CoreTransport = electronTransport) {}

  private call<T>(command: string, args: object = {}): Promise<T> {
    return this.transport(command, args as Record<string, unknown>) as Promise<T>;
  }

  lra(a: BendDataArgs) {
    return this.call<BendDataResult<LraRow>>('bend.lra', a);
  }
  xyz(a: BendDataArgs) {
    return this.call<BendDataResult<XyzRow>>('bend.xyz', a);
  }
  checkRun(tubes: CheckTubeInput[], options: { clearance?: number; tube_to_tube?: boolean } = {}) {
    return this.call<CheckResult>('check.run', { tubes, ...options });
  }

  instancesState(doc = 'default') {
    return this.call<InstancesState>('instances.state', { doc });
  }
  setPrimary(instance: string, doc = 'default') {
    return this.call<InstancesState>('instances.set_primary', { doc, instance });
  }
  detach(instance: string, newPart: string, doc = 'default') {
    return this.call<InstancesState>('instances.detach', { doc, instance, new_part: newPart });
  }
  setAdaptive(instance: string, adaptive: boolean, doc = 'default') {
    return this.call<InstancesState>('instances.set_adaptive', { doc, instance, adaptive });
  }
  unifyDesignation(instances: string[], designation: string, doc = 'default') {
    return this.call<InstancesState>('instances.unify', { doc, instances, designation });
  }

  drawingPart(args: Record<string, unknown>) {
    return this.call<DrawingResult>('drawing.part', args);
  }
  releaseCheck(args: Record<string, unknown>) {
    return this.call<ReleaseCheckResult>('drawing.release_check', args);
  }

  // Ассоциативные виды (версионирование)
  makeAssociativeDrawing(args: Record<string, unknown>) {
    return this.call<AssociativeDrawing>('drawing.make_associative', args);
  }
  updateAssociativeDrawing(args: Record<string, unknown>) {
    return this.call<{ ok: boolean; changes: DrawingChanges }>(
      'drawing.update',
      args
    );
  }
  getDrawingVersions(tubeId: string) {
    return this.call<DrawingVersion[]>('drawing.get_versions', { tube_id: tubeId });
  }
  compareVersions(tubeId: string, rev1: number, rev2: number) {
    return this.call<{ changed_sheets: string[]; changed_views: string[]; changed_tables: string[] }>(
      'drawing.compare_versions',
      { tube_id: tubeId, rev1, rev2 }
    );
  }
  restoreVersion(tubeId: string, revision: number) {
    return this.call<DrawingResult>('drawing.restore', { tube_id: tubeId, revision });
  }

  // Пакетный вывод (сборочный чертёж)
  assemblyDrawing(args: Record<string, unknown>) {
    return this.call<DrawingResult>('drawing.assembly', args);
  }

  // PDF экспорт
  exportPDF(args: Record<string, unknown>) {
    return this.call<PDFExportResult>('drawing.export_pdf', args);
  }

  // 2D Sketches (v7.9)
  addSketch(args: AddSketchRequest) {
    return this.call<AddSketchResult>('drawing.add_sketch', args);
  }
  sketchBbox(sketch: Sketch2D) {
    return this.call<SketchBboxResult>('drawing.sketch_bbox', {
      sheet_number: sketch.sheet_number,
      name: sketch.name,
      author: sketch.created_by,
      elements: sketch.elements,
      dimensions: sketch.dimensions,
    });
  }
}
