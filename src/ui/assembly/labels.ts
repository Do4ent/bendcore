// Russian labels of the assembly page (docs/ui/ASSEMBLY_UI.md).

import type { MateStatus, MateType } from '../../wasm/bendcore_assembly_api.mjs';

export const MATE_LABELS: Record<MateType, string> = {
  coincident: 'Совпадение',
  concentric: 'Соосность',
  distance: 'Расстояние',
  angle: 'Угол',
  parallel: 'Параллельность',
  perpendicular: 'Перпендикулярность',
  tangent: 'Касание',
  rigid: 'Жёсткая связь',
};

export const MATE_STATUS_LABELS: Record<MateStatus, string> = {
  solved: 'решено',
  redundant: 'избыточно',
  conflicting: 'противоречит',
  unsolved: 'не решено',
  broken: 'ссылка потеряна',
  suppressed: 'подавлено',
};

export const GEOMETRY_LABELS: Record<string, string> = {
  Plane: 'плоскость',
  Cylinder: 'цилиндр',
  Cone: 'конус',
  Sphere: 'сфера',
  Torus: 'тор',
  BSpline: 'сплайн',
  Line: 'прямая',
  Circle: 'окружность',
  Ellipse: 'эллипс',
  Point: 'точка',
  Other: 'другое',
};

export const FASTENER_KIND_LABELS: Record<string, string> = {
  'hex-bolt': 'Болт',
  'socket-head-screw': 'Винт',
  'hex-nut': 'Гайка',
  'plain-washer': 'Шайба',
  'spring-washer': 'Шайба пружинная',
  'parallel-pin': 'Штифт',
};

/** "1 степень свободы", "2 степени свободы", "5 степеней свободы". */
export function dofText(n: number): string {
  const mod10 = n % 10;
  const mod100 = n % 100;
  let word = 'степеней';
  if (mod10 === 1 && mod100 !== 11) word = 'степень';
  else if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) word = 'степени';
  return `${n} ${word} свободы`;
}

/** Mass in kg with three significant decimals, or a dash when unknown. */
export function massText(kg: number | null | undefined): string {
  if (kg === null || kg === undefined) return '—';
  if (kg >= 1) return kg.toFixed(3);
  return kg.toPrecision(3);
}
