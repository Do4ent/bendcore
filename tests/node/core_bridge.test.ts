// Проверка моста Electron ↔ ядро на собранном bendcore_cli (без Electron).
// Запуск: BENDCORE_CLI=<путь> node --import tsx --test tests/node/core_bridge.test.ts
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { CoreBridge, CoreError } from '../../src/core_bridge';

const exe = process.env.BENDCORE_CLI ?? 'build/bendcore_cli';
const L = [
  { kind: 'straight', length: 100 },
  { kind: 'bend', radius: 50, angle: 90, rotation: 0 },
  { kind: 'straight', length: 100 },
];

test('команда ядра возвращает данные', async () => {
  const core = new CoreBridge({ executable: exe });
  try {
    const r = await core.call<{ rows: Array<{ L: number }> }>('bend.lra', { path: L });
    assert.equal(r.rows.length, 2);
    assert.equal(r.rows[0].L, 100);
  } finally {
    core.dispose();
  }
});

test('параллельные запросы сопоставляются по id', async () => {
  const core = new CoreBridge({ executable: exe });
  try {
    const lens = [10, 20, 30, 40, 50];
    const res = await Promise.all(
      lens.map((x) => core.call<{ last_S: number }>('bend.table', { path: [{ kind: 'straight', length: x }] })),
    );
    assert.deepEqual(res.map((r) => r.last_S), lens);
  } finally {
    core.dispose();
  }
});

test('ошибка ядра — CoreError с текстом', async () => {
  const core = new CoreBridge({ executable: exe });
  try {
    await assert.rejects(core.call('bend.lra', {}), (e: unknown) => e instanceof CoreError && e.message === 'нет поля path');
    // Ядро продолжает работать после ошибки.
    const ok = await core.call<{ api: number }>('health');
    assert.equal(ok.api, 1);
  } finally {
    core.dispose();
  }
});

test('ядро перезапускается после падения процесса', async () => {
  const core = new CoreBridge({ executable: exe });
  try {
    await core.call('health');
    (core as unknown as { proc: { kill: () => void } }).proc.kill();
    await new Promise((r) => setTimeout(r, 100));
    assert.equal(core.running, false);
    const ok = await core.call<{ api: number }>('health');
    assert.equal(ok.api, 1);
  } finally {
    core.dispose();
  }
});

test('нет исполняемого файла — понятная ошибка', async () => {
  const core = new CoreBridge({ executable: '/nonexistent/bendcore_cli' });
  await assert.rejects(core.call('health'), (e: unknown) => e instanceof CoreError && /не удалось запустить ядро/.test(e.message));
});
