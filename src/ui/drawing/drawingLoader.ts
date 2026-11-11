// Starts bendcore_drawing_wasm in a Web Worker and loads its font (docs/ui/DRAWING_UI.md,
// docs/ui/APP_SHELL.md).
//
// The worker imports the Emscripten glue (bendcore_drawing.mjs) from the URL where the build
// output is served and gives the TrueType font of the same directory to the kernel (text metrics,
// PDF embedding). The page loads the same font as a FontFace, so the sheet on screen uses the
// same glyphs as the PDF. The page talks to the JS API (bendcore_drawing_api.mjs) through
// WasmClient: every call returns a Promise.

import type { DrawingKernel } from '../../wasm/bendcore_drawing_api.mjs';
import { startWasmWorker } from '../worker/startWorker';
import type { WasmClient } from '../worker/wasmClient';

export const FONT_FILE = 'DejaVuSans.ttf';
/** Family name the SVG writer of the kernel uses for text. */
export const FONT_FAMILY = 'DejaVu Sans';

/** Directory URL (with trailing slash) that serves bendcore_drawing.mjs, .wasm and the font. */
export function defaultDrawingBaseUrl(): string {
  return new URL('drawing-wasm/', document.baseURI).href;
}

let fontLoaded: Promise<void> | null = null;

/** Adds the kernel font to the page once (FontFace); later calls share the first load. */
export function loadPageFont(baseUrl: string): Promise<void> {
  if (typeof FontFace === 'undefined' || !document.fonts) return Promise.resolve();
  if (!fontLoaded) {
    const url = new URL(FONT_FILE, baseUrl).href;
    const loading = (async () => {
      const response = await fetch(url);
      if (!response.ok) throw new Error(`font ${url}: HTTP ${response.status}`);
      const face = new FontFace(FONT_FAMILY, await response.arrayBuffer());
      await face.load();
      document.fonts.add(face);
    })();
    loading.catch(() => {
      fontLoaded = null;
    });
    fontLoaded = loading;
  }
  return fontLoaded;
}

export async function loadDrawingFrom(baseUrl: string): Promise<WasmClient<DrawingKernel>> {
  const [client] = await Promise.all([startWasmWorker<DrawingKernel>('drawing', baseUrl), loadPageFont(baseUrl)]);
  return client;
}
