/**
 * BendCore UI Entry Point
 * React root component for CommandManager Ribbon UI
 */

import React from 'react';
import ReactDOM from 'react-dom/client';
import RibbonBar from './RibbonBar';
import './styles.css';
export { default as TubeDialog } from './TubeDialog';
export type { TubeConfig } from './TubeDialog';

interface AppProps {
  onCommand?: (commandId: string, args?: Record<string, any>) => void;
}

const App: React.FC<AppProps> = ({ onCommand }) => {
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

  return (
    <div className="flex flex-col h-screen bg-gray-50">
      {/* Ribbon UI */}
      <RibbonBar onCommand={handleCommand} />

      {/* Main Work Area (placeholder) */}
      <div className="flex-1 overflow-auto">
        <div className="p-4 text-gray-500">
          Work area (3D viewport to be integrated)
        </div>
      </div>
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
