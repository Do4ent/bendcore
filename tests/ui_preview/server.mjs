// Локальный сервер стенда: статика dist/ и POST /call → bendcore_cli.
import http from 'node:http';
import { readFileSync } from 'node:fs';
import { spawn } from 'node:child_process';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const cli = spawn(process.env.BENDCORE_CLI ?? 'build/bendcore_cli');
let buf = '';
let id = 0;
const pending = new Map();
cli.stdout.setEncoding('utf8');
cli.stdout.on('data', (c) => {
  buf += c;
  let i;
  while ((i = buf.indexOf('\n')) >= 0) {
    const line = buf.slice(0, i);
    buf = buf.slice(i + 1);
    const m = JSON.parse(line);
    pending.get(m.id)?.(m);
    pending.delete(m.id);
  }
});
const call = (command, args) =>
  new Promise((res) => {
    const n = ++id;
    pending.set(n, res);
    cli.stdin.write(JSON.stringify({ id: n, command, args }) + '\n');
  });

// Реестр экземпляров для стенда: деталь из трёх экземпляров и независимая копия с другим обозначением.
const L = [{ kind: 'straight', length: 100 }, { kind: 'bend', radius: 50, angle: 90 }, { kind: 'straight', length: 100 }];
const data = { path: L, od: 12, material: 'Cu-DHP' };
async function seed() {
  await call('instances.reset', {});
  await call('instances.add_part', { part: 'Д-1', designation: 'БК.200.001', data });
  for (const i of ['Т-11', 'Т-12', 'Т-13']) await call('instances.insert', { part: 'Д-1', instance: i });
  await call('instances.add_part', { part: 'Д-2', designation: 'БК.200.009', data });
  await call('instances.insert', { part: 'Д-2', instance: 'Т-21' });
  await call('instances.set_adaptive', { instance: 'Т-11', adaptive: true });
}
await seed();

const types = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript', '.css': 'text/css' };
http
  .createServer(async (req, res) => {
    if (req.method === 'POST' && req.url === '/reset') {
      await seed();
      res.writeHead(204);
      res.end();
      return;
    }
    if (req.method === 'POST' && req.url === '/call') {
      let body = '';
      for await (const c of req) body += c;
      const { command, args } = JSON.parse(body);
      const r = await call(command, args);
      res.writeHead(200, { 'content-type': 'application/json' });
      res.end(JSON.stringify(r));
      return;
    }
    const path = req.url.split('?')[0] === '/' ? '/index.html' : req.url.split('?')[0];
    try {
      const f = readFileSync(join(here, 'dist', path));
      res.writeHead(200, { 'content-type': types[path.slice(path.lastIndexOf('.'))] ?? 'application/octet-stream' });
      res.end(f);
    } catch {
      res.writeHead(404);
      res.end();
    }
  })
  .listen(Number(process.env.PORT ?? 5199), () => console.log('ready'));
