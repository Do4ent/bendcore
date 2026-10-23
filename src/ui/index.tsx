/**
 * BendCore UI Entry Point
 * React root component for CommandManager Ribbon UI
 */

import React, { useState } from 'react';
import ReactDOM from 'react-dom/client';
import RibbonBar from './RibbonBar';
import ContextMenu, { ContextMenuAction } from './ContextMenu';
import PropertiesPanel, { SelectedObject } from './PropertiesPanel';
import StatusBar, { StatusBarState } from './StatusBar';
import { createTubePropertyGroup } from './PropertyGroups';
import './styles.css';
export { default as TubeDialog } from './TubeDialog';
export type { TubeConfig } from './TubeDialog';

interface AppProps {
  onCommand?: (commandId: string, args?: Record<string, any>) => void;
}

const App: React.FC<AppProps> = ({ onCommand }) => {
  // Context menu state
  const [contextMenuOpen, setContextMenuOpen] = useState(false);
  const [contextMenuPos, setContextMenuPos] = useState({ x: 0, y: 0 });
  const [contextMenuActions, setContextMenuActions] = useState<ContextMenuAction[]>([]);

  // Properties panel state
  const [selectedObject, setSelectedObject] = useState<SelectedObject | undefined>();
  const [propertiesPanelPinned, setPropertiesPanelPinned] = useState(false);

  // Status bar state
  const [statusState, setStatusState] = useState<StatusBarState>({
    stepHint: 'Ready',
    cursorX: 0,
    cursorY: 0,
    cursorZ: 0,
    activePlane: 'XY Plane',
    snapMode: false,
    snapType: 'grid',
    units: 'mm',
    standard: 'ISO',
    violations: { errors: 0, warnings: 0, info: 0 },
    isCalculating: false,
  });

  const handleCommand = async (commandId: string, args?: Record<string, any>) => {
    console.log(`Command: ${commandId}`, args);

    // Call IPC to C++ backend
    if (window.electron?.ipc) {
      try {
        const result = await window.electron.ipc.invoke('bendcore:command', {
          command: commandId,
          args: args || {},
        });
        console.log('Command result:', result);
      } catch (error) {
        console.error('Command error:', error);
      }
    }

    // Call callback if provided
    if (onCommand) {
      onCommand(commandId, args);
    }
  };

  const handleContextMenu = (e: React.MouseEvent) => {
    e.preventDefault();
    const x = e.clientX;
    const y = e.clientY;

    // Generate context menu based on what was clicked
    // For now, show empty space menu
    const actions: ContextMenuAction[] = [
      { id: 'new-tube', label: 'New Tube', icon: '➕', shortcut: 'Ctrl+T', action: () => handleCommand('new-tube') },
      { id: 'new-sketch', label: 'New Sketch', icon: '✏', shortcut: 'Ctrl+Shift+K', action: () => handleCommand('new-sketch') },
      { divider: true },
      { id: 'paste', label: 'Paste', icon: '📋', shortcut: 'Ctrl+V', disabled: true },
    ];

    setContextMenuPos({ x, y });
    setContextMenuActions(actions);
    setContextMenuOpen(true);
  };

  const handleViewportMouseMove = (e: React.MouseEvent) => {
    // Update cursor position in status bar
    const rect = e.currentTarget.getBoundingClientRect();
    const x = (e.clientX - rect.left) / 10;
    const y = (rect.height - (e.clientY - rect.top)) / 10;

    setStatusState((prev) => ({
      ...prev,
      cursorX: x,
      cursorY: y,
    }));
  };

  const handlePropertiesChange = (objectId: string, propertyName: string, value: any) => {
    console.log(`Property change: ${objectId}.${propertyName} = ${value}`);

    // Send to backend
    if (window.electron?.ipc) {
      window.electron.ipc.invoke('bendcore:set-property', {
        objectId,
        propertyName,
        value,
      }).catch((err) => console.error('Property update error:', err));
    }
  };

  const handleSelectObject = () => {
    // Example: select a tube
    setSelectedObject({
      id: 'tube-1',
      type: 'tube',
      name: 'Tube_1',
      groups: createTubePropertyGroup({
        name: 'Tube_1',
        od: 12.7,
        wt: 1.6,
        material: 'Steel',
      }),
    });
  };

  return (
    <div className="flex flex-col h-screen bg-gray-50">
      {/* Ribbon UI */}
      <RibbonBar onCommand={handleCommand} />

      {/* Main layout with properties panel */}
      <div className="flex flex-1 overflow-hidden">
        {/* Viewport area */}
        <div
          className="flex-1 overflow-auto bg-white"
          onContextMenu={handleContextMenu}
          onMouseMove={handleViewportMouseMove}
          onClick={handleSelectObject}
        >
          <div className="p-4 text-gray-500">
            3D Viewport (right-click for context menu, click to select)
          </div>
        </div>

        {/* Properties Panel */}
        {(selectedObject || propertiesPanelPinned) && (
          <PropertiesPanel
            selectedObject={selectedObject}
            width={350}
            onPropertyChange={handlePropertiesChange}
            onClose={() => !propertiesPanelPinned && setSelectedObject(undefined)}
            isPinned={propertiesPanelPinned}
            onPinChange={setPropertiesPanelPinned}
          />
        )}
      </div>

      {/* Context Menu */}
      <ContextMenu
        isOpen={contextMenuOpen}
        position={contextMenuPos}
        actions={contextMenuActions}
        onClose={() => setContextMenuOpen(false)}
        onAction={(actionId) => {
          console.log(`Context menu action: ${actionId}`);
          setContextMenuOpen(false);
        }}
      />

      {/* Status Bar */}
      <StatusBar
        state={statusState}
        onStatusClick={(section) => console.log(`Status section clicked: ${section}`)}
        onCommand={handleCommand}
      />
    </div>
  );
};

// Declare window.electron for IPC
declare global {
  interface Window {
    electron?: {
      ipc: {
        invoke: (channel: string, data: any) => Promise<any>;
        send: (channel: string, data: any) => void;
        on: (channel: string, callback: (data: any) => void) => void;
      };
    };
  }
}

// Initialize React root
const root = ReactDOM.createRoot(document.getElementById('root')!);
root.render(<App />);
