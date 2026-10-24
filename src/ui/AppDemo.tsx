/**
 * BendCore UI Components Integration Demo
 * Demonstrates all 10 UI components working together
 *
 * Components:
 * 1. RibbonBar - Top toolbar with tabs and commands
 * 2. TreePanel - Feature tree with expand/collapse
 * 3. PropertiesPanel - Object properties editor
 * 4. StatusBar - Status information display
 * 5. ContextMenu - Right-click context menu
 * 6. Tooltip - Hover tooltips
 * 7. Icon - Icon component
 * 8. Badge - Status badges
 * 9. Spinner - Loading spinner
 * 10. Divider - Visual dividers
 */

import React, { useState, useCallback } from 'react';
import { RibbonBar } from './components/RibbonBar';
import { TreePanel } from './components/TreePanel';
import { PropertiesPanel } from './components/PropertiesPanel';
import { StatusBar } from './components/StatusBar';
import { ContextMenu } from './components/ContextMenu';
import { Tooltip } from './components/Tooltip';
import { Icon } from './components/Icon';
import { Badge } from './components/Badge';
import { Spinner } from './components/Spinner';
import { Divider } from './components/Divider';
import type { RibbonCommand, RibbonTab } from './components/RibbonBar';
import type { TreeNode } from './components/TreePanel';
import type { PropertyField, PropertyValue } from './components/PropertiesPanel';
import type { ContextMenuItem } from './components/ContextMenu';
import './styles.css';

// ============ Types ============

interface SelectedObject {
  id: string;
  type: string;
  name: string;
}

// ============ Demo Data ============

const DEMO_TREE_NODES: TreeNode[] = [
  {
    id: 'root-1',
    name: 'Assembly 1',
    type: 'tube',
    icon: 'tube',
    visible: true,
    expanded: true,
    children: [
      {
        id: 'tube-1',
        name: 'Tube_Main',
        type: 'tube',
        icon: 'tube',
        visible: true,
        expanded: true,
        children: [
          {
            id: 'bend-1',
            name: 'Bend_90',
            type: 'bend',
            icon: 'arc',
            visible: true,
          },
          {
            id: 'bend-2',
            name: 'Bend_45',
            type: 'bend',
            icon: 'arc',
            visible: true,
          },
        ],
      },
      {
        id: 'sketch-1',
        name: 'Sketch_Profile',
        type: 'sketch',
        icon: 'sketch',
        visible: true,
      },
    ],
  },
];

const DEMO_PROPERTIES: PropertyField[] = [
  {
    id: 'name',
    label: 'Name',
    type: 'text',
    value: 'Tube_Main',
    section: 'General',
  },
  {
    id: 'material',
    label: 'Material',
    type: 'select',
    value: 'steel',
    section: 'General',
    options: [
      { label: 'Steel', value: 'steel' },
      { label: 'Aluminum', value: 'aluminum' },
      { label: 'Stainless', value: 'stainless' },
    ],
  },
  {
    id: 'od',
    label: 'Outer Diameter (mm)',
    type: 'number',
    value: 12.7,
    section: 'Dimensions',
    min: 1,
    max: 1000,
  },
  {
    id: 'wt',
    label: 'Wall Thickness (mm)',
    type: 'number',
    value: 1.6,
    section: 'Dimensions',
    min: 0.1,
    max: 100,
  },
  {
    id: 'color',
    label: 'Color',
    type: 'color',
    value: '#0066cc',
    section: 'Appearance',
  },
  {
    id: 'visible',
    label: 'Visible',
    type: 'checkbox',
    value: true,
    section: 'Appearance',
  },
];

// ============ Component ============

export const AppDemo: React.FC = () => {
  // State
  const [activeTab, setActiveTab] = useState<string>('file');
  const [selectedObject, setSelectedObject] = useState<SelectedObject | null>(null);
  const [treeNodes, setTreeNodes] = useState<TreeNode[]>(DEMO_TREE_NODES);
  const [isLoading, setIsLoading] = useState(false);
  const [statusMessage, setStatusMessage] = useState('Ready');
  const [coordinates, setCoordinates] = useState({ x: 0, y: 0, z: 0 });
  const [contextMenu, setContextMenu] = useState<{
    visible: boolean;
    x: number;
    y: number;
    items: ContextMenuItem[];
  }>({
    visible: false,
    x: 0,
    y: 0,
    items: [],
  });
  const [isDarkMode] = useState(false);

  // Ribbon commands
  const ribbonTabs: RibbonTab[] = [
    {
      id: 'file',
      label: 'File',
      commands: [
        {
          id: 'new',
          label: 'New',
          icon: 'file',
          tooltip: 'Create new project',
          shortcut: 'Ctrl+N',
          action: () => handleCommand('new'),
        },
        {
          id: 'open',
          label: 'Open',
          icon: 'folder-open',
          tooltip: 'Open existing project',
          shortcut: 'Ctrl+O',
          action: () => handleCommand('open'),
        },
        {
          id: 'save',
          label: 'Save',
          icon: 'save',
          tooltip: 'Save project',
          shortcut: 'Ctrl+S',
          action: () => handleCommand('save'),
        },
      ],
    },
    {
      id: 'edit',
      label: 'Edit',
      commands: [
        {
          id: 'undo',
          label: 'Undo',
          icon: 'undo',
          tooltip: 'Undo last action',
          shortcut: 'Ctrl+Z',
          action: () => handleCommand('undo'),
        },
        {
          id: 'redo',
          label: 'Redo',
          icon: 'redo',
          tooltip: 'Redo last action',
          shortcut: 'Ctrl+Y',
          action: () => handleCommand('redo'),
        },
      ],
    },
    {
      id: 'view',
      label: 'View',
      commands: [
        {
          id: 'zoom-fit',
          label: 'Fit All',
          icon: 'fit',
          tooltip: 'Fit all objects in view',
          action: () => handleCommand('zoom-fit'),
        },
      ],
    },
  ];

  // Handlers
  const handleCommand = useCallback((commandId: string) => {
    setStatusMessage(`Command executed: ${commandId}`);
    console.log(`Command: ${commandId}`);

    // Simulate loading
    if (commandId === 'save') {
      setIsLoading(true);
      setTimeout(() => setIsLoading(false), 2000);
    }
  }, []);

  const handleViewportContextMenu = useCallback(
    (e: React.MouseEvent) => {
      e.preventDefault();
      const x = e.clientX;
      const y = e.clientY;

      const items: ContextMenuItem[] = [
        {
          id: 'new-tube',
          label: 'New Tube',
          icon: 'tube',
          action: () => handleCommand('new-tube'),
        },
        {
          id: 'new-sketch',
          label: 'New Sketch',
          icon: 'sketch',
          action: () => handleCommand('new-sketch'),
        },
        { id: 'divider-1', label: '', divider: true, action: () => {} },
        {
          id: 'paste',
          label: 'Paste',
          icon: 'paste',
          action: () => handleCommand('paste'),
        },
      ];

      setContextMenu({
        visible: true,
        x,
        y,
        items,
      });
    },
    [handleCommand]
  );

  const handleViewportMouseMove = useCallback((e: React.MouseEvent) => {
    const rect = e.currentTarget.getBoundingClientRect();
    const x = Number(((e.clientX - rect.left) / 10).toFixed(2));
    const y = Number(((rect.height - (e.clientY - rect.top)) / 10).toFixed(2));

    setCoordinates({ x, y, z: 0 });
  }, []);

  const handleSelectTreeNode = useCallback((nodeId: string) => {
    const node = findNode(DEMO_TREE_NODES, nodeId);
    if (node) {
      setSelectedObject({
        id: nodeId,
        type: node.type,
        name: node.name,
      });
    }
  }, []);

  const handlePropertyChange = useCallback((fieldId: string, value: PropertyValue) => {
    console.log(`Property change: ${fieldId} = ${value}`);
    setStatusMessage(`Updated: ${fieldId} = ${value}`);
  }, []);

  const handleToggleVisibility = useCallback((nodeId: string) => {
    setTreeNodes((prevNodes) => toggleNodeVisibility(prevNodes, nodeId));
  }, []);

  // Helper functions
  function findNode(nodes: TreeNode[], id: string): TreeNode | null {
    for (const node of nodes) {
      if (node.id === id) return node;
      if (node.children) {
        const found = findNode(node.children, id);
        if (found) return found;
      }
    }
    return null;
  }

  function toggleNodeVisibility(nodes: TreeNode[], id: string): TreeNode[] {
    return nodes.map((node) => {
      if (node.id === id) {
        return { ...node, visible: !node.visible };
      }
      if (node.children) {
        return { ...node, children: toggleNodeVisibility(node.children, id) };
      }
      return node;
    });
  }

  return (
    <div className={`flex flex-col h-screen ${isDarkMode ? 'bg-gray-900' : 'bg-gray-50'}`}>
      {/* Ribbon Bar */}
      <RibbonBar
        activeTab={activeTab}
        onTabChange={setActiveTab}
        onCommand={handleCommand}
        isDarkMode={isDarkMode}
        tabs={ribbonTabs}
      />

      {/* Main Content Area */}
      <div className="flex flex-1 overflow-hidden gap-0">
        {/* Left Panel - Tree */}
        <div className={`w-64 border-r ${isDarkMode ? 'border-gray-700' : 'border-gray-200'} overflow-auto`}>
          <TreePanel
            nodes={treeNodes}
            selectedNode={selectedObject?.id || null}
            onSelect={handleSelectTreeNode}
            onToggleVisibility={handleToggleVisibility}
            isDarkMode={isDarkMode}
          />
        </div>

        {/* Center - Viewport */}
        <div
          className={`flex-1 overflow-auto ${isDarkMode ? 'bg-gray-800' : 'bg-white'}`}
          onContextMenu={handleViewportContextMenu}
          onMouseMove={handleViewportMouseMove}
        >
          <div className={`p-8 text-center ${isDarkMode ? 'text-gray-400' : 'text-gray-500'}`}>
            <div className="mb-4">
              <Icon name="tube" size="lg" />
            </div>
            <p className="text-lg mb-4">3D Viewport</p>
            <div className="flex justify-center gap-2">
              <Badge variant="info">Right-click for menu</Badge>
              <Badge variant="default">Click to select</Badge>
            </div>
            <div className="mt-8 flex justify-center gap-4">
              {isLoading && <Spinner size="md" />}
              {isLoading && <span className={isDarkMode ? 'text-gray-400' : 'text-gray-500'}>Saving...</span>}
            </div>
          </div>
        </div>

        {/* Right Panel - Properties */}
        {selectedObject && (
          <div className={`w-80 border-l ${isDarkMode ? 'border-gray-700' : 'border-gray-200'} overflow-auto`}>
            <PropertiesPanel
              title={`Properties: ${selectedObject.name}`}
              fields={DEMO_PROPERTIES}
              onPropertyChange={handlePropertyChange}
              onReset={() => setStatusMessage('Properties reset')}
            />
          </div>
        )}
      </div>

      {/* Divider (example) */}
      <Divider orientation="horizontal" />

      {/* Status Bar */}
      <StatusBar
        message={statusMessage}
        coordinates={coordinates}
        mode={activeTab.toUpperCase()}
        progress={isLoading ? 50 : undefined}
        isLoading={isLoading}
      />

      {/* Context Menu */}
      {contextMenu.visible && (
        <ContextMenu
          items={contextMenu.items}
          position={{ x: contextMenu.x, y: contextMenu.y }}
          onClose={() => setContextMenu({ ...contextMenu, visible: false })}
        />
      )}

      {/* Tooltip Example */}
      <div className="fixed bottom-8 right-8">
        <Tooltip content="Click to toggle dark mode" position="top">
          <button className={`p-3 rounded-full ${isDarkMode ? 'bg-gray-700' : 'bg-gray-200'}`}>
            <Icon name="eye" size="md" />
          </button>
        </Tooltip>
      </div>
    </div>
  );
};

export default AppDemo;
