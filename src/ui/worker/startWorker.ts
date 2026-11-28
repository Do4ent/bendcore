/**
 * Starts a WASM worker in the browser (docs/ui/APP_SHELL.md). The page configurations bundle
 * src/ui/worker/wasmWorker.ts as wasm-worker.js next to the page; the module build output is
 * served under `baseUrl`.
 */

import type { ModuleKind } from './protocol';
import { WasmClient } from './wasmClient';
import type { WorkerLike } from './wasmClient';

export const WORKER_SCRIPT = 'wasm-worker.js';

export function startWasmWorker<Root>(module: ModuleKind, baseUrl: string): Promise<WasmClient<Root>> {
  const worker = new Worker(new URL(WORKER_SCRIPT, document.baseURI).href, { name: `bendcore-${module}` });
  return WasmClient.start<Root>(worker as unknown as WorkerLike, module, baseUrl);
}
