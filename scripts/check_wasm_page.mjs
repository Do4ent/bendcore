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
  // index.html: результат «Тест пройден». view.html: статус с числом точек оси (импорт и трассировка прошли).
  const checks = [
    { file: 'index.html', wait: (t) => t.includes('Тест') || t.includes('Ошибка'), ok: (t) => t === 'Тест пройден' },
    { file: 'view.html', wait: (t) => t.includes('точек оси') || t.includes('Ошибка'), ok: (t) => t.includes('точек оси') },
  ];
  code = 0;
  for (const c of checks) {
    const page = await browser.newPage();
    const errors = [];
    page.on('pageerror', (e) => errors.push(String(e)));
    page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });
    await page.goto(`http://127.0.0.1:${server.address().port}/${c.file}`);
    const resultSel = c.file === 'index.html' ? '#result' : '#status';
    await page.waitForFunction((sel) => {
      const t = document.querySelector(sel)?.textContent ?? '';
      return t.includes('Тест') || t.includes('точек оси') || t.includes('Ошибка');
    }, resultSel, { timeout: 60000 });
    const result = (await page.textContent(resultSel)).trim();
    console.log(`[${c.file}] ${result}`);
    if (c.file === 'index.html') console.log((await page.textContent('#log')).trim());
    if (errors.length) console.log(`[${c.file}] ошибки в консоли:\n` + errors.join('\n'));
    if (!c.ok(result) || errors.length) code = 1;
    await page.close();
  }
} finally {
  await browser.close();
  server.close();
}
process.exit(code);
