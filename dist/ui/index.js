import { jsx as _jsx, jsxs as _jsxs } from "react/jsx-runtime";
/**
 * BendCore UI Entry Point
 * React root component for CommandManager Ribbon UI
 */
import React from 'react';
import ReactDOM from 'react-dom/client';
import RibbonBar from './RibbonBar';
import './styles.css';
export { default as TubeDialog } from './TubeDialog';
const App = ({ onCommand }) => {
    const handleCommand = async (commandId, args) => {
        console.log(`Command: ${commandId}`, args);
        // Call IPC to C++ backend
        if (window.electron?.ipc) {
            try {
                const result = await window.electron.ipc.invoke('bendcore:command', {
                    command: commandId,
                    args: args || {},
                });
                console.log('Command result:', result);
            }
            catch (error) {
                console.error('Command error:', error);
            }
        }
        // Call callback if provided
        if (onCommand) {
            onCommand(commandId, args);
        }
    };
    return (_jsxs("div", { className: "flex flex-col h-screen bg-gray-50", children: [_jsx(RibbonBar, { onCommand: handleCommand }), _jsx("div", { className: "flex-1 overflow-auto", children: _jsx("div", { className: "p-4 text-gray-500", children: "Work area (3D viewport to be integrated)" }) })] }));
};
// Initialize React root
const root = ReactDOM.createRoot(document.getElementById('root'));
root.render(_jsx(App, {}));
//# sourceMappingURL=index.js.map