/**
 * Global Setup for Playwright E2E Tests
 * Инициализация перед запуском тестов
 */

import { chromium, FullConfig } from '@playwright/test';
import * as fs from 'fs';
import * as path from 'path';

async function globalSetup(config: FullConfig) {
  console.log('🚀 Starting BendCore E2E Test Setup...');

  try {
    // Создаём директорию для результатов тестов
    const reportDir = path.join(__dirname, '../../playwright-report');
    const resultsDir = path.join(__dirname, '../../test-results');

    if (!fs.existsSync(reportDir)) {
      fs.mkdirSync(reportDir, { recursive: true });
    }
    if (!fs.existsSync(resultsDir)) {
      fs.mkdirSync(resultsDir, { recursive: true });
    }

    console.log('✅ Created test report directories');

    // Ожидаем, чтобы убедиться, что серверы запущены
    const maxWaitTime = 30000; // 30 секунд
    const startTime = Date.now();

    // Проверяем доступность backend сервера
    let backendReady = false;
    while (!backendReady && Date.now() - startTime < maxWaitTime) {
      try {
        const response = await fetch('http://localhost:3001/api/health');
        if (response.ok) {
          backendReady = true;
          console.log('✅ Backend server is ready');
        }
      } catch (e) {
        // Ещё не готов
        await new Promise((resolve) => setTimeout(resolve, 1000));
      }
    }

    if (!backendReady) {
      console.warn('⚠️ Backend server did not start in time, continuing anyway...');
    }

    // Проверяем доступность frontend сервера
    let frontendReady = false;
    while (!frontendReady && Date.now() - startTime < maxWaitTime) {
      try {
        const response = await fetch('http://localhost:3000');
        if (response.ok) {
          frontendReady = true;
          console.log('✅ Frontend server is ready');
        }
      } catch (e) {
        // Ещё не готов
        await new Promise((resolve) => setTimeout(resolve, 1000));
      }
    }

    if (!frontendReady) {
      console.warn('⚠️ Frontend server did not start in time, continuing anyway...');
    }

    // Инициализируем базу данных (если необходимо)
    console.log('✅ Global setup completed successfully');
  } catch (error) {
    console.error('❌ Global setup failed:', error);
    process.exit(1);
  }
}

export default globalSetup;
