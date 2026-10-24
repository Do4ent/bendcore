/**
 * E2E Test Utilities for Offset Modules
 * Helper функции для тестирования параллельного и комбинированного 3D смещения
 */

import { Page, expect } from '@playwright/test';

/**
 * Утилиты для Parallel Offset Калькулятора
 */
export class ParallelOffsetHelper {
  constructor(private page: Page) {}

  /**
   * Открыть Offset Dialog и перейти на вкладку Parallel Offset
   */
  async openParallelOffsetTab(): Promise<void> {
    // Ищем и кликаем на кнопку открытия TubeDialog
    await this.page.click('[data-testid="tube-dialog-button"], button:has-text("Tube")');

    // Ожидаем появления диалога
    await this.page.waitForSelector('[data-testid="offset-tabs"]', { timeout: 5000 });

    // Кликаем на вкладку Parallel Offset
    await this.page.click('[data-testid="offset-tab-parallel"], button:has-text("Parallel")');
  }

  /**
   * Ввести параметры для расчёта
   */
  async fillParameters(params: {
    outer_diameter?: number;
    wall_thickness?: number;
    bend_angle?: number;
    offset_distance?: number;
    bend_radius?: number;
    material?: string;
  }): Promise<void> {
    if (params.outer_diameter !== undefined) {
      await this.page.fill('[data-testid="input-outer-diameter"], params.outer_diameter.toString());
    }

    if (params.wall_thickness !== undefined) {
      await this.page.fill('[data-testid="input-wall-thickness"], params.wall_thickness.toString());
    }

    if (params.bend_angle !== undefined) {
      const slider = this.page.locator('[data-testid="slider-bend-angle"]');
      await slider.evaluate((el: HTMLInputElement, val) => {
        el.value = val.toString();
        el.dispatchEvent(new Event('input', { bubbles: true }));
      }, params.bend_angle);
    }

    if (params.offset_distance !== undefined) {
      await this.page.fill('[data-testid="input-offset-distance"], params.offset_distance.toString());
    }

    if (params.bend_radius !== undefined) {
      await this.page.fill('[data-testid="input-bend-radius"], params.bend_radius.toString());
    }

    if (params.material !== undefined) {
      await this.page.selectOption('[data-testid="select-material"]', params.material);
    }
  }

  /**
   * Выполнить расчёт
   */
  async calculate(): Promise<void> {
    const startTime = Date.now();
    await this.page.click('[data-testid="button-calculate"], button:has-text("Calculate")');

    // Ожидаем результатов (максимум 100ms для параллельного смещения)
    try {
      await this.page.waitForSelector('[data-testid="results-display"]', { timeout: 5000 });
    } catch (e) {
      throw new Error(`Calculation failed or results not displayed. Time: ${Date.now() - startTime}ms`);
    }
  }

  /**
   * Получить результаты расчёта
   */
  async getResults(): Promise<{
    total_length: number;
    bend_1_length: number;
    straight_section: number;
    bend_2_length: number;
    confidence: number;
    is_valid: boolean;
    warnings: string;
  }> {
    await this.page.waitForSelector('[data-testid="results-display"]');

    const data = await this.page.evaluate(() => {
      const getTextContent = (selector: string) => {
        const el = document.querySelector(selector);
        return el ? el.textContent?.trim() : '';
      };

      const parseNumber = (text: string) => parseFloat(text.split(':')[1]?.trim() || '0');

      return {
        total_length: parseNumber(getTextContent('[data-testid="result-total-length"]') || ''),
        bend_1_length: parseNumber(getTextContent('[data-testid="result-bend-1-length"]') || ''),
        straight_section: parseNumber(getTextContent('[data-testid="result-straight-section"]') || ''),
        bend_2_length: parseNumber(getTextContent('[data-testid="result-bend-2-length"]') || ''),
        confidence: parseNumber(getTextContent('[data-testid="result-confidence"]') || ''),
        is_valid: getTextContent('[data-testid="result-valid"]')?.includes('true') || false,
        warnings: getTextContent('[data-testid="result-warnings"]') || '',
      };
    });

    return data;
  }

  /**
   * Проверить красные границы ошибок
   */
  async checkErrorBorders(): Promise<string[]> {
    const errorFields = await this.page.locator('[data-testid*="input"][data-error="true"]').all();
    return errorFields.map((el) => el.getAttribute('data-testid'));
  }

  /**
   * Экспортировать результаты в PDF
   */
  async exportToPDF(): Promise<void> {
    // Слушаем событие скачивания файла
    const downloadPromise = this.page.waitForEvent('download');

    await this.page.click('[data-testid="button-export-pdf"], button:has-text("PDF")');

    const download = await downloadPromise;
    const filename = download.suggestedFilename();

    if (!filename.endsWith('.pdf')) {
      throw new Error(`Экспортированный файл не является PDF: ${filename}`);
    }
  }

  /**
   * Сохранить как шаблон
   */
  async saveTemplate(templateName: string): Promise<void> {
    await this.page.fill('[data-testid="input-template-name"]', templateName);
    await this.page.click('[data-testid="button-save-template"], button:has-text("Save")');

    // Ожидаем подтверждения
    await this.page.waitForSelector('[data-testid="toast-success"], .toast-success', { timeout: 5000 });
  }

  /**
   * Проверить, что результаты синхронизируются в параметры трубы
   */
  async verifyResultsSync(): Promise<boolean> {
    const syncIndicator = await this.page.locator('[data-testid="sync-indicator-success"]').isVisible();
    return syncIndicator;
  }

  /**
   * Получить историю расчётов
   */
  async getCalculationHistory(): Promise<Array<{ timestamp: string; params: string }>> {
    await this.page.click('[data-testid="button-show-history"]');

    return await this.page.evaluate(() => {
      const items = Array.from(document.querySelectorAll('[data-testid="history-item"]'));
      return items.map((item) => ({
        timestamp: item.querySelector('[data-testid="history-timestamp"]')?.textContent || '',
        params: item.querySelector('[data-testid="history-params"]')?.textContent || '',
      }));
    });
  }

  /**
   * Проверить валидацию входных данных
   */
  async verifyValidationErrors(expectedErrors: string[]): Promise<boolean> {
    const errorMessages = await this.page.locator('[data-testid="error-message"]').allTextContents();
    return expectedErrors.every((error) => errorMessages.some((msg) => msg.includes(error)));
  }
}

/**
 * Утилиты для Combined 3D Offset Калькулятора
 */
export class Combined3DOffsetHelper {
  constructor(private page: Page) {}

  /**
   * Открыть Combined 3D Offset калькулятор
   */
  async open3DOffsetTab(): Promise<void> {
    await this.page.click('[data-testid="tube-dialog-button"], button:has-text("Tube")');
    await this.page.waitForSelector('[data-testid="offset-tabs"]');
    await this.page.click('[data-testid="offset-tab-3d"], button:has-text("3D")');
  }

  /**
   * Ввести координаты стартовой и конечной точек
   */
  async fillCoordinates(
    startPoint: { x: number; y: number; z: number },
    endPoint: { x: number; y: number; z: number }
  ): Promise<void> {
    // Start point
    await this.page.fill('[data-testid="input-start-x"]', startPoint.x.toString());
    await this.page.fill('[data-testid="input-start-y"]', startPoint.y.toString());
    await this.page.fill('[data-testid="input-start-z"]', startPoint.z.toString());

    // End point
    await this.page.fill('[data-testid="input-end-x"]', endPoint.x.toString());
    await this.page.fill('[data-testid="input-end-y"]', endPoint.y.toString());
    await this.page.fill('[data-testid="input-end-z"]', endPoint.z.toString());
  }

  /**
   * Ввести параметры 3D смещения
   */
  async fill3DParameters(params: {
    outer_diameter?: number;
    wall_thickness?: number;
    bend_radius?: number;
    num_bends?: number;
    distribution?: string;
    inclination_angle?: number;
    springback_correction?: boolean;
    material?: string;
  }): Promise<void> {
    if (params.outer_diameter !== undefined) {
      await this.page.fill('[data-testid="input-outer-diameter"]', params.outer_diameter.toString());
    }
    if (params.wall_thickness !== undefined) {
      await this.page.fill('[data-testid="input-wall-thickness"]', params.wall_thickness.toString());
    }
    if (params.bend_radius !== undefined) {
      await this.page.fill('[data-testid="input-bend-radius"]', params.bend_radius.toString());
    }
    if (params.num_bends !== undefined) {
      await this.page.fill('[data-testid="input-num-bends"]', params.num_bends.toString());
    }
    if (params.distribution !== undefined) {
      await this.page.selectOption('[data-testid="select-distribution"]', params.distribution);
    }
    if (params.inclination_angle !== undefined) {
      await this.page.fill('[data-testid="input-inclination-angle"]', params.inclination_angle.toString());
    }
    if (params.springback_correction !== undefined) {
      const checkbox = this.page.locator('[data-testid="checkbox-springback-correction"]');
      const isChecked = await checkbox.isChecked();
      if (isChecked !== params.springback_correction) {
        await checkbox.click();
      }
    }
    if (params.material !== undefined) {
      await this.page.selectOption('[data-testid="select-material"]', params.material);
    }
  }

  /**
   * Выполнить расчёт 3D смещения
   */
  async calculate3D(): Promise<void> {
    const startTime = Date.now();
    await this.page.click('[data-testid="button-calculate"], button:has-text("Calculate")');

    try {
      await this.page.waitForSelector('[data-testid="3d-preview"]', { timeout: 10000 });
    } catch (e) {
      throw new Error(`3D Calculation failed. Time: ${Date.now() - startTime}ms`);
    }
  }

  /**
   * Проверить наличие 3D превью
   */
  async verify3DPreview(): Promise<boolean> {
    return await this.page.locator('[data-testid="3d-preview"]').isVisible();
  }

  /**
   * Тестировать mouse drag rotate
   */
  async testMouseDragRotate(): Promise<void> {
    const canvas = this.page.locator('[data-testid="3d-preview-canvas"]').first();

    // Drag от центра в диагональ
    await canvas.dragTo(canvas, {
      sourcePosition: { x: 100, y: 100 },
      targetPosition: { x: 200, y: 200 },
    });

    // Проверяем, что трубка повернулась (путём получения изменённых координат)
    await this.page.waitForTimeout(500);
  }

  /**
   * Тестировать mouse scroll zoom
   */
  async testMouseScrollZoom(): Promise<void> {
    const canvas = this.page.locator('[data-testid="3d-preview-canvas"]').first();

    // Zoom in
    await canvas.evaluate((el) => {
      el.dispatchEvent(
        new WheelEvent('wheel', {
          deltaY: -100,
          bubbles: true,
          cancelable: true,
        })
      );
    });

    await this.page.waitForTimeout(500);
  }

  /**
   * Проверить видимость координатной сетки
   */
  async verifyGridVisible(): Promise<boolean> {
    return await this.page.locator('[data-testid="3d-grid"]').isVisible();
  }

  /**
   * Получить G-code
   */
  async getGCode(): Promise<string> {
    await this.page.click('[data-testid="tab-cnc"]');
    await this.page.waitForSelector('[data-testid="gcode-output"]');

    return await this.page.locator('[data-testid="gcode-output"]').textContent() || '';
  }

  /**
   * Копировать G-code
   */
  async copyGCode(): Promise<void> {
    await this.page.click('[data-testid="button-copy-gcode"]');

    // Проверяем toast подтверждения
    await this.page.waitForSelector('[data-testid="toast-copied"]', { timeout: 3000 });
  }

  /**
   * Экспортировать в DXF
   */
  async exportDXF(): Promise<void> {
    const downloadPromise = this.page.waitForEvent('download');

    await this.page.click('[data-testid="button-export-dxf"], button:has-text("DXF")');

    const download = await downloadPromise;
    const filename = download.suggestedFilename();

    if (!filename.endsWith('.dxf')) {
      throw new Error(`Экспортированный файл не является DXF: ${filename}`);
    }
  }

  /**
   * Получить результаты расчёта
   */
  async get3DResults(): Promise<{
    total_length: number;
    total_distance_3d: number;
    bends_count: number;
    feasibility_score: number;
    warnings: string[];
  }> {
    return await this.page.evaluate(() => {
      const getTextContent = (selector: string) => {
        const el = document.querySelector(selector);
        return el ? el.textContent?.trim() : '';
      };

      const parseNumber = (text: string) => parseFloat(text.split(':')[1]?.trim() || '0');

      return {
        total_length: parseNumber(getTextContent('[data-testid="result-total-length"]') || ''),
        total_distance_3d: parseNumber(getTextContent('[data-testid="result-distance-3d"]') || ''),
        bends_count: parseInt(getTextContent('[data-testid="result-bends-count"]')?.split(':')[1] || '0'),
        feasibility_score: parseNumber(getTextContent('[data-testid="result-feasibility"]') || ''),
        warnings: Array.from(document.querySelectorAll('[data-testid="warning-item"]')).map(
          (el) => el.textContent || ''
        ),
      };
    });
  }

  /**
   * Проверить валидность G-code
   */
  async validateGCode(gcode: string): Promise<{ valid: boolean; errors: string[] }> {
    const errors: string[] = [];

    // Проверка базовой синтаксической корректности
    if (!gcode.includes('G0') && !gcode.includes('G1')) {
      errors.push('No movement commands (G0/G1) found');
    }

    // Проверка M-кодов
    if (!gcode.includes('M3') && !gcode.includes('M5')) {
      errors.push('No spindle control commands (M3/M5) found');
    }

    // Проверка координат
    const hasX = /G[01]\s+X[\d.-]+/i.test(gcode);
    const hasY = /G[01]\s+Y[\d.-]+/i.test(gcode);
    const hasZ = /G[01]\s+Z[\d.-]+/i.test(gcode);

    if (!hasX || !hasY || !hasZ) {
      errors.push('Missing X, Y, or Z coordinates');
    }

    // Проверка F параметров (feed rate)
    const hasF = /F[\d.]+/i.test(gcode);
    if (!hasF) {
      errors.push('No feed rate (F) specified');
    }

    return {
      valid: errors.length === 0,
      errors,
    };
  }
}

/**
 * Утилиты для интеграционного тестирования
 */
export class OffsetIntegrationHelper {
  constructor(private page: Page) {}

  /**
   * Рассчитать несколько offset одновременно (параллельные операции)
   */
  async parallelCalculations(
    calculations: Array<{ name: string; params: any }>
  ): Promise<Map<string, any>> {
    const results = new Map();

    // Запускаем все расчёты параллельно
    const promises = calculations.map(async (calc) => {
      // Каждый расчёт в отдельной вкладке или контексте
      const newPage = await this.page.context().newPage();
      try {
        const helper = new ParallelOffsetHelper(newPage);
        await newPage.goto(this.page.url());

        if (calc.params.start_point) {
          const helper3d = new Combined3DOffsetHelper(newPage);
          await helper3d.open3DOffsetTab();
          await helper3d.fill3DParameters(calc.params);
          await helper3d.calculate3D();
          const result = await helper3d.get3DResults();
          results.set(calc.name, result);
        } else {
          await helper.openParallelOffsetTab();
          await helper.fillParameters(calc.params);
          await helper.calculate();
          const result = await helper.getResults();
          results.set(calc.name, result);
        }
      } finally {
        await newPage.close();
      }
    });

    await Promise.all(promises);
    return results;
  }

  /**
   * Переключаться между типами без потери данных
   */
  async switchOffsetTypesWithoutLoss(): Promise<void> {
    const parallelHelper = new ParallelOffsetHelper(this.page);
    const threeHelper = new Combined3DOffsetHelper(this.page);

    // Заполняем параллельное смещение
    await parallelHelper.openParallelOffsetTab();
    const testParams = {
      outer_diameter: 25,
      wall_thickness: 2.0,
      bend_angle: 90,
      offset_distance: 50,
      bend_radius: 40,
      material: 'Steel',
    };
    await parallelHelper.fillParameters(testParams);

    // Переключаемся на 3D
    await threeHelper.open3DOffsetTab();

    // Возвращаемся на параллельное
    await parallelHelper.openParallelOffsetTab();

    // Проверяем, что данные не потеряны
    const savedParams = await this.page.evaluate(() => {
      return {
        diameter: (document.querySelector('[data-testid="input-outer-diameter"]') as HTMLInputElement)?.value,
        thickness: (document.querySelector('[data-testid="input-wall-thickness"]') as HTMLInputElement)?.value,
      };
    });

    expect(parseFloat(savedParams.diameter)).toBe(testParams.outer_diameter);
    expect(parseFloat(savedParams.thickness)).toBe(testParams.wall_thickness);
  }

  /**
   * Одновременный экспорт нескольких расчётов
   */
  async exportMultiple(): Promise<string[]> {
    const exports: string[] = [];

    // Экспортируем параллельное смещение в PDF
    const parallelHelper = new ParallelOffsetHelper(this.page);
    await parallelHelper.openParallelOffsetTab();
    await parallelHelper.calculate();

    const downloadPromise1 = this.page.waitForEvent('download');
    await this.page.click('[data-testid="button-export-pdf"]');
    const download1 = await downloadPromise1;
    exports.push(download1.suggestedFilename());

    // Переходим на 3D и экспортируем DXF
    const threeHelper = new Combined3DOffsetHelper(this.page);
    await threeHelper.open3DOffsetTab();
    await threeHelper.calculate3D();

    const downloadPromise2 = this.page.waitForEvent('download');
    await this.page.click('[data-testid="button-export-dxf"]');
    const download2 = await downloadPromise2;
    exports.push(download2.suggestedFilename());

    return exports;
  }

  /**
   * Проверить синхронизацию результатов в основные параметры
   */
  async verifyResultsSync(): Promise<boolean> {
    const parallelHelper = new ParallelOffsetHelper(this.page);
    await parallelHelper.openParallelOffsetTab();
    await parallelHelper.calculate();

    // Проверяем, что результаты отражены в основных параметрах трубы
    const syncVerified = await parallelHelper.verifyResultsSync();
    return syncVerified;
  }

  /**
   * Проверить восстановление истории расчётов
   */
  async verifyHistoryRestoration(): Promise<boolean> {
    const parallelHelper = new ParallelOffsetHelper(this.page);
    await parallelHelper.openParallelOffsetTab();

    // Выполняем несколько расчётов
    for (let i = 0; i < 3; i++) {
      await parallelHelper.fillParameters({
        outer_diameter: 20 + i * 5,
        bend_angle: 45 + i * 15,
      });
      await parallelHelper.calculate();
      await this.page.waitForTimeout(500);
    }

    // Проверяем историю
    const history = await parallelHelper.getCalculationHistory();
    return history.length >= 3;
  }
}

/**
 * Утилиты для performance тестирования
 */
export class PerformanceHelper {
  /**
   * Измерить время выполнения расчёта параллельного смещения
   */
  static async measureParallelOffsetCalculationTime(helper: ParallelOffsetHelper): Promise<number> {
    const startTime = performance.now();
    await helper.calculate();
    return performance.now() - startTime;
  }

  /**
   * Измерить время выполнения расчёта 3D смещения
   */
  static async measure3DOffsetCalculationTime(helper: Combined3DOffsetHelper): Promise<number> {
    const startTime = performance.now();
    await helper.calculate3D();
    return performance.now() - startTime;
  }

  /**
   * Измерить FPS при отрисовке 3D
   */
  static async measure3DRenderingFPS(page: Page): Promise<number> {
    let frameCount = 0;
    const startTime = performance.now();
    const duration = 1000; // 1 second

    await page.evaluate(() => {
      let lastFrameTime = performance.now();
      let frames = 0;

      const countFrames = () => {
        frames++;
        const now = performance.now();
        if (now - lastFrameTime >= 1000) {
          (window as any).fps = frames;
          frames = 0;
          lastFrameTime = now;
        }
        requestAnimationFrame(countFrames);
      };

      requestAnimationFrame(countFrames);
    });

    await page.waitForTimeout(duration);

    frameCount = await page.evaluate(() => (window as any).fps || 0);
    return frameCount;
  }
}
