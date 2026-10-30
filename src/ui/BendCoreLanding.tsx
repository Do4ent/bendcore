import React, { useState } from 'react';
import './styles.css';
import './BendCoreLanding.css';
import M22Integration from './M22Integration';
import M23Integration from './M23Integration';
import M24Integration from './M24Integration';
import M25Integration from './M25Integration';

/**
 * BendCore Main Application Interface
 * Professional tube bending CAD system
 *
 * Color Palette (from bendcore design system):
 * - Tube: #1F6FC5 (Blue)
 * - Create: #2E9E5B (Green)
 * - Delete: #D93A3A (Red)
 * - Import: #0E97A3 (Cyan)
 * - Links: #8A4FD8 (Purple)
 * - Edit: #E8820C (Orange)
 * - Update: #F5B517 (Yellow-Orange)
 * - Freeze: #2D9CDB (Light Blue)
 * - Auxiliary: #C9920A (Brown)
 */

interface TreeNode {
  id: string;
  name: string;
  icon: string;
  type: 'project' | 'sketch' | 'tube' | 'feature' | 'analysis' | 'drawing';
  children?: TreeNode[];
  visible?: boolean;
  locked?: boolean;
}

const BendCoreLanding: React.FC = () => {
  const [selectedNode, setSelectedNode] = useState<string>('tube-assembly');
  const [viewMode, setViewMode] = useState<'shaded' | 'wireframe' | 'hiddenline' | 'render'>('shaded');
  const [openMenus, setOpenMenus] = useState<{ [key: string]: boolean }>({});

  // Project tree structure from INTERFACE_OVERVIEW.md
  const treeData: TreeNode = {
    id: 'project-root',
    name: 'My_First_Bend',
    icon: '📁',
    type: 'project',
    children: [
      { id: 'sketch-01', name: 'Sketch_01', icon: '📋', type: 'sketch' },
      {
        id: 'tube-assembly',
        name: 'Tube_01',
        icon: '🔲',
        type: 'tube',
        children: [
          { id: 'bend-01', name: 'Bend_01', icon: '🔶', type: 'feature' }
        ]
      },
      { id: 'bend-02', name: 'Bend_002', icon: '🔶', type: 'feature' },
      { id: 'weld-01', name: 'Weld_001', icon: '⚡', type: 'feature' },
      { id: 'assembly-01', name: 'Assembly_001', icon: '🔗', type: 'feature' },
      { id: 'cut-01', name: 'Cut_001', icon: '✂️', type: 'feature' },
      { id: 'analysis-01', name: 'Analysis_01', icon: '📊', type: 'analysis' },
      { id: 'drawing-01', name: 'Drawing_01', icon: '📄', type: 'drawing' }
    ]
  };

  const renderTreeNode = (node: TreeNode, level: number = 0): JSX.Element => {
    const isSelected = node.id === selectedNode;

    return (
      <div key={node.id}>
        <div
          className={`tree-item ${isSelected ? 'selected' : ''}`}
          onClick={() => setSelectedNode(node.id)}
          style={{ paddingLeft: `${level * 16}px` }}
        >
          <span className="tree-icon">{node.icon}</span>
          <span className="tree-label">{node.name}</span>
          {node.type === 'tube' && (
            <span className="tree-visibility-icon" title="Toggle visibility">👁️</span>
          )}
        </div>
        {node.children && node.children.map(child => renderTreeNode(child, level + 1))}
      </div>
    );
  };

  /**
   * Handle command execution
   * Routes commands to appropriate dialog or action
   */
  const handleCommand = (command: string) => {
    console.log(`Command: ${command}`);

    // Get M22 integration API
    const m22api = (window as any).__bendcore_m22;

    switch (command) {
      // File operations
      case 'open-document':
        m22api?.openDialog('file-open');
        break;
      case 'save-document':
        m22api?.openDialog('file-save');
        break;
      case 'recent-files':
        m22api?.openDialog('recent-files');
        break;
      // Settings
      case 'menu-file':
      case 'menu-edit':
      case 'menu-view':
      case 'menu-tools':
      case 'menu-window':
      case 'menu-help':
        // These would show submenus - for now just log
        break;
      // Sketch operations
      case 'new-sketch':
        console.log('Opening sketch editor...');
        break;
      // Assembly operations
      case 'new-assembly':
        console.log('Opening assembly dialog...');
        break;
      // Manufacturing
      case 'generate-bom':
        console.log('Generating BOM...');
        break;
      default:
        console.log(`Command not yet implemented: ${command}`);
    }
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.ctrlKey && e.shiftKey) {
      switch (e.key.toLowerCase()) {
        case 'k':
          e.preventDefault();
          handleCommand('new-sketch');
          break;
      }
    } else if (e.ctrlKey) {
      switch (e.key.toLowerCase()) {
        case 'n':
          e.preventDefault();
          handleCommand('new-document');
          break;
        case 'o':
          e.preventDefault();
          handleCommand('open-document');
          break;
        case 's':
          e.preventDefault();
          handleCommand('save-document');
          break;
        case 'z':
          e.preventDefault();
          handleCommand('undo');
          break;
        case 'y':
          e.preventDefault();
          handleCommand('redo');
          break;
        case 'x':
          e.preventDefault();
          handleCommand('cut');
          break;
        case 'c':
          e.preventDefault();
          handleCommand('copy');
          break;
        case 'v':
          e.preventDefault();
          handleCommand('paste');
          break;
        case 't':
          e.preventDefault();
          handleCommand('new-tube');
          break;
        case 'g':
          e.preventDefault();
          handleCommand('check-gaps');
          break;
        case 'm':
          e.preventDefault();
          handleCommand('calculate-mass');
          break;
        case '/':
          e.preventDefault();
          handleCommand('command-palette');
          break;
      }
    } else if (e.key === 'F1') {
      e.preventDefault();
      handleCommand('help');
    } else if (e.key === 'F3') {
      e.preventDefault();
      handleCommand('toggle-snap');
    } else if (e.key === 'v' && !e.ctrlKey) {
      e.preventDefault();
      handleCommand('fit-all');
    }
  };

  return (
    <div className="bendcore-app" onKeyDown={handleKeyDown} tabIndex={0}>
      {/* Menu Bar */}
      <div className="menubar">
        <div className="logo">🔧 BendCore CAD</div>
        <div className="menu-items">
          <div className="menu-item" onClick={() => handleCommand('menu-file')}>File</div>
          <div className="menu-item" onClick={() => handleCommand('menu-edit')}>Edit</div>
          <div className="menu-item" onClick={() => handleCommand('menu-view')}>View</div>
          <div className="menu-item" onClick={() => handleCommand('menu-tools')}>Tools</div>
          <div className="menu-item" onClick={() => handleCommand('menu-window')}>Window</div>
          <div className="menu-item" onClick={() => handleCommand('menu-help')}>Help</div>
        </div>
      </div>

      {/* Toolbar */}
      <div className="toolbar">
        <div className="toolbar-group">
          <button className="toolbar-btn" title="New (Ctrl+N)" onClick={() => handleCommand('new-document')}>🏠</button>
          <button className="toolbar-btn" title="Open (Ctrl+O)" onClick={() => handleCommand('open-document')}>📁</button>
          <button className="toolbar-btn" title="Save (Ctrl+S)" onClick={() => handleCommand('save-document')}>💾</button>
        </div>

        <div className="toolbar-separator"></div>

        <div className="toolbar-group">
          <button className="toolbar-btn" title="Undo (Ctrl+Z)" onClick={() => handleCommand('undo')}>↶</button>
          <button className="toolbar-btn" title="Redo (Ctrl+Y)" onClick={() => handleCommand('redo')}>↷</button>
        </div>

        <div className="toolbar-separator"></div>

        <div className="toolbar-group">
          <button className="toolbar-btn" title="Sketch (Ctrl+Shift+K)" onClick={() => handleCommand('new-sketch')}>✏️</button>
          <button className="toolbar-btn" title="Tube (Ctrl+T)" onClick={() => handleCommand('new-tube')}>📦</button>
          <button className="toolbar-btn" title="Bend" onClick={() => handleCommand('new-bend')}>🔀</button>
          <button className="toolbar-btn" title="Weld" onClick={() => handleCommand('new-weld')}>⚡</button>
          <button className="toolbar-btn" title="Assembly" onClick={() => handleCommand('new-assembly')}>🔗</button>
        </div>

        <div className="toolbar-separator"></div>

        <div className="toolbar-group">
          <button className="toolbar-btn" title="Check Gaps (Ctrl+G)" onClick={() => handleCommand('check-gaps')}>📊</button>
          <button className="toolbar-btn" title="Calculate Mass (Ctrl+M)" onClick={() => handleCommand('calculate-mass')}>⚙️</button>
        </div>

        <div className="toolbar-separator"></div>

        <div className="toolbar-group">
          <button className="toolbar-btn" title="Fit All (V)" onClick={() => handleCommand('fit-all')}>🔍</button>
          <button className="toolbar-btn" title="View Presets" onClick={() => handleCommand('view-presets')}>📐</button>
        </div>
      </div>

      {/* Main Content Area */}
      <div className="main-content">
        {/* Left Panel - Project Tree */}
        <div className="left-panel">
          <div className="panel-header">MODEL TREE</div>
          <div className="model-tree">
            {renderTreeNode(treeData)}
          </div>
        </div>

        {/* Center Panel - 3D Viewport */}
        <div className="center-panel">
          <div className="viewport-toolbar">
            <div className="view-preset-buttons">
              <button className="view-btn" title="Front View">Front</button>
              <button className="view-btn" title="Top View">Top</button>
              <button className="view-btn" title="Right View">Right</button>
              <button className="view-btn active" title="Isometric View">Iso</button>
            </div>

            <div className="display-mode-buttons">
              <button
                className={`display-btn ${viewMode === 'wireframe' ? 'active' : ''}`}
                onClick={() => setViewMode('wireframe')}
                title="Wireframe"
              >
                🗦
              </button>
              <button
                className={`display-btn ${viewMode === 'shaded' ? 'active' : ''}`}
                onClick={() => setViewMode('shaded')}
                title="Shaded"
              >
                ⚪
              </button>
              <button
                className={`display-btn ${viewMode === 'hiddenline' ? 'active' : ''}`}
                onClick={() => setViewMode('hiddenline')}
                title="Hidden Line"
              >
                ⊞
              </button>
              <button
                className={`display-btn ${viewMode === 'render' ? 'active' : ''}`}
                onClick={() => setViewMode('render')}
                title="Render"
              >
                🌟
              </button>
            </div>
          </div>

          <div className="viewport">
            <div className="model-preview">
              <div className="preview-content">
                <div className="preview-text">3D Model Viewer</div>
                <div className="preview-text">(Three.js)</div>
                <div className="preview-text" style={{ marginTop: '12px' }}>Tube Bending Design</div>
                <div className="preview-stats">
                  <div>View: {viewMode.charAt(0).toUpperCase() + viewMode.slice(1)}</div>
                  <div>Scale: 100%</div>
                </div>
              </div>
            </div>
          </div>
        </div>

        {/* Right Panel - Properties & Browser */}
        <div className="right-panel">
          <div className="panel-header">PROPERTIES</div>

          <div className="properties-section">
            <div className="property-label">Tube Diameter</div>
            <div className="property-value">12.7 mm</div>

            <div className="property-label">Wall Thickness</div>
            <div className="property-value">1.24 mm</div>

            <div className="property-label">Material</div>
            <div className="property-value">Stainless 304</div>
          </div>

          <div className="properties-section">
            <div className="property-label">Bend Radius</div>
            <div className="property-value">2.5 in</div>

            <div className="property-label">Bend Angle</div>
            <div className="property-value">90°</div>

            <div className="property-label">Spring-back</div>
            <div className="property-value">2.3°</div>
          </div>

          <div className="properties-section">
            <div className="property-label">Weight</div>
            <div className="property-value">0.354 kg</div>

            <div className="property-label">Cost Estimate</div>
            <div className="property-value">$42.50</div>
          </div>

          <div className="properties-section">
            <div className="property-label">Analysis Status</div>
            <div className="property-value-status">✓ Complete</div>
          </div>

          <div style={{ flex: 1 }}></div>
        </div>
      </div>

      {/* Status Bar */}
      <div className="statusbar">
        <div className="status-left">
          <div>✓ Ready</div>
          <div>Tube_01 selected</div>
          <div>Scale: 100%</div>
        </div>
        <div className="status-right">
          v2.1.0-unified | BendCore CAD | Coordinates: X: 10.5 Y: 20.3 Z: 0
        </div>
      </div>

      {/* Keyboard Shortcuts Help */}
      <div className="shortcuts-hint">
        Press F1 for Help | F3 to Toggle Snap | Ctrl+/ for Commands
      </div>

      {/* M22 Integration - File Operations & Settings Dialogs */}
      <M22Integration
        onCommandExecuted={(command, data) => {
          console.log(`M22 Command: ${command}`, data);
          // Handle M22 dialog actions here
        }}
      />

      {/* M23 Integration - Sketch Editor Components */}
      <M23Integration
        isDarkMode={false}
        onSketchSaved={(sketchData) => {
          console.log('M23 Sketch Saved:', sketchData);
          // Handle sketch save here
        }}
      />

      {/* M24 Integration - Assembly & Bend Components */}
      <M24Integration
        isDarkMode={false}
        onOperationComplete={(data) => {
          console.log('M24 Operation Complete:', data);
          // Handle operation complete here
        }}
      />

      {/* M25 Integration - Manufacturing & Analysis Components */}
      <M25Integration
        isDarkMode={false}
      />
    </div>
  );
};

export default BendCoreLanding;
