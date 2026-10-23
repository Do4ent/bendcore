/**
 * Preload Script
 * Expose safe IPC interface to renderer process
 */
import { contextBridge, ipcRenderer } from 'electron';
// Expose IPC API to window object
contextBridge.exposeInMainWorld('electron', {
    ipc: {
        invoke: (channel, data) => {
            // Whitelist allowed channels
            const allowedChannels = ['bendcore:command'];
            if (!allowedChannels.includes(channel)) {
                throw new Error(`IPC channel not allowed: ${channel}`);
            }
            return ipcRenderer.invoke(channel, data);
        },
        send: (channel, data) => {
            const allowedChannels = ['bendcore:event'];
            if (!allowedChannels.includes(channel)) {
                throw new Error(`IPC channel not allowed: ${channel}`);
            }
            ipcRenderer.send(channel, data);
        },
        on: (channel, callback) => {
            const allowedChannels = ['bendcore:response'];
            if (!allowedChannels.includes(channel)) {
                throw new Error(`IPC channel not allowed: ${channel}`);
            }
            ipcRenderer.on(channel, (event, data) => callback(data));
        },
    },
});
//# sourceMappingURL=preload.js.map