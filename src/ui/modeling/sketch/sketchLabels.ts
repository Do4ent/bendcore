// Russian captions of the sketch editor: constraint types, definition states
// (docs/ui/SKETCH_EDITOR.md).

import type { SketchConstraintDef } from '../../../wasm/bendcore_modeling_api.mjs';
import { dimensionLabel } from './dimensionLayout';
import { CONSTRAINT_OPERANDS, hasValue } from './sketchModel';

export const CONSTRAINT_LABELS: Record<string, string> = {
  coincident: 'Совпадение',
  horizontal: 'Горизонтальность',
  vertical: 'Вертикальность',
  parallel: 'Параллельность',
  perpendicular: 'Перпендикулярность',
  tangent: 'Касание',
  equal: 'Равенство',
  'equal-length': 'Равенство длин',
  'equal-radius': 'Равенство радиусов',
  symmetric: 'Симметрия (ось)',
  'symmetric-point': 'Симметрия (точка)',
  midpoint: 'Середина',
  'point-on': 'Точка на кривой',
  'point-on-line': 'Точка на прямой',
  'point-on-circle': 'Точка на окружности',
  concentric: 'Концентричность',
  fix: 'Закрепление',
  rotation: 'Поворот',
  distance: 'Расстояние',
  'distance-x': 'Расстояние по X',
  'distance-y': 'Расстояние по Y',
  'point-curve-distance': 'Расстояние до кривой',
  radius: 'Радиус',
  diameter: 'Диаметр',
  angle: 'Угол',
  'arc-length': 'Длина дуги',
  'major-radius': 'Большая полуось',
  'minor-radius': 'Малая полуось',
};

export const DEFINITION_LABELS: Record<string, string> = {
  'fully-defined': 'Полностью определён',
  'under-defined': 'Недоопределён',
  'over-defined': 'Переопределён',
};

/** "Расстояние p0, p1 = 60" style description of a constraint. */
export function describeConstraint(c: SketchConstraintDef | undefined, value?: number): string {
  if (!c) return '';
  const ops = CONSTRAINT_OPERANDS[c.type] ?? [];
  const names = [c.a, c.b, c.c]
    .map((v, i) => (ops[i] === undefined || v < 0 ? null : ops[i] === 'point' ? `т${v + 1}` : `к${v}`))
    .filter((s): s is string => s !== null);
  const head = `${CONSTRAINT_LABELS[c.type] ?? c.type} ${names.join(', ')}`;
  if (!hasValue(c.type)) return head;
  const shown = value !== undefined && Number.isFinite(value) ? dimensionLabel(c, value) : c.value;
  return c.driven || c.value === String(value) ? `${head}: ${shown}` : `${head}: ${c.value} = ${shown}`;
}
