// Starts bendcore_kernel_wasm in a Web Worker (docs/ui/KERNEL_VIEWPORT.md, docs/ui/APP_SHELL.md).
//
// The worker imports the Emscripten glue (bendcore_kernel.mjs) at run time from the URL where the
// build output is served; the glue finds bendcore_kernel.wasm next to itself. The page talks to
// the JS API (bendcore_kernel_api.mjs) through WasmClient: every call returns a Promise.

import type { Kernel } from '../../wasm/bendcore_kernel_api.mjs';
import { startWasmWorker } from '../worker/startWorker';
import type { WasmClient } from '../worker/wasmClient';

/** Directory URL (with trailing slash) that serves bendcore_kernel.mjs and bendcore_kernel.wasm. */
export function defaultKernelBaseUrl(): string {
  return new URL('kernel/', document.baseURI).href;
}

export function loadKernelFrom(baseUrl: string): Promise<WasmClient<Kernel>> {
  return startWasmWorker<Kernel>('kernel', baseUrl);
}
