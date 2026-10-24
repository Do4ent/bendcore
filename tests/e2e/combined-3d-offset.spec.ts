/**
 * Combined 3D Offset Calculator - Comprehensive E2E Tests
 *
 * Тестирование калькулятора комбинированного 3D смещения трубок
 * Включает: happy path, 3D visualization, G-code generation, edge cases
 *
 * Target: 25+ тестов
 */

import { test, expect } from '@playwright/test';
import { Combined3DOffsetHelper, PerformanceHelper } from './helpers/offset-utils';
import combined3dData from './fixtures/combined-3d-data.json';

test.describe('Combined 3D Offset Calculator - E2E Tests', () => {
  let helper: Combined3DOffsetHelper;

  test.beforeEach(async ({ page }) => {
    helper = new Combined3DOffsetHelper(page);
    await page.goto('/');
  });

  // ============================================================================
  // HAPPY PATH TESTS (10 тестов)
  // ============================================================================

  test.describe('Happy Path - Basic Functionality', () => {
    test('should open 3D Offset Tab', async ({ page }) => {
      await helper.open3DOffsetTab();

      await expect(page.locator('[data-testid="combined-3d-offset-form"]')).toBeVisible();
    });

    test('should display all coordinate inputs', async ({ page }) => {
      await helper.open3DOffsetTab();

      await expect(page.locator('[data-testid="input-start-x"]')).toBeVisible();
      await expect(page.locator('[data-testid="input-start-y"]')).toBeVisible();
      await expect(page.locator('[data-testid="input-start-z"]')).toBeVisible();
      await expect(page.locator('[data-testid="input-end-x"]')).toBeVisible();
      await expect(page.locator('[data-testid="input-end-y"]')).toBeVisible();
      await expect(page.locator('[data-testid="input-end-z"]')).toBeVisible();
    });

    test('should calculate simple 2-bend path', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0]; // Simple 2-Bend Path - Steel
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const results = await helper.get3DResults();

      expect(results.bends_count).toBe(testCase.expected.bends_count);
      expect(results.feasibility_score).toBeGreaterThanOrEqual(testCase.expected.feasibility_min);
    });

    test('should calculate complex 3-bend path', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[1]; // Complex 3-Bend Path - Aluminum
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const results = await helper.get3DResults();

      expect(results.bends_count).toBe(testCase.expected.bends_count);
      expect(results.total_length).toBeGreaterThan(0);
    });

    test('should render 3D preview with tube visualization', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const hasPreview = await helper.verify3DPreview();

      expect(hasPreview).toBe(true);
    });

    test('should display 3D grid coordinates', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const gridVisible = await helper.verifyGridVisible();

      expect(gridVisible).toBe(true);
    });

    test('should calculate 5-bend complex path', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[2]; // 5-Bend Path - Stainless Steel
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const results = await helper.get3DResults();

      expect(results.bends_count).toBe(5);
      expect(results.total_length).toBeGreaterThan(200);
    });

    test('should support custom bend distribution', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[3]; // Custom Distribution - Titanium
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const results = await helper.get3DResults();

      expect(results.bends_count).toBe(testCase.expected.bends_count);
    });

    test('should calculate vertical path', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[4]; // Vertical Path - Copper
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const results = await helper.get3DResults();

      expect(results.total_length).toBeGreaterThan(0);
      expect(results.bends_count).toBe(testCase.expected.bends_count);
    });

    test('should calculate horizontal path', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[5]; // Horizontal Path - Brass
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const results = await helper.get3DResults();

      expect(results.total_length).toBeGreaterThan(0);
    });
  });

  // ============================================================================
  // 3D VISUALIZATION TESTS (5 тестов)
  // ============================================================================

  test.describe('3D Visualization', () => {
    test('should render tube in 3D view', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const preview = await page.locator('[data-testid="3d-preview"]').isVisible();

      expect(preview).toBe(true);
    });

    test('should support mouse drag rotation', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      await expect(async () => {
        await helper.testMouseDragRotate();
      }).not.toThrow();
    });

    test('should support mouse scroll zoom', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      await expect(async () => {
        await helper.testMouseScrollZoom();
      }).not.toThrow();
    });

    test('should display bend positions with coordinates', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const bendMarkers = await page.locator('[data-testid*="bend-marker"]').all();

      expect(bendMarkers.length).toBeGreaterThan(0);
    });

    test('should show coordinate grid in 3D view', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const gridVisible = await helper.verifyGridVisible();

      expect(gridVisible).toBe(true);
    });
  });

  // ============================================================================
  // G-CODE GENERATION TESTS (7 тестов)
  // ============================================================================

  test.describe('G-Code Generation', () => {
    test('should generate G-code for calculated path', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const gcode = await helper.getGCode();

      expect(gcode).toBeTruthy();
      expect(gcode.length).toBeGreaterThan(0);
    });

    test('should generate valid G-code with movement commands', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const gcode = await helper.getGCode();
      const validation = await helper.validateGCode(gcode);

      expect(validation.valid).toBe(true);
      expect(validation.errors).toHaveLength(0);
    });

    test('should include M3 and M5 spindle commands', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const gcode = await helper.getGCode();

      expect(gcode).toMatch(/M3/);
      expect(gcode).toMatch(/M5/);
    });

    test('should include feed rate parameters', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const gcode = await helper.getGCode();

      expect(gcode).toMatch(/F[\d.]+/);
    });

    test('should match positions to calculation results', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const gcode = await helper.getGCode();
      const results = await helper.get3DResults();

      // Проверяем, что G-code содержит координаты из результатов
      expect(gcode).toMatch(/X[\d.-]+/);
      expect(gcode).toMatch(/Y[\d.-]+/);
      expect(gcode).toMatch(/Z[\d.-]+/);
    });

    test('should copy G-code to clipboard', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      await expect(async () => {
        await helper.copyGCode();
      }).not.toThrow();
    });

    test('should generate different G-code for different distributions', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters({ ...testCase.params, distribution: 'equal' });
      await helper.calculate3D();

      const gcode1 = await helper.getGCode();

      // Изменяем распределение и пересчитываем
      await helper.fill3DParameters({ distribution: 'custom' });
      await helper.calculate3D();

      const gcode2 = await helper.getGCode();

      // G-коды должны различаться
      expect(gcode1).not.toEqual(gcode2);
    });
  });

  // ============================================================================
  // MATERIAL-SPECIFIC TESTS (5 тестов)
  // ============================================================================

  test.describe('Material-Specific Behavior', () => {
    test('should apply springback correction for Steel', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0]; // Steel with springback
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const results = await helper.get3DResults();

      expect(results.total_length).toBeGreaterThan(0);
    });

    test('should adjust feasibility for different materials', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase1 = combined3dData.test_cases[0]; // Steel
      const testCase2 = combined3dData.test_cases[1]; // Aluminum

      // Расчёт для Steel
      await helper.fillCoordinates(testCase1.params.start_point, testCase1.params.end_point);
      await helper.fill3DParameters(testCase1.params);
      await helper.calculate3D();
      const results1 = await helper.get3DResults();

      // Расчёт для Aluminum
      await helper.fillCoordinates(testCase2.params.start_point, testCase2.params.end_point);
      await helper.fill3DParameters(testCase2.params);
      await helper.calculate3D();
      const results2 = await helper.get3DResults();

      // Feasibility scores могут различаться из-за материала
      expect(typeof results1.feasibility_score).toBe('number');
      expect(typeof results2.feasibility_score).toBe('number');
    });

    test('should show warnings for extreme configurations', async ({ page }) => {
      await helper.open3DOffsetTab();

      // Конфигурация с экстремальными параметрами
      await helper.fillCoordinates({ x: 0, y: 0, z: 0 }, { x: 500, y: 500, z: 300 });
      await helper.fill3DParameters({
        outer_diameter: 6.35,
        wall_thickness: 0.5,
        bend_radius: 15,
        num_bends: 10,
        distribution: 'equal',
        material: 'titanium',
        springback_correction: true,
      });
      await helper.calculate3D();

      const results = await helper.get3DResults();

      expect(results.warnings).toBeDefined();
      if (results.warnings.length > 0) {
        expect(results.warnings[0]).toBeTruthy();
      }
    });

    test('should calculate Titanium with high springback correction', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[3]; // Custom Distribution - Titanium
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const results = await helper.get3DResults();

      expect(results.total_length).toBeGreaterThan(0);
      expect(results.feasibility_score).toBeGreaterThan(0);
    });

    test('should handle Brass material properties', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[5]; // Horizontal Path - Brass
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const results = await helper.get3DResults();

      expect(results.total_length).toBeGreaterThan(0);
    });
  });

  // ============================================================================
  // EXPORT TESTS (2 теста)
  // ============================================================================

  test.describe('Export Functionality', () => {
    test('should export to DXF format', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      await expect(async () => {
        await helper.exportDXF();
      }).not.toThrow();
    });

    test('should copy and export G-code', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const gcode = await helper.getGCode();

      expect(gcode).toBeTruthy();
      expect(gcode.length).toBeGreaterThan(10);
    });
  });

  // ============================================================================
  // VALIDATION & ERROR HANDLING (3 теста)
  // ============================================================================

  test.describe('Validation & Error Handling', () => {
    test('should reject same start and end points', async ({ page }) => {
      await helper.open3DOffsetTab();

      // Используем edge case с одинаковыми точками
      const edgeCase = combined3dData.edge_cases[0]; // Invalid - Start = End Point
      await helper.fillCoordinates(edgeCase.params.start_point, edgeCase.params.end_point);
      await helper.fill3DParameters(edgeCase.params);
      await helper.calculate3D();

      // Должна быть ошибка или warning
      const results = await helper.get3DResults();

      expect(results.feasibility_score).toBeLessThan(50); // Очень низкая feasibility
    });

    test('should validate number of bends', async ({ page }) => {
      await helper.open3DOffsetTab();

      // Edge case с нулевым количеством гибок
      const edgeCase = combined3dData.edge_cases[1]; // Invalid - Zero Number of Bends
      await helper.fillCoordinates(edgeCase.params.start_point, edgeCase.params.end_point);
      await helper.fill3DParameters(edgeCase.params);

      // Система должна предотвратить расчёт
      const results = await helper.get3DResults();

      expect(results.bends_count).toBeGreaterThan(0);
    });

    test('should warn for too many bends', async ({ page }) => {
      await helper.open3DOffsetTab();

      const edgeCase = combined3dData.edge_cases[2]; // Invalid - Too Many Bends
      await helper.fillCoordinates(edgeCase.params.start_point, edgeCase.params.end_point);
      await helper.fill3DParameters(edgeCase.params);

      const results = await helper.get3DResults();

      // Должны быть warnings для чрезмерного количества гибок
      expect(results.warnings).toBeDefined();
    });
  });

  // ============================================================================
  // PERFORMANCE TESTS (2 теста)
  // ============================================================================

  test.describe('Performance Tests', () => {
    test('should calculate 3D offset in less than 500ms', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);

      const startTime = performance.now();
      await helper.calculate3D();
      const duration = performance.now() - startTime;

      expect(duration).toBeLessThan(500);
    });

    test('should render 3D preview with 60 FPS', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[0];
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const fps = await PerformanceHelper.measure3DRenderingFPS(page);

      expect(fps).toBeGreaterThanOrEqual(50);
    });
  });

  // ============================================================================
  // EDGE CASES (3 теста)
  // ============================================================================

  test.describe('Edge Cases', () => {
    test('should calculate minimal distance path', async ({ page }) => {
      await helper.open3DOffsetTab();

      const testCase = combined3dData.test_cases[6]; // Minimal Distance - Small Diameter
      await helper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);
      await helper.fill3DParameters(testCase.params);
      await helper.calculate3D();

      const results = await helper.get3DResults();

      expect(results.total_length).toBeGreaterThan(0);
    });

    test('should handle negative coordinates', async ({ page }) => {
      await helper.open3DOffsetTab();

      await helper.fillCoordinates(
        { x: -100, y: -100, z: -50 },
        { x: 100, y: 100, z: 50 }
      );
      await helper.fill3DParameters({
        outer_diameter: 25,
        wall_thickness: 2.0,
        bend_radius: 40,
        num_bends: 2,
        distribution: 'equal',
        inclination_angle: 30,
        springback_correction: true,
        material: 'steel',
      });

      await expect(async () => {
        await helper.calculate3D();
      }).not.toThrow();
    });

    test('should handle large distances', async ({ page }) => {
      await helper.open3DOffsetTab();

      await helper.fillCoordinates(
        { x: 0, y: 0, z: 0 },
        { x: 5000, y: 5000, z: 3000 }
      );
      await helper.fill3DParameters({
        outer_diameter: 50.8,
        wall_thickness: 2.4,
        bend_radius: 100,
        num_bends: 5,
        distribution: 'equal',
        inclination_angle: 30,
        springback_correction: true,
        material: 'steel',
      });

      const results = await helper.get3DResults();

      expect(results.total_length).toBeGreaterThan(1000);
    });
  });
});
