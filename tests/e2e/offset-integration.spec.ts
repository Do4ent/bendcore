/**
 * Offset Modules Integration - Comprehensive E2E Tests
 *
 * Тестирование интеграции параллельного и комбинированного 3D смещения
 * Включает: параллельные операции, синхронизация, переключение, история
 *
 * Target: 15+ тестов
 */

import { test, expect } from '@playwright/test';
import { ParallelOffsetHelper, Combined3DOffsetHelper, OffsetIntegrationHelper, PerformanceHelper } from './helpers/offset-utils';
import parallelData from './fixtures/parallel-data.json';
import combined3dData from './fixtures/combined-3d-data.json';

test.describe('Offset Modules Integration - E2E Tests', () => {
  let parallelHelper: ParallelOffsetHelper;
  let threeHelper: Combined3DOffsetHelper;
  let integrationHelper: OffsetIntegrationHelper;

  test.beforeEach(async ({ page }) => {
    parallelHelper = new ParallelOffsetHelper(page);
    threeHelper = new Combined3DOffsetHelper(page);
    integrationHelper = new OffsetIntegrationHelper(page);

    await page.goto('/');
  });

  // ============================================================================
  // PARALLEL OPERATIONS (2 теста)
  // ============================================================================

  test.describe('Parallel Operations', () => {
    test('should handle multiple simultaneous calculations', async ({ page }) => {
      // Открываем несколько вкладок и запускаем расчёты одновременно
      const calculations = [
        {
          name: 'calc1',
          params: parallelData.test_cases[0].params,
        },
        {
          name: 'calc2',
          params: parallelData.test_cases[1].params,
        },
        {
          name: 'calc3',
          params: combined3dData.test_cases[0].params,
        },
      ];

      const results = await integrationHelper.parallelCalculations(calculations);

      expect(results.size).toBe(3);
      expect(results.get('calc1')).toBeDefined();
      expect(results.get('calc2')).toBeDefined();
      expect(results.get('calc3')).toBeDefined();
    });

    test('should handle concurrent exports', async ({ page }) => {
      await parallelHelper.openParallelOffsetTab();
      await parallelHelper.fillParameters(parallelData.test_cases[0].params);
      await parallelHelper.calculate();

      const exports = await integrationHelper.exportMultiple();

      expect(exports.length).toBe(2);
      expect(exports[0]).toMatch(/\.pdf$/);
      expect(exports[1]).toMatch(/\.dxf$/);
    });
  });

  // ============================================================================
  // DATA SYNCHRONIZATION (4 теста)
  // ============================================================================

  test.describe('Data Synchronization', () => {
    test('should sync OD changes between Parallel and 3D modules', async ({ page }) => {
      // Устанавливаем OD в параллельном смещении
      await parallelHelper.openParallelOffsetTab();
      const testOD = 38.1;
      await parallelHelper.fillParameters({ outer_diameter: testOD });
      await parallelHelper.calculate();

      // Переходим на 3D и проверяем, что OD синхронизирован
      await threeHelper.open3DOffsetTab();
      const savedOD = await page.inputValue('[data-testid="input-outer-diameter"]');

      expect(parseFloat(savedOD)).toBe(testOD);
    });

    test('should sync results to main tube parameters', async ({ page }) => {
      await parallelHelper.openParallelOffsetTab();
      await parallelHelper.fillParameters(parallelData.test_cases[0].params);
      await parallelHelper.calculate();

      const isSynced = await parallelHelper.verifyResultsSync();

      expect(isSynced).toBe(true);
    });

    test('should maintain state when switching between modules', async ({ page }) => {
      await integrationHelper.switchOffsetTypesWithoutLoss();

      // Метод verifyresults по проверке сохранения
      const savedParams = await page.evaluate(() => {
        return {
          diameter: (document.querySelector('[data-testid="input-outer-diameter"]') as HTMLInputElement)?.value,
          thickness: (document.querySelector('[data-testid="input-wall-thickness"]') as HTMLInputElement)?.value,
        };
      });

      expect(parseFloat(savedParams.diameter)).toBe(25);
      expect(parseFloat(savedParams.thickness)).toBe(2.0);
    });

    test('should update derived calculations when parameters change', async ({ page }) => {
      await parallelHelper.openParallelOffsetTab();
      await parallelHelper.fillParameters(parallelData.test_cases[0].params);
      await parallelHelper.calculate();

      const result1 = await parallelHelper.getResults();

      // Изменяем параметр
      await parallelHelper.fillParameters({ offset_distance: 100 });
      await parallelHelper.calculate();

      const result2 = await parallelHelper.getResults();

      // Результаты должны измениться
      expect(result2.total_length).not.toEqual(result1.total_length);
    });
  });

  // ============================================================================
  // MODULE SWITCHING (3 теста)
  // ============================================================================

  test.describe('Module Switching', () => {
    test('should switch from Parallel to 3D without data loss', async ({ page }) => {
      await parallelHelper.openParallelOffsetTab();
      await parallelHelper.fillParameters({
        outer_diameter: 25,
        wall_thickness: 2.0,
        bend_angle: 90,
        offset_distance: 50,
        bend_radius: 40,
        material: 'Steel',
      });
      await parallelHelper.calculate();

      const result1 = await parallelHelper.getResults();

      // Переключаемся на 3D
      await threeHelper.open3DOffsetTab();

      // Возвращаемся на Parallel
      await parallelHelper.openParallelOffsetTab();

      const result2 = await parallelHelper.getResults();

      // Результаты должны совпадать
      expect(result2.total_length).toEqual(result1.total_length);
    });

    test('should switch from 3D to Parallel without data loss', async ({ page }) => {
      await threeHelper.open3DOffsetTab();
      await threeHelper.fillCoordinates(
        combined3dData.test_cases[0].params.start_point,
        combined3dData.test_cases[0].params.end_point
      );
      await threeHelper.fill3DParameters(combined3dData.test_cases[0].params);
      await threeHelper.calculate3D();

      const result1 = await threeHelper.get3DResults();

      // Переключаемся на Parallel
      await parallelHelper.openParallelOffsetTab();

      // Возвращаемся на 3D
      await threeHelper.open3DOffsetTab();

      const result2 = await threeHelper.get3DResults();

      // Результаты должны совпадать
      expect(result2.total_length).toEqual(result1.total_length);
    });

    test('should maintain separate state for each module', async ({ page }) => {
      // Заполняем параллельное смещение
      await parallelHelper.openParallelOffsetTab();
      await parallelHelper.fillParameters({
        outer_diameter: 25,
        wall_thickness: 2.0,
      });

      // Переходим на 3D и заполняем другие параметры
      await threeHelper.open3DOffsetTab();
      await threeHelper.fill3DParameters({
        num_bends: 5,
      });

      // Проверяем, что параллельное смещение всё ещё имеет свои параметры
      await parallelHelper.openParallelOffsetTab();

      const diameterValue = await page.inputValue('[data-testid="input-outer-diameter"]');
      expect(parseFloat(diameterValue)).toBe(25);
    });
  });

  // ============================================================================
  // CALCULATION HISTORY (3 теста)
  // ============================================================================

  test.describe('Calculation History', () => {
    test('should maintain calculation history', async ({ page }) => {
      await parallelHelper.openParallelOffsetTab();

      // Выполняем 3 расчёта
      for (let i = 0; i < 3; i++) {
        await parallelHelper.fillParameters({
          outer_diameter: 20 + i * 5,
        });
        await parallelHelper.calculate();
        await page.waitForTimeout(200);
      }

      const history = await parallelHelper.getCalculationHistory();

      expect(history.length).toBeGreaterThanOrEqual(3);
    });

    test('should restore previous calculation from history', async ({ page }) => {
      const verifyHistoryRestored = await integrationHelper.verifyHistoryRestoration();

      expect(verifyHistoryRestored).toBe(true);
    });

    test('should track both Parallel and 3D calculations in unified history', async ({ page }) => {
      // Расчёт в Parallel
      await parallelHelper.openParallelOffsetTab();
      await parallelHelper.fillParameters(parallelData.test_cases[0].params);
      await parallelHelper.calculate();
      await page.waitForTimeout(200);

      // Расчёт в 3D
      await threeHelper.open3DOffsetTab();
      await threeHelper.fillCoordinates(
        combined3dData.test_cases[0].params.start_point,
        combined3dData.test_cases[0].params.end_point
      );
      await threeHelper.fill3DParameters(combined3dData.test_cases[0].params);
      await threeHelper.calculate3D();
      await page.waitForTimeout(200);

      // Расчёт снова в Parallel
      await parallelHelper.openParallelOffsetTab();
      await parallelHelper.fillParameters(parallelData.test_cases[1].params);
      await parallelHelper.calculate();

      const history = await parallelHelper.getCalculationHistory();

      // История должна содержать все расчёты
      expect(history.length).toBeGreaterThanOrEqual(3);
    });
  });

  // ============================================================================
  // EXPORT INTEGRATION (2 теста)
  // ============================================================================

  test.describe('Export Integration', () => {
    test('should export both Parallel and 3D results', async ({ page }) => {
      // Параллельное смещение -> PDF
      await parallelHelper.openParallelOffsetTab();
      await parallelHelper.fillParameters(parallelData.test_cases[0].params);
      await parallelHelper.calculate();

      const downloadPromise1 = page.waitForEvent('download');
      await page.click('[data-testid="button-export-pdf"]');
      const download1 = await downloadPromise1;

      expect(download1.suggestedFilename()).toMatch(/\.pdf$/);

      // 3D смещение -> DXF
      await threeHelper.open3DOffsetTab();
      await threeHelper.fillCoordinates(
        combined3dData.test_cases[0].params.start_point,
        combined3dData.test_cases[0].params.end_point
      );
      await threeHelper.fill3DParameters(combined3dData.test_cases[0].params);
      await threeHelper.calculate3D();

      const downloadPromise2 = page.waitForEvent('download');
      await page.click('[data-testid="button-export-dxf"]');
      const download2 = await downloadPromise2;

      expect(download2.suggestedFilename()).toMatch(/\.dxf$/);
    });

    test('should export combined report with both calculations', async ({ page }) => {
      // Выполняем расчёты в обоих модулях
      await parallelHelper.openParallelOffsetTab();
      await parallelHelper.fillParameters(parallelData.test_cases[0].params);
      await parallelHelper.calculate();

      await threeHelper.open3DOffsetTab();
      await threeHelper.fillCoordinates(
        combined3dData.test_cases[0].params.start_point,
        combined3dData.test_cases[0].params.end_point
      );
      await threeHelper.fill3DParameters(combined3dData.test_cases[0].params);
      await threeHelper.calculate3D();

      // Экспортируем объединённый отчёт
      const downloadPromise = page.waitForEvent('download');
      await page.click('[data-testid="button-export-combined-report"]');
      const download = await downloadPromise;

      expect(download.suggestedFilename()).toMatch(/report|combined/i);
    });
  });

  // ============================================================================
  // PERFORMANCE INTEGRATION (2 теста)
  // ============================================================================

  test.describe('Performance Integration', () => {
    test('should handle sequential calculations within time budget', async ({ page }) => {
      await parallelHelper.openParallelOffsetTab();

      const startTime = performance.now();

      // Параллельное смещение
      for (let i = 0; i < 3; i++) {
        await parallelHelper.fillParameters({
          outer_diameter: 20 + i * 5,
        });
        await parallelHelper.calculate();
      }

      // 3D смещение
      await threeHelper.open3DOffsetTab();
      for (let i = 0; i < 2; i++) {
        await threeHelper.fillCoordinates(
          { x: 0, y: 0, z: 0 },
          { x: 100 + i * 50, y: 100 + i * 50, z: 50 }
        );
        await threeHelper.fill3DParameters({
          num_bends: 2 + i,
        });
        await threeHelper.calculate3D();
      }

      const duration = performance.now() - startTime;

      // Все 5 расчётов должны выполниться за менее 3 секунд
      expect(duration).toBeLessThan(3000);
    });

    test('should maintain performance with complex workflows', async ({ page }) => {
      const startTime = performance.now();

      // Сложный workflow
      for (let i = 0; i < 2; i++) {
        await parallelHelper.openParallelOffsetTab();
        await parallelHelper.fillParameters(parallelData.test_cases[i].params);
        await parallelHelper.calculate();

        await threeHelper.open3DOffsetTab();
        await threeHelper.fillCoordinates(
          combined3dData.test_cases[i].params.start_point,
          combined3dData.test_cases[i].params.end_point
        );
        await threeHelper.fill3DParameters(combined3dData.test_cases[i].params);
        await threeHelper.calculate3D();

        // Экспортируем
        const downloadPromise = page.waitForEvent('download');
        await page.click('[data-testid="button-export-pdf"]');
        await downloadPromise;
      }

      const duration = performance.now() - startTime;

      // Весь workflow должен выполниться за менее 5 секунд
      expect(duration).toBeLessThan(5000);
    });
  });

  // ============================================================================
  // ERROR RECOVERY (2 теста)
  // ============================================================================

  test.describe('Error Recovery', () => {
    test('should recover from invalid input in Parallel module', async ({ page }) => {
      await parallelHelper.openParallelOffsetTab();

      // Вводим невалидные данные
      await parallelHelper.fillParameters({
        outer_diameter: -25,
      });
      await parallelHelper.calculate();

      let hasErrors = (await parallelHelper.checkErrorBorders()).length > 0;
      expect(hasErrors).toBe(true);

      // Исправляем ошибку
      await parallelHelper.fillParameters({
        outer_diameter: 25,
      });

      hasErrors = (await parallelHelper.checkErrorBorders()).length > 0;
      expect(hasErrors).toBe(false);

      // Расчёт должен работать
      await parallelHelper.calculate();
      const results = await parallelHelper.getResults();
      expect(results.is_valid).toBe(true);
    });

    test('should recover from invalid coordinates in 3D module', async ({ page }) => {
      await threeHelper.open3DOffsetTab();

      // Вводим одинаковые координаты (невалидно)
      await threeHelper.fillCoordinates(
        { x: 0, y: 0, z: 0 },
        { x: 0, y: 0, z: 0 }
      );

      // Система должна предотвратить некорректный расчёт или показать warning
      const results = await threeHelper.get3DResults();

      // Либо feasibility очень низкая, либо есть error
      expect(results.feasibility_score).toBeLessThan(50);

      // Исправляем координаты
      await threeHelper.fillCoordinates(
        { x: 0, y: 0, z: 0 },
        { x: 100, y: 100, z: 50 }
      );
      await threeHelper.calculate3D();

      const correctedResults = await threeHelper.get3DResults();
      expect(correctedResults.feasibility_score).toBeGreaterThan(50);
    });
  });

  // ============================================================================
  // USER WORKFLOW SCENARIOS (2 теста)
  // ============================================================================

  test.describe('User Workflow Scenarios', () => {
    test('should support complete parallel offset design workflow', async ({ page }) => {
      // 1. Открываем калькулятор
      await parallelHelper.openParallelOffsetTab();

      // 2. Вводим параметры
      await parallelHelper.fillParameters(parallelData.test_cases[0].params);

      // 3. Рассчитываем
      await parallelHelper.calculate();

      // 4. Проверяем результаты
      const results = await parallelHelper.getResults();
      expect(results.is_valid).toBe(true);

      // 5. Сохраняем как шаблон
      const templateName = `Workflow Template ${Date.now()}`;
      await parallelHelper.saveTemplate(templateName);

      // 6. Экспортируем PDF
      await parallelHelper.exportToPDF();

      // 7. Проверяем синхронизацию
      const isSynced = await parallelHelper.verifyResultsSync();
      expect(isSynced).toBe(true);

      // 8. Проверяем историю
      const history = await parallelHelper.getCalculationHistory();
      expect(history.length).toBeGreaterThan(0);
    });

    test('should support complete 3D offset design workflow', async ({ page }) => {
      // 1. Открываем 3D калькулятор
      await threeHelper.open3DOffsetTab();

      // 2. Вводим координаты
      const testCase = combined3dData.test_cases[0];
      await threeHelper.fillCoordinates(testCase.params.start_point, testCase.params.end_point);

      // 3. Вводим параметры
      await threeHelper.fill3DParameters(testCase.params);

      // 4. Рассчитываем
      await threeHelper.calculate3D();

      // 5. Проверяем 3D превью
      const hasPreview = await threeHelper.verify3DPreview();
      expect(hasPreview).toBe(true);

      // 6. Тестируем интерактивность
      await threeHelper.testMouseDragRotate();
      await threeHelper.testMouseScrollZoom();

      // 7. Получаем G-код
      const gcode = await threeHelper.getGCode();
      expect(gcode.length).toBeGreaterThan(0);

      // 8. Валидируем G-код
      const validation = await threeHelper.validateGCode(gcode);
      expect(validation.valid).toBe(true);

      // 9. Экспортируем DXF
      await threeHelper.exportDXF();

      // 10. Копируем G-код
      await threeHelper.copyGCode();
    });
  });
});
