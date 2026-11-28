// Ribbon of the assembly editor (docs/ui/ASSEMBLY_UI.md, "Лента"): quick-access commands of the
// document and the tabs "Компоненты", "Сопряжения", "Анализ", "Исполнения". The panel commands
// switch the right panel (each panel then calls the kernel); the mate type commands open the
// mate panel with that type chosen and are unavailable for a picked pair that does not allow it.
// Tested by src/ui/assembly/__tests__/assemblyRibbon.test.ts.

import type { ChangeEvent, ReactNode, Ref } from 'react';
import { caption } from '../app/i18n';
import type { Translate } from '../app/i18n';
import type { RibbonCommand, RibbonTab } from '../app/ribbonModel';
import { MATE_TYPES } from './mateRules';
import type { MateType } from '../../wasm/bendcore_assembly_api.mjs';

export type AssemblyPanel = 'mate' | 'fasteners' | 'interference' | 'bom' | 'explode' | 'pattern' | 'properties' | 'configurations';

export const PANEL_ICONS: Record<AssemblyPanel, string> = {
  mate: 'mate',
  fasteners: 'fastener',
  interference: 'interference',
  bom: 'bom',
  explode: 'explode',
  pattern: 'pattern-component',
  properties: 'component-properties',
  configurations: 'configurations',
};

export const MATE_ICONS: Record<MateType, string> = {
  coincident: 'mate-coincident',
  concentric: 'mate-concentric',
  distance: 'mate-distance',
  angle: 'mate-angle',
  parallel: 'mate-parallel',
  perpendicular: 'mate-perpendicular',
  tangent: 'mate-tangent',
  rigid: 'mate-rigid',
};

export interface AssemblyRibbonState {
  ready: boolean;
  hasParts: boolean;
  canUndo: boolean;
  canRedo: boolean;
  undoLabel: string | null;
  redoLabel: string | null;
  panel: AssemblyPanel;
  canOpenComponentPart: boolean;
  /** Mate types the picked pair allows; null when the pair is not complete (all offered). */
  allowedMates: readonly MateType[] | null;
  measuring: boolean;
  section: boolean | null;
}

export interface AssemblyRibbonActions {
  newDocument(): void;
  open(e: ChangeEvent<HTMLInputElement>): void;
  openInputRef?: Ref<HTMLInputElement>;
  save(): void;
  importModel(e: ChangeEvent<HTMLInputElement>): void;
  exportModel(): void;
  insertParts(e: ChangeEvent<HTMLInputElement>): void;
  insertSubassembly(e: ChangeEvent<HTMLInputElement>): void;
  openComponentPart?: () => void;
  /** Part library of the shell (select and insert), when there is one. */
  library?: () => ReactNode;
  pickModes(): ReactNode;
  undo(): void;
  redo(): void;
  solve(): void;
  showPanel(panel: AssemblyPanel): void;
  mateType(type: MateType): void;
  toggleMeasure(): void;
  toggleSection(): void;
}

export function assemblyRibbon(
  t: Translate,
  s: AssemblyRibbonState,
  a: AssemblyRibbonActions,
  options: { importAccept: string; partAccept: string },
): { quick: RibbonCommand[]; tabs: RibbonTab[] } {
  const off = !s.ready;
  const panel = (p: AssemblyPanel, size: 'large' | 'small' = 'large'): RibbonCommand => ({
    id: `panel:${p}`,
    testId: `tab-${p}`,
    icon: PANEL_ICONS[p],
    label: t(`assembly.panel.${p}`),
    tip: t(`ribbon.tip.panel.${p}`),
    size,
    active: s.panel === p,
    className: s.panel === p ? 'as-tab-active' : undefined,
    onClick: () => a.showPanel(p),
  });
  const quick: RibbonCommand[] = [
    { id: 'new', testId: 'assembly-new', icon: 'new', label: t('assembly.new'), tip: t('ribbon.tip.new'), disabled: off, onClick: a.newDocument },
    {
      id: 'open',
      kind: 'file',
      testId: 'open-input',
      icon: 'open',
      label: t('assembly.open'),
      tip: t('ribbon.tip.open'),
      hotkey: 'Ctrl+O',
      accept: '.json',
      multiple: true,
      inputRef: a.openInputRef,
      onFiles: a.open,
    },
    { id: 'save', testId: 'save', icon: 'save', label: t('assembly.save'), tip: t('ribbon.tip.save'), hotkey: 'Ctrl+S', disabled: off, onClick: a.save },
    {
      id: 'undo',
      testId: 'undo',
      icon: 'undo',
      label: t('assembly.undo'),
      tip: s.undoLabel ? t('assembly.undoNamed', { name: s.undoLabel }) : t('ribbon.tip.undo'),
      hotkey: 'Ctrl+Z',
      disabled: !s.canUndo,
      onClick: a.undo,
    },
    {
      id: 'redo',
      testId: 'redo',
      icon: 'redo',
      label: t('assembly.redo'),
      tip: s.redoLabel ? t('assembly.redoNamed', { name: s.redoLabel }) : t('ribbon.tip.redo'),
      hotkey: 'Ctrl+Y',
      disabled: !s.canRedo,
      onClick: a.redo,
    },
  ];
  const insert: RibbonTab['groups'][number]['items'] = [
    {
      id: 'insertPart',
      kind: 'file',
      testId: 'insert-part-input',
      icon: 'insert-part',
      label: t('assembly.insertPart'),
      tip: t('ribbon.tip.insertPart'),
      size: 'large',
      accept: options.partAccept,
      multiple: true,
      onFiles: a.insertParts,
    },
    {
      id: 'insertSubassembly',
      kind: 'file',
      testId: 'insert-subassembly-input',
      icon: 'insert-subassembly',
      label: t('assembly.insertSubassembly'),
      tip: t('ribbon.tip.insertSubassembly'),
      size: 'large',
      accept: '.json',
      onFiles: a.insertSubassembly,
    },
  ];
  if (a.library) insert.push({ kind: 'custom', id: 'library', render: a.library });
  if (a.openComponentPart) {
    insert.push({
      id: 'openComponentPart',
      testId: 'open-component-part',
      icon: 'open-component',
      label: t('action.openComponentPart'),
      tip: t('ribbon.tip.openComponentPart'),
      size: 'large',
      disabled: off || !s.canOpenComponentPart,
      onClick: a.openComponentPart,
    });
  }
  const tabs: RibbonTab[] = [
    {
      id: 'components',
      label: t('ribbon.tab.components'),
      groups: [
        { id: 'insert', label: t('ribbon.group.insert'), items: insert },
        { id: 'components', label: t('ribbon.group.components'), items: [panel('pattern'), panel('fasteners'), panel('properties')] },
        {
          id: 'exchange',
          label: t('ribbon.group.exchange'),
          items: [
            {
              id: 'import',
              kind: 'file',
              testId: 'import-step-input',
              icon: 'import',
              label: t('exchange.import'),
              tip: t('ribbon.tip.import'),
              accept: options.importAccept,
              onFiles: a.importModel,
            },
            {
              id: 'export',
              testId: 'export-open',
              icon: 'export',
              label: t('exchange.export'),
              tip: t('ribbon.tip.export'),
              disabled: off || !s.hasParts,
              onClick: a.exportModel,
            },
          ],
        },
        { id: 'selection', label: t('ribbon.group.selection'), items: [{ kind: 'custom', id: 'pickModes', render: a.pickModes }] },
      ],
    },
    {
      id: 'mates',
      label: t('ribbon.tab.mates'),
      groups: [
        { id: 'mate', label: t('ribbon.group.mate'), items: [panel('mate')] },
        {
          id: 'mateTypes',
          label: t('ribbon.group.mateTypes'),
          items: MATE_TYPES.map((m) => {
            const name = caption(t, 'assembly.mateType.', m);
            return {
              id: `mate:${m}`,
              testId: `ribbon-mate-${m}`,
              icon: MATE_ICONS[m],
              label: name,
              tip: t('ribbon.tip.mateType', { name }),
              disabled: off || (s.allowedMates !== null && !s.allowedMates.includes(m)),
              onClick: () => a.mateType(m),
            };
          }),
        },
        {
          id: 'solve',
          label: t('ribbon.group.solve'),
          items: [{ id: 'solve', testId: 'solve', icon: 'solve', label: t('assembly.solve'), tip: t('ribbon.tip.solve'), size: 'large', disabled: off, onClick: a.solve }],
        },
      ],
    },
    {
      id: 'analysis',
      label: t('ribbon.tab.analysis'),
      groups: [
        { id: 'check', label: t('ribbon.group.check'), items: [panel('interference'), panel('bom')] },
        { id: 'presentation', label: t('ribbon.group.presentation'), items: [panel('explode')] },
        {
          id: 'measure',
          label: t('ribbon.group.measure'),
          items: [
            {
              id: 'measure',
              testId: 'ribbon-measure',
              icon: 'measure',
              label: t('ribbon.measure'),
              tip: t('ribbon.tip.measure'),
              size: 'large',
              active: s.measuring,
              disabled: off,
              onClick: a.toggleMeasure,
            },
            {
              id: 'section',
              testId: 'ribbon-section',
              icon: 'section-view',
              label: t('ribbon.section'),
              tip: t('ribbon.tip.section'),
              size: 'large',
              active: s.section === true,
              disabled: s.section === null,
              onClick: a.toggleSection,
            },
          ],
        },
      ],
    },
    {
      id: 'configurations',
      label: t('ribbon.tab.configurations'),
      groups: [{ id: 'configurations', label: t('ribbon.group.configurations'), items: [panel('configurations')] }],
    },
  ];
  return { quick, tabs };
}
