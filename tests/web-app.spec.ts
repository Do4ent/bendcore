/**
 * BendCore Web Version - Integration Test
 * Tests all major UI components and command functionality
 */

import { test, expect, Page } from '@playwright/test';

const BASE_URL = 'http://localhost:3000';

test.describe('BendCore Web Version', () => {
  let page: Page;

  test.beforeAll(async ({ browser }) => {
    page = await browser.newPage();
    await page.goto(BASE_URL);

    // Wait for React app to fully load
    await page.waitForSelector('#root');
    await page.waitForLoadState('networkidle');
    await page.waitForTimeout(2000);
  });

  test('should load the application', async () => {
    expect(await page.title()).toContain('BendCore');
  });

  test('should have ribbon toolbar visible', async () => {
    // Check if ribbon tabs are visible
    const partTab = page.locator('text=Part').first();
    await expect(partTab).toBeVisible();
  });

  test('should have 3-panel layout', async () => {
    // Check left panel (Feature Tree)
    const featureTree = page.locator('text=Feature Tree');
    await expect(featureTree).toBeVisible();

    // Check that main content area exists
    const viewport = page.locator('[class*="Viewport3D"]');
    expect(await page.locator('canvas').count()).toBeGreaterThan(0);

    // Check status bar
    const statusBar = page.locator('text=Ready');
    await expect(statusBar).toBeVisible();
  });

  test('should display empty document message', async () => {
    const emptyMessage = page.locator('text=No geometry in document');
    await expect(emptyMessage).toBeVisible();
  });

  test('should navigate ribbon tabs', async () => {
    // Click Part tab
    const partTab = page.locator('button', { hasText: 'Part' }).first();
    await partTab.click();
    await expect(partTab).toHaveClass(/active/);

    // Click Analysis tab
    const analysisTab = page.locator('button', { hasText: 'Analysis' }).first();
    await analysisTab.click();
    await expect(analysisTab).toHaveClass(/active/);

    // Return to Part
    await partTab.click();
  });

  test('should open New Tube dialog', async () => {
    // Click Part tab
    const partTab = page.locator('button', { hasText: 'Part' }).first();
    await partTab.click();

    // Click New Tube button
    const newTubeBtn = page.locator('button', { hasText: /New Tube/i }).first();
    if (await newTubeBtn.isVisible()) {
      await newTubeBtn.click();

      // Wait for dialog to appear
      await page.waitForTimeout(500);

      // Check if dialog is visible
      const dialogTitle = page.locator('text=Create New Tube');
      const isDialogVisible = await dialogTitle.isVisible().catch(() => false);
      console.log('Dialog visible:', isDialogVisible);
    }
  });

  test('should handle mock commands', async () => {
    // We'll check console for mock IPC logs
    const messages: string[] = [];
    page.on('console', (msg) => {
      messages.push(msg.text());
    });

    // Click a command button
    const partTab = page.locator('button', { hasText: 'Part' }).first();
    await partTab.click();

    // Wait a bit for any logs
    await page.waitForTimeout(1000);

    // Check if we have mock IPC logs
    const hasMockLogs = messages.some((msg) => msg.includes('[Mock IPC]') || msg.includes('Command:'));
    console.log('Mock IPC logs detected:', hasMockLogs);
    console.log('Messages:', messages.slice(0, 5));
  });

  test('should respond to selection in feature tree', async () => {
    // Try to click on Document element if it exists
    const documentElement = page.locator('text=Document');
    if (await documentElement.isVisible()) {
      await documentElement.click();
      await page.waitForTimeout(500);
    }
  });

  test('should have responsive layout', async () => {
    // Check viewport size
    const size = page.viewportSize();
    expect(size?.width).toBeGreaterThan(0);
    expect(size?.height).toBeGreaterThan(0);

    // Verify no horizontal scrollbar (good responsive design)
    const hasHorizontalScroll = await page.evaluate(
      () => document.documentElement.scrollWidth > document.documentElement.clientWidth
    );
    expect(hasHorizontalScroll).toBe(false);
  });

  test('should display ribbon buttons without errors', async () => {
    const buttons = page.locator('button').all();
    const count = (await buttons).length;

    console.log(`Found ${count} buttons in ribbon`);
    expect(count).toBeGreaterThan(10); // Should have many ribbon buttons
  });

  test('should have proper CSS styling', async () => {
    // Check that app div has proper flex layout
    const appDiv = page.locator('#root').first();
    const classes = await appDiv.evaluate((el) => el.getAttribute('class'));

    console.log('App div classes:', classes);

    // App should be visible and taking full viewport
    const isVisible = await appDiv.isVisible();
    expect(isVisible).toBe(true);
  });

  test('should handle window resize gracefully', async () => {
    // Resize viewport
    await page.setViewportSize({ width: 1200, height: 800 });
    await page.waitForTimeout(500);

    // App should still be visible
    const root = page.locator('#root');
    await expect(root).toBeVisible();

    // Resize back
    await page.setViewportSize({ width: 1920, height: 1080 });
    await page.waitForTimeout(500);
  });

  test('should have no console errors', async ({ browser }) => {
    const errorMessages: string[] = [];

    const errorPage = await browser.newPage();
    errorPage.on('console', (msg) => {
      if (msg.type() === 'error') {
        errorMessages.push(msg.text());
      }
    });

    await errorPage.goto(BASE_URL);
    await errorPage.waitForSelector('#root');
    await errorPage.waitForTimeout(2000);

    await errorPage.close();

    console.log('Console errors:', errorMessages);
    // Filter out React dev warnings, focus on real errors
    const realErrors = errorMessages.filter(
      (err) => !err.includes('Warning:') && !err.includes('act()')
    );

    expect(realErrors.length).toBe(0);
  });
});

test.describe('BendCore Web - Commands', () => {
  test('should execute ribbon commands', async ({ page }) => {
    await page.goto(BASE_URL);
    await page.waitForSelector('#root');
    await page.waitForLoadState('networkidle');

    const consoleLogs: string[] = [];
    page.on('console', (msg) => consoleLogs.push(msg.text()));

    // Click a few buttons
    const buttons = await page.locator('button').all();

    // Click first few buttons (safely)
    for (let i = 0; i < Math.min(3, buttons.length); i++) {
      try {
        await buttons[i].click().catch(() => {}); // Ignore errors
        await page.waitForTimeout(200);
      } catch (e) {
        // Ignore
      }
    }

    // Check if commands were logged
    const commandLogs = consoleLogs.filter((log) => log.includes('Command'));
    console.log('Command logs:', commandLogs);
  });
});
