// Print document of a sheet (src/ui/drawing/printSheet.ts) and the outline / group pick rules of
// the local section and chain/baseline tools (sheetGeometry.ts). The page size must equal the
// sheet format (ГОСТ 2.301: А3 420×297, А4 210×297) so the sheet prints at 1:1.

import { pageSize, printDocument } from '../printSheet';
import { addOutlinePoint, addPick, canDimensionGroup, emptyOutline, emptyPicks, outlinePath } from '../sheetGeometry';

const SVG =
  '<?xml version="1.0" encoding="UTF-8"?>\n<svg xmlns="http://www.w3.org/2000/svg" width="420mm" height="297mm" viewBox="0 0 420 297"><line x1="0" y1="0" x2="420" y2="297"/></svg>';

describe('printDocument', () => {
  it('sets the page and the SVG to the sheet format', () => {
    const html = printDocument(SVG, 420, 297, 'РД.10.001', { family: 'DejaVu Sans', url: 'http://x/drawing-wasm/DejaVuSans.ttf' });
    expect(pageSize(html)).toEqual({ width: 420, height: 297 });
    expect(html).toContain('margin: 0');
    expect(html).not.toContain('<?xml');
    // Exactly one width/height on the root element, in mm; the viewBox stays (1 unit = 1 mm).
    const root = /<svg[^>]*>/.exec(html)![0];
    expect(root.match(/width=/g)).toHaveLength(1);
    expect(root).toContain('width="420mm"');
    expect(root).toContain('height="297mm"');
    expect(root).toContain('viewBox="0 0 420 297"');
    expect(html).toContain("@font-face { font-family: 'DejaVu Sans'; src: url('http://x/drawing-wasm/DejaVuSans.ttf'); }");
    expect(html).toContain('<title>РД.10.001</title>');
  });

  it('portrait A4 and fractional sizes', () => {
    expect(pageSize(printDocument(SVG, 210, 297))).toEqual({ width: 210, height: 297 });
    expect(pageSize(printDocument(SVG, 841.0004, 594))).toEqual({ width: 841, height: 594 });
    expect(() => printDocument(SVG, 0, 297)).toThrow();
    expect(pageSize('<html></html>')).toBeNull();
  });

  it('escapes the title', () => {
    expect(printDocument(SVG, 420, 297, '<a&b>')).toContain('<title>&lt;a&amp;b&gt;</title>');
  });
});

describe('local section outline', () => {
  it('adds points, ignores double clicks and closes on the first point', () => {
    let o = addOutlinePoint(emptyOutline, [10, 10]);
    o = addOutlinePoint(o, [10.2, 10.1]); // double click
    expect(o.points).toHaveLength(1);
    o = addOutlinePoint(o, [30, 10]);
    o = addOutlinePoint(o, [30, 30]);
    expect(o.closed).toBe(false);
    expect(outlinePath(o)).toBe('M10 10L30 10L30 30');
    o = addOutlinePoint(o, [11, 11]); // near the first point: closes
    expect(o.closed).toBe(true);
    expect(o.points).toHaveLength(3);
    expect(outlinePath(o).endsWith('Z')).toBe(true);
    expect(addOutlinePoint(o, [50, 50]).points).toEqual([[50, 50]]); // a new outline after closing
  });

  it('two points do not close', () => {
    let o = addOutlinePoint(emptyOutline, [0, 0]);
    o = addOutlinePoint(o, [10, 0]);
    o = addOutlinePoint(o, [0.5, 0.5]);
    expect(o.closed).toBe(false);
    expect(o.points).toHaveLength(3);
  });
});

describe('chain and baseline picks', () => {
  it('collects edges of one view and is never ready by a click', () => {
    let s = addPick(emptyPicks, 'chain', { view: 'F', index: 1, type: 'line' });
    s = addPick(s, 'chain', { view: 'F', index: 2, type: 'line' });
    s = addPick(s, 'chain', { view: 'F', index: 3, type: 'circle' });
    expect(s.picks).toHaveLength(3);
    expect(s.ready).toBe(false);
    expect(canDimensionGroup(s)).toBe(true);
    const other = addPick(s, 'baseline', { view: 'T', index: 1, type: 'line' });
    expect(other.picks).toHaveLength(1);
    expect(other.message).toBe('drawing.pick.oneView');
    expect(canDimensionGroup(other)).toBe(false);
  });
});
