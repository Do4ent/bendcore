import React, { useState, useCallback } from 'react';
import {
  RibbonBar,
  TreePanel,
  PropertiesPanel,
  StatusBar,
  ContextMenu,
  type RibbonTab,
  type TreeNode,
  type PropertyField,
} from '../ui/components';
import './WorkspaceScreen.css';

interface WorkspaceScreenProps {
  projectId: string;
}

interface Coordinates {
  x: number;
  y: number;
  z: number;
}

export const WorkspaceScreen: React.FC<WorkspaceScreenProps> = ({ projectId }) => {
  // State
  const [activeRibbonTab, setActiveRibbonTab] = useState<RibbonTab>('file');
  const [selectedNodeId, setSelectedNodeId] = useState<string | null>(null);
  const [isDarkMode, setIsDarkMode] = useState(false);
  const [coordinates, setCoordinates] = useState<Coordinates>({ x: 0, y: 0, z: 0 });
  const [statusMessage, setStatusMessage] = useState('Project loaded');
  const [contextMenu, setContextMenu] = useState<{ x: number; y: number } | null>(null);
  const [contextMenuNodeId, setContextMenuNodeId] = useState<string | null>(null);

  // Tree nodes (sample data)
  const [treeNodes, setTreeNodes] = useState<TreeNode[]>([
    {
      id: 'tube-1',
      name: 'Tube_1',
      type: 'tube',
      icon: 'tube',
      visible: true,
      children: [
        {
          id: 'path-sketch-1',
          name: 'Path_Sketch',
          type: 'sketch',
          icon: 'sketch',
          visible: true,
          children: [],
        },
        {
          id: 'bend-1',
          name: 'Bend_1 (90°)',
          type: 'bend',
          icon: 'arc',
          visible: true,
          children: [],
        },
        {
          id: 'bend-2',
          name: 'Bend_2 (45°)',
          type: 'bend',
          icon: 'arc',
          visible: true,
          children: [],
        },
        {
          id: 'offset-bend',
          name: 'Offset_Bend',
          type: 'feature',
          icon: 'tube',
          visible: true,
          children: [],
        },
      ],
    },
    {
      id: 'tube-2',
      name: 'Tube_2',
      type: 'tube',
      icon: 'tube',
      visible: true,
      children: [
        {
          id: 'path-sketch-2',
          name: 'Path_Sketch',
          type: 'sketch',
          icon: 'sketch',
          visible: true,
          children: [],
        },
        {
          id: 'bend-3',
          name: 'Bend_1 (90°)',
          type: 'bend',
          icon: 'arc',
          visible: true,
          children: [],
        },
      ],
    },
    {
      id: 'sketch-1',
      name: 'Sketch_1',
      type: 'sketch',
      icon: 'sketch',
      visible: true,
      children: [],
    },
  ]);

  // Properties for selected node
  const [propertyFields, setPropertyFields] = useState<PropertyField[]>([
    {
      id: 'name',
      label: 'Name',
      type: 'text',
      value: 'Tube_1',
      section: 'Basic',
    },
    {
      id: 'type',
      label: 'Type',
      type: 'select',
      value: 'pipe',
      options: [
        { label: 'Pipe', value: 'pipe' },
        { label: 'Tube', value: 'tube' },
        { label: 'Wire', value: 'wire' },
      ],
      section: 'Basic',
    },
    {
      id: 'od',
      label: 'OD (mm)',
      type: 'number',
      value: 25.4,
      min: 1,
      max: 500,
      section: 'Dimensions',
    },
    {
      id: 'wall',
      label: 'Wall (mm)',
      type: 'number',
      value: 2.0,
      min: 0.1,
      max: 10,
      section: 'Dimensions',
    },
    {
      id: 'length',
      label: 'Length (mm)',
      type: 'number',
      value: 500.0,
      min: 10,
      max: 5000,
      section: 'Dimensions',
    },
    {
      id: 'material',
      label: 'Material',
      type: 'select',
      value: 'steel',
      options: [
        { label: 'Steel', value: 'steel' },
        { label: 'Aluminum', value: 'aluminum' },
        { label: 'Copper', value: 'copper' },
        { label: 'Stainless Steel', value: 'stainless' },
      ],
      section: 'Material',
    },
    {
      id: 'color',
      label: 'Color',
      type: 'color',
      value: '#c0c0c0',
      section: 'Appearance',
    },
  ]);

  // Handlers
  const handleRibbonCommand = useCallback((commandId: string) => {
    console.log('RibbonBar command:', commandId);

    switch (commandId) {
      case 'save':
        setStatusMessage('✅ Project saved');
        break;
      case 'new':
        setStatusMessage('Creating new project...');
        break;
      case 'undo':
        setStatusMessage('Undo last action');
        break;
      default:
        setStatusMessage(`Command: ${commandId}`);
    }
  }, []);

  const handleNodeSelect = useCallback((nodeId: string) => {
    setSelectedNodeId(nodeId);
    setStatusMessage(`Selected: ${nodeId}`);
    setContextMenu(null);
  }, []);

  const handleToggleVisibility = useCallback((nodeId: string) => {
    setTreeNodes((prevNodes) =>
      prevNodes.map((node) => toggleNodeVisibility(node, nodeId))
    );
  }, []);

  const toggleNodeVisibility = (node: TreeNode, targetId: string): TreeNode => {
    if (node.id === targetId) {
      return { ...node, visible: !node.visible };
    }
    if (node.children) {
      return {
        ...node,
        children: node.children.map((child) => toggleNodeVisibility(child, targetId)),
      };
    }
    return node;
  };

  const handleNodeDelete = useCallback((nodeId: string) => {
    setTreeNodes((prevNodes) => prevNodes.filter((node) => node.id !== nodeId));
    setStatusMessage(`Deleted: ${nodeId}`);
  }, []);

  const handlePropertyChange = useCallback((fieldId: string, value: any) => {
    setPropertyFields((prevFields) =>
      prevFields.map((field) =>
        field.id === fieldId ? { ...field, value } : field
      )
    );
  }, []);

  const handleContextMenu = useCallback((nodeId: string, event: React.MouseEvent) => {
    event.preventDefault();
    setContextMenuNodeId(nodeId);
    setContextMenu({ x: event.clientX, y: event.clientY });
  }, []);

  const contextMenuItems = contextMenuNodeId
    ? [
        {
          id: 'edit',
          label: 'Edit',
          icon: 'pencil',
          action: () => setStatusMessage(`Editing ${contextMenuNodeId}`),
        },
        {
          id: 'duplicate',
          label: 'Duplicate',
          icon: 'copy',
          action: () => setStatusMessage(`Duplicated ${contextMenuNodeId}`),
        },
        {
          id: 'delete',
          label: 'Delete',
          icon: 'trash',
          action: () => handleNodeDelete(contextMenuNodeId),
          divider: true,
        },
        {
          id: 'properties',
          label: 'Properties',
          icon: 'settings',
          action: () => setStatusMessage(`Properties of ${contextMenuNodeId}`),
        },
      ]
    : [];

  return (
    <div className={`workspace-screen ${isDarkMode ? 'dark' : 'light'}`}>
      {/* Ribbon Bar */}
      <RibbonBar
        activeTab={activeRibbonTab}
        onTabChange={setActiveRibbonTab}
        onCommand={handleRibbonCommand}
        isDarkMode={isDarkMode}
      />

      {/* Main Content Area */}
      <div className="workspace-main">
        {/* Left Panel: Tree */}
        <div className="workspace-left">
          <TreePanel
            nodes={treeNodes.map((node) => ({
              ...node,
              selected: node.id === selectedNodeId,
            }))}
            onSelect={handleNodeSelect}
            onToggleVisibility={handleToggleVisibility}
            onDelete={handleNodeDelete}
            onContextMenu={handleContextMenu}
          />
        </div>

        {/* Center: 3D Viewport */}
        <div className="workspace-viewport">
          <canvas id="3d-viewport" />
          <div className="viewport-overlay">
            <span>3D View (Click to focus)</span>
            <div className="viewport-controls">
              <button title="Fit All">⊡</button>
              <button title="Zoom In">+</button>
              <button title="Zoom Out">−</button>
              <button title="Home View">⌂</button>
            </div>
          </div>
        </div>

        {/* Right Panel: Properties */}
        <div className="workspace-right">
          <PropertiesPanel
            title={selectedNodeId || 'Properties'}
            fields={propertyFields}
            onPropertyChange={handlePropertyChange}
            onReset={() => setStatusMessage('Reset to defaults')}
          />
        </div>
      </div>

      {/* Status Bar */}
      <StatusBar
        message={statusMessage}
        coordinates={coordinates}
        mode="Design"
        progress={undefined}
      />

      {/* Context Menu */}
      {contextMenu && (
        <ContextMenu
          items={contextMenuItems}
          position={contextMenu}
          onClose={() => setContextMenu(null)}
        />
      )}
    </div>
  );
};
