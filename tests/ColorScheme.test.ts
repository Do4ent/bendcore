/**
 * ColorScheme Tests
 * Validates color definitions, hex values, and utility functions
 */

import {
  colorScheme,
  getColorHex,
  getCategoryByUsage,
  listAllCategories,
} from '../src/ui/ColorScheme';

describe('ColorScheme', () => {
  describe('Color Definitions', () => {
    test('should have 11 color categories defined', () => {
      const categories = Object.keys(colorScheme);
      expect(categories.length).toBe(11);
    });

    test('should have all required color categories', () => {
      const required = [
        'tube',
        'create',
        'delete',
        'import',
        'links',
        'edit',
        'update',
        'freeze',
        'auxiliary',
        'axes_x',
        'axes_y',
        'axes_z',
      ];
      required.forEach((cat) => {
        expect(colorScheme[cat]).toBeDefined();
      });
    });

    test('tube color should be correct hex value', () => {
      expect(colorScheme.tube.hex).toBe('#1F6FC5');
    });

    test('create color should be correct hex value', () => {
      expect(colorScheme.create.hex).toBe('#2E9E5B');
    });

    test('delete color should be correct hex value', () => {
      expect(colorScheme.delete.hex).toBe('#D93A3A');
    });

    test('import color should be correct hex value', () => {
      expect(colorScheme.import.hex).toBe('#0E97A3');
    });

    test('links color should be correct hex value', () => {
      expect(colorScheme.links.hex).toBe('#8A4FD8');
    });

    test('edit color should be correct hex value', () => {
      expect(colorScheme.edit.hex).toBe('#E8820C');
    });

    test('update color should be correct hex value', () => {
      expect(colorScheme.update.hex).toBe('#F5B517');
    });

    test('freeze color should be correct hex value', () => {
      expect(colorScheme.freeze.hex).toBe('#2D9CDB');
    });

    test('auxiliary color should be correct hex value', () => {
      expect(colorScheme.auxiliary.hex).toBe('#C9920A');
    });

    test('axes_x should be red', () => {
      expect(colorScheme.axes_x.hex).toBe('#FF0000');
    });

    test('axes_y should be green', () => {
      expect(colorScheme.axes_y.hex).toBe('#00FF00');
    });

    test('axes_z should be blue', () => {
      expect(colorScheme.axes_z.hex).toBe('#0000FF');
    });
  });

  describe('Hex Validation', () => {
    test('all colors should have valid hex format', () => {
      const hexRegex = /^#[0-9A-F]{6}$/i;
      Object.values(colorScheme).forEach((cat) => {
        expect(cat.hex).toMatch(hexRegex);
      });
    });

    test('hex values should be uppercase', () => {
      Object.values(colorScheme).forEach((cat) => {
        const hex = cat.hex.slice(1);
        expect(hex).toMatch(/^[0-9A-F]{6}$/);
      });
    });
  });

  describe('Color Metadata', () => {
    test('all colors should have name property', () => {
      Object.values(colorScheme).forEach((cat) => {
        expect(cat.name).toBeDefined();
        expect(cat.name.length).toBeGreaterThan(0);
      });
    });

    test('all colors should have usage property', () => {
      Object.values(colorScheme).forEach((cat) => {
        expect(cat.usage).toBeDefined();
        expect(cat.usage.length).toBeGreaterThan(0);
      });
    });

    test('all colors should have examples property', () => {
      Object.values(colorScheme).forEach((cat) => {
        expect(cat.examples).toBeDefined();
        expect(Array.isArray(cat.examples)).toBe(true);
        expect(cat.examples.length).toBeGreaterThan(0);
      });
    });

    test('tube category should have correct examples', () => {
      expect(colorScheme.tube.examples).toContain('New Tube');
    });

    test('create category should have correct examples', () => {
      expect(colorScheme.create.examples).toContain('New Tube');
    });
  });

  describe('getColorHex()', () => {
    test('should return correct hex for valid category', () => {
      expect(getColorHex('tube')).toBe('#1F6FC5');
      expect(getColorHex('create')).toBe('#2E9E5B');
      expect(getColorHex('delete')).toBe('#D93A3A');
    });

    test('should return neutral color for invalid category', () => {
      expect(getColorHex('invalid')).toBe('#757575');
      expect(getColorHex('nonexistent')).toBe('#757575');
    });

    test('should handle empty string', () => {
      expect(getColorHex('')).toBe('#757575');
    });

    test('should be case-sensitive for category names', () => {
      expect(getColorHex('Tube')).toBe('#757575'); // Should not match
      expect(getColorHex('TUBE')).toBe('#757575'); // Should not match
    });
  });

  describe('getCategoryByUsage()', () => {
    test('should find category by usage keyword', () => {
      const cat = getCategoryByUsage('tube');
      expect(cat).toBeDefined();
      expect(cat?.name).toBe('Tube');
    });

    test('should find category by partial usage match', () => {
      const cat = getCategoryByUsage('creation');
      expect(cat).toBeDefined();
      expect(cat?.name).toBe('Create');
    });

    test('should be case-insensitive', () => {
      const cat1 = getCategoryByUsage('tube');
      const cat2 = getCategoryByUsage('TUBE');
      const cat3 = getCategoryByUsage('Tube');
      expect(cat1?.name).toBe(cat2?.name);
      expect(cat2?.name).toBe(cat3?.name);
    });

    test('should return undefined for non-existent usage', () => {
      const cat = getCategoryByUsage('nonexistent');
      expect(cat).toBeUndefined();
    });

    test('should find delete category by error keyword', () => {
      const cat = getCategoryByUsage('error');
      expect(cat?.name).toBe('Delete');
    });
  });

  describe('listAllCategories()', () => {
    test('should return array of all categories', () => {
      const categories = listAllCategories();
      expect(Array.isArray(categories)).toBe(true);
      expect(categories.length).toBe(11);
    });

    test('should return all unique categories', () => {
      const categories = listAllCategories();
      const uniqueNames = new Set(categories.map((c) => c.name));
      expect(uniqueNames.size).toBe(11);
    });

    test('returned categories should match schema', () => {
      const categories = listAllCategories();
      categories.forEach((cat) => {
        expect(cat.name).toBeDefined();
        expect(cat.hex).toBeDefined();
        expect(cat.usage).toBeDefined();
        expect(cat.examples).toBeDefined();
        expect(Array.isArray(cat.examples)).toBe(true);
      });
    });

    test('should include all required categories', () => {
      const categories = listAllCategories();
      const names = categories.map((c) => c.name);
      expect(names).toContain('Tube');
      expect(names).toContain('Create');
      expect(names).toContain('Delete');
      expect(names).toContain('X Axis');
      expect(names).toContain('Y Axis');
      expect(names).toContain('Z Axis');
    });
  });
});
