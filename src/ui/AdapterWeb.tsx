/**
 * BendCore Web Adapter
 * Provides browser-compatible mock IPC and file handling
 */

import React, { useState, useCallback, useEffect } from 'react';
import ReactDOM from 'react-dom/client';
import RibbonBar from './RibbonBar';
import Viewport3D from './Viewport3D';
import PropertiesPanel from './components/PropertiesPanel';
import StatusBar from './StatusBar';
import './styles.css';

// Mock IPC for web version
interface MockIPCMessage {
  command: string;
  args?: Record<string, any>;
}

interface MockIPCResult {
  success: boolean;
  data?: any;
  error?: string;
}

// Create mock window.electron interface
const createMockElectron = () => ({
  ipc: {
    invoke: async (channel: string, message: MockIPCMessage): Promise<MockIPCResult> => {
      console.log(`[Mock IPC] ${message.command}`, message.args);

      const { command, args } = message;

      // Simulate command responses
      switch (command) {
        case 'new-tube':
          return {
            success: true,
            data: {
              id: 'tube-' + Date.now(),
              name: args?.designation || 'Tube_1',
              path: [],
            },
          };

        case 'edit-tube':
          return { success: true, data: { updated: true } };

        case 'import-dwfx':
        case 'import-step':
          return {
            success: true,
            data: {
              tubes: [
                {
                  id: 'imported-' + Date.now(),
                  name: args?.filename || 'Imported_Tube',
                  path: [
                    { kind: 'straight', length: 100 },
                    { kind: 'bend', radius: 50, angle: 90 },
                    { kind: 'straight', length: 100 },
                  ],
                },
              ],
            },
          };

        case 'check-all':
          return {
            success: true,
            data: { violations: [] },
          };

        case 'check-gaps':
          return {
            success: true,
            data: {
              gaps: [
                { id: 'gap-1', severity: 'WARNING', message: 'Small gap detected' },
              ],
            },
          };

        case 'calculate-mass':
          return {
            success: true,
            data: {
              mass: 2.5,
              unit: 'kg',
              material: 'Steel 1020',
            },
          };

        case 'bend-data':
          return {
            success: true,
            data: {
              bends: [
                { id: 'bend-1', angle: 90, radius: 50 },
                { id: 'bend-2', angle: 45, radius: 30 },
              ],
            },
          };

        default:
          return {
            success: true,
            data: { message: `Command ${command} executed` },
          };
      }
    },
  },
});

interface SelectedTube {
  id: string;
  name: string;
  designation?: string;
  material?: string;
  path: any[];
}

interface PropertiesObject {
  selectedNode?: SelectedTube;
  properties?: Record<string, any>;
}

const WebApp: React.FC = () => {
  const [selectedTube, setSelectedTube] = useState<SelectedTube | null>(null);
  const [meshes, setMeshes] = useState<any[]>([]);
  const [statusMessage, setStatusMessage] = useState('Ready');
  const [documentTubes, setDocumentTubes] = useState<SelectedTube[]>([]);

  // Initialize mock Electron IPC
  useEffect(() => {
    if (!window.electron) {
      (window as any).electron = createMockElectron();
      console.log('[Web] Mock Electron IPC initialized');
    }
  }, []);

  const handleCommand = useCallback(
    async (commandId: string, args?: Record<string, any>) => {
      console.log(`[Web] Command: ${commandId}`, args);

      try {
        if (window.electron?.ipc) {
          const result = await window.electron.ipc.invoke('bendcore:command', {
            command: commandId,
            args: args || {},
          });

          const res = result as any as MockIPCResult;
          if (res.success) {
            if (commandId === 'import-step' || commandId === 'import-dwfx') {
              // Add imported tubes to document
              const tubes = res.data?.tubes || [];
              setDocumentTubes((prev) => [...prev, ...tubes]);
              setStatusMessage(`Imported ${tubes.length} tube(s)`);
            } else if (commandId === 'new-tube') {
              setStatusMessage('New tube created - fill in the wizard');
            } else {
              setStatusMessage(`${commandId} executed`);
            }
          } else {
            setStatusMessage(`Error: ${res.error}`);
          }
        }
      } catch (error) {
        console.error('Command error:', error);
        setStatusMessage(`Error: ${String(error)}`);
      }
    },
    []
  );

  return (
    <div className="flex flex-col h-screen w-screen bg-white">
      {/* Ribbon Toolbar */}
      <RibbonBar onCommand={handleCommand} />

      {/* Main Content Area - 3-Panel Layout */}
      <div className="flex flex-1 overflow-hidden">
        {/* Left: Feature Tree */}
        <div className="w-64 border-r border-gray-300 bg-gray-900 text-white p-4 overflow-y-auto">
          <h3 className="text-sm font-bold mb-4">Feature Tree</h3>
          <div className="space-y-2 text-sm">
            <div className="p-2 hover:bg-gray-800 cursor-pointer rounded">
              📁 Document
            </div>
            {documentTubes.map((tube) => (
              <div
                key={tube.id}
                className={`p-2 hover:bg-gray-800 cursor-pointer rounded ${
                  selectedTube?.id === tube.id ? 'bg-blue-600' : ''
                }`}
                onClick={() => setSelectedTube(tube)}
              >
                🔲 {tube.name}
              </div>
            ))}
          </div>
        </div>

        {/* Center: 3D Viewport */}
        <div className="flex-1 bg-white relative flex flex-col">
          <Viewport3D
            meshes={meshes}
            onCommand={handleCommand}
            onObjectSelect={(id) => {
              const tube = documentTubes.find((t) => t.id === id);
              if (tube) setSelectedTube(tube);
            }}
          />
          {documentTubes.length === 0 && (
            <div className="absolute inset-0 flex items-center justify-center bg-white/80">
              <div className="text-center">
                <p className="text-xl text-gray-600 mb-4">No geometry in document</p>
                <p className="text-sm text-gray-500 mb-6">
                  Click "New Tube" to create or "Import" to load STEP/DWF
                </p>
                <button
                  onClick={() => handleCommand('new-tube')}
                  className="px-6 py-2 bg-blue-600 text-white rounded hover:bg-blue-700"
                >
                  Create New Tube
                </button>
              </div>
            </div>
          )}
        </div>

        {/* Right: Properties Panel */}
        {selectedTube && (
          <div className="w-80 border-l border-gray-300 bg-gray-50 p-4 overflow-y-auto">
            <h3 className="text-sm font-bold mb-4">Properties</h3>
            <div className="space-y-3 text-sm">
              <div>
                <label className="text-gray-600">Name</label>
                <p className="font-medium">{selectedTube.name}</p>
              </div>
              <div>
                <label className="text-gray-600">Designation</label>
                <p className="font-medium">{selectedTube.designation || '—'}</p>
              </div>
              <div>
                <label className="text-gray-600">Material</label>
                <p className="font-medium">{selectedTube.material || '—'}</p>
              </div>
              <div>
                <label className="text-gray-600">Path Segments</label>
                <p className="font-medium">{selectedTube.path?.length || 0}</p>
              </div>
              <button
                onClick={() => handleCommand('edit-tube')}
                className="w-full mt-4 px-3 py-2 bg-blue-600 text-white rounded text-xs font-medium hover:bg-blue-700"
              >
                Edit Properties
              </button>
            </div>
          </div>
        )}
      </div>

      {/* Status Bar */}
      <div className="h-8 border-t border-gray-300 bg-gray-100 px-4 flex items-center text-xs text-gray-700">
        <span>{statusMessage}</span>
        <span className="ml-auto">
          Ready | Grid: 0.5mm | Snap: ON
        </span>
      </div>
    </div>
  );
};

// Mount app
const root = document.getElementById('root');
if (root) {
  const reactRoot = ReactDOM.createRoot(root);
  reactRoot.render(<WebApp />);
  console.log('[Web] BendCore Web App mounted');
}

export default WebApp;
