// Логика новых панелей без React: модель «Проверки», блокировка оси, SVG листа, клиент ядра.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { checkState, countBy, factNorm, filterViolations, iconFor, place } from '../../src/ui/core/checkModel';
import { closestOnLineToRay, lockedPoint, toggleAxis } from '../../src/ui/core/axisLock';
import { drawingToSvg, escapeXml, tableHeight } from '../../src/ui/core/drawingSvg';
import { CoreClient, CoreClientError, wasmTransport } from '../../src/ui/core/coreClient';
import { Severity, type CheckResult, type DrawingData, type Violation } from '../../src/ui/core/types';
import { CoreBridge } from '../../src/core_bridge';

const exe = process.env.BENDCORE_CLI ?? 'build/bendcore_cli';

const v = (p: Partial<Violation>): Violation => ({
  tube: 'T1', element: -1, severity: Severity.Info, class: '', rule: 'other', message: 'm',
  value: null, limit: null, other: '', at_s: null, ...p,
});

test('значки по единому языку состояний (6.2)', () => {
  assert.equal(iconFor(v({ severity: Severity.Technological })).glyph, '●');
  assert.equal(iconFor(v({ severity: Severity.Warning })).glyph, '▲');
  assert.match(iconFor(v({ severity: Severity.Mark, rule: 'tube_intersection' })).color, /D93A3A/);
  assert.match(iconFor(v({ severity: Severity.Mark, rule: 'clearance' })).color, /E8820C/);
});

test('фильтр, место, факт/норма, счётчики', () => {
  const list = [
    v({ severity: Severity.Geometric, tube: 'A', element: 2, message: 'S < 0' }),
    v({ severity: Severity.Mark, tube: 'B', other: 'C', value: 3, limit: 5, rule: 'clearance' }),
  ];
  assert.equal(filterViolations(list, new Set([Severity.Mark])).length, 1);
  assert.equal(filterViolations(list, new Set([Severity.Geometric, Severity.Mark]), 'c').length, 1);
  assert.equal(place(list[0]), 'A, элемент 3');
  assert.equal(place(list[1]), 'B ↔ C');
  assert.equal(factNorm(list[1]), '3.0 / 5.0');
  assert.equal(countBy(list)[Severity.Geometric], 1);
});

test('состояние проверки и устаревание по отпечаткам (4.2)', () => {
  const r: CheckResult = {
    violations: [], counts: {}, pairs_tested: 0,
    tubes: { T1: { state: 'passed', worst: null, blocks_output: false, fingerprint: '1' } },
  };
  assert.equal(checkState(null, false, null), 'not_checked');
  assert.equal(checkState(r, true, null), 'running');
  assert.equal(checkState(r, false, { T1: '1' }), 'passed');
  assert.equal(checkState(r, false, { T1: '2' }), 'stale');
  assert.equal(checkState(r, false, { T1: '1', T2: '9' }), 'stale');
});

test('блокировка оси: точка на оси, ближайшая к лучу', () => {
  const p = closestOnLineToRay({ x: 0, y: 0, z: 0 }, { x: 1, y: 0, z: 0 }, { x: 5, y: 10, z: 3 }, { x: 0, y: -1, z: 0 });
  assert.deepEqual(p, { x: 5, y: 0, z: 0 });
  const free = { x: 1, y: 2, z: 3 };
  assert.equal(lockedPoint(null, 'x', free, free, free), free);
  assert.equal(toggleAxis(null, 'Z'), 'z');
  assert.equal(toggleAxis('z', 'z'), null);
  assert.equal(toggleAxis('x', 'q'), 'x');
});

test('SVG листа: рамка, штамп, «Не для производства», экранирование', async () => {
  const core = new CoreBridge({ executable: exe });
  try {
    const r = await core.call<{ sheets: DrawingData[] }>('drawing.part', {
      path: [{ kind: 'straight', length: 100 }, { kind: 'bend', radius: 50, angle: 90 }, { kind: 'straight', length: 100 }],
      od: 20, has_errors: true, title: { 'Обозначение': 'A<B&C' },
    });
    const svg = drawingToSvg(r.sheets[0], { stamp: { released: true, by: 'Петров', date: '2026-10-23' } });
    assert.match(svg, /^<svg /);
    assert.match(svg, /viewBox="0 0 420 297"/);
    assert.match(svg, /НЕ ДЛЯ ПРОИЗВОДСТВА/);
    assert.match(svg, /A&lt;B&amp;C/);
    assert.match(svg, /ВЫПУЩЕН 2026-10-23 Петров/);
    assert.match(svg, /ГОСТ \(ЕСКД\)/);
    assert.equal((svg.match(/data-view=/g) ?? []).length, r.sheets[0].views.length);
    assert.equal(tableHeight({ title: '', header: ['a'], rows: [['1'], ['2']], note: 'n' }), 8 + 7 + 6 + 12);
  } finally {
    core.dispose();
  }
  assert.equal(escapeXml('"x"'), '&quot;x&quot;');
});

test('клиент ядра через WebAssembly-подобный dispatch', async () => {
  const fake = (line: string) => {
    const req = JSON.parse(line);
    return req.command === 'bend.lra'
      ? JSON.stringify({ id: req.id, ok: true, data: { rows: [], csv: 'x' } })
      : JSON.stringify({ id: req.id, ok: false, error: 'нет поля path' });
  };
  const c = new CoreClient(wasmTransport(fake));
  assert.equal((await c.lra({ path: [] })).csv, 'x');
  await assert.rejects(c.xyz({ path: [] }), (e: unknown) => e instanceof CoreClientError && e.message === 'нет поля path');
});
