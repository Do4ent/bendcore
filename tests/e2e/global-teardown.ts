/**
 * Global Teardown for Playwright E2E Tests
 * Очистка после завершения тестов
 */

import { FullConfig } from '@playwright/test';
import * as fs from 'fs';
import * as path from 'path';

async function globalTeardown(config: FullConfig) {
  console.log('🧹 Starting BendCore E2E Test Teardown...');

  try {
    // Генерируем отчёт о результатах
    const resultsDir = path.join(__dirname, '../../test-results');
    const reportDir = path.join(__dirname, '../../playwright-report');

    // Проверяем наличие файла результатов
    const resultsFile = path.join(resultsDir, 'results.json');
    if (fs.existsSync(resultsFile)) {
      try {
        const results = JSON.parse(fs.readFileSync(resultsFile, 'utf-8'));

        // Считаем статистику
        const totalTests = results.tests?.length || 0;
        const passedTests = results.tests?.filter((t: any) => t.status === 'passed').length || 0;
        const failedTests = results.tests?.filter((t: any) => t.status === 'failed').length || 0;
        const skippedTests = results.tests?.filter((t: any) => t.status === 'skipped').length || 0;

        console.log('\n📊 Test Results Summary:');
        console.log(`   Total: ${totalTests}`);
        console.log(`   ✅ Passed: ${passedTests}`);
        console.log(`   ❌ Failed: ${failedTests}`);
        console.log(`   ⏭️  Skipped: ${skippedTests}`);
        console.log(`   Success Rate: ${totalTests > 0 ? ((passedTests / totalTests) * 100).toFixed(2) : 0}%`);
      } catch (e) {
        // Не удалось спарсить результаты
      }
    }

    // Очищаем временные файлы (опционально)
    // fs.rmSync(path.join(__dirname, '.playwright-cache'), { recursive: true, force: true });

    console.log('✅ Teardown completed successfully');
    console.log(`📁 Test reports available at: ${reportDir}`);
    console.log(`📁 Test results available at: ${resultsDir}`);
  } catch (error) {
    console.error('❌ Teardown error:', error);
    // Не завершаем с ошибкой, т.к. тесты уже выполнены
  }
}

export default globalTeardown;
