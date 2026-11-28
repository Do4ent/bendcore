/**
 * @jest-environment node
 */
// Icon registry (src/ui/icons/README.md): the Tabler icons are what the pinned package holds, the
// licence of the package is MIT, and every icon keeps the style: 24×24 grid, stroke only, no
// fills or colours of its own, drawing inside the grid.

import { execFileSync } from 'child_process';
import fs from 'fs';
import path from 'path';
import { CAD_ICONS } from '../cad';
import { FALLBACK_ICON, GENERAL_ICONS, ICONS, iconEntry, iconSvg } from '../index';
import { TABLER_ICONS, TABLER_VERSION } from '../tabler';

const root = path.resolve(__dirname, '../../../..');

describe('Tabler icons', () => {
  test('the package is pinned to an exact version with the MIT licence', () => {
    const pkg = JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8'));
    expect(pkg.devDependencies['@tabler/icons']).toBe(TABLER_VERSION);
    const lock = JSON.parse(fs.readFileSync(path.join(root, 'package-lock.json'), 'utf8'));
    expect(lock.packages['node_modules/@tabler/icons']).toMatchObject({ version: TABLER_VERSION, license: 'MIT' });
  });

  test('src/ui/icons/tabler.ts is what scripts/gen_tabler_icons.mjs makes of the installed package', () => {
    const out = execFileSync(process.execPath, [path.join(root, 'scripts/gen_tabler_icons.mjs'), '--check'], { encoding: 'utf8' });
    expect(out).toContain('matches the package');
  });

  test('every general action has its Tabler icon', () => {
    for (const [name, tabler] of Object.entries(GENERAL_ICONS)) {
      expect(TABLER_ICONS[tabler]).toBeDefined();
      expect(ICONS[name]).toMatchObject({ source: 'tabler', tabler });
    }
    for (const action of ['new', 'open', 'save', 'undo', 'redo', 'export', 'print', 'measure', 'search', 'settings']) {
      expect(ICONS[action].source).toBe('tabler');
    }
  });
});

describe('style of every icon', () => {
  const numbers = (d: string) => (d.match(/-?\d*\.?\d+/g) ?? []).map(Number);

  test.each(Object.keys(ICONS))('%s', (name) => {
    const body = ICONS[name].body;
    // Only outline shapes; no fill, colour, stroke width, transform or text of their own.
    const tags = [...body.matchAll(/<(\w+)/g)].map((m) => m[1]);
    expect(tags.length).toBeGreaterThan(0);
    for (const tag of tags) expect(['path', 'circle', 'ellipse', 'rect', 'line', 'polyline']).toContain(tag);
    expect(body).not.toMatch(/fill=|stroke=|stroke-width|transform|style=|<text|#[0-9a-f]{3}/i);
    // Circles and ellipses stay on the grid.
    for (const m of body.matchAll(/<circle cx="([\d.]+)" cy="([\d.]+)" r="([\d.]+)"/g)) {
      const [cx, cy, r] = m.slice(1).map(Number);
      expect(cx - r).toBeGreaterThanOrEqual(1);
      expect(cy - r).toBeGreaterThanOrEqual(1);
      expect(cx + r).toBeLessThanOrEqual(23);
      expect(cy + r).toBeLessThanOrEqual(23);
    }
    // Absolute moves start inside the grid.
    for (const m of body.matchAll(/d="M([\d.]+)[ ,]([\d.]+)/g)) {
      const [x, y] = [Number(m[1]), Number(m[2])];
      expect(x).toBeGreaterThanOrEqual(0);
      expect(x).toBeLessThanOrEqual(24);
      expect(y).toBeGreaterThanOrEqual(0);
      expect(y).toBeLessThanOrEqual(24);
    }
    expect(numbers(body).every((v) => Number.isFinite(v))).toBe(true);
  });

  test('the standalone SVG is a 24×24 outline icon in currentColor', () => {
    const svg = iconSvg('extrude', 16);
    expect(svg).toContain('viewBox="0 0 24 24"');
    expect(svg).toContain('width="16"');
    expect(svg).toContain('stroke="currentColor"');
    expect(svg).toContain('stroke-width="2"');
    expect(svg).toContain('stroke-linecap="round"');
    expect(svg).toContain('stroke-linejoin="round"');
    expect(svg).toContain('fill="none"');
  });
});

describe('registry', () => {
  test('CAD icons are drawn for bendcore, the sets the ribbon needs are complete', () => {
    const own = Object.values(ICONS).filter((i) => i.source === 'bendcore');
    expect(own.length).toBe(Object.keys(CAD_ICONS).length);
    const required = [
      // sketch
      'sketch-line', 'sketch-circle', 'sketch-arc', 'sketch-rectangle', 'sketch-spline', 'sketch-trim', 'sketch-extend',
      'sketch-offset', 'sketch-mirror', 'sketch-pattern', 'sketch-project', 'constraint-coincident', 'constraint-horizontal',
      'constraint-vertical', 'constraint-parallel', 'constraint-perpendicular', 'constraint-tangent', 'constraint-equal',
      'constraint-symmetric', 'constraint-fix', 'sketch-dimension',
      // features
      'extrude', 'cut', 'revolve', 'hole', 'hole-threaded', 'fillet', 'chamfer', 'shell', 'draft', 'rib', 'pattern-linear',
      'pattern-circular', 'mirror', 'sweep', 'loft', 'gear-rim', 'boolean', 'plane', 'axis', 'point', 'csys',
      'parameters-table', 'reference-mesh',
      // assembly
      'insert-part', 'insert-subassembly', 'mate-coincident', 'mate-concentric', 'mate-distance', 'mate-angle',
      'mate-parallel', 'mate-perpendicular', 'mate-tangent', 'mate-rigid', 'mate-gear', 'fastener', 'interference', 'bom',
      'explode', 'pattern-component', 'configurations',
      // drawing
      'view-base', 'view-projected', 'view-section', 'view-local-section', 'view-cross-section', 'view-detail',
      'view-auxiliary', 'dimension-linear', 'dimension-chain', 'dimension-baseline', 'dimension-arrange', 'roughness',
      'form-tolerance', 'datum', 'balloon', 'balloon-arrange', 'thread-symbol', 'update-from-model', 'export-pdf', 'export-dxf',
    ];
    expect(required.filter((n) => ICONS[n]?.source !== 'bendcore')).toEqual([]);
  });

  test('an unknown name gives the general feature icon', () => {
    expect(iconEntry('NewKernelFeature').name).toBe(FALLBACK_ICON);
    expect(ICONS[FALLBACK_ICON].source).toBe('bendcore');
  });

  test('no two icons draw the same thing', () => {
    const seen = new Map<string, string>();
    const twice: string[] = [];
    for (const [name, i] of Object.entries(ICONS)) {
      if (seen.has(i.body)) twice.push(`${seen.get(i.body)} = ${name}`);
      seen.set(i.body, name);
    }
    expect(twice).toEqual([]);
  });
});
