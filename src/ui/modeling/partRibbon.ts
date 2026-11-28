// Ribbon of the part editor (docs/ui/PART_MODELING_UI.md, "Лента"): quick-access commands of the
// document and the tabs "Эскиз", "Операции", "Массивы", "Вспомогательная геометрия", "Анализ".
// The feature commands come from the kernel catalog (toolbarTypes): a known type goes to its
// place in FEATURE_PLACES, a type the kernel adds later to "Операции / Другие операции" with the
// general feature icon and its caption or kernel name. Tested by src/ui/modeling/__tests__/partRibbon.test.ts.

import type { ChangeEvent, ReactNode, Ref } from 'react';
import { caption, isMessageKey } from '../app/i18n';
import type { MessageKey, Translate } from '../app/i18n';
import type { RibbonCommand, RibbonGroup, RibbonItem, RibbonTab } from '../app/ribbonModel';
import { FALLBACK_ICON } from '../icons';
import { toolbarTypes } from './labels';

export type PartTabId = 'sketch' | 'features' | 'patterns' | 'reference' | 'analysis';

export interface FeaturePlace {
  tab: PartTabId;
  group: string;
  icon: string;
  size: 'large' | 'small';
}

/** Place of each feature type the editor knows on the ribbon. */
export const FEATURE_PLACES: Record<string, FeaturePlace> = {
  Sketch: { tab: 'sketch', group: 'sketch', icon: 'sketch', size: 'large' },
  Extrude: { tab: 'features', group: 'body', icon: 'extrude', size: 'large' },
  Revolve: { tab: 'features', group: 'body', icon: 'revolve', size: 'large' },
  Sweep: { tab: 'features', group: 'body', icon: 'sweep', size: 'small' },
  Loft: { tab: 'features', group: 'body', icon: 'loft', size: 'small' },
  Hole: { tab: 'features', group: 'holes', icon: 'hole', size: 'large' },
  ThreadedHole: { tab: 'features', group: 'holes', icon: 'hole-threaded', size: 'large' },
  Fillet: { tab: 'features', group: 'edges', icon: 'fillet', size: 'large' },
  Chamfer: { tab: 'features', group: 'edges', icon: 'chamfer', size: 'large' },
  Shell: { tab: 'features', group: 'modify', icon: 'shell', size: 'small' },
  Draft: { tab: 'features', group: 'modify', icon: 'draft', size: 'small' },
  Rib: { tab: 'features', group: 'modify', icon: 'rib', size: 'small' },
  GearTeeth: { tab: 'features', group: 'special', icon: 'gear-rim', size: 'large' },
  BodyBoolean: { tab: 'features', group: 'bodies', icon: 'boolean', size: 'small' },
  DeleteBody: { tab: 'features', group: 'bodies', icon: 'body-delete', size: 'small' },
  LinearPattern: { tab: 'patterns', group: 'featurePatterns', icon: 'pattern-linear', size: 'large' },
  CircularPattern: { tab: 'patterns', group: 'featurePatterns', icon: 'pattern-circular', size: 'large' },
  Mirror: { tab: 'patterns', group: 'featurePatterns', icon: 'mirror', size: 'large' },
  BodyLinearPattern: { tab: 'patterns', group: 'bodyPatterns', icon: 'pattern-linear-body', size: 'small' },
  BodyCircularPattern: { tab: 'patterns', group: 'bodyPatterns', icon: 'pattern-circular-body', size: 'small' },
  BodyMirror: { tab: 'patterns', group: 'bodyPatterns', icon: 'mirror-body', size: 'small' },
  DatumPlane: { tab: 'reference', group: 'datums', icon: 'plane', size: 'large' },
  DatumAxis: { tab: 'reference', group: 'datums', icon: 'axis', size: 'large' },
  DatumPoint: { tab: 'reference', group: 'datums', icon: 'point', size: 'large' },
  DatumCsys: { tab: 'reference', group: 'datums', icon: 'csys', size: 'large' },
};

/** Place of a catalog type the editor does not know (a feature the kernel adds later). */
export const OTHER_PLACE: FeaturePlace = { tab: 'features', group: 'other', icon: FALLBACK_ICON, size: 'small' };

/** Groups of each tab in order. */
export const PART_GROUPS: Record<PartTabId, string[]> = {
  sketch: ['sketch'],
  features: ['body', 'holes', 'edges', 'modify', 'special', 'bodies', 'other'],
  patterns: ['featurePatterns', 'bodyPatterns'],
  reference: ['datums', 'referenceData'],
  analysis: ['measure', 'configurations'],
};

export interface CatalogType {
  type: string;
  /** Allowed values of the type's options (Extrude: mode boss / cut). */
  options?: { name: string; allowed?: readonly string[] }[];
}

export interface PartRibbonState {
  ready: boolean;
  /** A feature panel or the sketch editor is open. */
  busy: boolean;
  catalog: readonly CatalogType[];
  canUndo: boolean;
  canRedo: boolean;
  undoName: string | null;
  redoName: string | null;
  canCreateDrawing: boolean;
  measuring: boolean;
  /** null: the section tool is unavailable (no model). */
  section: boolean | null;
  configurationRows: number;
}

export interface PartRibbonActions {
  newDocument(): void;
  openFile(e: ChangeEvent<HTMLInputElement>): void;
  openInputRef?: Ref<HTMLInputElement>;
  save(): void;
  undo(): void;
  redo(): void;
  createDrawing?: () => void;
  drawSketch(): void;
  /** Sketch plane choice (rendered by the editor). */
  plane(): ReactNode;
  addFeature(type: string, options?: Record<string, string>): void;
  toggleMeasure(): void;
  toggleSection(): void;
  insertMesh(e: ChangeEvent<HTMLInputElement>): void;
  checkConfigurations(): void;
}

function featureTip(t: Translate, type: string): string {
  const key = `ribbon.tip.feature.${type}`;
  return isMessageKey(key) ? t(key as MessageKey) : t('ribbon.tip.feature', { name: caption(t, 'part.feature.', type) });
}

/** The quick-access commands and the tabs of the part editor. */
export function partRibbon(t: Translate, s: PartRibbonState, a: PartRibbonActions): { quick: RibbonCommand[]; tabs: RibbonTab[] } {
  const docOff = !s.ready || s.busy;
  const quick: RibbonCommand[] = [
    { id: 'new', testId: 'doc-new', icon: 'new', label: t('part.new'), tip: t('ribbon.tip.new'), disabled: docOff, onClick: a.newDocument },
    {
      id: 'open',
      kind: 'file',
      testId: 'doc-open',
      icon: 'open',
      label: t('part.open'),
      tip: t('ribbon.tip.open'),
      hotkey: 'Ctrl+O',
      accept: '.json,application/json',
      disabled: docOff,
      inputRef: a.openInputRef,
      onFiles: a.openFile,
    },
    { id: 'save', testId: 'doc-save', icon: 'save', label: t('part.save'), tip: t('ribbon.tip.save'), hotkey: 'Ctrl+S', disabled: !s.ready, onClick: a.save },
    {
      id: 'undo',
      testId: 'undo',
      icon: 'undo',
      label: t('part.undo'),
      tip: s.undoName ? t('part.undoNamed', { name: s.undoName }) : t('ribbon.tip.undo'),
      hotkey: 'Ctrl+Z',
      disabled: s.busy || !s.canUndo,
      onClick: a.undo,
    },
    {
      id: 'redo',
      testId: 'redo',
      icon: 'redo',
      label: t('part.redo'),
      tip: s.redoName ? t('part.redoNamed', { name: s.redoName }) : t('ribbon.tip.redo'),
      hotkey: 'Ctrl+Y',
      disabled: s.busy || !s.canRedo,
      onClick: a.redo,
    },
  ];
  if (s.canCreateDrawing && a.createDrawing) {
    quick.splice(3, 0, {
      id: 'createDrawing',
      testId: 'doc-create-drawing',
      icon: 'create-drawing',
      label: t('action.createDrawing'),
      tip: t('ribbon.tip.createDrawing'),
      disabled: docOff,
      onClick: a.createDrawing,
    });
  }

  const groups: Record<string, RibbonItem[]> = {};
  const push = (group: string, item: RibbonItem) => {
    (groups[group] ??= []).push(item);
  };
  push('sketch', { kind: 'custom', id: 'sketchPlane', render: a.plane });
  push('sketch', {
    id: 'drawSketch',
    testId: 'sketch-draw',
    icon: 'sketch-on-plane',
    label: t('part.drawSketch'),
    tip: t('ribbon.tip.drawSketch'),
    size: 'large',
    disabled: docOff,
    onClick: a.drawSketch,
  });
  for (const type of toolbarTypes(s.catalog.map((c) => c.type))) {
    const place = FEATURE_PLACES[type] ?? OTHER_PLACE;
    push(place.group, {
      id: `feature:${type}`,
      testId: `add-${type}`,
      icon: place.icon,
      label: caption(t, 'part.feature.', type),
      tip: featureTip(t, type),
      size: place.size,
      disabled: docOff,
      onClick: () => a.addFeature(type),
    });
    // Extrude in cut mode: the same kernel feature with the option preset.
    const mode = type === 'Extrude' ? s.catalog.find((c) => c.type === type)?.options?.find((o) => o.name === 'mode') : undefined;
    if (mode?.allowed?.includes('cut')) {
      push(place.group, {
        id: 'cut',
        testId: 'add-cut',
        icon: 'cut',
        label: t('ribbon.cut'),
        tip: t('ribbon.tip.cut'),
        size: 'large',
        disabled: docOff,
        onClick: () => a.addFeature(type, { mode: 'cut' }),
      });
    }
  }
  push('referenceData', {
    id: 'insertMesh',
    kind: 'file',
    testId: 'ribbon-insert-mesh',
    icon: 'reference-mesh',
    label: t('ribbon.insertMesh'),
    tip: t('ribbon.tip.insertMesh'),
    size: 'large',
    accept: '.stl',
    disabled: docOff,
    onFiles: a.insertMesh,
  });
  push('measure', {
    id: 'measure',
    testId: 'ribbon-measure',
    icon: 'measure',
    label: t('ribbon.measure'),
    tip: t('ribbon.tip.measure'),
    size: 'large',
    active: s.measuring,
    disabled: !s.ready || (s.busy && !s.measuring),
    onClick: a.toggleMeasure,
  });
  push('measure', {
    id: 'section',
    testId: 'ribbon-section',
    icon: 'section-view',
    label: t('ribbon.section'),
    tip: t('ribbon.tip.section'),
    size: 'large',
    active: s.section === true,
    disabled: s.section === null,
    onClick: a.toggleSection,
  });
  push('configurations', {
    id: 'checkConfigurations',
    testId: 'ribbon-check-configurations',
    icon: 'parameters-table',
    label: t('ribbon.checkConfigurations'),
    tip: t('ribbon.tip.checkConfigurations'),
    size: 'large',
    disabled: docOff || s.configurationRows === 0,
    onClick: a.checkConfigurations,
  });

  const tabs: RibbonTab[] = (Object.keys(PART_GROUPS) as PartTabId[]).map((tab) => ({
    id: tab,
    label: t(`ribbon.tab.${tab}`),
    groups: PART_GROUPS[tab].map(
      (g): RibbonGroup => ({ id: g, label: t(`ribbon.group.${g}` as MessageKey), items: groups[g] ?? [] }),
    ),
  }));
  return { quick, tabs };
}
