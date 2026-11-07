// Loads bendcore_kernel_wasm in the browser (docs/ui/KERNEL_VIEWPORT.md).
//
// The Emscripten glue (bendcore_kernel.mjs) is imported at run time from the URL where the
// build output is served, not bundled: it locates bendcore_kernel.wasm next to itself through
// import.meta.url. The JS API (bendcore_kernel_api.mjs) is bundled from src/wasm.

import { loadKernel } from '../../wasm/bendcore_kernel_api.mjs';
import type { Kernel, KernelModuleFactory } from '../../wasm/bendcore_kernel_api.mjs';

/** Directory URL (with trailing slash) that serves bendcore_kernel.mjs and bendcore_kernel.wasm. */
export function defaultKernelBaseUrl(): string {
  return new URL('kernel/', document.baseURI).href;
}

export async function loadKernelFrom(baseUrl: string): Promise<Kernel> {
  const glueUrl = new URL('bendcore_kernel.mjs', baseUrl).href;
  const glue = (await import(/* webpackIgnore: true */ glueUrl)) as { default: KernelModuleFactory };
  return loadKernel(glue.default);
}
