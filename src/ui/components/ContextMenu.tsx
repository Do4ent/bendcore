/**
 * BendCore UI Component: ContextMenu
 * Right-click context menu with keyboard navigation
 *
 * @component
 * @example
 * <ContextMenu
 *   items={items}
 *   position={{ x: 100, y: 200 }}
 *   onClose={() => setOpen(false)}
 * />
 */

import React, { useEffect, useRef, useCallback, useState } from 'react';
import './ContextMenu.css';

// ============ Types ============

export interface ContextMenuItem {
  id: string;
  label: string;
  icon?: string;
  action: () => void;
  submenu?: ContextMenuItem[];
  disabled?: boolean;
  divider?: boolean;
}

interface Position {
  x: number;
  y: number;
}

interface ContextMenuProps {
  items: ContextMenuItem[];
  position: Position;
  onClose: () => void;
  isDarkMode?: boolean;
}

// ============ Component ============

export const ContextMenu: React.FC<ContextMenuProps> = ({
  items,
  position,
  onClose,
  isDarkMode = false,
}) => {
  const menuRef = useRef<HTMLDivElement>(null);
  const [focusedItemId, setFocusedItemId] = useState<string | null>(null);
  const [openSubmenu, setOpenSubmenu] = useState<string | null>(null);

  // Adjust position to stay in viewport
  const adjustedPosition = React.useMemo(() => {
    const adjusted = { ...position };
    if (menuRef.current) {
      const rect = menuRef.current.getBoundingClientRect();
      if (rect.right > window.innerWidth) {
        adjusted.x -= rect.width;
      }
      if (rect.bottom > window.innerHeight) {
        adjusted.y -= rect.height;
      }
    }
    return adjusted;
  }, [position]);

  // Close on outside click
  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(event.target as Node)) {
        onClose();
      }
    };

    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, [onClose]);

  // Handle keyboard navigation
  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent) => {
      const validItems = items.filter((item) => !item.divider && !item.disabled);

      if (e.key === 'Escape') {
        e.preventDefault();
        onClose();
      } else if (e.key === 'ArrowDown') {
        e.preventDefault();
        const focusedIndex = validItems.findIndex((item) => item.id === focusedItemId);
        const nextIndex = focusedIndex < validItems.length - 1 ? focusedIndex + 1 : 0;
        setFocusedItemId(validItems[nextIndex].id);
      } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        const focusedIndex = validItems.findIndex((item) => item.id === focusedItemId);
        const prevIndex = focusedIndex > 0 ? focusedIndex - 1 : validItems.length - 1;
        setFocusedItemId(validItems[prevIndex].id);
      } else if (e.key === 'Enter') {
        e.preventDefault();
        const item = items.find((i) => i.id === focusedItemId);
        if (item && !item.disabled) {
          item.action();
          onClose();
        }
      } else if (e.key === 'ArrowRight') {
        e.preventDefault();
        const item = items.find((i) => i.id === focusedItemId);
        if (item?.submenu) {
          setOpenSubmenu(item.id);
        }
      } else if (e.key === 'ArrowLeft') {
        e.preventDefault();
        setOpenSubmenu(null);
      }
    },
    [focusedItemId, items, onClose]
  );

  // Focus management
  useEffect(() => {
    menuRef.current?.focus();
    if (!focusedItemId) {
      const firstItem = items.find((item) => !item.divider && !item.disabled);
      if (firstItem) {
        setFocusedItemId(firstItem.id);
      }
    }
  }, [focusedItemId, items]);

  return (
    <div
      ref={menuRef}
      className={`context-menu ${isDarkMode ? 'context-menu--dark' : ''}`}
      style={{
        position: 'fixed',
        top: adjustedPosition.y,
        left: adjustedPosition.x,
      }}
      role="menu"
      onKeyDown={handleKeyDown}
      tabIndex={-1}
    >
      {items.map((item, index) =>
        item.divider ? (
          <div key={`divider-${index}`} className="context-menu__divider" role="separator" />
        ) : (
          <ContextMenuItemComponent
            key={item.id}
            item={item}
            isFocused={focusedItemId === item.id}
            isSubmenuOpen={openSubmenu === item.id}
            onMouseEnter={() => setFocusedItemId(item.id)}
            onMouseLeave={() => {}}
            onClick={() => {
              if (!item.disabled) {
                item.action();
                onClose();
              }
            }}
            onSubmenuToggle={(open) => setOpenSubmenu(open ? item.id : null)}
            onClose={onClose}
          />
        )
      )}
    </div>
  );
};

// ============ MenuItem Component ============

interface ContextMenuItemComponentProps {
  item: ContextMenuItem;
  isFocused: boolean;
  isSubmenuOpen: boolean;
  onMouseEnter: () => void;
  onMouseLeave: () => void;
  onClick: () => void;
  onSubmenuToggle: (open: boolean) => void;
  onClose: () => void;
}

const ContextMenuItemComponent: React.FC<ContextMenuItemComponentProps> = ({
  item,
  isFocused,
  isSubmenuOpen,
  onMouseEnter,
  onMouseLeave,
  onClick,
  onSubmenuToggle,
  onClose,
}) => {
  const submenuRef = useRef<HTMLDivElement>(null);

  const handleMouseEnter = () => {
    onMouseEnter();
    if (item.submenu && item.submenu.length > 0) {
      onSubmenuToggle(true);
    }
  };

  const handleMouseLeave = () => {
    onMouseLeave();
    if (item.submenu && item.submenu.length > 0) {
      onSubmenuToggle(false);
    }
  };

  return (
    <div
      className={`context-menu__item-wrapper ${isFocused ? 'context-menu__item-wrapper--focused' : ''}`}
      onMouseEnter={handleMouseEnter}
      onMouseLeave={handleMouseLeave}
    >
      <button
        className={`context-menu__item ${
          item.disabled ? 'context-menu__item--disabled' : ''
        }`}
        onClick={onClick}
        disabled={item.disabled}
        role="menuitem"
        aria-disabled={item.disabled}
        aria-haspopup={item.submenu ? 'menu' : undefined}
        aria-expanded={isSubmenuOpen}
      >
        {item.icon && <span className="context-menu__item-icon">{item.icon}</span>}
        <span className="context-menu__item-label">{item.label}</span>
        {item.submenu && item.submenu.length > 0 && (
          <span className="context-menu__item-arrow">▶</span>
        )}
      </button>

      {/* === Submenu === */}
      {item.submenu && item.submenu.length > 0 && isSubmenuOpen && (
        <div
          ref={submenuRef}
          className="context-menu__submenu"
          role="menu"
        >
          {item.submenu.map((subitem) =>
            subitem.divider ? (
              <div
                key={`divider-${subitem.id}`}
                className="context-menu__divider"
                role="separator"
              />
            ) : (
              <button
                key={subitem.id}
                className={`context-menu__item ${
                  subitem.disabled ? 'context-menu__item--disabled' : ''
                }`}
                onClick={() => {
                  if (!subitem.disabled) {
                    subitem.action();
                    onClose();
                  }
                }}
                disabled={subitem.disabled}
                role="menuitem"
                aria-disabled={subitem.disabled}
              >
                {subitem.icon && (
                  <span className="context-menu__item-icon">{subitem.icon}</span>
                )}
                <span className="context-menu__item-label">{subitem.label}</span>
              </button>
            )
          )}
        </div>
      )}
    </div>
  );
};

export default ContextMenu;
