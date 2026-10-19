// Проверка web/index.html в headless Chromium (Playwright): страница должна показать «Тест пройден».
// Использование: node scripts/check_wasm_page.mjs <каталог с index.html>   (нужен пакет playwright и браузер)
import http from 'node:http';
import { readFile } from 'node:fs/promises';
import { extname, join, normalize, resolve } from 'node:path';
import { chromium } from 'playwright';

const root = resolve(process.argv[2] ?? 'build-wasm/web');
const types = { '.html': 'text/html', '.mjs': 'text/javascript', '.js': 'text/javascript', '.wasm': 'application/wasm' };

const server = http.createServer(async (req, res) => {
  const rel = normalize(decodeURIComponent(new URL(req.url, 'http://x').pathname)).replace(/^(\.\.[/\\])+/, '');
  const file = join(root, rel === '/' || rel === '.' ? 'index.html' : rel);
  try {
    const body = await readFile(file);
    res.writeHead(200, { 'Content-Type': types[extname(file)] ?? 'application/octet-stream' });
    res.end(body);
  } catch {
    res.writeHead(404).end();
  }
});
await new Promise((ok) => server.listen(0, '127.0.0.1', ok));
const url = `http://127.0.0.1:${server.address().port}/index.html`;

const browser = await chromium.launch();
let code = 1;
try {
  const page = await browser.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(String(e)));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });
  await page.goto(url);
  await page.waitForFunction(() => {
    const t = document.getElementById('result')?.textContent ?? '';
    return t.includes('Тест') || t.includes('Ошибка');
  }, null, { timeout: 60000 });
  const result = (await page.textContent('#result')).trim();
  const log = (await page.textContent('#log')).trim();
  console.log(`Результат: ${result}`);
  console.log(log);
  if (errors.length) console.log('Ошибки в консоли:\n' + errors.join('\n'));
  code = result === 'Тест пройден' ? 0 : 1;
} finally {
  await browser.close();
  server.close();
}
process.exit(code);
