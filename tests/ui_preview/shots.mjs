// Проверка панелей в Chromium против настоящего ядра: действия пользователя и снимки экрана.
import { chromium } from '/opt/npm-tools/node_modules/playwright/index.mjs';
import assert from 'node:assert/strict';

const base = process.env.BASE ?? 'http://localhost:5199';
const out = process.env.OUT ?? '.';
const b = await chromium.launch();
const page = await b.newPage({ viewport: { width: 1280, height: 820 } });
const errors = [];
page.on('pageerror', (e) => errors.push(e.message));
page.on('console', (m) => m.type() === 'error' && errors.push(m.text()));

// 1. Проверка: до запуска — «Не проверено», после — нарушения всех видов, клик — переход.
await page.goto(`${base}/?view=check`);
await page.getByRole('status').filter({ hasText: 'Не проверено' }).waitFor();
await page.getByRole('button', { name: 'Проверить всё' }).click();
await page.getByRole('status').filter({ hasText: 'Есть нарушения' }).waitFor();
const rows = page.locator('tbody tr');
assert.ok((await rows.count()) >= 4, 'нарушения в таблице');
await page.getByText('прямой между гибами меньше минимума').first().click();
assert.match(await page.locator('#msg').innerText(), /→ Т-0[13] #/);
await page.screenshot({ path: `${out}/ui_check.png` });
// Фильтр: выключаем пометки — строки зазора пропадают.
const before = await rows.count();
await page.getByRole('button', { name: /Пометки/ }).click();
assert.ok((await rows.count()) < before, 'фильтр по классу');

// 2. Карта гибки: LRA по умолчанию, переключение на XYZ и на конец Б, сохранение CSV.
await page.goto(`${base}/?view=bend`);
await page.getByRole('cell', { name: 'Конец' }).waitFor();
await page.screenshot({ path: `${out}/ui_bend_lra.png` });
await page.getByRole('button', { name: /XYZ/ }).click();
await page.getByRole('cell', { name: 'Б', exact: true }).waitFor();
await page.getByRole('button', { name: 'Б', exact: true }).click();
await page.waitForTimeout(200);
assert.equal(await page.locator('tbody tr').first().locator('td').first().innerText(), 'Б', 'с конца Б первая строка — торец Б');
await page.getByRole('button', { name: 'Сохранить CSV' }).click();
assert.match(await page.locator('#msg').innerText(), /^БК\.100\.001_XYZ_B\.csv\nТочка;X;Y;Z;R/);
await page.screenshot({ path: `${out}/ui_bend_xyz.png` });

await page.goto(`${base}/?view=bend-blocked`);
await page.getByText('Вывод заблокирован').waitFor();
assert.equal(await page.getByRole('button', { name: 'Сохранить CSV' }).isDisabled(), true, 'вывод заблокирован');

// 3. Экземпляры: копия → «Сделать главным», «Привести к одному».
await fetch(`${base}/reset`, { method: 'POST' });
await page.goto(`${base}/?view=instances`);
await page.getByText('БК.200.009').first().waitFor();
await page.screenshot({ path: `${out}/ui_instances.png` });
await page.locator('tr', { hasText: 'Т-12' }).getByRole('button', { name: 'Сделать главным' }).click();
await page.locator('tr', { hasText: 'Т-12' }).getByText('Главный').waitFor();
await page.getByRole('button', { name: 'Привести к одному' }).click();
await page.waitForTimeout(200);
assert.equal(await page.getByText('БК.200.009').count(), 0, 'обозначения приведены к одному');

// 4. Чертёж: два листа, выпуск, только чтение, снять выпуск.
await page.goto(`${base}/?view=drawing`);
await page.locator('.bc-sheet-host svg').waitFor();
await page.screenshot({ path: `${out}/ui_drawing.png` });
await page.getByRole('combobox', { name: 'Набор видов' }).selectOption('full');
await page.getByRole('tab', { name: 'Лист 2' }).waitFor();
await page.getByRole('tab', { name: 'Лист 2' }).click();
await page.waitForTimeout(200);
await page.screenshot({ path: `${out}/ui_drawing_sheet2.png` });
await page.getByRole('button', { name: 'Выпустить' }).click();
await page.getByRole('dialog', { name: 'Выпустить' }).getByRole('button', { name: 'Выпустить' }).click();
await page.getByText('Выпущен').first().waitFor();
assert.equal(await page.getByRole('combobox', { name: 'Стандарт' }).isDisabled(), true, 'выпущенный — только чтение');
assert.ok((await page.locator('.bc-sheet-host svg').innerHTML()).includes('ВЫПУЩЕН'), 'отметка в штампе');
await page.screenshot({ path: `${out}/ui_drawing_released.png` });
await page.getByRole('button', { name: 'Снять выпуск' }).click();
await page.getByRole('button', { name: 'Выпустить' }).waitFor();
await page.getByRole('combobox', { name: 'Стандарт' }).selectOption('ANSI');
await page.waitForFunction(() => document.querySelector('.bc-sheet-host')?.innerHTML.includes('viewBox="0 0 431.8 279.4"'));
assert.equal(await page.getByRole('combobox', { name: 'Формат' }).inputValue(), 'ANSI B', 'ANSI: формат ANSI B');

// 5. Эскиз: угол подтверждения и блокировка оси клавишами.
await page.goto(`${base}/?view=sketch`);
await page.keyboard.press('x');
assert.equal(await page.getByRole('button', { name: 'X' }).getAttribute('aria-pressed'), 'true', 'клавиша X блокирует ось');
await page.keyboard.press('x');
assert.equal(await page.getByRole('button', { name: 'X' }).getAttribute('aria-pressed'), 'false', 'повтор снимает блокировку');
await page.getByRole('button', { name: 'Завершить эскиз' }).click();
assert.equal(await page.locator('#msg').innerText(), 'finish');

assert.deepEqual(errors, [], 'нет ошибок в консоли');
await b.close();
console.log('UI: OK');
