/**
 * Feature tree of the part document (docs/ui/PART_MODELING_UI.md, "Дерево построения"):
 * nodes in tree order with their state, the rollback marker, and a context menu whose actions
 * are part document calls (treeActions.ts).
 */

import React, { useEffect, useState } from 'react';
import type { FeatureView } from '../../wasm/bendcore_modeling_api.mjs';
import { TREE_ACTION_KEYS, TreeAction, treeActions } from './treeActions';
import { caption, useI18n } from '../app/i18n';

export interface FeatureTreeProps {
  features: FeatureView[];
  rollback: number | null;
  selectedId: string | null;
  disabled: boolean;
  onSelect: (id: string | null) => void;
  onAction: (feature: FeatureView, action: TreeAction) => void;
  onClearRollback: () => void;
}

const STATE_MARK: Record<string, string> = {
  ok: '✓',
  warning: '!',
  error: '✕',
  suppressed: '–',
  'rolled-back': '·',
};

const FeatureTree: React.FC<FeatureTreeProps> = ({
  features,
  rollback,
  selectedId,
  disabled,
  onSelect,
  onAction,
  onClearRollback,
}) => {
  const { t } = useI18n();
  const [menuFor, setMenuFor] = useState<string | null>(null);

  useEffect(() => {
    if (!menuFor) return undefined;
    const close = () => setMenuFor(null);
    window.addEventListener('click', close);
    return () => window.removeEventListener('click', close);
  }, [menuFor]);

  const rows: React.ReactNode[] = [];
  features.forEach((f) => {
    if (rollback !== null && f.index === rollback) rows.push(<RollbackMarker key="rollback" onClear={onClearRollback} disabled={disabled} />);
    const actions = treeActions(f, features.length, rollback);
    rows.push(
      <li
        key={f.id}
        className={`pm-tree-item pm-state-${f.state}${selectedId === f.id ? ' pm-selected' : ''}`}
        data-testid={`tree-${f.id}`}
        data-state={f.state}
        onClick={() => onSelect(selectedId === f.id ? null : f.id)}
        onContextMenu={(e) => {
          e.preventDefault();
          if (!disabled) setMenuFor(f.id);
        }}
        title={f.messages.join('\n')}
      >
        <span className="pm-state-mark" aria-label={caption(t, 'part.state.', f.state)}>
          {STATE_MARK[f.state] ?? '?'}
        </span>
        <span className="pm-tree-label">{f.label}</span>
        <span className="pm-tree-type">{caption(t, 'part.feature.', f.type)}</span>
        <button
          type="button"
          className="pm-tree-menu-button"
          data-testid={`tree-menu-${f.id}`}
          aria-label={t('part.tree.actions')}
          disabled={disabled}
          onClick={(e) => {
            e.stopPropagation();
            setMenuFor(menuFor === f.id ? null : f.id);
          }}
        >
          ⋯
        </button>
        {menuFor === f.id && (
          <ul className="pm-menu" role="menu" onClick={(e) => e.stopPropagation()}>
            {actions.map((a) => (
              <li key={a}>
                <button
                  type="button"
                  role="menuitem"
                  data-testid={`action-${a}`}
                  onClick={() => {
                    setMenuFor(null);
                    onAction(f, a);
                  }}
                >
                  {t(TREE_ACTION_KEYS[a])}
                </button>
              </li>
            ))}
          </ul>
        )}
        {f.messages.length > 0 && f.state !== 'ok' && <div className="pm-tree-message">{f.messages.join('; ')}</div>}
      </li>,
    );
  });
  if (rollback !== null && rollback >= features.length) {
    rows.push(<RollbackMarker key="rollback" onClear={onClearRollback} disabled={disabled} />);
  }

  return (
    <section className="pm-tree">
      <h2>{t('part.tree.title')}</h2>
      {features.length === 0 ? (
        <p className="pm-hint">{t('part.tree.empty')}</p>
      ) : (
        <ul data-testid="feature-tree">{rows}</ul>
      )}
    </section>
  );
};

const RollbackMarker: React.FC<{ onClear: () => void; disabled: boolean }> = ({ onClear, disabled }) => {
  const { t } = useI18n();
  return (
    <li className="pm-rollback" data-testid="rollback-marker">
      <span>{t('part.tree.rollbackMarker')}</span>
      <button type="button" data-testid="rollback-clear" disabled={disabled} onClick={onClear}>
        {t('part.tree.removeMarker')}
      </button>
    </li>
  );
};

export default FeatureTree;
