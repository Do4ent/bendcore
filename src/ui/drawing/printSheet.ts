// Printing a drawing sheet from the browser (docs/ui/DRAWING_UI.md, «Печать»): the kernel's SVG of
// the sheet (1 user unit = 1 mm) is placed in a print document whose page size is the sheet
// format — CSS `@page { size: W mm H mm; margin: 0 }` — and whose SVG is W mm × H mm, so the
// printer gets the sheet at 1:1 (ГОСТ 2.301 formats, ISO 5457). The browser's print dialog has
// to be set to «actual size» / 100 %; the page size itself comes from the document.

/** Millimetres with at most 3 decimals, dot as the separator (CSS lengths). */
function mm(v: number): string {
  return String(Math.round(v * 1000) / 1000);
}

/**
 * Print document of a sheet: `svg` — the SVG text of the kernel's writer (root element with a
 * viewBox in mm), widthMm × heightMm — the sheet format. The root element of the SVG gets the
 * physical size of the sheet; the page has no margins. `font` — the family and URL of the font the
 * SVG text uses (the page's FontFace is not visible in the print frame).
 */
export function printDocument(
  svg: string,
  widthMm: number,
  heightMm: number,
  title = 'bendcore',
  font: { family: string; url: string } | null = null,
): string {
  if (!(widthMm > 0) || !(heightMm > 0)) throw new Error('printDocument: the sheet size must be positive');
  const body = svg
    .replace(/^<\?xml[^>]*>\s*/, '')
    .replace(/<svg([^>]*?)\s(width|height)="[^"]*"/g, '<svg$1')
    .replace(/<svg([^>]*?)\s(width|height)="[^"]*"/g, '<svg$1')
    .replace(/<svg\b/, `<svg width="${mm(widthMm)}mm" height="${mm(heightMm)}mm"`);
  const safeTitle = title.replace(/[<&>]/g, (c) => ({ '<': '&lt;', '&': '&amp;', '>': '&gt;' })[c] as string);
  return (
    '<!doctype html><html><head><meta charset="utf-8">' +
    `<title>${safeTitle}</title>` +
    '<style>' +
    (font ? `@font-face { font-family: '${font.family.replace(/'/g, '')}'; src: url('${encodeURI(font.url).replace(/'/g, '%27')}'); }` : '') +
    `@page { size: ${mm(widthMm)}mm ${mm(heightMm)}mm; margin: 0; }` +
    'html, body { margin: 0; padding: 0; background: #fff; }' +
    `svg { display: block; width: ${mm(widthMm)}mm; height: ${mm(heightMm)}mm; }` +
    '</style></head><body>' +
    body +
    '</body></html>'
  );
}

/** Page size of a print document (the `@page` rule), mm; null when there is none. */
export function pageSize(html: string): { width: number; height: number } | null {
  const m = /@page\s*{\s*size:\s*([\d.]+)mm\s+([\d.]+)mm/.exec(html);
  return m ? { width: Number(m[1]), height: Number(m[2]) } : null;
}

declare global {
  interface Window {
    /** Test hook: when set, the print document goes here instead of the browser's print dialog. */
    bendcoreDrawingPrintHook?: (html: string) => void;
  }
}

/**
 * Prints the document through a hidden iframe (the page itself is not printed). The iframe is
 * removed after printing.
 */
export function printHtml(html: string): void {
  if (typeof window.bendcoreDrawingPrintHook === 'function') {
    window.bendcoreDrawingPrintHook(html);
    return;
  }
  const frame = document.createElement('iframe');
  frame.setAttribute('aria-hidden', 'true');
  frame.style.position = 'fixed';
  frame.style.width = '0';
  frame.style.height = '0';
  frame.style.border = '0';
  frame.style.right = '0';
  frame.style.bottom = '0';
  document.body.appendChild(frame);
  const doc = frame.contentDocument;
  const win = frame.contentWindow;
  if (!doc || !win) {
    frame.remove();
    throw new Error('print: no document in the print frame');
  }
  doc.open();
  doc.write(html);
  doc.close();
  const done = () => setTimeout(() => frame.remove(), 1000);
  win.addEventListener('afterprint', done, { once: true });
  // Fonts of the SVG text must be ready before printing.
  const ready = doc.fonts ? doc.fonts.ready : Promise.resolve();
  void ready.then(() => {
    win.focus();
    win.print();
    done();
  });
}
