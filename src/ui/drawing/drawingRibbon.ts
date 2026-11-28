// Ribbon of the drawing editor (docs/ui/DRAWING_UI.md, "Лента"): quick-access commands of the
// document and the tabs "Виды", "Разрезы", "Размеры", "Обозначения", "Спецификация", "Вывод".
// The tools (data-testid tool-<name>) switch the sheet tool; the other commands call the drawing
// module. Tested by src/ui/drawing/__tests__/drawingRibbon.test.ts.

import type { ChangeEvent, ReactNode, Ref } from 'react';
import type { MessageKey, Translate } from '../app/i18n';
import type { RibbonCommand, RibbonItem, RibbonTab } from '../app/ribbonModel';

export type DrawingTool =
  | 'select'
  | 'dimension'
  | 'chain'
  | 'baseline'
  | 'roughness'
  | 'tolerance'
  | 'datum'
  | 'balloon'
  | 'thread'
  | 'section'
  | 'localSection'
  | 'detail';

export const TOOL_ICONS: Record<DrawingTool, string> = {
  select: 'select',
  dimension: 'dimension-linear',
  chain: 'dimension-chain',
  baseline: 'dimension-baseline',
  roughness: 'roughness',
  tolerance: 'form-tolerance',
  datum: 'datum',
  balloon: 'balloon',
  thread: 'dimension-thread',
  section: 'view-section',
  localSection: 'view-local-section',
  detail: 'view-detail',
};

export type ProjectedSide = 'top' | 'left' | 'right' | 'bottom' | 'rear';

export interface DrawingRibbonState {
  /** The drawing module is loaded. */
  kernel: boolean;
  /** A drawing is open. */
  drawing: boolean;
  tool: DrawingTool;
  canUndo: boolean;
  canRedo: boolean;
  undoLabel: string;
  redoLabel: string;
  hasBase: boolean;
  assembly: boolean;
  /** A valid arrow angle is typed for the auxiliary view. */
  auxiliaryReady: boolean;
  /** The selected view has dimensions to arrange. */
  canArrangeDimensions: boolean;
  /** The sheet of an assembly drawing has balloons. */
  canArrangeBalloons: boolean;
  modelChanged: boolean;
  picture: boolean;
  tab: 'sheet' | 'specification';
}

export interface DrawingRibbonActions {
  openModel(e: ChangeEvent<HTMLInputElement>): void;
  openDrawing(e: ChangeEvent<HTMLInputElement>): void;
  openDrawingRef?: Ref<HTMLInputElement>;
  replaceModel(e: ChangeEvent<HTMLInputElement>): void;
  save(): void;
  undo(): void;
  redo(): void;
  setTool(tool: DrawingTool): void;
  /** Direction of the front view (rendered by the editor). */
  baseStandard(): ReactNode;
  addBase(): void;
  addProjected(side: ProjectedSide): void;
  addAxonometric(kind: 'isometric' | 'dimetric'): void;
  /** Arrow angle and "rotated" of the auxiliary view (rendered by the editor). */
  auxiliaryFields(): ReactNode;
  addAuxiliary(): void;
  update(): void;
  fit(): void;
  arrangeDimensions(): void;
  arrangeBalloons(): void;
  showTab(tab: 'sheet' | 'specification'): void;
  exportFile(format: 'pdf' | 'dxf' | 'svg'): void;
  exportSheetPdf(): void;
  print(): void;
}

export const PROJECTED_SIDES: ProjectedSide[] = ['top', 'left', 'right', 'bottom', 'rear'];

export function drawingRibbon(t: Translate, s: DrawingRibbonState, a: DrawingRibbonActions): { quick: RibbonCommand[]; tabs: RibbonTab[] } {
  const off = !s.drawing;
  const tool = (k: DrawingTool, size: 'large' | 'small' = 'large'): RibbonCommand => {
    const balloonOff = k === 'balloon' && !s.assembly;
    return {
      id: `tool:${k}`,
      testId: `tool-${k}`,
      icon: TOOL_ICONS[k],
      label: t(`drawing.tool.${k}`),
      tip: t(`ribbon.tip.tool.${k}`),
      size,
      active: s.tool === k,
      className: s.tool === k ? 'dr-active' : undefined,
      disabled: off || balloonOff,
      reason: balloonOff ? t('drawing.help.balloonPart') : undefined,
      onClick: () => a.setTool(k),
    };
  };
  const quick: RibbonCommand[] = [
    {
      id: 'openModel',
      kind: 'file',
      testId: 'open-model',
      icon: 'import',
      label: t('drawing.openModel'),
      tip: t('ribbon.tip.openModel'),
      accept: '.json,application/json',
      disabled: !s.kernel,
      onFiles: a.openModel,
    },
    {
      id: 'openDrawing',
      kind: 'file',
      testId: 'open-drawing',
      icon: 'open',
      label: t('drawing.file.open'),
      tip: t('drawing.file.openHint'),
      hotkey: 'Ctrl+O',
      accept: '.json,application/json',
      multiple: true,
      inputRef: a.openDrawingRef,
      disabled: !s.kernel,
      onFiles: a.openDrawing,
    },
    { id: 'save', testId: 'drawing-save', icon: 'save', label: t('drawing.save'), tip: t('ribbon.tip.save'), hotkey: 'Ctrl+S', disabled: off, onClick: a.save },
    {
      id: 'undo',
      testId: 'drawing-undo',
      icon: 'undo',
      label: t('drawing.undo'),
      tip: s.undoLabel ? t('drawing.undoHint', { label: s.undoLabel }) : t('ribbon.tip.undo'),
      hotkey: 'Ctrl+Z',
      disabled: off || !s.canUndo,
      onClick: a.undo,
    },
    {
      id: 'redo',
      testId: 'drawing-redo',
      icon: 'redo',
      label: t('drawing.redo'),
      tip: s.redoLabel ? t('drawing.redoHint', { label: s.redoLabel }) : t('ribbon.tip.redo'),
      hotkey: 'Ctrl+Y',
      disabled: off || !s.canRedo,
      onClick: a.redo,
    },
    tool('select', 'small'),
  ];
  const projected: RibbonItem[] = PROJECTED_SIDES.map((side) => ({
    id: `projected:${side}`,
    testId: `add-${side}`,
    icon: 'view-projected',
    label: t('drawing.projectedView', { side: t(`drawing.side.${side}` as MessageKey) }),
    tip: t('ribbon.tip.projected'),
    disabled: off || !s.hasBase,
    onClick: () => a.addProjected(side),
  }));
  const axonometric = (kind: 'isometric' | 'dimetric'): RibbonCommand => ({
    id: kind,
    testId: `add-${kind}`,
    icon: 'view-axonometric',
    label: t(`drawing.${kind}`),
    tip: t(`ribbon.tip.${kind}`),
    disabled: off || !s.hasBase,
    onClick: () => a.addAxonometric(kind),
  });
  const tabs: RibbonTab[] = [
    {
      id: 'views',
      label: t('ribbon.tab.views'),
      groups: [
        {
          id: 'baseView',
          label: t('ribbon.group.baseView'),
          items: [
            { kind: 'custom', id: 'baseStandard', render: a.baseStandard },
            { id: 'addBase', testId: 'add-base', icon: 'view-base', label: t('drawing.baseView'), tip: t('ribbon.tip.baseView'), size: 'large', disabled: off || s.hasBase, onClick: a.addBase },
          ],
        },
        { id: 'projected', label: t('ribbon.group.projected'), items: [...projected, axonometric('isometric'), axonometric('dimetric')] },
        {
          id: 'auxiliary',
          label: t('ribbon.group.auxiliary'),
          items: [
            { kind: 'custom', id: 'auxiliaryFields', render: a.auxiliaryFields },
            {
              id: 'addAuxiliary',
              testId: 'add-auxiliary',
              icon: 'view-auxiliary',
              label: t('drawing.auxiliaryView'),
              tip: t('ribbon.tip.auxiliary'),
              size: 'large',
              disabled: off || !s.hasBase || !s.auxiliaryReady,
              onClick: a.addAuxiliary,
            },
          ],
        },
        {
          id: 'model',
          label: t('ribbon.group.model'),
          items: [
            {
              id: 'update',
              testId: 'update-from-model',
              icon: 'update-from-model',
              label: t('drawing.updateButton'),
              tip: t('ribbon.tip.update'),
              size: 'large',
              active: s.modelChanged ? true : undefined,
              disabled: off,
              onClick: a.update,
            },
            {
              id: 'replaceModel',
              kind: 'file',
              testId: 'replace-model',
              icon: 'insert-part',
              label: t('drawing.file.replaceModel'),
              tip: t('drawing.file.replaceModelHint'),
              accept: '.json,application/json',
              disabled: off,
              onFiles: a.replaceModel,
            },
          ],
        },
        {
          id: 'view',
          label: t('ribbon.group.view'),
          items: [{ id: 'fit', testId: 'drawing-fit', icon: 'view-fit', label: t('drawing.fit'), tip: t('ribbon.tip.fit'), size: 'large', disabled: !s.picture, onClick: a.fit }],
        },
      ],
    },
    {
      id: 'sections',
      label: t('ribbon.tab.sections'),
      groups: [
        { id: 'sections', label: t('ribbon.group.sections'), items: [tool('section'), tool('localSection')] },
        { id: 'details', label: t('ribbon.group.details'), items: [tool('detail')] },
      ],
    },
    {
      id: 'dimensions',
      label: t('ribbon.tab.dimensions'),
      groups: [
        { id: 'linear', label: t('ribbon.group.linear'), items: [tool('dimension'), tool('chain'), tool('baseline'), tool('thread')] },
        {
          id: 'layout',
          label: t('ribbon.group.layout'),
          items: [
            {
              id: 'arrangeDimensions',
              testId: 'view-arrange-dimensions',
              icon: 'dimension-arrange',
              label: t('drawing.arrangeDimensions'),
              tip: t('drawing.arrangeDimensionsHint'),
              size: 'large',
              disabled: off || !s.canArrangeDimensions,
              onClick: a.arrangeDimensions,
            },
          ],
        },
      ],
    },
    {
      id: 'annotations',
      label: t('ribbon.tab.annotations'),
      groups: [
        { id: 'surface', label: t('ribbon.group.surface'), items: [tool('roughness'), tool('tolerance'), tool('datum')] },
        { id: 'positions', label: t('ribbon.group.positions'), items: [tool('balloon')] },
      ],
    },
    {
      id: 'specification',
      label: t('ribbon.tab.specification'),
      groups: [
        {
          id: 'document',
          label: t('ribbon.group.document'),
          items: [
            {
              id: 'showSpecification',
              testId: 'ribbon-show-specification',
              icon: 'specification',
              label: t('ribbon.showSpecification'),
              tip: t('ribbon.tip.showSpecification'),
              size: 'large',
              active: s.tab === 'specification',
              disabled: off,
              onClick: () => a.showTab('specification'),
            },
            {
              id: 'showSheet',
              testId: 'ribbon-show-sheet',
              icon: 'sheet',
              label: t('ribbon.showSheet'),
              tip: t('ribbon.tip.showSheet'),
              size: 'large',
              active: s.tab === 'sheet',
              disabled: off,
              onClick: () => a.showTab('sheet'),
            },
          ],
        },
        {
          id: 'positions',
          label: t('ribbon.group.positions'),
          items: [
            {
              id: 'arrangeBalloons',
              testId: 'arrange-balloons',
              icon: 'balloon-arrange',
              label: t('drawing.arrangeBalloons'),
              tip: t('drawing.arrangeBalloonsHint'),
              size: 'large',
              disabled: off || !s.canArrangeBalloons,
              onClick: a.arrangeBalloons,
            },
          ],
        },
      ],
    },
    {
      id: 'output',
      label: t('ribbon.tab.output'),
      groups: [
        {
          id: 'export',
          label: t('ribbon.group.export'),
          items: [
            { id: 'pdf', testId: 'export-pdf', icon: 'export-pdf', label: 'PDF', tip: t('ribbon.tip.pdf'), size: 'large', disabled: off, onClick: () => a.exportFile('pdf') },
            { id: 'dxf', testId: 'export-dxf', icon: 'export-dxf', label: 'DXF', tip: t('ribbon.tip.dxf'), size: 'large', disabled: off, onClick: () => a.exportFile('dxf') },
            { id: 'svg', testId: 'export-svg', icon: 'export-svg', label: 'SVG', tip: t('ribbon.tip.svg'), size: 'large', disabled: off, onClick: () => a.exportFile('svg') },
            {
              id: 'sheetPdf',
              testId: 'export-sheet-pdf',
              icon: 'export-sheet-pdf',
              label: t('drawing.pdfSheet'),
              tip: t('drawing.pdfSheetHint'),
              size: 'large',
              disabled: off,
              onClick: a.exportSheetPdf,
            },
          ],
        },
        {
          id: 'print',
          label: t('ribbon.group.print'),
          items: [{ id: 'print', testId: 'drawing-print', icon: 'print', label: t('drawing.print'), tip: t('drawing.printHint'), size: 'large', disabled: off, onClick: a.print }],
        },
      ],
    },
  ];
  return { quick, tabs };
}
