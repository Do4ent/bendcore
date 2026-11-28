// End-to-end test of the part modeling page (docs/ui/PART_MODELING_UI.md).
//
//   npx playwright test -c web/part-modeler/playwright.config.ts
//
// Needs the bendcore_modeling_wasm build (BENDCORE_MODELING_WASM_DIR, default
// build-wasm-modeling/wasm_modeling). PLAYWRIGHT_CHROMIUM_EXECUTABLE selects a Chromium binary
// when the one matching the installed @playwright/test is not downloaded.
import { defineConfig } from '@playwright/test';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const port = Number(process.env.PART_MODELER_PORT || 3110);

export default defineConfig({
  testDir: path.join(root, 'tests/e2e-modeling'),
  testMatch: '**/*.spec.ts',
  timeout: 180_000,
  expect: { timeout: 30_000 },
  workers: 1,
  reporter: [['list']],
  outputDir: path.join(root, 'test-results/part-modeler'),
  webServer: {
    command: `npx webpack serve --config web/part-modeler/webpack.config.js --mode development --port ${port}`,
    cwd: root,
    url: `http://127.0.0.1:${port}/`,
    reuseExistingServer: !process.env.CI,
    timeout: 180_000,
  },
  use: {
    baseURL: `http://127.0.0.1:${port}/`,
    trace: 'off',
    screenshot: 'only-on-failure',
    viewport: { width: 1440, height: 860 },
    deviceScaleFactor: 1,
    launchOptions: {
      executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE || undefined,
      // WebGL without a GPU (headless CI): software rasteriser.
      args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'],
    },
  },
});
