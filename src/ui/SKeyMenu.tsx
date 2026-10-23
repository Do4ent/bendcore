/**
 * S-Key Context Menu for BendCore 3D Viewport
 * Triggered by pressing the 'S' key for quick access to common commands
 */

import React, { useState, useEffect } from 'react';
import './SKeyMenu.css';

export interface SKeyMenuItem {
  id: string;
  label: string;
  icon: string;
  shortcut?: string;
  description?: string;
}

export interface SKeyMenuProps {
  position: { x: number; y: number };
  isOpen: boolean;
  onClose: () => void;
  onSelect: (itemId: string) => void;
}

/**
 * SKeyMenu: Quick access menu triggered by 'S' key
 */
const SKeyMenu: React.FC<SKeyMenuProps> = ({
  position,
  isOpen,
  onClose,
  onSelect,
}) => {
  const [selectedIndex, setSelectedIndex] = useState(0);

  const menuItems: SKeyMenuItem[] = [
    {
      id: 'new-sketch',
      label: 'Create Sketch',
      icon: '✏',
      shortcut: 'Ctrl+K',
      description: 'Create a new sketch on the current plane',
    },
    {
      id: 'new-tube',
      label: 'Create Tube',
      icon: '○',
      shortcut: 'Ctrl+T',
      description: 'Create a new tube body',
    },
    {
      id: 'new-port',
      label: 'Create Port',
      icon: '◐',
      shortcut: 'Ctrl+P',
      description: 'Create a new port on the tube',
    },
    {
      id: 'reference-plane',
      label: 'Create Reference Plane',
      icon: '▯',
      shortcut: '',
      description: 'Create an auxiliary reference plane',
    },
    {
      id: 'reference-axis',
      label: 'Create Reference Axis',
      icon: '⊢',
      shortcut: '',
      description: 'Create an auxiliary reference axis',
    },
    {
      id: 'edit-selected',
      label: 'Edit Selected',
      icon: '✎',
      shortcut: 'Double-click',
      description: 'Edit the currently selected object',
    },
  ];

  // Handle keyboard navigation
  useEffect(() => {
    if (!isOpen) return;

    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.code === 'ArrowUp') {
        e.preventDefault();
        setSelectedIndex((prev) => (prev > 0 ? prev - 1 : menuItems.length - 1));
      } else if (e.code === 'ArrowDown') {
        e.preventDefault();
        setSelectedIndex((prev) => (prev < menuItems.length - 1 ? prev + 1 : 0));
      } else if (e.code === 'Enter') {
        e.preventDefault();
        handleSelect(menuItems[selectedIndex].id);
      } else if (e.code === 'Escape') {
        e.preventDefault();
        onClose();
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [isOpen, selectedIndex, menuItems, onClose]);

  const handleSelect = (itemId: string) => {
    onSelect(itemId);
    onClose();
  };

  const handleMouseEnter = (index: number) => {
    setSelectedIndex(index);
  };

  if (!isOpen) {
    return null;
  }

  return (
    <div
      className="s-key-menu"
      style={{
        left: `${position.x}px`,
        top: `${position.y}px`,
      }}
    >
      <div className="s-key-menu-header">
        <span className="s-key-menu-title">Quick Actions</span>
        <span className="s-key-menu-hint">↑↓ Navigate • Enter Select • Esc Close</span>
      </div>

      <div className="s-key-menu-list">
        {menuItems.map((item, index) => (
          <div
            key={item.id}
            className={`s-key-menu-item ${
              index === selectedIndex ? 'selected' : ''
            }`}
            onClick={() => handleSelect(item.id)}
            onMouseEnter={() => handleMouseEnter(index)}
          >
            <span className="s-key-menu-icon">{item.icon}</span>
            <div className="s-key-menu-content">
              <div className="s-key-menu-label">
                {item.label}
                {item.shortcut && (
                  <span className="s-key-menu-shortcut">{item.shortcut}</span>
                )}
              </div>
              {item.description && (
                <div className="s-key-menu-description">{item.description}</div>
              )}
            </div>
            {index === selectedIndex && (
              <span className="s-key-menu-indicator">→</span>
            )}
          </div>
        ))}
      </div>

      <div className="s-key-menu-footer">
        <span className="s-key-menu-footer-text">
          Use arrow keys to navigate • Press Enter to select
        </span>
      </div>
    </div>
  );
};

export default SKeyMenu;
