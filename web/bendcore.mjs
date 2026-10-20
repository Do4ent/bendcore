// JS-обёртка над WebAssembly-модулем bendcore (собирается CMake-пресетом `wasm`).
// Использование:
//   import { loadBendcore, Sketch2D } from './bendcore.mjs';
//   const core = await loadBendcore();
//   const s = new Sketch2D(core);
//   ...
//   s.solve();  // { status, dof, conflicting: [], redundant: [] }
//   s.dispose();

import createBendcore from './bendcore.js';

let modulePromise = null;

// Загрузка модуля один раз; повторные вызовы возвращают тот же объект.
export function loadBendcore(options = {}) {
  if (!modulePromise) {
    modulePromise = createBendcore(options);
  }
  return modulePromise;
}

// Импорт трубы из текста CSV или JSON (формат — README, примеры в examples/). Возвращает модель:
// { id, source, od, wall, path: [участки] | null, ports: [{ name, pos: {x,y,z}, direction: {x,y,z}, od }] }.
// При ошибке бросает Error с текстом причины (номер строки для CSV, описание для JSON).
export function importTube(core, format, text, { id = 'труба', od, wall = 0 } = {}) {
  const r = core.importTube(format, text, id, od, wall);
  if (!r.ok) throw new Error(r.error);
  return r;
}

// Длина оси пути (мм). path — массив участков, как в importTube(...).path.
export function axisLength(core, path) {
  const r = core.axisLength(path);
  if (!r.ok) throw new Error(r.error);
  return r.value;
}

// Ломаная оси пути: массив {x, y, z} (мм). steps — отсчётов на гиб, по умолчанию 64.
export function tracePath(core, path, steps = 64) {
  const r = core.tracePath(path, steps);
  if (!r.ok) throw new Error(r.error);
  return r.value;
}

// Оболочка над эскизом (7.9). Номера точек, отрезков, окружностей и ограничений — целые числа ≥ 1 (ограничения).
export class Sketch2D {
  constructor(core) {
    this.core = core;
    this.raw = new core.Sketch();
  }

  #ensure() {
    if (!this.raw) throw new Error('Sketch2D уже освобождён (dispose)');
    return this.raw;
  }

  addPoint(x, y) { return this.#ensure().addPoint(x, y); }
  fixPoint(p) { this.#ensure().fixPoint(p); }
  addLine(a, b) { return this.#ensure().addLine(a, b); }
  addCircle(center, radius) { return this.#ensure().addCircle(center, radius); }

  constrainCoincident(a, b) { return this.#ensure().constrainCoincident(a, b); }
  constrainHorizontal(a, b) { return this.#ensure().constrainHorizontal(a, b); }
  constrainVertical(a, b) { return this.#ensure().constrainVertical(a, b); }
  constrainDistance(a, b, d) { return this.#ensure().constrainDistance(a, b, d); }
  constrainRadius(circle, r) { return this.#ensure().constrainRadius(circle, r); }
  constrainParallel(l1, l2) { return this.#ensure().constrainParallel(l1, l2); }
  constrainPerpendicular(l1, l2) { return this.#ensure().constrainPerpendicular(l1, l2); }
  constrainEqualLength(l1, l2) { return this.#ensure().constrainEqualLength(l1, l2); }
  constrainAngle(l1, l2, deg) { return this.#ensure().constrainAngle(l1, l2, deg); }
  constrainPointOnLine(p, line) { return this.#ensure().constrainPointOnLine(p, line); }
  constrainPointOnCircle(p, circle) { return this.#ensure().constrainPointOnCircle(p, circle); }
  constrainEqualRadius(c1, c2) { return this.#ensure().constrainEqualRadius(c1, c2); }
  constrainSymmetric(p1, p2, axis) { return this.#ensure().constrainSymmetric(p1, p2, axis); }
  removeConstraint(c) { this.#ensure().removeConstraint(c); }

  solve() { return this.#ensure().solve(); }
  point(p) { return this.#ensure().point(p); }
  circleRadius(c) { return this.#ensure().circleRadius(c); }

  dispose() {
    if (this.raw) {
      this.raw.delete();
      this.raw = null;
    }
  }
}
