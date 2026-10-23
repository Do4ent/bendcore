/**
 * Блокировка оси при построении пути (1.2): клавиши X/Y/Z. Новая точка берётся на прямой,
 * проходящей через последнюю вершину вдоль оси, — ближайшая к лучу курсора.
 */
export type Axis = 'x' | 'y' | 'z';
export interface P3 {
  x: number;
  y: number;
  z: number;
}

const AXES: Record<Axis, P3> = { x: { x: 1, y: 0, z: 0 }, y: { x: 0, y: 1, z: 0 }, z: { x: 0, y: 0, z: 1 } };

const sub = (a: P3, b: P3): P3 => ({ x: a.x - b.x, y: a.y - b.y, z: a.z - b.z });
const dot = (a: P3, b: P3) => a.x * b.x + a.y * b.y + a.z * b.z;

/** Точка на прямой base + t·dir, ближайшая к лучу origin + s·rayDir (s ≥ 0). */
export function closestOnLineToRay(base: P3, dir: P3, origin: P3, rayDir: P3): P3 {
  const w = sub(base, origin);
  const a = dot(dir, dir);
  const b = dot(dir, rayDir);
  const c = dot(rayDir, rayDir);
  const d = dot(dir, w);
  const e = dot(rayDir, w);
  const den = a * c - b * b;
  let t: number;
  if (Math.abs(den) < 1e-12) {
    t = 0;  // луч параллелен оси: точка остаётся в последней вершине
  } else {
    t = (b * e - c * d) / den;
    const s = (a * e - b * d) / den;
    if (s < 0) t = -d / a;  // ближайшая к началу луча
  }
  return { x: base.x + dir.x * t, y: base.y + dir.y * t, z: base.z + dir.z * t };
}

export function lockedPoint(last: P3 | null, axis: Axis | null, rayOrigin: P3, rayDir: P3, free: P3): P3 {
  if (!last || !axis) return free;
  return closestOnLineToRay(last, AXES[axis], rayOrigin, rayDir);
}

/** Клавиша → новое состояние блокировки: та же клавиша снимает блокировку. */
export function toggleAxis(current: Axis | null, key: string): Axis | null {
  const k = key.toLowerCase();
  if (k !== 'x' && k !== 'y' && k !== 'z') return current;
  return current === k ? null : k;
}
