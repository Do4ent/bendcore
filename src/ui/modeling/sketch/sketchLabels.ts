// Captions of the sketch editor (docs/ui/SKETCH_EDITOR.md): constraint types, definition states
// and constraint descriptions, from the messages sketch.constraint.*, sketch.definition.* of
// src/ui/app/messages/sketch.ts.

import type { SketchConstraintDef } from '../../../wasm/bendcore_modeling_api.mjs';
import { caption, translate } from '../../app/i18n';
import type { Translate } from '../../app/i18n';
import { dimensionLabel } from './dimensionLayout';
import { CONSTRAINT_OPERANDS, hasValue } from './sketchModel';

const RU: Translate = (key, params) => translate('ru', key, params);

export const constraintCaption = (t: Translate, type: string): string => caption(t, 'sketch.constraint.', type);

export const definitionCaption = (t: Translate, state: string): string => caption(t, 'sketch.definition.', state);

/** "Расстояние т1, т2: 60" style description of a constraint. */
export function describeConstraint(c: SketchConstraintDef | undefined, value?: number, t: Translate = RU): string {
  if (!c) return '';
  const ops = CONSTRAINT_OPERANDS[c.type] ?? [];
  const names = [c.a, c.b, c.c]
    .map((v, i) => (ops[i] === undefined || v < 0 ? null : ops[i] === 'point' ? t('sketch.pointName', { n: v + 1 }) : t('sketch.curveName', { n: v })))
    .filter((s): s is string => s !== null);
  const head = `${constraintCaption(t, c.type)} ${names.join(', ')}`;
  if (!hasValue(c.type)) return head;
  const shown = value !== undefined && Number.isFinite(value) ? dimensionLabel(c, value) : c.value;
  return c.driven || c.value === String(value) ? `${head}: ${shown}` : `${head}: ${c.value} = ${shown}`;
}
