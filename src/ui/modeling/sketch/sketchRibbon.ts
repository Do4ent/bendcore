// Ribbon of the sketch editor (docs/ui/SKETCH_EDITOR.md, "Лента"): shown in place of the part
// ribbon while a sketch is edited. Quick access: finish, cancel, undo a step; the tab "Эскиз":
// drawing tools, editing tools, dimensions and regions, insertion (projection of model edges,
// DXF), drawing modes and the view. Tested by src/ui/modeling/__tests__/partRibbon.test.ts.

import type { ReactNode } from 'react';
import type { Translate } from '../../app/i18n';
import type { RibbonCommand, RibbonTab } from '../../app/ribbonModel';
import { DRAW_TOOLS } from './drawTools';

export const EDIT_TOOLS = ['trim', 'extend', 'fillet', 'chamfer'] as const;
export const DIMENSION_TOOLS = ['dimension', 'regions'] as const;
export type SketchRibbonTool = 'select' | (typeof DRAW_TOOLS)[number] | (typeof EDIT_TOOLS)[number] | (typeof DIMENSION_TOOLS)[number] | 'project';

/** Icon of each tool of the sketch editor. */
export const SKETCH_TOOL_ICONS: Record<SketchRibbonTool, string> = {
  select: 'select',
  line: 'sketch-line',
  rectangle: 'sketch-rectangle',
  'rectangle-center': 'sketch-rectangle-center',
  circle: 'sketch-circle',
  'circle-3': 'sketch-circle-3',
  arc: 'sketch-arc',
  'arc-3': 'sketch-arc-3',
  'arc-tangent': 'sketch-arc-tangent',
  ellipse: 'sketch-ellipse',
  spline: 'sketch-spline',
  point: 'sketch-point',
  trim: 'sketch-trim',
  extend: 'sketch-extend',
  fillet: 'sketch-fillet',
  chamfer: 'sketch-chamfer',
  dimension: 'sketch-dimension',
  regions: 'sketch-regions',
  project: 'sketch-project',
};

/** Frequent tools: large buttons. */
const LARGE_TOOLS = new Set<SketchRibbonTool>(['select', 'line', 'rectangle', 'circle', 'arc', 'trim', 'dimension']);

export interface SketchRibbonState {
  tool: SketchRibbonTool;
  canUndo: boolean;
  dxfOpen: boolean;
}

export interface SketchRibbonActions {
  setTool(tool: SketchRibbonTool): void;
  exit(): void;
  cancel(): void;
  undo(): void;
  fit(): void;
  openDxf(): void;
  /** Check boxes of the drawing modes (construction, grid snap, reference dimensions). */
  modes(): ReactNode;
}

export function sketchRibbon(t: Translate, s: SketchRibbonState, a: SketchRibbonActions): { quick: RibbonCommand[]; tabs: RibbonTab[] } {
  const tool = (k: SketchRibbonTool): RibbonCommand => ({
    id: `tool:${k}`,
    testId: `tool-${k}`,
    icon: SKETCH_TOOL_ICONS[k],
    label: t(`sketch.tool.${k}`),
    tip: t(`ribbon.tip.sketchTool.${k}`),
    size: LARGE_TOOLS.has(k) ? 'large' : 'small',
    active: s.tool === k,
    className: s.tool === k ? 'sk-active' : undefined,
    onClick: () => a.setTool(k),
  });
  const quick: RibbonCommand[] = [
    { id: 'exit', testId: 'sketch-exit', icon: 'sketch-finish', label: t('sketch.exit'), tip: t('ribbon.tip.sketchExit'), className: 'pm-primary', onClick: a.exit },
    { id: 'cancel', testId: 'sketch-cancel', icon: 'close', label: t('sketch.cancel'), tip: t('ribbon.tip.sketchCancel'), hotkey: 'Esc', onClick: a.cancel },
    { id: 'undo', testId: 'sketch-undo', icon: 'undo', label: t('sketch.undoStep'), tip: t('ribbon.tip.sketchUndo'), hotkey: 'Ctrl+Z', disabled: !s.canUndo, onClick: a.undo },
  ];
  const tabs: RibbonTab[] = [
    {
      id: 'sketchEdit',
      label: t('ribbon.tab.sketchEdit'),
      groups: [
        { id: 'draw', label: t('ribbon.group.draw'), items: [tool('select'), ...DRAW_TOOLS.map(tool)] },
        { id: 'edit', label: t('ribbon.group.edit'), items: EDIT_TOOLS.map(tool) },
        { id: 'dimensions', label: t('ribbon.group.dimensions'), items: DIMENSION_TOOLS.map(tool) },
        {
          id: 'insert',
          label: t('ribbon.group.insert'),
          items: [
            tool('project'),
            { id: 'dxf', testId: 'sketch-dxf-open', icon: 'sketch-dxf', label: t('sketch.dxf.open'), tip: t('ribbon.tip.dxfImport'), disabled: s.dxfOpen, onClick: a.openDxf },
          ],
        },
        { id: 'modes', label: t('ribbon.group.modes'), items: [{ kind: 'custom', id: 'modes', render: a.modes }] },
        {
          id: 'view',
          label: t('ribbon.group.view'),
          items: [{ id: 'fit', testId: 'sketch-fit', icon: 'fit', label: t('sketch.fit'), tip: t('ribbon.tip.fit'), size: 'large', onClick: a.fit }],
        },
      ],
    },
  ];
  return { quick, tabs };
}

/** Icon of a sketch constraint type (the proposals of the selection panel). */
export function constraintIcon(type: string): string {
  if (type.startsWith('equal')) return 'constraint-equal';
  if (type.startsWith('symmetric')) return 'constraint-symmetric';
  if (type.startsWith('point-on')) return 'constraint-point-on';
  const own: Record<string, string> = {
    coincident: 'constraint-coincident',
    horizontal: 'constraint-horizontal',
    vertical: 'constraint-vertical',
    parallel: 'constraint-parallel',
    perpendicular: 'constraint-perpendicular',
    tangent: 'constraint-tangent',
    midpoint: 'constraint-midpoint',
    concentric: 'constraint-concentric',
    fix: 'constraint-fix',
  };
  return own[type] ?? 'constraint';
}
