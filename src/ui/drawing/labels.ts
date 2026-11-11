// Russian labels of the drawing editor (docs/ui/DRAWING_UI.md).

import type { DimensionType, GeometricCharacteristic, StandardView, ViewKind } from '../../wasm/bendcore_drawing_api.mjs';

export const VIEW_KIND_LABELS: Record<ViewKind, string> = {
  base: 'Главный вид',
  projected: 'Проекционный вид',
  axonometric: 'Аксонометрия',
  section: 'Разрез',
  detail: 'Выносной элемент',
};

export const STANDARD_VIEW_LABELS: Record<StandardView, string> = {
  front: 'спереди',
  top: 'сверху',
  left: 'слева',
  right: 'справа',
  bottom: 'снизу',
  rear: 'сзади',
};

export const DIMENSION_TYPE_LABELS: Record<DimensionType, string> = {
  horizontal: 'горизонтальный',
  vertical: 'вертикальный',
  aligned: 'параллельный',
  radius: 'радиус',
  diameter: 'диаметр',
  angle: 'угол',
};

/** GOST 2.308 names; the second element: a datum is required (location, orientation, run-out). */
export const CHARACTERISTICS: [GeometricCharacteristic, string, boolean][] = [
  ['straightness', 'Прямолинейность', false],
  ['flatness', 'Плоскостность', false],
  ['roundness', 'Круглость', false],
  ['cylindricity', 'Цилиндричность', false],
  ['lineProfile', 'Форма заданного профиля', false],
  ['surfaceProfile', 'Форма заданной поверхности', false],
  ['parallelism', 'Параллельность', true],
  ['perpendicularity', 'Перпендикулярность', true],
  ['angularity', 'Наклон', true],
  ['position', 'Позиционный', false],
  ['coaxiality', 'Соосность', true],
  ['symmetry', 'Симметричность', true],
  ['circularRunout', 'Биение', true],
  ['totalRunout', 'Полное биение', true],
];

export const SHEET_SIDES: [string, string][] = [
  ['leftOf', 'слева от вида'],
  ['rightOf', 'справа от вида'],
  ['above', 'над видом'],
  ['below', 'под видом'],
];

/** GOST 2.302 scales offered in the lists (the kernel checks the series). */
export const SCALES: [number, number][] = [
  [10, 1], [5, 1], [4, 1], [5, 2], [2, 1], [1, 1], [1, 2], [2, 5], [1, 4], [1, 5], [1, 10], [1, 20],
];

