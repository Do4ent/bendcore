/**
 * @jest-environment node
 */
import fs from 'fs';
import path from 'path';
import { en, ru } from '../../app/i18n';
import { paramCaption } from '../labels';

/** Parameters the kernel declares for GearTeeth (src/modeling/document/advanced_features.cpp). */
function gearParameters(): string[] {
  const src = fs.readFileSync(path.resolve(__dirname, '../../../modeling/document/advanced_features.cpp'), 'utf8');
  const cls = src.slice(src.indexOf('class GearTeethFeature'));
  const block = cls.slice(cls.indexOf('parameterSpecs()'), cls.indexOf('referenceSlots()'));
  return [...block.matchAll(/\{"(\w+)",\s*"[^"]*"\}/g)].map((m) => m[1]);
}

describe('GearTeeth parameter captions', () => {
  const names = gearParameters();
  const tr = (dict: Record<string, string>) => ((key: string) => dict[key] ?? key) as never;

  it('reads the eight kernel parameters', () => {
    expect(names).toEqual(['module', 'teeth', 'shift', 'width', 'helix', 'offset', 'angle', 'chamfer']);
  });

  it.each([
    ['ru', ru],
    ['en', en],
  ] as const)('every parameter has a caption in %s that is not the kernel name', (_l, dict) => {
    for (const n of names) {
      const c = paramCaption(tr(dict as Record<string, string>), 'GearTeeth', n);
      expect(c).not.toBe(n);
      expect(c.length).toBeGreaterThan(n.length);
    }
  });

  it('uses the type-specific caption where the name means something else', () => {
    const t = tr(ru as Record<string, string>);
    expect(paramCaption(t, 'GearTeeth', 'width')).toContain('венца');
    expect(paramCaption(t, 'Box', 'width')).toBe('Ширина, мм');
    expect(paramCaption(t, 'Unknown', 'nothing')).toBe('nothing');
  });
});
