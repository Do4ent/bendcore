/**
 * Клиент сервиса тела трубы (bendcore_server, src/server/main.cpp).
 *   POST /v1/tube?format=csv|json&od&wall&deflection  — тело по пути (CSV/JSON) с сеткой и портами
 *   POST /v1/analyze?format=step&deflection          — распознавание трубы в файле STEP с сеткой тела
 * Сетка строится ядром на Open CASCADE; интерфейс только отображает её.
 */

/** Треугольная сетка: vertices — x, y, z подряд (мм), indices — тройки индексов вершин. */
export interface MeshData {
  vertices: number[];
  indices: number[];
}

/** Объект сцены Viewport3D. */
export interface ViewportMesh {
  id: string;
  name: string;
  mesh: MeshData;
  color?: number;
}

export interface TubePort {
  name: string;
  pos: [number, number, number];
  direction: [number, number, number];
  od: number;
}

export interface TubeBody {
  od: number;
  wall: number;
  volume: number;
  valid: boolean;
  axisLength: number;
  ports: TubePort[];
  mesh: MeshData;
}

export interface RecognizedBend {
  angle: number;
  radius: number;
}

export interface RecognizedTube {
  od: number;
  wall: number;
  innerDiameter: number;
  length: number;
  bends: RecognizedBend[];
  shapeType: string;
  confidence: number;
  notes: string;
  valid: boolean;
  mesh: MeshData;
}

export class TubeServiceError extends Error {
  constructor(message: string, readonly status?: number) {
    super(message);
    this.name = 'TubeServiceError';
  }
}

const STORAGE_KEY = 'bendcore.serviceUrl';
const DEFAULT_BASE = 'http://127.0.0.1:8080';

/** Адрес сервиса: сохранённый пользователем или локальный по умолчанию. */
export function getServiceBase(): string {
  try {
    const saved = window.localStorage.getItem(STORAGE_KEY);
    if (saved && saved.trim()) return saved.trim().replace(/\/+$/, '');
  } catch {
    // localStorage недоступен (приватный режим, песочница) — используем адрес по умолчанию.
  }
  return DEFAULT_BASE;
}

export function setServiceBase(base: string): void {
  try {
    window.localStorage.setItem(STORAGE_KEY, base.trim());
  } catch {
    // не сохраняется — адрес действует только до перезагрузки
  }
}

function isMesh(value: unknown): value is MeshData {
  const m = value as MeshData | null;
  return (
    !!m &&
    Array.isArray(m.vertices) &&
    Array.isArray(m.indices) &&
    m.vertices.length % 3 === 0 &&
    m.indices.length % 3 === 0
  );
}

async function postJson(url: string, body: BodyInit, timeoutMs: number): Promise<Record<string, unknown>> {
  let res: Response;
  try {
    res = await fetch(url, { method: 'POST', body, signal: AbortSignal.timeout(timeoutMs) });
  } catch (err) {
    throw new TubeServiceError(`сервис недоступен (${url}): ${err instanceof Error ? err.message : String(err)}`);
  }
  let data: Record<string, unknown>;
  try {
    data = (await res.json()) as Record<string, unknown>;
  } catch {
    throw new TubeServiceError(`сервис вернул не JSON (HTTP ${res.status})`, res.status);
  }
  if (data.ok !== true) {
    throw new TubeServiceError(String(data.error ?? `ошибка HTTP ${res.status}`), res.status);
  }
  if (!isMesh(data.mesh)) throw new TubeServiceError('в ответе сервиса нет корректной сетки', res.status);
  return data;
}

/** Тело трубы по файлу пути (CSV или JSON) — та же операция, что в web/view.html. */
export async function buildTubeBody(
  text: string,
  format: 'csv' | 'json',
  od: number,
  wall = 0,
  deflection = 0.05,
  base = getServiceBase(),
): Promise<TubeBody> {
  const q = new URLSearchParams({ format, od: String(od), wall: String(wall), deflection: String(deflection) });
  const d = await postJson(`${base}/v1/tube?${q}`, text, 60000);
  return {
    od: Number(d.od),
    wall: Number(d.wall),
    volume: Number(d.volume),
    valid: d.valid === true,
    axisLength: Number(d.axis_length),
    ports: (d.ports as TubePort[]) ?? [],
    mesh: d.mesh as MeshData,
  };
}

/** Распознавание трубы в файле STEP: ∅, стенка, длина, гибы и сетка тела. */
export async function analyzeStep(file: Blob, deflection = 0.1, base = getServiceBase()): Promise<RecognizedTube> {
  const q = new URLSearchParams({ format: 'step', deflection: String(deflection) });
  const d = await postJson(`${base}/v1/analyze?${q}`, file, 120000);
  const t = d.tube as Record<string, unknown>;
  return {
    od: Number(t.od),
    wall: Number(t.wall),
    innerDiameter: Number(t.inner_diameter),
    length: Number(t.length),
    bends: (t.bends as RecognizedBend[]) ?? [],
    shapeType: String(t.shape_type),
    confidence: Number(t.confidence),
    notes: String(t.notes ?? ''),
    valid: t.valid === true,
    mesh: d.mesh as MeshData,
  };
}
