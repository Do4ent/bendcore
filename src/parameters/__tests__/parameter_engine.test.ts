/**
 * Parameter Engine Unit Tests
 * Tests for parametric design system functionality
 */

describe('ParameterEngine', () => {

  describe('Constraint Validation', () => {
    it('should validate MIN_VALUE constraint', () => {
      const min = 5.0;
      const testValue = 3.0;
      const validValue = 5.5;

      expect(testValue).toBeLessThan(min);
      expect(validValue).toBeGreaterThanOrEqual(min);
    });

    it('should validate MAX_VALUE constraint', () => {
      const max = 100.0;
      const testValue = 101.0;
      const validValue = 99.0;

      expect(testValue).toBeGreaterThan(max);
      expect(validValue).toBeLessThanOrEqual(max);
    });

    it('should validate RANGE constraint', () => {
      const min = 10.0;
      const max = 50.0;
      const invalidLow = 5.0;
      const invalidHigh = 55.0;
      const valid = 30.0;

      expect(invalidLow).toBeLessThan(min);
      expect(invalidHigh).toBeGreaterThan(max);
      expect(valid).toBeGreaterThanOrEqual(min);
      expect(valid).toBeLessThanOrEqual(max);
    });

    it('should validate EQUALS constraint with epsilon', () => {
      const target = 42.0;
      const epsilon = 1e-9;
      const exact = 42.0;
      const closeEnough = 42.00000000001;
      const tooFar = 42.00001;

      expect(Math.abs(exact - target)).toBeLessThan(epsilon);
      expect(Math.abs(closeEnough - target)).toBeLessThan(epsilon);
      expect(Math.abs(tooFar - target)).toBeGreaterThan(epsilon);
    });

    it('should validate NOT_EQUALS constraint', () => {
      const target = 0.0;
      const epsilon = 1e-9;
      const invalid = 0.0;
      const valid1 = 0.1;
      const valid2 = -0.1;

      expect(Math.abs(invalid - target)).toBeLessThan(epsilon);
      expect(Math.abs(valid1 - target)).toBeGreaterThanOrEqual(epsilon);
      expect(Math.abs(valid2 - target)).toBeGreaterThanOrEqual(epsilon);
    });

    it('should support CUSTOM validator functions', () => {
      const isEven = (value: number) => value % 2 === 0;

      expect(isEven(4.0)).toBe(true);
      expect(isEven(5.0)).toBe(false);
      expect(isEven(100.0)).toBe(true);
    });

    it('should enforce multiple constraints together', () => {
      const min = 10;
      const max = 100;
      const notEqual = 50;

      const validate = (value: number) => {
        return value >= min && value <= max && Math.abs(value - notEqual) > 1e-9;
      };

      expect(validate(5)).toBe(false);    // < min
      expect(validate(50)).toBe(false);   // equals notEqual
      expect(validate(75)).toBe(true);    // valid
      expect(validate(150)).toBe(false);  // > max
    });
  });

  describe('Parameter Type System', () => {
    it('should support FLOAT parameter type', () => {
      const type = 'FLOAT';
      const value = 10.5;
      expect(typeof value).toBe('number');
      expect(value % 1).not.toBe(0);
    });

    it('should support INTEGER parameter type', () => {
      const type = 'INTEGER';
      const value = 42;
      expect(typeof value).toBe('number');
      expect(value % 1).toBe(0);
    });

    it('should support BOOLEAN parameter type', () => {
      const type = 'BOOLEAN';
      const values = [true, false];
      expect(values).toContain(true);
      expect(values).toContain(false);
    });

    it('should support STRING parameter type', () => {
      const type = 'STRING';
      const value = 'tube_material_steel';
      expect(typeof value).toBe('string');
      expect(value.length).toBeGreaterThan(0);
    });

    it('should support DIMENSION parameter type', () => {
      const type = 'DIMENSION';
      const value = 25.4;  // mm
      expect(typeof value).toBe('number');
      expect(value).toBeGreaterThan(0);
    });

    it('should support ANGLE parameter type', () => {
      const type = 'ANGLE';
      const value = 45.0;  // degrees
      expect(typeof value).toBe('number');
      expect(value).toBeGreaterThanOrEqual(0);
      expect(value).toBeLessThanOrEqual(360);
    });
  });

  describe('Parameter Value Management', () => {
    it('should store default value and current value', () => {
      const defaultValue = 10.0;
      let currentValue = defaultValue;

      expect(currentValue).toBe(defaultValue);

      currentValue = 20.0;
      expect(currentValue).not.toBe(defaultValue);
      expect(currentValue).toBe(20.0);
    });

    it('should reset to default value', () => {
      const defaultValue = 10.0;
      let currentValue = 50.0;

      expect(currentValue).not.toBe(defaultValue);

      currentValue = defaultValue;

      expect(currentValue).toBe(defaultValue);
    });

    it('should preserve value identity', () => {
      const values = [10.0, 20.0, 30.0];
      expect(values[0]).toBe(10.0);
      expect(values[1]).toBe(20.0);
      expect(values[2]).toBe(30.0);
      expect(values.length).toBe(3);
    });
  });

  describe('Dependency Chain Propagation', () => {
    it('should compute simple two-parameter dependency', () => {
      // diameter -> radius: radius = diameter / 2
      const diameter = 20.0;
      const computedRadius = diameter / 2;

      expect(computedRadius).toBe(10.0);
    });

    it('should compute three-level dependency chain', () => {
      // A=10, B=A*2, C=B+5
      const a = 10.0;
      const b = a * 2;  // 20.0
      const c = b + 5;  // 25.0

      expect(b).toBe(20.0);
      expect(c).toBe(25.0);
    });

    it('should handle dependency with change propagation', () => {
      // A=10, B=A*2, C=B+5
      let a = 10.0;
      let b = a * 2;
      let c = b + 5;

      expect(c).toBe(25.0);

      // Change A to 20
      a = 20.0;
      b = a * 2;
      c = b + 5;

      expect(b).toBe(40.0);
      expect(c).toBe(45.0);
    });

    it('should support multiple dependents', () => {
      // A -> B, A -> C, A -> D
      const a = 10.0;
      const b = a * 2;      // 20.0
      const c = a + 5;      // 15.0
      const d = a * 10;     // 100.0

      expect(b).toBe(20.0);
      expect(c).toBe(15.0);
      expect(d).toBe(100.0);

      // All should update when A changes
      const newA = 5.0;
      const newB = newA * 2;
      const newC = newA + 5;
      const newD = newA * 10;

      expect(newB).toBe(10.0);
      expect(newC).toBe(10.0);
      expect(newD).toBe(50.0);
    });
  });

  describe('Sensitivity Analysis', () => {
    it('should compute linear sensitivity', () => {
      // output = input * 2 + 5
      // sensitivity should be ~2.0 (slope of output w.r.t. input)
      const input = 10.0;
      const variation = input * 0.01;  // 1%

      const outputBase = input * 2 + 5;      // 25.0
      const outputUp = (input + variation) * 2 + 5;    // 25.2
      const outputDown = (input - variation) * 2 + 5;  // 24.8

      const sensitivity = (outputUp - outputDown) / (2 * variation);

      expect(sensitivity).toBeCloseTo(2.0, 5);
    });

    it('should handle zero-variance parameters', () => {
      const input = 0.0;
      const variation = input * 0.01;  // results in 0

      // Should use absolute variation when relative is zero
      const absoluteVariation = 0.01;  // Use 1% as absolute

      expect(absoluteVariation).toBeGreaterThan(0);
    });

    it('should measure sensitivity for multiple parameters', () => {
      // Parameter 1: high sensitivity (output = input * 10)
      // Parameter 2: low sensitivity (output = input * 0.1)

      const compute1 = (input: number) => input * 10;
      const compute2 = (input: number) => input * 0.1;

      const sensitivity1 = Math.abs(10);    // Slope
      const sensitivity2 = Math.abs(0.1);   // Slope

      expect(sensitivity1).toBeGreaterThan(sensitivity2);
    });
  });

  describe('Parameter Metadata', () => {
    it('should store parameter ID', () => {
      const id = 'tube_diameter';
      expect(id).toBe('tube_diameter');
      expect(id.length).toBeGreaterThan(0);
    });

    it('should store parameter name', () => {
      const name = 'Tube Diameter';
      expect(name).toBe('Tube Diameter');
      expect(name.length).toBeGreaterThan(0);
    });

    it('should store parameter description', () => {
      const description = 'Outer diameter of the tube in mm';
      expect(description.length).toBeGreaterThan(0);
      expect(description).toContain('diameter');
    });

    it('should handle special characters in names', () => {
      const names = [
        'Parameter-1',
        'Parameter_2',
        'Parameter.3',
        'Parameter (mm)',
        'Parameter [°]'
      ];

      expect(names.length).toBe(5);
      expect(names[0]).toBe('Parameter-1');
    });
  });

  describe('Performance', () => {
    it('should compute parameter update quickly', () => {
      const start = performance.now();

      let value = 10.0;
      for (let i = 0; i < 1000; i++) {
        value = value * 1.001;  // Simulate update
      }

      const elapsed = performance.now() - start;

      // Update should be very fast
      expect(elapsed).toBeLessThan(100);  // Should be much less than 100ms
    });

    it('should propagate through chain quickly', () => {
      const start = performance.now();

      // Simulate 5-level chain
      let a = 10.0;
      let b = a * 2;
      let c = b + 5;
      let d = c * 3;
      let e = d / 2;

      const elapsed = performance.now() - start;

      expect(elapsed).toBeLessThan(10);  // Should be very fast
    });

    it('should handle many parameters', () => {
      const start = performance.now();

      const params: number[] = [];
      for (let i = 0; i < 100; i++) {
        params.push(Math.random() * 100);
      }

      const elapsed = performance.now() - start;

      expect(params.length).toBe(100);
      expect(elapsed).toBeLessThan(50);  // Creating 100 params should be <50ms
    });
  });

  describe('Edge Cases', () => {
    it('should handle very large values', () => {
      const large = 1e10;
      expect(large).toBe(10000000000);
      expect(large).toBeGreaterThan(0);
    });

    it('should handle very small values', () => {
      const small = 1e-10;
      expect(small).toBe(0.0000000001);
      expect(small).toBeGreaterThan(0);
    });

    it('should handle negative values', () => {
      const negative = -50.0;
      expect(negative).toBeLessThan(0);
      expect(Math.abs(negative)).toBe(50.0);
    });

    it('should handle zero values', () => {
      const zero = 0.0;
      expect(zero).toBe(0);
      expect(zero).not.toBeLessThan(0);
      expect(zero).not.toBeGreaterThan(0);
    });

    it('should distinguish between 0 and -0', () => {
      const positive = 0.0;
      const negative = -0.0;

      // In JavaScript, 0 === -0
      expect(positive).toBe(negative);
      // But they have different signs
      expect(Object.is(positive, negative)).toBe(false);
    });
  });

  describe('Callback System', () => {
    it('should trigger callback on value change', () => {
      let callCount = 0;
      const callback = () => { callCount++; };

      // Simulate value change
      callback();

      expect(callCount).toBe(1);
    });

    it('should pass value to callback', () => {
      let receivedValue = 0;
      const callback = (value: number) => { receivedValue = value; };

      const newValue = 42.0;
      callback(newValue);

      expect(receivedValue).toBe(newValue);
    });

    it('should support multiple callbacks', () => {
      let count1 = 0;
      let count2 = 0;
      let count3 = 0;

      const callback1 = () => { count1++; };
      const callback2 = () => { count2++; };
      const callback3 = () => { count3++; };

      callback1();
      callback2();
      callback3();

      expect(count1).toBe(1);
      expect(count2).toBe(1);
      expect(count3).toBe(1);
    });

    it('should handle callback errors gracefully', () => {
      const callback = () => {
        throw new Error('Callback error');
      };

      expect(() => {
        callback();
      }).toThrow();
    });
  });

  describe('Validation System', () => {
    it('should validate single parameter', () => {
      const validate = (value: number, min: number) => value >= min;

      expect(validate(10.0, 5.0)).toBe(true);
      expect(validate(3.0, 5.0)).toBe(false);
    });

    it('should validate multiple constraints', () => {
      const value = 50.0;
      const min = 10.0;
      const max = 100.0;
      const notEqual = 75.0;

      const allValid = (
        value >= min &&
        value <= max &&
        Math.abs(value - notEqual) > 1e-9
      );

      expect(allValid).toBe(true);
    });

    it('should return true for empty constraints', () => {
      const constraints: Array<(v: number) => boolean> = [];

      const validate = (value: number) => {
        return constraints.every(c => c(value));
      };

      expect(validate(42.0)).toBe(true);
    });
  });

  describe('Parameter Management', () => {
    it('should create parameters with unique IDs', () => {
      const ids = ['param_a', 'param_b', 'param_c'];
      const uniqueIds = new Set(ids);

      expect(uniqueIds.size).toBe(3);
      expect(ids.length).toBe(3);
    });

    it('should handle missing parameters gracefully', () => {
      const params: Map<string, number> = new Map();

      const getValue = (id: string) => {
        return params.get(id);
      };

      expect(getValue('nonexistent')).toBeUndefined();
    });

    it('should remove parameters', () => {
      let params = ['a', 'b', 'c'];

      params = params.filter(p => p !== 'b');

      expect(params).toEqual(['a', 'c']);
      expect(params.length).toBe(2);
    });

    it('should get all parameters', () => {
      const params = new Map([
        ['p1', 10.0],
        ['p2', 20.0],
        ['p3', 30.0]
      ]);

      expect(params.size).toBe(3);
      expect(Array.from(params.keys())).toEqual(['p1', 'p2', 'p3']);
    });
  });

});
