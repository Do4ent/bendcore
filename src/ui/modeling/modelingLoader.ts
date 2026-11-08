// Loads bendcore_modeling_wasm in the browser (docs/ui/PART_MODELING_UI.md).
//
// The Emscripten glue (bendcore_modeling.mjs) is imported at run time from the URL where the
// build output is served, not bundled: it finds bendcore_modeling.wasm next to itself through
// import.meta.url. The JS API (bendcore_modeling_api.mjs) is bundled from src/wasm.

import { loadModeling } from '../../wasm/bendcore_modeling_api.mjs';
import type { ModelingKernel, ModelingModuleFactory } from '../../wasm/bendcore_modeling_api.mjs';

/** Directory URL (with trailing slash) that serves bendcore_modeling.mjs and .wasm. */
export function defaultModelingBaseUrl(): string {
  return new URL('modeling/', document.baseURI).href;
}

export async function loadModelingFrom(baseUrl: string): Promise<ModelingKernel> {
  const glueUrl = new URL('bendcore_modeling.mjs', baseUrl).href;
  const glue = (await import(/* webpackIgnore: true */ glueUrl)) as { default: ModelingModuleFactory };
  return loadModeling(glue.default);
}
