// Lists of the drawing editor (docs/ui/DRAWING_UI.md). Their captions are messages of
// src/ui/app/messages/drawing.ts: drawing.viewKind.*, drawing.side.*, drawing.dimType.*,
// drawing.char.*, drawing.sectionAt.*.

import type { GeometricCharacteristic, StandardView } from '../../wasm/bendcore_drawing_api.mjs';

/** Standard views (GOST 2.305) in the order of the base view list. */
export const STANDARD_VIEWS: StandardView[] = ['front', 'top', 'left', 'right', 'bottom', 'rear'];

/** GOST 2.308 characteristics in the order of the standard; true: a datum is required. */
export const CHARACTERISTICS: [GeometricCharacteristic, boolean][] = [
  ['straightness', false],
  ['flatness', false],
  ['roundness', false],
  ['cylindricity', false],
  ['lineProfile', false],
  ['surfaceProfile', false],
  ['parallelism', true],
  ['perpendicularity', true],
  ['angularity', true],
  ['position', false],
  ['coaxiality', true],
  ['symmetry', true],
  ['circularRunout', true],
  ['totalRunout', true],
];

/** Places of a section view next to its parent (ViewSpec.side). */
export const SHEET_SIDES = ['leftOf', 'rightOf', 'above', 'below'] as const;

/** GOST 2.302 scales offered in the lists (the kernel checks the series). */
export const SCALES: [number, number][] = [
  [10, 1], [5, 1], [4, 1], [5, 2], [2, 1], [1, 1], [1, 2], [2, 5], [1, 4], [1, 5], [1, 10], [1, 20],
];
