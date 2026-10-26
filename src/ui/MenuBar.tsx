import React, { useState } from 'react';
import './styles.css';

interface MenuItem {
  id: string;
  label: string;
  shortcut?: string;
  submenu?: MenuItem[];
  onClick?: () => void;
  divider?: boolean;
}

interface MenuBarProps {
  onCommand: (commandId: string) => void;
}

/**
 * Enhanced MenuBar with full File, Edit, View, Tools, Window, Help menus
 * SolidWorks/Inventor standard CAD menus
 */
const MenuBar: React.FC<MenuBarProps> = ({ onCommand }) => {
  const [activeMenu, setActiveMenu] = useState<string | null>(null);

  const fileMenuItems: MenuItem[] = [
    { id: 'new-document', label: 'New', shortcut: 'Ctrl+N', onClick: () => onCommand('new-document') },
    { id: 'open-document', label: 'Open', shortcut: 'Ctrl+O', onClick: () => onCommand('open-document') },
    { divider: true },
    { id: 'save-document', label: 'Save', shortcut: 'Ctrl+S', onClick: () => onCommand('save-document') },
    { id: 'save-as', label: 'Save As', shortcut: 'Ctrl+Shift+S', onClick: () => onCommand('save-as') },
    { divider: true },
    { id: 'recent-files', label: 'Recent Files', onClick: () => onCommand('recent-files') },
    { divider: true },
    { id: 'close-document', label: 'Close', shortcut: 'Ctrl+W', onClick: () => onCommand('close-document') },
    { id: 'exit', label: 'Exit', shortcut: 'Alt+F4', onClick: () => onCommand('exit') },
  ];

  const editMenuItems: MenuItem[] = [
    { id: 'undo', label: 'Undo', shortcut: 'Ctrl+Z', onClick: () => onCommand('undo') },
    { id: 'redo', label: 'Redo', shortcut: 'Ctrl+Y', onClick: () => onCommand('redo') },
    { divider: true },
    { id: 'cut', label: 'Cut', shortcut: 'Ctrl+X', onClick: () => onCommand('cut') },
    { id: 'copy', label: 'Copy', shortcut: 'Ctrl+C', onClick: () => onCommand('copy') },
    { id: 'paste', label: 'Paste', shortcut: 'Ctrl+V', onClick: () => onCommand('paste') },
    { divider: true },
    { id: 'select-all', label: 'Select All', shortcut: 'Ctrl+A', onClick: () => onCommand('select-all') },
    { id: 'delete', label: 'Delete', shortcut: 'Del', onClick: () => onCommand('delete') },
    { divider: true },
    { id: 'find', label: 'Find', shortcut: 'Ctrl+F', onClick: () => onCommand('find') },
    { id: 'preferences', label: 'Preferences', shortcut: 'Ctrl+,', onClick: () => onCommand('preferences') },
  ];

  const viewMenuItems: MenuItem[] = [
    { id: 'view-front', label: 'Front View', onClick: () => onCommand('view-front') },
    { id: 'view-top', label: 'Top View', onClick: () => onCommand('view-top') },
    { id: 'view-right', label: 'Right View', onClick: () => onCommand('view-right') },
    { id: 'view-isometric', label: 'Isometric', onClick: () => onCommand('view-isometric') },
    { divider: true },
    { id: 'display-shaded', label: 'Shaded', onClick: () => onCommand('display-shaded') },
    { id: 'display-wireframe', label: 'Wireframe', onClick: () => onCommand('display-wireframe') },
    { id: 'display-hiddenline', label: 'Hidden Line', onClick: () => onCommand('display-hiddenline') },
    { id: 'display-render', label: 'Render', onClick: () => onCommand('display-render') },
    { divider: true },
    { id: 'fit-all', label: 'Fit All', shortcut: 'V', onClick: () => onCommand('fit-all') },
    { id: 'zoom-in', label: 'Zoom In', shortcut: '+', onClick: () => onCommand('zoom-in') },
    { id: 'zoom-out', label: 'Zoom Out', shortcut: '-', onClick: () => onCommand('zoom-out') },
  ];

  const toolsMenuItems: MenuItem[] = [
    { id: 'new-sketch', label: 'New Sketch', shortcut: 'Ctrl+Shift+K', onClick: () => onCommand('new-sketch') },
    { id: 'new-tube', label: 'New Tube', shortcut: 'Ctrl+T', onClick: () => onCommand('new-tube') },
    { id: 'new-bend', label: 'New Bend', onClick: () => onCommand('new-bend') },
    { id: 'new-weld', label: 'New Weld', onClick: () => onCommand('new-weld') },
    { id: 'new-assembly', label: 'New Assembly', onClick: () => onCommand('new-assembly') },
    { divider: true },
    { id: 'check-gaps', label: 'Check Gaps', shortcut: 'Ctrl+G', onClick: () => onCommand('check-gaps') },
    { id: 'calculate-mass', label: 'Calculate Mass', shortcut: 'Ctrl+M', onClick: () => onCommand('calculate-mass') },
    { id: 'measure', label: 'Measure', onClick: () => onCommand('measure') },
    { divider: true },
    { id: 'bend-table', label: 'Bend Table', onClick: () => onCommand('bend-table') },
    { id: 'material-library', label: 'Material Library', onClick: () => onCommand('material-library') },
    { id: 'templates', label: 'Templates', onClick: () => onCommand('templates') },
  ];

  const windowMenuItems: MenuItem[] = [
    { id: 'new-window', label: 'New Window', onClick: () => onCommand('new-window') },
    { divider: true },
    { id: 'tile-windows', label: 'Tile Windows', onClick: () => onCommand('tile-windows') },
    { id: 'cascade-windows', label: 'Cascade Windows', onClick: () => onCommand('cascade-windows') },
  ];

  const helpMenuItems: MenuItem[] = [
    { id: 'help', label: 'Help', shortcut: 'F1', onClick: () => onCommand('help') },
    { id: 'search-help', label: 'Search Help', shortcut: 'Ctrl+H', onClick: () => onCommand('search-help') },
    { divider: true },
    { id: 'about', label: 'About BendCore', onClick: () => onCommand('about') },
    { id: 'check-updates', label: 'Check for Updates', onClick: () => onCommand('check-updates') },
  ];

  const renderMenuItems = (items: MenuItem[]) => (
    <div className="dropdown-menu">
      {items.map((item, idx) =>
        item.divider ? (
          <div key={`divider-${idx}`} className="menu-divider" />
        ) : (
          <div
            key={item.id}
            className="dropdown-item"
            onClick={() => {
              item.onClick?.();
              setActiveMenu(null);
            }}
          >
            <span className="item-label">{item.label}</span>
            {item.shortcut && <span className="item-shortcut">{item.shortcut}</span>}
          </div>
        )
      )}
    </div>
  );

  return (
    <div className="menubar-enhanced">
      <div className="logo">🔧 BendCore CAD</div>

      <div className="menu-items">
        <div
          className={`menu-item ${activeMenu === 'file' ? 'active' : ''}`}
          onMouseEnter={() => setActiveMenu('file')}
          onMouseLeave={() => setActiveMenu(null)}
        >
          File
          {activeMenu === 'file' && renderMenuItems(fileMenuItems)}
        </div>

        <div
          className={`menu-item ${activeMenu === 'edit' ? 'active' : ''}`}
          onMouseEnter={() => setActiveMenu('edit')}
          onMouseLeave={() => setActiveMenu(null)}
        >
          Edit
          {activeMenu === 'edit' && renderMenuItems(editMenuItems)}
        </div>

        <div
          className={`menu-item ${activeMenu === 'view' ? 'active' : ''}`}
          onMouseEnter={() => setActiveMenu('view')}
          onMouseLeave={() => setActiveMenu(null)}
        >
          View
          {activeMenu === 'view' && renderMenuItems(viewMenuItems)}
        </div>

        <div
          className={`menu-item ${activeMenu === 'tools' ? 'active' : ''}`}
          onMouseEnter={() => setActiveMenu('tools')}
          onMouseLeave={() => setActiveMenu(null)}
        >
          Tools
          {activeMenu === 'tools' && renderMenuItems(toolsMenuItems)}
        </div>

        <div
          className={`menu-item ${activeMenu === 'window' ? 'active' : ''}`}
          onMouseEnter={() => setActiveMenu('window')}
          onMouseLeave={() => setActiveMenu(null)}
        >
          Window
          {activeMenu === 'window' && renderMenuItems(windowMenuItems)}
        </div>

        <div
          className={`menu-item ${activeMenu === 'help' ? 'active' : ''}`}
          onMouseEnter={() => setActiveMenu('help')}
          onMouseLeave={() => setActiveMenu(null)}
        >
          Help
          {activeMenu === 'help' && renderMenuItems(helpMenuItems)}
        </div>
      </div>
    </div>
  );
};

export default MenuBar;
