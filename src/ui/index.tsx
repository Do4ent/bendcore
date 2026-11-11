/**
 * Окно трубного приложения Electron (точка входа webpack.config.js).
 * Команды вида «раздел.действие» идут в ядро (bendcore_cli) через IPC главного процесса,
 * импорт STEP — в сервис bendcore_server (ImportDialog, tubeService).
 */

import React, { useState } from 'react';
import ReactDOM from 'react-dom/client';
import RibbonBar from './RibbonBar';
import ContextMenu, { ContextMenuAction } from './ContextMenu';
import PropertiesPanel, { SelectedObject } from './PropertiesPanel';
import StatusBar, { StatusBarState } from './StatusBar';
import { createTubePropertyGroup } from './PropertyGroups';
import Viewport3D from './Viewport3D';
import ImportDialog, { TubeCandidate } from './ImportDialog';
import type { ViewportMesh } from './tubeService';
import './styles.css';

export { default as TubeDialog } from './TubeDialog';
export type { TubeConfig } from './TubeDialog';
export { default as OffsetCalculatorTab } from './OffsetCalculatorTab';
export { OffsetTypeSelector } from './OffsetTypeSelector';
export type { OffsetType } from './OffsetTypeSelector';
export { OffsetResultsDisplay } from './OffsetResultsDisplay';
export { OffsetVisualization } from './OffsetVisualization';
export { ParallelOffsetCalculator } from './ParallelOffsetCalculator';
export { default as Combined3DOffsetCalculator } from './Combined3DOffsetCalculator';

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

  // Геометрия документа (сетки ядра) и импортированные трубы по id объекта сцены.
  const [documentMeshes, setDocumentMeshes] = useState<ViewportMesh[]>([]);
  const [importedTubes, setImportedTubes] = useState<Map<string, TubeCandidate>>(new Map());
  const [importOpen, setImportOpen] = useState(false);

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

    if (commandId === 'import-step') {
      setImportOpen(true);
      return;
    }

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
      { id: 'div-1', label: '', divider: true },
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

  const handleImport = (tubes: TubeCandidate[]) => {
    setImportOpen(false);
    if (tubes.length === 0) return;
    setImportedTubes((prev) => {
      const next = new Map(prev);
      tubes.forEach((t) => next.set(t.id, t));
      return next;
    });
    setDocumentMeshes((prev) => [...prev, ...tubes.map((t) => ({ id: t.id, name: t.name, mesh: t.mesh }))]);
    setStatusState((prev) => ({ ...prev, stepHint: `Импортировано труб: ${tubes.length}` }));
  };

  // Выбор объекта в сцене: свойства — по реальным параметрам распознанной трубы.
  const handleSelectObject = (objectId: string) => {
    const tube = importedTubes.get(objectId);
    if (!tube) return;
    setSelectedObject({
      id: tube.id,
      type: 'tube',
      name: tube.name,
      groups: createTubePropertyGroup({
        name: tube.name,
        od: tube.diameter,
        wt: tube.wall,
        material: tube.material ?? '',
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
          className="flex-1 overflow-hidden bg-white relative"
          onContextMenu={handleContextMenu}
          onMouseMove={handleViewportMouseMove}
        >
          <Viewport3D meshes={documentMeshes} onCommand={handleCommand} onObjectSelect={handleSelectObject} />
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

      {importOpen && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/30">
          <ImportDialog onImport={handleImport} onCancel={() => setImportOpen(false)} />
        </div>
      )}

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

// window.electron is declared in src/ui/core/coreClient.ts

const rootElement = document.getElementById('root');
if (rootElement) {
  ReactDOM.createRoot(rootElement).render(<App />);
}
