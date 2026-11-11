// End-to-end test of the drawing editor page (docs/ui/DRAWING_UI.md).
//
//   npx playwright test -c web/drawing/playwright.config.ts
//
// Needs the bendcore_drawing_wasm build (BENDCORE_DRAWING_WASM_DIR, default
// build-wasm-drawing/wasm_drawing, with DejaVuSans.ttf next to the module).
// PLAYWRIGHT_CHROMIUM_EXECUTABLE selects a Chromium binary when the one matching the installed
// @playwright/test is not downloaded.
import { defineConfig } from '@playwright/test';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const port = Number(process.env.DRAWING_EDITOR_PORT || 3150);

export default defineConfig({
  testDir: path.join(root, 'tests/e2e-drawing'),
  testMatch: '**/*.spec.ts',
  timeout: 240_000,
  expect: { timeout: 30_000 },
  workers: 1,
  reporter: [['list']],
  outputDir: path.join(root, 'test-results/drawing'),
  webServer: {
    command: `npx webpack serve --config web/drawing/webpack.config.js --mode development --port ${port}`,
    cwd: root,
    url: `http://127.0.0.1:${port}/`,
    reuseExistingServer: !process.env.CI,
    timeout: 180_000,
  },
  use: {
    baseURL: `http://127.0.0.1:${port}/`,
    trace: 'off',
    screenshot: 'only-on-failure',
    viewport: { width: 1600, height: 900 },
    deviceScaleFactor: 1,
    acceptDownloads: true,
    launchOptions: {
      executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE || undefined,
    },
  },
});
