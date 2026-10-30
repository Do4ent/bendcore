/**
 * Electron Main Process
 * Window management and IPC handlers for BendCore UI
 */

import { app, BrowserWindow, ipcMain } from 'electron';
import * as path from 'path';
import { CoreBridge, defaultCliPath } from './core_bridge';

let mainWindow: BrowserWindow | null = null;

// Ядро C++ отдельным процессом (bendcore_cli). Команды вида «раздел.действие» идут в ядро.
const core = new CoreBridge({
  executable: defaultCliPath(process.resourcesPath, path.join(__dirname, '..'), process.platform),
});

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      sandbox: true,
    },
  });

  const isDev = process.env.NODE_ENV === 'development';
  const startUrl = isDev
    ? 'http://localhost:3000'
    : `file://${path.join(__dirname, '../index.html')}`;

  mainWindow.loadURL(startUrl);

  if (isDev) {
    mainWindow.webContents.openDevTools();
  }

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

// IPC Handler: bendcore:command
ipcMain.handle('bendcore:command', async (event, { command, args }) => {
  console.log(`IPC Command: ${command}`, args);

  try {
    const result = command.includes('.')
      ? await core.call(command, args ?? {})
      : await handleCommand(command, args);
    return { status: 'ok', data: result };
  } catch (error) {
    console.error(`Command error: ${command}`, error);
    return { status: 'error', message: error instanceof Error ? error.message : String(error) };
  }
});

// Команды оболочки, ещё не перенесённые в ядро (заглушки документа).
async function handleCommand(command: string, args: any) {
  console.log(`Handling: ${command}`, args);

  switch (command) {
    case 'new-document':
      return { document: 'new_doc_001' };
    case 'new-sketch':
      return { sketch: 'sketch_001' };
    case 'new-tube':
      return { tube: 'tube_001' };
    // ... more commands
    default:
      throw new Error(`Unknown command: ${command}`);
  }
}

// App events
app.on('ready', createWindow);

app.on('will-quit', () => core.dispose());

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  if (mainWindow === null) {
    createWindow();
  }
});
