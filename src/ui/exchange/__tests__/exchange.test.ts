/**
 * @jest-environment node
 */
// Logic of the export dialog and import inputs (docs/ui/DATA_EXCHANGE.md): formats, file names,
// option validation against the ranges of src/wasm/exchange_common.hpp, density parsing,
// inertia read-out, and PartExchange (one document per part JSON, calls in order).

import {
  DEFAULT_EXPORT_FORM,
  EXPORT_FORMATS,
  baseNameOf,
  exportFileName,
  exportOptions,
  importFormatOf,
  inertiaDiagonal,
  inertiaProducts,
  parseDensity,
} from '../exchange';
import type { ExportForm, MassProperties } from '../exchange';
import { PartExchange } from '../partExchange';

describe('formats and names', () => {
  test('every export format has its extension', () => {
    expect(EXPORT_FORMATS.map((f) => exportFileName('Bracket.bcpart.json', f))).toEqual([
      'Bracket.step',
      'Bracket.igs',
      'Bracket.stl',
      'Bracket.3mf',
    ]);
  });

  test('base names drop paths and bendcore or exchange extensions', () => {
    expect(baseNameOf('C:\\work\\Housing.json')).toBe('Housing');
    expect(baseNameOf('/tmp/drive.bcasm.json')).toBe('drive');
    expect(baseNameOf('plate.STEP')).toBe('plate');
    expect(baseNameOf('   ')).toBe('model');
  });

  test('import format from the extension, case-insensitive', () => {
    expect(importFormatOf('a.step')).toBe('step');
    expect(importFormatOf('a.STP')).toBe('step');
    expect(importFormatOf('a.igs')).toBe('iges');
    expect(importFormatOf('a.IGES')).toBe('iges');
    expect(importFormatOf('a.stl')).toBeNull();
    expect(importFormatOf('step')).toBeNull();
  });
});

describe('export options', () => {
  const form = (patch: Partial<ExportForm>): ExportForm => ({ ...DEFAULT_EXPORT_FORM, ...patch });

  test('STEP carries the schema and the unit only', () => {
    expect(exportOptions(form({ format: 'step', schema: 'AP242', unit: 'inch' }), 'Part.json')).toEqual({
      options: { name: 'Part', schema: 'AP242', unit: 'inch' },
    });
  });

  test('IGES carries the entity mode and the unit', () => {
    expect(exportOptions(form({ format: 'iges', igesMode: 'faces' }), 'Part')).toEqual({
      options: { name: 'Part', igesMode: 'faces', unit: 'mm' },
    });
  });

  test('STL: encoding and mesh tolerances; 3MF: tolerances without encoding', () => {
    expect(exportOptions(form({ format: 'stl', stlFormat: 'ascii', linearDeflection: '0.01', angularDeflectionDeg: '10' }), 'P')).toEqual({
      options: { name: 'P', stlFormat: 'ascii', linearDeflection: 0.01, angularDeflectionDeg: 10 },
    });
    expect(exportOptions(form({ format: '3mf' }), 'P')).toEqual({
      options: { name: 'P', linearDeflection: 0.05, angularDeflectionDeg: 20 },
    });
  });

  test('tolerances outside the kernel ranges name the field', () => {
    expect(exportOptions(form({ format: 'stl', linearDeflection: '0' }), 'P')).toEqual({
      error: { field: 'linearDeflection', min: 0.0001, max: 100 },
    });
    expect(exportOptions(form({ format: '3mf', linearDeflection: 'abc' }), 'P')).toMatchObject({ error: { field: 'linearDeflection' } });
    expect(exportOptions(form({ format: 'stl', angularDeflectionDeg: '91' }), 'P')).toEqual({
      error: { field: 'angularDeflectionDeg', min: 0.5, max: 90 },
    });
    // Mesh tolerances are not checked for B-rep formats.
    expect(exportOptions(form({ format: 'step', linearDeflection: '0' }), 'P')).toHaveProperty('options');
  });
});

describe('mass read-out', () => {
  test('density: decimal comma accepted, out of range rejected', () => {
    expect(parseDensity('7850')).toBe(7850);
    expect(parseDensity('2,7e3')).toBe(2700);
    expect(parseDensity('0')).toBeNull();
    expect(parseDensity('-1')).toBeNull();
    expect(parseDensity('40000')).toBeNull();
    expect(parseDensity('')).toBeNull();
  });

  test('diagonal and products of the tensor', () => {
    const p: MassProperties = {
      mass: 1,
      volume: 1,
      area: 1,
      centerOfMass: [0, 0, 0],
      inertiaAtCenter: [
        [1, -2, -3],
        [-2, 4, -5],
        [-3, -5, 6],
      ],
      principalMoments: [0, 0, 0],
      principalAxes: [
        [1, 0, 0],
        [0, 1, 0],
        [0, 0, 1],
      ],
      bodies: 1,
    };
    expect(inertiaDiagonal(p)).toEqual([1, 4, 6]);
    expect(inertiaProducts(p)).toEqual([-2, -3, -5]);
  });
});

describe('PartExchange', () => {
  // A modeling client double that records the calls the exchange makes; documents answer with
  // the JSON they were loaded from so that the test sees which document served a call.
  function fakeModeling() {
    const log: string[] = [];
    let starts = 0;
    const start = async () => {
      starts += 1;
      return {
        dispose: () => log.push('client.dispose'),
        root: {
          loadDocument: async (text: string) => {
            log.push(`load ${text}`);
            return {
              document: {
                exportFile: async (format: string) => {
                  log.push(`export ${text} ${format}`);
                  return new TextEncoder().encode(`${format}:${text}`);
                },
                properties: async () => ({ volume: text === 'empty' ? null : 1000 }),
                massProperties: async (rho: number) => ({ mass: rho * 1e-6 }),
                dispose: async () => log.push(`dispose ${text}`),
              },
              report: {},
            };
          },
        },
      };
    };
    return { log, start, starts: () => starts };
  }

  test('one worker, a document per JSON, reloaded when the JSON changes', async () => {
    const m = fakeModeling();
    const ex = new PartExchange(m.start as never);
    expect(new TextDecoder().decode(await ex.exportFile('A', 'step', {}))).toBe('step:A');
    expect(new TextDecoder().decode(await ex.exportFile('A', 'stl', {}))).toBe('stl:A');
    expect((await ex.massProperties('A', 7850))?.mass).toBeCloseTo(7850e-6);
    await ex.exportFile('B', 'iges', {});
    await new Promise((r) => setTimeout(r, 0));
    expect(m.starts()).toBe(1);
    expect(m.log).toEqual(['load A', 'export A step', 'export A stl', 'load B', 'dispose A', 'export B iges']);
    ex.dispose();
    await new Promise((r) => setTimeout(r, 0));
    expect(m.log[m.log.length - 1]).toBe('client.dispose');
  });

  test('a part without a solid has no mass properties', async () => {
    const m = fakeModeling();
    const ex = new PartExchange(m.start as never);
    expect(await ex.massProperties('empty', 7850)).toBeNull();
    ex.dispose();
  });
});
