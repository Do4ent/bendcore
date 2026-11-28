// End-to-end test of the assembly page (docs/ui/ASSEMBLY_UI.md).
//
//   npx playwright test -c web/assembly/playwright.config.ts
//
// Needs the bendcore_assembly_wasm build (BENDCORE_ASSEMBLY_WASM_DIR, default
// build-wasm/wasm_assembly). PLAYWRIGHT_CHROMIUM_EXECUTABLE selects a Chromium binary
// when the one matching the installed @playwright/test is not downloaded.
import { defineConfig } from '@playwright/test';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const port = Number(process.env.ASSEMBLY_PAGE_PORT || 3120);

export default defineConfig({
  testDir: path.join(root, 'tests/e2e-assembly'),
  testMatch: '**/*.spec.ts',
  timeout: 180_000,
  expect: { timeout: 30_000 },
  workers: 1,
  reporter: [['list']],
  outputDir: path.join(root, 'test-results/assembly'),
  webServer: {
    command: `npx webpack serve --config web/assembly/webpack.config.js --mode development --port ${port}`,
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
