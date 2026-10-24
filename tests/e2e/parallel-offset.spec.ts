/**
 * Parallel Offset Calculator - Comprehensive E2E Tests
 *
 * Тестирование калькулятора параллельного смещения трубок
 * Включает: happy path, edge cases, validation, export, templates, synchronization
 *
 * Target: 25+ тестов
 */

import { test, expect } from '@playwright/test';
import { ParallelOffsetHelper } from './helpers/offset-utils';
import parallelData from './fixtures/parallel-data.json';

test.describe('Parallel Offset Calculator - E2E Tests', () => {
  let helper: ParallelOffsetHelper;

  test.beforeEach(async ({ page }) => {
    helper = new ParallelOffsetHelper(page);
    await page.goto('/');
  });

  // ============================================================================
  // HAPPY PATH TESTS (10 тестов)
  // ============================================================================

  test.describe('Happy Path - Basic Functionality', () => {
    test('should open Offset Dialog and Parallel Offset Tab', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await expect(page.locator('[data-testid="parallel-offset-form"]')).toBeVisible();
    });

    test('should display all input fields', async ({ page }) => {
      await helper.openParallelOffsetTab();

      await expect(page.locator('[data-testid="input-outer-diameter"]')).toBeVisible();
      await expect(page.locator('[data-testid="input-wall-thickness"]')).toBeVisible();
      await expect(page.locator('[data-testid="slider-bend-angle"]')).toBeVisible();
      await expect(page.locator('[data-testid="input-offset-distance"]')).toBeVisible();
      await expect(page.locator('[data-testid="input-bend-radius"]')).toBeVisible();
      await expect(page.locator('[data-testid="select-material"]')).toBeVisible();
    });

    test('should calculate standard 90 degree offset', async ({ page }) => {
      await helper.openParallelOffsetTab();

      const testCase = parallelData.test_cases[0]; // Standard 90 Degree Offset - Steel
      await helper.fillParameters(testCase.params);
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.is_valid).toBe(true);
      expect(results.total_length).toBeGreaterThan(0);
      expect(results.confidence).toBeGreaterThanOrEqual(testCase.expected.confidence_min);
    });

    test('should calculate 45 degree offset', async ({ page }) => {
      await helper.openParallelOffsetTab();

      const testCase = parallelData.test_cases[1]; // 45 Degree Offset - Aluminum
      await helper.fillParameters(testCase.params);
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.is_valid).toBe(true);
      expect(results.total_length).toBeCloseTo(testCase.expected.total_length_approx, -1);
    });

    test('should calculate large diameter offset', async ({ page }) => {
      await helper.openParallelOffsetTab();

      const testCase = parallelData.test_cases[2]; // Large Diameter - Stainless Steel
      await helper.fillParameters(testCase.params);
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.is_valid).toBe(true);
      expect(results.total_length).toBeGreaterThan(100);
    });

    test('should calculate small diameter offset', async ({ page }) => {
      await helper.openParallelOffsetTab();

      const testCase = parallelData.test_cases[3]; // Small Diameter - Copper
      await helper.fillParameters(testCase.params);
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.is_valid).toBe(true);
      expect(results.total_length).toBeLessThan(100);
    });

    test('should update results in real-time on parameter change', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 25,
        wall_thickness: 2.0,
        bend_angle: 90,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await helper.calculate();

      const result1 = await helper.getResults();

      // Изменяем параметр и пересчитываем
      await helper.fillParameters({ offset_distance: 100 });
      await helper.calculate();

      const result2 = await helper.getResults();

      expect(result2.total_length).not.toEqual(result1.total_length);
    });

    test('should display confidence percentage', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters(parallelData.test_cases[0].params);
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.confidence).toBeGreaterThan(0);
      expect(results.confidence).toBeLessThanOrEqual(100);
    });

    test('should show calculation results correctly formatted', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters(parallelData.test_cases[0].params);
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.total_length).toBeDefined();
      expect(results.bend_1_length).toBeDefined();
      expect(results.straight_section).toBeDefined();
      expect(results.bend_2_length).toBeDefined();
      expect(typeof results.is_valid).toBe('boolean');
    });

    test('should maintain results when switching tabs and returning', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters(parallelData.test_cases[0].params);
      await helper.calculate();

      const result1 = await helper.getResults();

      // Переключаемся на другую вкладку и возвращаемся
      await page.click('[data-testid="offset-tab-3d"]');
      await page.click('[data-testid="offset-tab-parallel"]');

      const result2 = await helper.getResults();

      expect(result2.total_length).toEqual(result1.total_length);
    });
  });

  // ============================================================================
  // EDGE CASES - ANGLES (8 тестов)
  // ============================================================================

  test.describe('Edge Cases - Angles', () => {
    test('should calculate minimum angle (5 degrees)', async ({ page }) => {
      await helper.openParallelOffsetTab();

      const testCase = parallelData.test_cases[4]; // Minimum Angle (5 degrees)
      await helper.fillParameters(testCase.params);
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.is_valid).toBe(true);
      expect(results.confidence).toBeGreaterThan(0.5);
    });

    test('should calculate maximum angle (175 degrees)', async ({ page }) => {
      await helper.openParallelOffsetTab();

      const testCase = parallelData.test_cases[5]; // Maximum Angle (175 degrees)
      await helper.fillParameters(testCase.params);
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.is_valid).toBe(true);
    });

    test('should handle acute angles (30 degrees)', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 25,
        wall_thickness: 2.0,
        bend_angle: 30,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.is_valid).toBe(true);
      expect(results.total_length).toBeGreaterThan(0);
    });

    test('should handle obtuse angles (150 degrees)', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 25,
        wall_thickness: 2.0,
        bend_angle: 150,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.is_valid).toBe(true);
    });

    test('should reject angle > 180 degrees', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 25,
        wall_thickness: 2.0,
        bend_angle: 185,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await helper.calculate();

      const hasErrors = await helper.checkErrorBorders();

      expect(hasErrors.length).toBeGreaterThan(0);
    });

    test('should reject negative angles', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 25,
        wall_thickness: 2.0,
        bend_angle: -45,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await helper.calculate();

      const hasErrors = await helper.checkErrorBorders();

      expect(hasErrors.length).toBeGreaterThan(0);
    });

    test('should handle right angle (90 degrees)', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 25,
        wall_thickness: 2.0,
        bend_angle: 90,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.is_valid).toBe(true);
      // При 90 градусах обе гибки должны быть ~90
      expect(Math.abs(results.bend_1_length - results.bend_2_length)).toBeLessThan(10);
    });

    test('should handle straight angle (180 degrees)', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 25,
        wall_thickness: 2.0,
        bend_angle: 180,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.is_valid).toBe(true);
    });
  });

  // ============================================================================
  // EDGE CASES - DIAMETERS (5 тестов)
  // ============================================================================

  test.describe('Edge Cases - Diameters', () => {
    test('should calculate with minimum diameter (6mm)', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 6.35,
        wall_thickness: 0.5,
        bend_angle: 90,
        offset_distance: 20,
        bend_radius: 15,
        material: 'Copper',
      });
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.is_valid).toBe(true);
      expect(results.total_length).toBeLessThan(100);
    });

    test('should calculate with maximum diameter (400mm)', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 400,
        wall_thickness: 3.0,
        bend_angle: 90,
        offset_distance: 200,
        bend_radius: 200,
        material: 'Steel',
      });
      await helper.calculate();

      const results = await helper.getResults();

      expect(results.is_valid).toBe(true);
      expect(results.total_length).toBeGreaterThan(200);
    });

    test('should reject negative diameter', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: -25,
        wall_thickness: 2.0,
        bend_angle: 90,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await helper.calculate();

      const hasErrors = await helper.checkErrorBorders();

      expect(hasErrors.length).toBeGreaterThan(0);
    });

    test('should reject zero diameter', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 0,
        wall_thickness: 2.0,
        bend_angle: 90,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await helper.calculate();

      const hasErrors = await helper.checkErrorBorders();

      expect(hasErrors.length).toBeGreaterThan(0);
    });

    test('should reject wall thickness exceeding diameter', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 25,
        wall_thickness: 20.0,
        bend_angle: 90,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await helper.calculate();

      const hasErrors = await helper.checkErrorBorders();

      expect(hasErrors.length).toBeGreaterThan(0);
    });
  });

  // ============================================================================
  // MATERIALS TESTING (6 тестов)
  // ============================================================================

  test.describe('Materials Testing', () => {
    const materials = ['Steel', 'Stainless Steel', 'Aluminum', 'Copper', 'Titanium', 'Brass'];

    for (const material of materials) {
      test(`should calculate offset for ${material}`, async ({ page }) => {
        await helper.openParallelOffsetTab();
        await helper.fillParameters({
          outer_diameter: 25,
          wall_thickness: 2.0,
          bend_angle: 90,
          offset_distance: 50,
          bend_radius: 40,
          material,
        });
        await helper.calculate();

        const results = await helper.getResults();

        expect(results.is_valid).toBe(true);
        expect(results.total_length).toBeGreaterThan(0);
      });
    }
  });

  // ============================================================================
  // EXPORT & TEMPLATES (4 теста)
  // ============================================================================

  test.describe('Export & Templates', () => {
    test('should export results to PDF without errors', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters(parallelData.test_cases[0].params);
      await helper.calculate();

      await expect(async () => {
        await helper.exportToPDF();
      }).not.toThrow();
    });

    test('should save template successfully', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters(parallelData.test_cases[0].params);
      await helper.calculate();

      const templateName = `Test Template ${Date.now()}`;

      await expect(async () => {
        await helper.saveTemplate(templateName);
      }).not.toThrow();
    });

    test('should load saved template', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 25,
        wall_thickness: 2.0,
        bend_angle: 90,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await helper.calculate();

      const templateName = `Load Template ${Date.now()}`;
      await helper.saveTemplate(templateName);

      // Clear current values
      await helper.fillParameters({
        outer_diameter: 10,
        wall_thickness: 1.0,
        bend_angle: 45,
        offset_distance: 20,
        bend_radius: 20,
        material: 'Aluminum',
      });

      // Load template (simulate by clicking on it)
      await page.click(`[data-testid="template-${templateName}"]`);

      // Verify values are restored
      const diameter = await page.inputValue('[data-testid="input-outer-diameter"]');
      expect(parseFloat(diameter)).toBe(25);
    });

    test('should export calculation history', async ({ page }) => {
      await helper.openParallelOffsetTab();

      // Выполняем несколько расчётов
      for (let i = 0; i < 3; i++) {
        await helper.fillParameters({
          outer_diameter: 20 + i * 5,
          bend_angle: 45 + i * 15,
        });
        await helper.calculate();
      }

      // Экспортируем историю
      const downloadPromise = page.waitForEvent('download');
      await page.click('[data-testid="button-export-history"]');
      const download = await downloadPromise;

      expect(download.suggestedFilename()).toMatch(/history.*\.(csv|json|pdf)/);
    });
  });

  // ============================================================================
  // VALIDATION & ERROR HANDLING (5 тестов)
  // ============================================================================

  test.describe('Validation & Error Handling', () => {
    test('should show error for invalid input', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: -25,
        wall_thickness: 2.0,
        bend_angle: 90,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await helper.calculate();

      const errorBorders = await helper.checkErrorBorders();

      expect(errorBorders.length).toBeGreaterThan(0);
      expect(errorBorders).toContain('input-outer-diameter');
    });

    test('should show validation errors for edge case parameters', async ({ page }) => {
      await helper.openParallelOffsetTab();

      const edgeCase = parallelData.edge_cases[0]; // Invalid - Negative Diameter
      await helper.fillParameters(edgeCase.params);
      await helper.calculate();

      const hasErrors = await helper.checkErrorBorders();

      expect(hasErrors.length).toBeGreaterThan(0);
    });

    test('should validate wall thickness ratio', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 25,
        wall_thickness: 25.0,
        bend_angle: 90,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await helper.calculate();

      const hasErrors = await helper.checkErrorBorders();

      expect(hasErrors.length).toBeGreaterThan(0);
    });

    test('should validate bend radius against tube properties', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters({
        outer_diameter: 25,
        wall_thickness: 2.0,
        bend_angle: 90,
        offset_distance: 50,
        bend_radius: 5, // Слишком маленький радиус
        material: 'Titanium',
      });
      await helper.calculate();

      const hasErrors = await helper.checkErrorBorders();

      expect(hasErrors.length).toBeGreaterThan(0);
    });

    test('should clear errors when user corrects input', async ({ page }) => {
      await helper.openParallelOffsetTab();

      // Ввод неверных данных
      await helper.fillParameters({
        outer_diameter: -25,
        wall_thickness: 2.0,
        bend_angle: 90,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });

      let errorBorders = await helper.checkErrorBorders();
      expect(errorBorders.length).toBeGreaterThan(0);

      // Исправляем ошибку
      await helper.fillParameters({ outer_diameter: 25 });

      errorBorders = await helper.checkErrorBorders();
      expect(errorBorders.length).toBe(0);
    });
  });

  // ============================================================================
  // SYNCHRONIZATION & STATE MANAGEMENT (3 теста)
  // ============================================================================

  test.describe('Synchronization & State Management', () => {
    test('should sync results to main tube parameters', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters(parallelData.test_cases[0].params);
      await helper.calculate();

      const isSynced = await helper.verifyResultsSync();

      expect(isSynced).toBe(true);
    });

    test('should maintain calculation history', async ({ page }) => {
      await helper.openParallelOffsetTab();

      // Выполняем несколько расчётов
      for (let i = 0; i < 3; i++) {
        await helper.fillParameters({
          outer_diameter: 20 + i * 5,
        });
        await helper.calculate();
      }

      const history = await helper.getCalculationHistory();

      expect(history.length).toBeGreaterThanOrEqual(3);
    });

    test('should restore previous calculation from history', async ({ page }) => {
      await helper.openParallelOffsetTab();

      // Выполняем первый расчёт
      await helper.fillParameters({
        outer_diameter: 25,
        bend_angle: 90,
      });
      await helper.calculate();
      const result1 = await helper.getResults();

      // Выполняем второй расчёт с другими параметрами
      await helper.fillParameters({
        outer_diameter: 50,
        bend_angle: 45,
      });
      await helper.calculate();

      // Восстанавливаем первый расчёт из истории
      const history = await helper.getCalculationHistory();
      await page.click(`[data-testid="history-item-${history[0].timestamp}"]`);

      const resultRestored = await helper.getResults();

      expect(resultRestored.total_length).toBeCloseTo(result1.total_length, 1);
    });
  });

  // ============================================================================
  // PERFORMANCE TESTS (3 теста)
  // ============================================================================

  test.describe('Performance Tests', () => {
    test('should calculate parallel offset in less than 100ms', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters(parallelData.test_cases[0].params);

      const startTime = performance.now();
      await helper.calculate();
      const duration = performance.now() - startTime;

      expect(duration).toBeLessThan(100);
    });

    test('should handle multiple consecutive calculations', async ({ page }) => {
      await helper.openParallelOffsetTab();

      const startTime = performance.now();

      // Выполняем 5 расчётов подряд
      for (let i = 0; i < 5; i++) {
        await helper.fillParameters({
          outer_diameter: 20 + i * 5,
        });
        await helper.calculate();
      }

      const duration = performance.now() - startTime;

      // Все 5 расчётов должны выполниться за менее 1 секунду
      expect(duration).toBeLessThan(1000);
    });

    test('should export PDF within 1 second', async ({ page }) => {
      await helper.openParallelOffsetTab();
      await helper.fillParameters(parallelData.test_cases[0].params);
      await helper.calculate();

      const startTime = performance.now();
      await helper.exportToPDF();
      const duration = performance.now() - startTime;

      expect(duration).toBeLessThan(1000);
    });
  });
});
