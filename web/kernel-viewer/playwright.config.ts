// End-to-end test of the kernel viewer page (docs/ui/KERNEL_VIEWPORT.md).
//
//   npx playwright test -c web/kernel-viewer/playwright.config.ts
//
// Needs the bendcore_kernel_wasm build (BENDCORE_KERNEL_WASM_DIR, default
// build-wasm-kernel/wasm_kernel). PLAYWRIGHT_CHROMIUM_EXECUTABLE selects a Chromium binary
// when the one matching the installed @playwright/test is not downloaded.
import { defineConfig } from '@playwright/test';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const port = Number(process.env.KERNEL_VIEWER_PORT || 3100);

export default defineConfig({
  testDir: path.join(root, 'tests/e2e-kernel'),
  testMatch: '**/*.spec.ts',
  timeout: 120_000,
  expect: { timeout: 30_000 },
  workers: 1,
  reporter: [['list']],
  outputDir: path.join(root, 'test-results/kernel-viewer'),
  webServer: {
    command: `npx webpack serve --config web/kernel-viewer/webpack.config.js --mode development --port ${port}`,
    cwd: root,
    url: `http://127.0.0.1:${port}/`,
    reuseExistingServer: !process.env.CI,
    timeout: 120_000,
  },
  use: {
    baseURL: `http://127.0.0.1:${port}/`,
    trace: 'off',
    screenshot: 'only-on-failure',
    viewport: { width: 1280, height: 800 },
    deviceScaleFactor: 1,
    launchOptions: {
      executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE || undefined,
      // WebGL without a GPU (headless CI): software rasteriser.
      args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'],
    },
  },
});
