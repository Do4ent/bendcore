/**
 * Модель панели «Проверка» (4.2): фильтр по классам, сортировка, значки по единому языку
 * состояний (6.2), состояние проверки с учётом устаревания по отпечаткам.
 */
import { Severity, type CheckResult, type Violation } from './types';

export type CheckState = 'not_checked' | 'running' | 'passed' | 'violations' | 'stale';

/** Значок и цвет класса (6.2): ошибки — красный круг, предупреждение — жёлтый треугольник,
 * пометка — красная (пересечение) или оранжевая (зазор) метка, информация — синий значок. */
export function iconFor(v: Pick<Violation, 'severity' | 'rule'>): { glyph: string; color: string } {
  switch (v.severity) {
    case Severity.Geometric:
    case Severity.Technological:
      return { glyph: '●', color: 'var(--color-delete, #D93A3A)' };
    case Severity.Warning:
      return { glyph: '▲', color: 'var(--color-update, #F5B517)' };
    case Severity.Mark:
      return v.rule === 'tube_intersection'
        ? { glyph: '◆', color: 'var(--color-delete, #D93A3A)' }
        : { glyph: '◆', color: 'var(--color-edit, #E8820C)' };
    default:
      return { glyph: 'i', color: 'var(--color-tube, #1F6FC5)' };
  }
}

export function filterViolations(list: Violation[], classes: Set<Severity>, text = ''): Violation[] {
  const q = text.trim().toLowerCase();
  return list.filter(
    (v) =>
      classes.has(v.severity) &&
      (!q || v.tube.toLowerCase().includes(q) || v.message.toLowerCase().includes(q) || v.other.toLowerCase().includes(q)),
  );
}

/** «Факт / норма» для таблицы. */
export function factNorm(v: Violation, fmt: (x: number) => string = (x) => x.toFixed(1)): string {
  if (v.value === null) return '';
  return v.limit === null ? fmt(v.value) : `${fmt(v.value)} / ${fmt(v.limit)}`;
}

/** Место: труба, элемент, вторая труба для коллизий. */
export function place(v: Violation, elementWord = 'элемент'): string {
  let s = v.tube;
  if (v.element >= 0) s += `, ${elementWord} ${v.element + 1}`;
  if (v.other) s += ` ↔ ${v.other}`;
  return s;
}

/**
 * Состояние проверки: результат устарел, если у какой-то трубы отпечаток изменился,
 * труба удалена или добавлена после запуска.
 */
export function checkState(
  result: CheckResult | null,
  running: boolean,
  currentFingerprints: Record<string, string> | null,
): CheckState {
  if (running) return 'running';
  if (!result) return 'not_checked';
  if (currentFingerprints) {
    const ids = Object.keys(result.tubes);
    const now = Object.keys(currentFingerprints);
    if (ids.length !== now.length) return 'stale';
    for (const id of ids) {
      if (currentFingerprints[id] !== result.tubes[id].fingerprint) return 'stale';
    }
  }
  return result.violations.length ? 'violations' : 'passed';
}

export function countBy(list: Violation[]): Record<Severity, number> {
  const c = { 0: 0, 1: 0, 2: 0, 3: 0, 4: 0 } as Record<Severity, number>;
  for (const v of list) c[v.severity]++;
  return c;
}
