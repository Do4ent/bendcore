/**
 * Preload Script
 * Expose safe IPC interface to renderer process
 */

import { contextBridge, ipcRenderer } from 'electron';

// Expose IPC API to window object
contextBridge.exposeInMainWorld('electron', {
  ipc: {
    invoke: (channel: string, data: any) => {
      // Whitelist allowed channels
      const allowedChannels = ['bendcore:command'];

      if (!allowedChannels.includes(channel)) {
        throw new Error(`IPC channel not allowed: ${channel}`);
      }

      return ipcRenderer.invoke(channel, data);
    },
    send: (channel: string, data: any) => {
      const allowedChannels = ['bendcore:event'];

      if (!allowedChannels.includes(channel)) {
        throw new Error(`IPC channel not allowed: ${channel}`);
      }

      ipcRenderer.send(channel, data);
    },
    on: (channel: string, callback: (data: any) => void) => {
      const allowedChannels = ['bendcore:response'];

      if (!allowedChannels.includes(channel)) {
        throw new Error(`IPC channel not allowed: ${channel}`);
      }

      ipcRenderer.on(channel, (event, data) => callback(data));
    },
  },
});
