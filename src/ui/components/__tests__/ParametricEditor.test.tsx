/**
 * CAD-CORE-026 Tests
 * ParametricEditor component tests
 */

import { ExpressionEvaluator, DependencyResolver, ParameterDefinition } from '../ParametricEditor';

describe('ParametricEditor', () => {
  describe('ExpressionEvaluator', () => {
    let evaluator: ExpressionEvaluator;

    beforeEach(() => {
      evaluator = new ExpressionEvaluator();
    });

    test('should evaluate simple arithmetic expressions', () => {
      evaluator.setContext({ a: 5, b: 3 });
      const result = evaluator.evaluate('a + b');
      expect(result).toBe(8);
    });

    test('should evaluate complex expressions', () => {
      evaluator.setContext({ x: 10, y: 20 });
      const result = evaluator.evaluate('(x + y) * 2');
      expect(result).toBe(60);
    });

    test('should handle multiplication and division', () => {
      evaluator.setContext({ a: 10, b: 4 });
      expect(evaluator.evaluate('a * b')).toBe(40);
      expect(evaluator.evaluate('a / b')).toBe(2.5);
    });

    test('should handle subtraction', () => {
      evaluator.setContext({ x: 100, y: 30 });
      const result = evaluator.evaluate('x - y');
      expect(result).toBe(70);
    });

    test('should handle nested parentheses', () => {
      evaluator.setContext({ a: 5, b: 2, c: 3 });
      const result = evaluator.evaluate('((a + b) * c)');
      expect(result).toBe(21);
    });

    test('should reject invalid expressions', () => {
      evaluator.setContext({ a: 5 });
      const result = evaluator.evaluate('a + ); invalid');
      expect(result).toBeNull();
    });

    test('should handle empty context gracefully', () => {
      evaluator.setContext({});
      const result = evaluator.evaluate('5 + 3');
      expect(result).toBe(8);
    });

    test('should evaluate with floating point numbers', () => {
      evaluator.setContext({ pi: 3.14159, r: 2 });
      const result = evaluator.evaluate('pi * r * r');
      expect(Math.round(result * 100) / 100).toBeCloseTo(12.57, 1);
    });
  });

  describe('DependencyResolver', () => {
    let resolver: DependencyResolver;
    let evaluator: ExpressionEvaluator;
    let parameters: ParameterDefinition[];

    beforeEach(() => {
      evaluator = new ExpressionEvaluator();
      parameters = [
        { id: 'width', name: 'Width', type: 'number', value: 10 },
        { id: 'height', name: 'Height', type: 'number', value: 20 },
        {
          id: 'area',
          name: 'Area',
          type: 'number',
          value: 0,
          expression: 'width * height',
          dependencies: ['width', 'height'],
        },
        {
          id: 'perimeter',
          name: 'Perimeter',
          type: 'number',
          value: 0,
          expression: '(width + height) * 2',
          dependencies: ['width', 'height'],
        },
      ];
      resolver = new DependencyResolver(parameters, evaluator);
    });

    test('should identify direct dependents', () => {
      const dependents = resolver.getDependents('width');
      expect(dependents.length).toBe(2);
      expect(dependents.map((d) => d.id)).toContain('area');
      expect(dependents.map((d) => d.id)).toContain('perimeter');
    });

    test('should build update chain for cascading updates', () => {
      const chain = resolver.getUpdateChain('width');
      expect(chain).toContain('width');
      expect(chain).toContain('area');
      expect(chain).toContain('perimeter');
    });

    test('should evaluate dependent parameters', () => {
      const values = { width: 10, height: 20, area: 0, perimeter: 0 };
      const updates = resolver.evaluateDependents('width', values);

      expect(updates.area).toBe(200);
      expect(updates.perimeter).toBe(60);
    });

    test('should handle empty dependents', () => {
      const params: ParameterDefinition[] = [
        { id: 'param1', name: 'Param 1', type: 'number', value: 5 },
      ];
      const res = new DependencyResolver(params, evaluator);
      const dependents = res.getDependents('param1');
      expect(dependents.length).toBe(0);
    });

    test('should handle multiple levels of dependencies', () => {
      const params: ParameterDefinition[] = [
        { id: 'a', name: 'A', type: 'number', value: 5 },
        {
          id: 'b',
          name: 'B',
          type: 'number',
          value: 0,
          expression: 'a * 2',
          dependencies: ['a'],
        },
        {
          id: 'c',
          name: 'C',
          type: 'number',
          value: 0,
          expression: 'b + a',
          dependencies: ['b', 'a'],
        },
      ];
      const res = new DependencyResolver(params, evaluator);
      const chain = res.getUpdateChain('a');
      expect(chain).toContain('a');
      expect(chain).toContain('b');
      expect(chain).toContain('c');
    });

    test('should update values in context during evaluation', () => {
      const values = { width: 15, height: 25, area: 0, perimeter: 0 };
      const updates = resolver.evaluateDependents('width', values);

      expect(updates.area).toBe(375); // 15 * 25
      expect(updates.perimeter).toBe(80); // (15 + 25) * 2
    });
  });

  describe('ParameterDefinition Types', () => {
    test('should support number parameters', () => {
      const param: ParameterDefinition = {
        id: 'diameter',
        name: 'Diameter',
        type: 'number',
        value: 10,
        min: 1,
        max: 100,
        step: 0.5,
      };
      expect(param.type).toBe('number');
      expect(param.min).toBe(1);
      expect(param.max).toBe(100);
    });

    test('should support string parameters', () => {
      const param: ParameterDefinition = {
        id: 'material',
        name: 'Material',
        type: 'string',
        value: 'steel',
      };
      expect(param.type).toBe('string');
      expect(param.value).toBe('steel');
    });

    test('should support boolean parameters', () => {
      const param: ParameterDefinition = {
        id: 'isVisible',
        name: 'Visible',
        type: 'boolean',
        value: true,
      };
      expect(param.type).toBe('boolean');
      expect(param.value).toBe(true);
    });

    test('should support select parameters with options', () => {
      const param: ParameterDefinition = {
        id: 'profile',
        name: 'Profile',
        type: 'select',
        value: 'round',
        options: [
          { label: 'Round', value: 'round' },
          { label: 'Square', value: 'square' },
        ],
      };
      expect(param.options?.length).toBe(2);
      expect(param.value).toBe('round');
    });

    test('should support expression parameters', () => {
      const param: ParameterDefinition = {
        id: 'calculated',
        name: 'Calculated',
        type: 'expression',
        value: 0,
        expression: 'width * height',
        dependencies: ['width', 'height'],
        readOnly: true,
      };
      expect(param.expression).toBeDefined();
      expect(param.readOnly).toBe(true);
    });
  });

  describe('Parameter Groups and Organization', () => {
    test('should group parameters by section', () => {
      const parameters: ParameterDefinition[] = [
        { id: 'width', name: 'Width', type: 'number', value: 10, group: 'Dimensions' },
        { id: 'height', name: 'Height', type: 'number', value: 20, group: 'Dimensions' },
        { id: 'material', name: 'Material', type: 'string', value: 'steel', group: 'Material' },
      ];

      const groups: Record<string, ParameterDefinition[]> = {};
      parameters.forEach((p) => {
        const groupName = p.group || 'General';
        if (!groups[groupName]) groups[groupName] = [];
        groups[groupName].push(p);
      });

      expect(Object.keys(groups)).toContain('Dimensions');
      expect(Object.keys(groups)).toContain('Material');
      expect(groups['Dimensions'].length).toBe(2);
    });

    test('should assign default group to ungrouped parameters', () => {
      const param: ParameterDefinition = {
        id: 'param1',
        name: 'Param 1',
        type: 'number',
        value: 5,
      };
      const groupName = param.group || 'General';
      expect(groupName).toBe('General');
    });
  });

  describe('Parameter Validation', () => {
    test('should validate number range', () => {
      const param: ParameterDefinition = {
        id: 'value',
        name: 'Value',
        type: 'number',
        value: 50,
        min: 0,
        max: 100,
      };

      const isValid = param.value >= (param.min || -Infinity) && param.value <= (param.max || Infinity);
      expect(isValid).toBe(true);
    });

    test('should detect out-of-range values', () => {
      const param: ParameterDefinition = {
        id: 'value',
        name: 'Value',
        type: 'number',
        value: 150,
        min: 0,
        max: 100,
      };

      const isValid = param.value >= (param.min || -Infinity) && param.value <= (param.max || Infinity);
      expect(isValid).toBe(false);
    });

    test('should validate select options', () => {
      const param: ParameterDefinition = {
        id: 'choice',
        name: 'Choice',
        type: 'select',
        value: 'option1',
        options: [
          { label: 'Option 1', value: 'option1' },
          { label: 'Option 2', value: 'option2' },
        ],
      };

      const isValid = param.options?.some((opt) => opt.value === param.value);
      expect(isValid).toBe(true);
    });
  });

  describe('Parameter History and Undo/Redo', () => {
    test('should track parameter change history', () => {
      const history: Array<Record<string, any>> = [];
      const values = { width: 10, height: 20 };

      history.push({ ...values });
      values.width = 15;
      history.push({ ...values });
      values.width = 12;
      history.push({ ...values });

      expect(history.length).toBe(3);
      expect(history[0].width).toBe(10);
      expect(history[1].width).toBe(15);
      expect(history[2].width).toBe(12);
    });

    test('should support undo operation', () => {
      const history: Array<Record<string, any>> = [
        { value: 10 },
        { value: 20 },
        { value: 30 },
      ];
      let historyIndex = 2;

      if (historyIndex > 0) {
        historyIndex--;
      }

      expect(historyIndex).toBe(1);
      expect(history[historyIndex].value).toBe(20);
    });

    test('should support redo operation', () => {
      const history: Array<Record<string, any>> = [
        { value: 10 },
        { value: 20 },
        { value: 30 },
      ];
      let historyIndex = 1;

      if (historyIndex < history.length - 1) {
        historyIndex++;
      }

      expect(historyIndex).toBe(2);
      expect(history[historyIndex].value).toBe(30);
    });
  });

  describe('Real-time Updates', () => {
    test('should batch update related parameters', () => {
      const evaluator = new ExpressionEvaluator();
      const parameters: ParameterDefinition[] = [
        { id: 'x', name: 'X', type: 'number', value: 5 },
        { id: 'y', name: 'Y', type: 'number', value: 10 },
        {
          id: 'distance',
          name: 'Distance',
          type: 'number',
          value: 0,
          expression: 'Math.sqrt(x*x + y*y)',
        },
      ];

      const resolver = new DependencyResolver(parameters, evaluator);
      const values = { x: 5, y: 10, distance: 0 };
      const updates = resolver.evaluateDependents('x', values);

      expect(updates).toHaveProperty('distance');
    });

    test('should handle rapid sequential updates', () => {
      const history: Array<Record<string, any>> = [];

      for (let i = 0; i < 10; i++) {
        history.push({ value: i });
      }

      expect(history.length).toBe(10);
      expect(history[9].value).toBe(9);
    });
  });
});

export {};
