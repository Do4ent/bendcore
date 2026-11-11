// Loads bendcore_drawing_wasm and its font in the browser (docs/ui/DRAWING_UI.md).
//
// The Emscripten glue (bendcore_drawing.mjs) is imported at run time from the URL where the
// build output is served, not bundled: it finds bendcore_drawing.wasm next to itself through
// import.meta.url. The JS API (bendcore_drawing_api.mjs) is bundled from src/wasm. The TrueType
// font is fetched from the same directory: its bytes go to the kernel (text metrics, PDF
// embedding) and to the page as a FontFace, so the sheet on screen uses the same glyphs as the PDF.

import { loadDrawingKernel } from '../../wasm/bendcore_drawing_api.mjs';
import type { DrawingKernel, DrawingModuleFactory } from '../../wasm/bendcore_drawing_api.mjs';

export const FONT_FILE = 'DejaVuSans.ttf';
/** Family name the SVG writer of the kernel uses for text. */
export const FONT_FAMILY = 'DejaVu Sans';

/** Directory URL (with trailing slash) that serves bendcore_drawing.mjs, .wasm and the font. */
export function defaultDrawingBaseUrl(): string {
  return new URL('drawing-wasm/', document.baseURI).href;
}

export async function loadDrawingFrom(baseUrl: string): Promise<DrawingKernel> {
  const glueUrl = new URL('bendcore_drawing.mjs', baseUrl).href;
  const fontUrl = new URL(FONT_FILE, baseUrl).href;
  const [glue, fontResponse] = await Promise.all([
    import(/* webpackIgnore: true */ glueUrl) as Promise<{ default: DrawingModuleFactory }>,
    fetch(fontUrl),
  ]);
  if (!fontResponse.ok) throw new Error(`font ${fontUrl}: HTTP ${fontResponse.status}`);
  const fontBytes = new Uint8Array(await fontResponse.arrayBuffer());
  const kernel = await loadDrawingKernel(glue.default);
  kernel.setFont(fontBytes, FONT_FILE);
  if (typeof FontFace !== 'undefined' && document.fonts) {
    const face = new FontFace(FONT_FAMILY, fontBytes.slice().buffer);
    await face.load();
    document.fonts.add(face);
  }
  return kernel;
}
