// Starts bendcore_modeling_wasm in a Web Worker (docs/ui/PART_MODELING_UI.md, docs/ui/APP_SHELL.md).
//
// The worker imports the Emscripten glue (bendcore_modeling.mjs) at run time from the URL where
// the build output is served; the glue finds bendcore_modeling.wasm next to itself. The page
// talks to the JS API (bendcore_modeling_api.mjs) through WasmClient: every call returns a Promise.

import type { ModelingKernel } from '../../wasm/bendcore_modeling_api.mjs';
import { startWasmWorker } from '../worker/startWorker';
import type { WasmClient } from '../worker/wasmClient';

/** Directory URL (with trailing slash) that serves bendcore_modeling.mjs and .wasm. */
export function defaultModelingBaseUrl(): string {
  return new URL('modeling/', document.baseURI).href;
}

export function loadModelingFrom(baseUrl: string): Promise<WasmClient<ModelingKernel>> {
  return startWasmWorker<ModelingKernel>('modeling', baseUrl);
}
