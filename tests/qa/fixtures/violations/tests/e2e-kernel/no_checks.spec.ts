// Фикстура самотеста check_no_stubs: сквозной тест, проверяющий только «не упало».
import { test } from '@playwright/test';

test('opens the viewer', async ({ page }) => {
  await page.goto('/');
  await page.click('canvas');
});
