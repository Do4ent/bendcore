// Ribbon of the STEP viewer (docs/ui/KERNEL_VIEWPORT.md, "Лента"): open and export in the quick
// access, the tab "Просмотр" with the tessellation tolerances and the measurement and section
// tools. Tested by src/ui/app/__tests__/ribbon.test.tsx.

import type { ChangeEvent, ReactNode } from 'react';
import type { Translate } from '../app/i18n';
import type { RibbonCommand, RibbonTab } from '../app/ribbonModel';

export interface ViewerRibbonState {
  ready: boolean;
  busy: boolean;
  hasModel: boolean;
  measuring: boolean;
  /** null: no section tool (no model). */
  section: boolean | null;
}

export interface ViewerRibbonActions {
  open(e: ChangeEvent<HTMLInputElement>): void;
  exportModel(): void;
  /** Tolerance fields (rendered by the viewer). */
  tolerances(): ReactNode;
  retessellate(): void;
  toggleMeasure(): void;
  toggleSection(): void;
}

export function viewerRibbon(t: Translate, s: ViewerRibbonState, a: ViewerRibbonActions, accept: string): { quick: RibbonCommand[]; tabs: RibbonTab[] } {
  const quick: RibbonCommand[] = [
    {
      id: 'open',
      kind: 'file',
      testId: 'step-input',
      icon: 'open',
      label: t('viewer.open'),
      tip: t('ribbon.tip.import'),
      accept,
      disabled: !s.ready || s.busy,
      onFiles: a.open,
    },
    {
      id: 'export',
      testId: 'viewer-export',
      icon: 'export',
      label: t('exchange.export'),
      tip: t('ribbon.tip.export'),
      disabled: !s.hasModel || s.busy,
      onClick: a.exportModel,
    },
  ];
  const tabs: RibbonTab[] = [
    {
      id: 'view',
      label: t('ribbon.tab.view'),
      groups: [
        {
          id: 'tessellation',
          label: t('ribbon.group.tessellation'),
          items: [
            { kind: 'custom', id: 'tolerances', render: a.tolerances },
            {
              id: 'retessellate',
              testId: 'viewer-retessellate',
              icon: 'tessellate',
              label: t('viewer.retessellate'),
              tip: t('ribbon.tip.retessellate'),
              size: 'large',
              disabled: !s.hasModel || s.busy,
              onClick: a.retessellate,
            },
          ],
        },
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
              disabled: !s.hasModel,
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
  ];
  return { quick, tabs };
}
