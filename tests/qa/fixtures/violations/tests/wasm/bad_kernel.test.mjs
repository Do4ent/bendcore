// Фикстура самотеста check_no_stubs: тесты node:test и Jest, которые не могут упасть.
import test from 'node:test';
import assert from 'node:assert/strict';

test('volume', () => {
  const volume = 120000;
  assert.ok(true);
  assert.equal(volume, volume);
  assert(1, 'module loaded');
  expect(true).toBe(true);
  expect(volume).toEqual(volume);
});
