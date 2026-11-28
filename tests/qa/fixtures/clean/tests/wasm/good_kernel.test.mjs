// Фикстура самотеста check_no_stubs: тест node:test, сравнивающий с формулой.
import test from 'node:test';
import assert from 'node:assert/strict';

function boxVolume(a, b, c) {
  return a * b * c;
}

test('box volume against the formula', () => {
  const expected = 100 * 60 * 20;
  assert.equal(boxVolume(100, 60, 20), expected);
  assert.ok(Math.abs(boxVolume(1, 2, 3) - 6) < 1e-12);
});
