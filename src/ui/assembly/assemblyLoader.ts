// Loads bendcore_assembly_wasm in the browser (docs/ui/ASSEMBLY_UI.md).
//
// The Emscripten glue (bendcore_assembly.mjs) is imported at run time from the URL where the
// build output is served, not bundled: it finds bendcore_assembly.wasm next to itself through
// import.meta.url. The JS API (bendcore_assembly_api.mjs) is bundled from src/wasm.

import { loadAssembly } from '../../wasm/bendcore_assembly_api.mjs';
import type { AssemblyKernel, AssemblyModuleFactory } from '../../wasm/bendcore_assembly_api.mjs';

/** Directory URL (with trailing slash) that serves bendcore_assembly.mjs and .wasm. */
export function defaultAssemblyBaseUrl(): string {
  return new URL('assembly-wasm/', document.baseURI).href;
}

export async function loadAssemblyFrom(baseUrl: string): Promise<AssemblyKernel> {
  const glueUrl = new URL('bendcore_assembly.mjs', baseUrl).href;
  const glue = (await import(/* webpackIgnore: true */ glueUrl)) as { default: AssemblyModuleFactory };
  return loadAssembly(glue.default);
}
