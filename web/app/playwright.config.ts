// End-to-end tests of the application shell (docs/ui/APP_SHELL.md).
//
//   npx playwright test -c web/app/playwright.config.ts
//
// Needs the build output of the four WASM modules (BENDCORE_{KERNEL,MODELING,ASSEMBLY,DRAWING}_WASM_DIR,
// default build-wasm-all/wasm_<module>). PLAYWRIGHT_CHROMIUM_EXECUTABLE selects a Chromium binary
// when the one matching the installed @playwright/test is not downloaded.
import { defineConfig } from '@playwright/test';
import path from 'path';

const root = path.resolve(__dirname, '../..');
const port = Number(process.env.APP_SHELL_PORT || 3200);

export default defineConfig({
  testDir: path.join(root, 'tests/e2e-app'),
  testMatch: '**/*.spec.ts',
  timeout: 300_000,
  expect: { timeout: 60_000 },
  workers: 1,
  reporter: [['list']],
  outputDir: path.join(root, 'test-results/app'),
  webServer: {
    command: `npx webpack serve --config web/app/webpack.config.js --mode development --port ${port}`,
    cwd: root,
    url: `http://127.0.0.1:${port}/`,
    reuseExistingServer: !process.env.CI,
    timeout: 180_000,
  },
  use: {
    baseURL: `http://127.0.0.1:${port}/`,
    trace: 'off',
    screenshot: 'only-on-failure',
    acceptDownloads: true,
    viewport: { width: 1440, height: 900 },
    deviceScaleFactor: 1,
    launchOptions: {
      executablePath: process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE || undefined,
      // WebGL without a GPU (headless CI): software rasteriser.
      args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'],
    },
  },
});
