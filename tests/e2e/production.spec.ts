/**
 * BendCore Production E2E Tests
 * Comprehensive test suite for production readiness
 */

import { test, expect } from '@playwright/test';

const BASE_URL = process.env.BASE_URL || 'http://localhost:3000';
const API_URL = process.env.API_URL || 'http://localhost:3001/api';

test.describe('BendCore Production E2E Tests', () => {
  // ============================================
  // MODELING FEATURES
  // ============================================
  test.describe('Modeling Features', () => {
    test('should load main application', async ({ page }) => {
      await page.goto(BASE_URL);
      await expect(page).toHaveTitle(/BendCore/i);
      await expect(page.locator('text=BendCore')).toBeVisible();
    });

    test('should create new project', async ({ page }) => {
      await page.goto(BASE_URL);
      await page.click('button:has-text("New Project")');

      const dialog = page.locator('[role="dialog"]');
      await expect(dialog).toBeVisible();

      await page.fill('input[name="projectName"]', 'Test Project');
      await page.click('button:has-text("Create")');

      await expect(page.locator('text=Test Project')).toBeVisible();
    });

    test('should create tube feature', async ({ page }) => {
      await page.goto(BASE_URL);
      await page.click('button:has-text("New Project")');
      await page.fill('input[name="projectName"]', 'Tube Test');
      await page.click('button:has-text("Create")');

      await page.click('button:has-text("Tube")');
      const dialog = page.locator('[role="dialog"]');

      await page.fill('input[name="outerRadius"]', '25');
      await page.fill('input[name="innerRadius"]', '20');
      await page.fill('input[name="length"]', '300');

      await page.click('button:has-text("Create")');

      await expect(page.locator('.feature-list')).toContainText('Tube');
    });

    test('should apply bend operation', async ({ page }) => {
      await page.goto(BASE_URL);
      await page.click('button:has-text("New Project")');
      await page.fill('input[name="projectName"]', 'Bend Test');
      await page.click('button:has-text("Create")');

      // Create tube
      await page.click('button:has-text("Tube")');
      await page.fill('input[name="outerRadius"]', '25');
      await page.fill('input[name="innerRadius"]', '20');
      await page.fill('input[name="length"]', '300');
      await page.click('button:has-text("Create")');

      // Create bend
      await page.click('button:has-text("Bend")');
      await page.fill('input[name="bendRadius"]', '100');
      await page.fill('input[name="angle"]', '90');
      await page.click('button:has-text("Apply")');

      await expect(page.locator('.feature-list')).toContainText('Bend');
    });
  });

  // ============================================
  // FEA SIMULATION
  // ============================================
  test.describe('FEA Simulation', () => {
    test('should run stress analysis', async ({ page }) => {
      await page.goto(`${API_URL}/simulation/materials`);

      const response = await page.evaluate(() => fetch('materials'));
      expect(response.ok).toBe(true);
    });

    test('should calculate stress analysis via API', async () => {
      const response = await fetch(`${API_URL}/simulation/stress`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          geometry: {
            outerRadius: 25,
            innerRadius: 20,
            length: 300,
          },
          material: 'steel',
          appliedLoad: 5000,
          supportType: 'fixed',
        }),
      });

      expect(response.ok).toBe(true);
      const data = await response.json();
      expect(data.result).toBeDefined();
      expect(data.result.maxStress).toBeGreaterThan(0);
      expect(data.result.safetyFactor).toBeGreaterThan(0);
    });

    test('should calculate displacement analysis', async () => {
      const response = await fetch(`${API_URL}/simulation/displacement`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          geometry: {
            outerRadius: 25,
            innerRadius: 20,
            length: 300,
          },
          material: 'aluminum',
          appliedLoad: 1000,
        }),
      });

      expect(response.ok).toBe(true);
      const data = await response.json();
      expect(data.result).toBeDefined();
      expect(data.result.maxDisplacement).toBeGreaterThanOrEqual(0);
    });

    test('should get materials library', async () => {
      const response = await fetch(`${API_URL}/simulation/materials`);

      expect(response.ok).toBe(true);
      const data = await response.json();
      expect(data.materials).toBeDefined();
      expect(data.materials.length).toBeGreaterThan(0);
      expect(data.materials[0].elasticModulus).toBeGreaterThan(0);
    });

    test('should run buckling analysis', async () => {
      const response = await fetch(`${API_URL}/simulation/buckling`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          geometry: {
            outerRadius: 15,
            innerRadius: 10,
            length: 500,
          },
          material: 'steel',
          supportCondition: 'both-fixed',
        }),
      });

      expect(response.ok).toBe(true);
      const data = await response.json();
      expect(data.buckling).toBeDefined();
      expect(data.buckling.criticalLoad).toBeGreaterThan(0);
    });

    test('should run bending analysis', async () => {
      const response = await fetch(`${API_URL}/simulation/bending`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          geometry: {
            outerRadius: 25,
            innerRadius: 20,
            length: 300,
          },
          material: 'steel',
          bendRadius: 100,
          bendAngle: 90,
        }),
      });

      expect(response.ok).toBe(true);
      const data = await response.json();
      expect(data.bending).toBeDefined();
      expect(data.bending.wipeoutPercentage).toBeDefined();
    });
  });

  // ============================================
  // API ENDPOINTS
  // ============================================
  test.describe('API Endpoints', () => {
    test('should respond to health check', async () => {
      const response = await fetch(`${API_URL}/health`);
      expect(response.ok).toBe(true);

      const data = await response.json();
      expect(data.status).toBe('ok');
      expect(data.version).toBeDefined();
    });

    test('should list all tubes', async () => {
      const response = await fetch(`${API_URL}/tubes`);
      expect(response.ok).toBe(true);

      const data = await response.json();
      expect(Array.isArray(data)).toBe(true);
    });

    test('should get tube profiles', async () => {
      const response = await fetch(`${API_URL}/profiles`);
      expect(response.ok).toBe(true);

      const data = await response.json();
      expect(data.profiles).toBeDefined();
    });

    test('should handle 404 gracefully', async () => {
      const response = await fetch(`${API_URL}/nonexistent`);
      expect(response.status).toBe(404);
    });
  });

  // ============================================
  // PERFORMANCE
  // ============================================
  test.describe('Performance', () => {
    test('health check should respond in < 200ms', async () => {
      const start = Date.now();
      await fetch(`${API_URL}/health`);
      const duration = Date.now() - start;

      expect(duration).toBeLessThan(200);
    });

    test('stress analysis should complete in < 5s', async () => {
      const start = Date.now();

      await fetch(`${API_URL}/simulation/stress`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          geometry: {
            outerRadius: 25,
            innerRadius: 20,
            length: 300,
          },
          material: 'steel',
        }),
      });

      const duration = Date.now() - start;
      expect(duration).toBeLessThan(5000);
    });

    test('concurrent requests should not block', async () => {
      const promises = [];

      for (let i = 0; i < 5; i++) {
        promises.push(
          fetch(`${API_URL}/health`)
        );
      }

      const start = Date.now();
      await Promise.all(promises);
      const duration = Date.now() - start;

      expect(duration).toBeLessThan(1000);
    });
  });

  // ============================================
  // SECURITY
  // ============================================
  test.describe('Security', () => {
    test('should have CORS headers', async () => {
      const response = await fetch(`${API_URL}/health`, {
        headers: {
          'Origin': 'http://example.com',
        },
      });

      const corsHeader = response.headers.get('Access-Control-Allow-Origin');
      expect(corsHeader).toBeDefined();
    });

    test('should reject invalid JSON', async () => {
      const response = await fetch(`${API_URL}/simulation/stress`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: '{invalid json',
      });

      expect(response.ok).toBe(false);
    });

    test('should require proper geometry', async () => {
      const response = await fetch(`${API_URL}/simulation/stress`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          geometry: null,
        }),
      });

      expect(response.status).toBe(400);
    });
  });

  // ============================================
  // INTEGRATION TESTS
  // ============================================
  test.describe('Integration Tests', () => {
    test('complete workflow: create -> analyze -> export', async ({ page }) => {
      await page.goto(BASE_URL);

      // Create project
      await page.click('button:has-text("New Project")');
      await page.fill('input[name="projectName"]', 'Integration Test');
      await page.click('button:has-text("Create")');

      // Create tube
      await page.click('button:has-text("Tube")');
      await page.fill('input[name="outerRadius"]', '20');
      await page.fill('input[name="innerRadius"]', '15');
      await page.fill('input[name="length"]', '250');
      await page.click('button:has-text("Create")');

      // Run analysis
      await page.click('button:has-text("Analyze")');
      await page.waitForTimeout(2000);

      // Check results
      await expect(page.locator('text=Stress')).toBeVisible();

      // Export
      await page.click('button:has-text("Export")');
      await expect(page.locator('text=Export')).toBeVisible();
    });

    test('should save and load project', async ({ page }) => {
      await page.goto(BASE_URL);

      // Create and save
      await page.click('button:has-text("New Project")');
      await page.fill('input[name="projectName"]', 'Save Test');
      await page.click('button:has-text("Create")');

      await page.keyboard.press('Control+S');
      await page.waitForTimeout(1000);

      // Verify save
      await expect(page.locator('text=Saved')).toBeVisible();
    });
  });

  // ============================================
  // UI RESPONSIVENESS
  // ============================================
  test.describe('UI Responsiveness', () => {
    test('should work on mobile viewport', async ({ page }) => {
      await page.setViewportSize({ width: 375, height: 667 });
      await page.goto(BASE_URL);

      await expect(page).toHaveTitle(/BendCore/i);
    });

    test('should work on tablet viewport', async ({ page }) => {
      await page.setViewportSize({ width: 768, height: 1024 });
      await page.goto(BASE_URL);

      await expect(page).toHaveTitle(/BendCore/i);
    });

    test('should work on desktop viewport', async ({ page }) => {
      await page.setViewportSize({ width: 1920, height: 1080 });
      await page.goto(BASE_URL);

      await expect(page).toHaveTitle(/BendCore/i);
    });
  });

  // ============================================
  // ACCESSIBILITY
  // ============================================
  test.describe('Accessibility', () => {
    test('should have proper heading hierarchy', async ({ page }) => {
      await page.goto(BASE_URL);

      const h1s = await page.$$eval('h1', (els) => els.length);
      expect(h1s).toBeGreaterThanOrEqual(1);
    });

    test('should have alt text on images', async ({ page }) => {
      await page.goto(BASE_URL);

      const images = await page.locator('img');
      const count = await images.count();

      for (let i = 0; i < count; i++) {
        const alt = await images.nth(i).getAttribute('alt');
        if (!alt?.includes('data:image')) {
          expect(alt).not.toBeEmpty();
        }
      }
    });
  });

  // ============================================
  // ERROR HANDLING
  // ============================================
  test.describe('Error Handling', () => {
    test('should handle network errors gracefully', async ({ page }) => {
      await page.context().setOffline(true);
      await page.goto(BASE_URL);

      await page.context().setOffline(false);
      await expect(page.locator('text=BendCore')).toBeVisible();
    });

    test('should display error messages', async () => {
      const response = await fetch(`${API_URL}/simulation/stress`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          geometry: {
            outerRadius: 0, // Invalid
            innerRadius: 0,
            length: 0,
          },
        }),
      });

      expect(response.ok).toBe(false);
      const data = await response.json();
      expect(data.error).toBeDefined();
    });
  });
});
