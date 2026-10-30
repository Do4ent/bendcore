/**
 * BendCore Context Menu Component
 * Right-click (ПКМ) context menus for various object types
 * Phase 32: Professional UI
 */

import React, { useCallback, useEffect, useRef, useState } from 'react';
import './ContextMenu.css';

export interface ContextMenuAction {
  id: string;
  label: string;
  icon?: string;
  shortcut?: string;
  disabled?: boolean;
  divider?: boolean;
  submenu?: ContextMenuAction[];
  action?: () => void;
}

export interface ContextMenuProps {
  isOpen: boolean;
  position: { x: number; y: number };
  actions: ContextMenuAction[];
  onClose: () => void;
  onAction?: (actionId: string) => void;
}

export const ContextMenu: React.FC<ContextMenuProps> = ({
  isOpen,
  position,
  actions,
  onClose,
  onAction,
}) => {
  const menuRef = useRef<HTMLDivElement>(null);
  const [selectedIndex, setSelectedIndex] = useState(-1);
  const [openSubmenu, setOpenSubmenu] = useState<string | null>(null);
  const [adjustedPosition, setAdjustedPosition] = useState(position);

  // Adjust menu position to avoid screen edges
  useEffect(() => {
    if (!isOpen || !menuRef.current) return;

    const rect = menuRef.current.getBoundingClientRect();
    const viewport = {
      width: window.innerWidth,
      height: window.innerHeight,
    };

    let x = position.x;
    let y = position.y;

    // Adjust horizontal position
    if (x + rect.width > viewport.width) {
      x = Math.max(0, viewport.width - rect.width - 8);
    }

    // Adjust vertical position
    if (y + rect.height > viewport.height) {
      y = Math.max(0, viewport.height - rect.height - 8);
    }

    setAdjustedPosition({ x, y });
  }, [isOpen, position]);

  // Handle keyboard navigation
  const handleKeyDown = useCallback(
    (e: KeyboardEvent) => {
      if (!isOpen) return;

      const validActions = actions.filter((a) => !a.divider);

      switch (e.key) {
        case 'ArrowUp':
          e.preventDefault();
          setSelectedIndex((prev) =>
            prev <= 0 ? validActions.length - 1 : prev - 1
          );
          setOpenSubmenu(null);
          break;

        case 'ArrowDown':
          e.preventDefault();
          setSelectedIndex((prev) =>
            prev >= validActions.length - 1 ? 0 : prev + 1
          );
          setOpenSubmenu(null);
          break;

        case 'ArrowRight':
          e.preventDefault();
          if (selectedIndex >= 0 && validActions[selectedIndex]?.submenu) {
            setOpenSubmenu(validActions[selectedIndex].id);
          }
          break;

        case 'ArrowLeft':
          e.preventDefault();
          setOpenSubmenu(null);
          break;

        case 'Enter':
          e.preventDefault();
          if (selectedIndex >= 0) {
            const action = validActions[selectedIndex];
            if (action.submenu) {
              setOpenSubmenu(openSubmenu ? null : action.id);
            } else {
              handleActionClick(action);
            }
          }
          break;

        case 'Escape':
          e.preventDefault();
          onClose();
          break;

        default:
          break;
      }
    },
    [isOpen, actions, selectedIndex, openSubmenu, onClose]
  );

  useEffect(() => {
    if (isOpen) {
      window.addEventListener('keydown', handleKeyDown);
      return () => window.removeEventListener('keydown', handleKeyDown);
    }
  }, [isOpen, handleKeyDown]);

  // Close on outside click
  useEffect(() => {
    const handleOutsideClick = (e: MouseEvent) => {
      if (
        isOpen &&
        menuRef.current &&
        !menuRef.current.contains(e.target as Node)
      ) {
        onClose();
      }
    };

    if (isOpen) {
      document.addEventListener('mousedown', handleOutsideClick);
      return () => document.removeEventListener('mousedown', handleOutsideClick);
    }
  }, [isOpen, onClose]);

  const handleActionClick = (action: ContextMenuAction) => {
    if (action.disabled) return;

    if (action.action) {
      action.action();
    }

    if (onAction) {
      onAction(action.id);
    }

    onClose();
  };

  if (!isOpen) return null;

  return (
    <div
      ref={menuRef}
      className="bendcore-context-menu"
      style={{
        position: 'fixed',
        left: `${adjustedPosition.x}px`,
        top: `${adjustedPosition.y}px`,
        zIndex: 10000,
      }}
    >
      <ContextMenuItems
        actions={actions}
        onAction={handleActionClick}
        selectedIndex={selectedIndex}
        onSelectIndex={setSelectedIndex}
        openSubmenuId={openSubmenu}
        onOpenSubmenu={setOpenSubmenu}
      />
    </div>
  );
};

interface ContextMenuItemsProps {
  actions: ContextMenuAction[];
  onAction: (action: ContextMenuAction) => void;
  selectedIndex: number;
  onSelectIndex: (index: number) => void;
  openSubmenuId: string | null;
  onOpenSubmenu: (id: string | null) => void;
}

const ContextMenuItems: React.FC<ContextMenuItemsProps> = ({
  actions,
  onAction,
  selectedIndex,
  onSelectIndex,
  openSubmenuId,
  onOpenSubmenu,
}) => {
  const validActions = actions.filter((a) => !a.divider);
  const dividerIndices = actions
    .map((a, i) => (a.divider ? i : -1))
    .filter((i) => i !== -1);

  return (
    <ul className="context-menu-list">
      {actions.map((action, index) => {
        if (action.divider) {
          return <li key={`divider-${index}`} className="context-menu-divider" />;
        }

        const itemIndex = validActions.findIndex((a) => a.id === action.id);
        const isSelected = itemIndex === selectedIndex;
        const hasSubmenu = !!action.submenu;
        const isSubmenuOpen = openSubmenuId === action.id;

        return (
          <li
            key={action.id}
            className={`context-menu-item ${
              isSelected ? 'selected' : ''
            } ${action.disabled ? 'disabled' : ''} ${
              hasSubmenu ? 'has-submenu' : ''
            }`}
            onMouseEnter={() => onSelectIndex(itemIndex)}
            onClick={() => {
              if (hasSubmenu) {
                onOpenSubmenu(isSubmenuOpen ? null : action.id);
              } else {
                onAction(action);
              }
            }}
          >
            {action.icon && (
              <span className="context-menu-icon">{action.icon}</span>
            )}
            <span className="context-menu-label">{action.label}</span>
            {action.shortcut && (
              <span className="context-menu-shortcut">{action.shortcut}</span>
            )}
            {hasSubmenu && (
              <span className="context-menu-arrow">›</span>
            )}

            {hasSubmenu && isSubmenuOpen && (
              <div className="context-menu-submenu">
                <ContextMenuItems
                  actions={action.submenu!}
                  onAction={onAction}
                  selectedIndex={-1}
                  onSelectIndex={() => {}}
                  openSubmenuId={null}
                  onOpenSubmenu={() => {}}
                />
              </div>
            )}
          </li>
        );
      })}
    </ul>
  );
};

// Preset context menu generators for different object types

export function createTubeContextMenu(
  onCommand: (cmd: string) => void
): ContextMenuAction[] {
  return [
    {
      id: 'bend-table',
      label: 'Bend Table',
      icon: '📊',
      shortcut: 'Ctrl+Shift+B',
      action: () => onCommand('bend-table'),
    },
    {
      id: 'edit-spec',
      label: 'Edit Specification',
      icon: '✎',
      shortcut: 'Ctrl+E',
      action: () => onCommand('edit-tube'),
    },
    { id: 'div-1', label: '', divider: true },
    {
      id: 'show-deps',
      label: 'Show Dependencies',
      icon: '🔗',
      action: () => onCommand('show-dependencies'),
    },
    { id: 'div-2', label: '', divider: true },
    {
      id: 'hide',
      label: 'Hide',
      icon: '👁️',
      action: () => onCommand('hide-object'),
    },
    {
      id: 'isolate',
      label: 'Isolate',
      icon: '⊙',
      action: () => onCommand('isolate-object'),
    },
    { id: 'div-3', label: '', divider: true },
    {
      id: 'properties',
      label: 'Properties',
      icon: '⚙',
      action: () => onCommand('show-properties'),
    },
  ];
}

export function createVertexContextMenu(
  onCommand: (cmd: string) => void
): ContextMenuAction[] {
  return [
    {
      id: 'edit',
      label: 'Edit',
      icon: '✎',
      shortcut: 'F2',
      action: () => onCommand('edit-vertex'),
    },
    {
      id: 'delete',
      label: 'Delete',
      icon: '✕',
      shortcut: 'Delete',
      action: () => onCommand('delete-vertex'),
    },
    { id: 'div-v1', label: '', divider: true },
    {
      id: 'show-deps',
      label: 'Show Dependencies',
      icon: '🔗',
      action: () => onCommand('show-dependencies'),
    },
    { id: 'div-v2', label: '', divider: true },
    {
      id: 'properties',
      label: 'Properties',
      icon: '⚙',
      action: () => onCommand('show-properties'),
    },
  ];
}

export function createPortContextMenu(
  onCommand: (cmd: string) => void
): ContextMenuAction[] {
  return [
    {
      id: 'edit-params',
      label: 'Edit Parameters',
      icon: '✎',
      action: () => onCommand('edit-port'),
    },
    {
      id: 'delete',
      label: 'Delete',
      icon: '✕',
      shortcut: 'Delete',
      action: () => onCommand('delete-port'),
    },
    {
      id: 'hide',
      label: 'Hide',
      icon: '👁️',
      action: () => onCommand('hide-object'),
    },
    { id: 'div-p1', label: '', divider: true },
    {
      id: 'properties',
      label: 'Properties',
      icon: '⚙',
      action: () => onCommand('show-properties'),
    },
  ];
}

export function createEmptySpaceContextMenu(
  onCommand: (cmd: string) => void
): ContextMenuAction[] {
  return [
    {
      id: 'new-tube',
      label: 'New Tube',
      icon: '➕',
      shortcut: 'Ctrl+T',
      action: () => onCommand('new-tube'),
    },
    {
      id: 'new-sketch',
      label: 'New Sketch',
      icon: '✏',
      shortcut: 'Ctrl+Shift+K',
      action: () => onCommand('new-sketch'),
    },
    {
      id: 'aux-geometry',
      label: 'Aux Geometry',
      icon: '⊞',
      action: () => onCommand('new-aux-geometry'),
    },
    {
      id: 'paste',
      label: 'Paste',
      icon: '📋',
      shortcut: 'Ctrl+V',
      disabled: true,
      action: () => onCommand('paste'),
    },
  ];
}

export function createTreeNodeContextMenu(
  onCommand: (cmd: string) => void
): ContextMenuAction[] {
  return [
    {
      id: 'rename',
      label: 'Rename',
      icon: '✎',
      shortcut: 'F2',
      action: () => onCommand('rename'),
    },
    {
      id: 'edit',
      label: 'Edit',
      icon: '✎',
      action: () => onCommand('edit'),
    },
    {
      id: 'delete',
      label: 'Delete',
      icon: '✕',
      shortcut: 'Delete',
      action: () => onCommand('delete'),
    },
    { id: 'div-t1', label: '', divider: true },
    {
      id: 'duplicate',
      label: 'Duplicate',
      icon: '⧉',
      shortcut: 'Ctrl+D',
      action: () => onCommand('duplicate'),
    },
    { id: 'div-t2', label: '', divider: true },
    {
      id: 'hide',
      label: 'Hide',
      icon: '👁️',
      action: () => onCommand('hide'),
    },
    {
      id: 'show',
      label: 'Show',
      icon: '👁️',
      action: () => onCommand('show'),
    },
    {
      id: 'isolate',
      label: 'Isolate',
      icon: '⊙',
      action: () => onCommand('isolate'),
    },
    { id: 'div-t3', label: '', divider: true },
    {
      id: 'show-deps',
      label: 'Show Dependencies',
      icon: '🔗',
      action: () => onCommand('show-dependencies'),
    },
    {
      id: 'properties',
      label: 'Properties',
      icon: '⚙',
      action: () => onCommand('show-properties'),
    },
  ];
}

export function createCommandConfirmationMenu(
  onCommand: (cmd: string) => void
): ContextMenuAction[] {
  return [
    {
      id: 'ok',
      label: '✓ OK',
      shortcut: 'Enter',
      action: () => onCommand('confirm-command'),
    },
    {
      id: 'cancel',
      label: '✗ Cancel',
      shortcut: 'Esc',
      action: () => onCommand('cancel-command'),
    },
    { id: 'div-c1', label: '', divider: true },
    {
      id: 'repeat',
      label: '↻ Repeat',
      action: () => onCommand('repeat-command'),
    },
  ];
}

export default ContextMenu;
