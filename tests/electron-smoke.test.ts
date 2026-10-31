/**
 * Smoke test for Electron main process
 * Verifies that the main process can be instantiated without critical errors
 */

import { spawn } from 'child_process';
import path from 'path';

describe('Electron Smoke Test', () => {
  // Skip GUI tests on CI (no display available)
  const testFn = process.env.CI ? it.skip : it;

  testFn('should start Electron without critical errors', async () => {
    const electronPath = path.join(__dirname, '../node_modules/.bin/electron');
    const appPath = path.join(__dirname, '..');

    return new Promise<void>((resolve, reject) => {
      const electron = spawn(electronPath, [appPath, '--no-sandbox'], {
        cwd: appPath,
        timeout: 10000,
      });

      let output = '';
      let hasError = false;

      electron.stdout?.on('data', (data) => {
        output += data.toString();
        console.log('[Electron stdout]', data.toString());
      });

      electron.stderr?.on('data', (data) => {
        output += data.toString();
        console.log('[Electron stderr]', data.toString());

        // Look for critical errors (not just warnings)
        if (data.toString().includes('FATAL') || data.toString().includes('Cannot find module')) {
          hasError = true;
        }
      });

      electron.on('error', (error) => {
        console.error('[Electron error]', error);
        hasError = true;
      });

      // Kill after 10 seconds (app should have started by then)
      setTimeout(() => {
        electron.kill();
        if (hasError) {
          reject(new Error('Electron failed to start: ' + output));
        } else {
          resolve();
        }
      }, 10000);
    });
  }, 15000);

  it('should have main.js file', () => {
    const mainPath = path.join(__dirname, '../dist/main.js');
    const fs = require('fs');
    expect(fs.existsSync(mainPath)).toBe(true);
  });

  it('should have preload.js file', () => {
    const preloadPath = path.join(__dirname, '../dist/preload.js');
    const fs = require('fs');
    expect(fs.existsSync(preloadPath)).toBe(true);
  });

  it('should have web index.html', () => {
    const indexPath = path.join(__dirname, '../dist/web/index.html');
    const fs = require('fs');
    expect(fs.existsSync(indexPath)).toBe(true);
  });

  it('web index.html should reference webpack bundles', () => {
    const indexPath = path.join(__dirname, '../dist/web/index.html');
    const fs = require('fs');
    const content = fs.readFileSync(indexPath, 'utf-8');
    expect(content).toMatch(/runtime\./);
    expect(content).toMatch(/vendors\./);
    expect(content).toMatch(/main\./);
  });
});
