// Context actions of a feature tree node (docs/ui/PART_MODELING_UI.md, "Дерево построения").
// Each action maps to one part document call; unavailable actions are not offered.

import type { FeatureView } from '../../wasm/bendcore_modeling_api.mjs';

export type TreeAction = 'edit' | 'editSketch' | 'suppress' | 'unsuppress' | 'remove' | 'moveUp' | 'moveDown' | 'rollbackHere';

export const TREE_ACTION_LABELS: Record<TreeAction, string> = {
  edit: 'Редактировать',
  editSketch: 'Редактировать эскиз',
  suppress: 'Подавить',
  unsuppress: 'Включить',
  remove: 'Удалить',
  moveUp: 'Перенести выше',
  moveDown: 'Перенести ниже',
  rollbackHere: 'Откатить до этой операции',
};

/**
 * Actions for `feature` in a tree of `count` nodes with rollback marker `rollback`
 * (features at index >= rollback are rolled back).
 */
export function treeActions(feature: FeatureView, count: number, rollback: number | null): TreeAction[] {
  const out: TreeAction[] = [];
  const rolledBack = rollback !== null && feature.index >= rollback;
  if (!rolledBack) out.push('edit');
  // Sketches open in the sketch editor (docs/ui/SKETCH_EDITOR.md).
  if (!rolledBack && feature.type === 'Sketch') out.push('editSketch');
  out.push(feature.suppressed ? 'unsuppress' : 'suppress');
  out.push('remove');
  if (feature.index > 0) out.push('moveUp');
  if (feature.index + 1 < count) out.push('moveDown');
  // Roll back to just after this feature, unless the marker is already there.
  if (rollback !== feature.index + 1 && feature.index + 1 < count) out.push('rollbackHere');
  return out;
}
