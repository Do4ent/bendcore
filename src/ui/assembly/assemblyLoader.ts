// Starts bendcore_assembly_wasm in a Web Worker (docs/ui/ASSEMBLY_UI.md, docs/ui/APP_SHELL.md).
//
// The worker imports the Emscripten glue (bendcore_assembly.mjs) at run time from the URL where
// the build output is served; the page talks to the JS API (bendcore_assembly_api.mjs) through
// WasmClient: every call returns a Promise.

import type { AssemblyKernel } from '../../wasm/bendcore_assembly_api.mjs';
import { startWasmWorker } from '../worker/startWorker';
import type { WasmClient } from '../worker/wasmClient';

/** Directory URL (with trailing slash) that serves bendcore_assembly.mjs and .wasm. */
export function defaultAssemblyBaseUrl(): string {
  return new URL('assembly-wasm/', document.baseURI).href;
}

export function loadAssemblyFrom(baseUrl: string): Promise<WasmClient<AssemblyKernel>> {
  return startWasmWorker<AssemblyKernel>('assembly', baseUrl);
}
