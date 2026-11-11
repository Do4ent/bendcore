// Dictionaries, keyboard shortcuts, recent-file list and file detection of the application shell
// (docs/ui/APP_SHELL.md), and the LatestRunner of pointer-move solves.

import { DICTIONARIES, LANGS, en, placeholders, ru, translate } from '../i18n';
import type { MessageKey } from '../i18n';
import { hotkeyAction } from '../hotkeys';
import { detectKind, mergeRecent, overflowRecent, recentId } from '../storage';
import type { RecentEntry } from '../storage';
import { LatestRunner } from '../../worker/latestRunner';

describe('dictionaries', () => {
  test('both languages have exactly the same keys', () => {
    expect(Object.keys(en).sort()).toEqual(Object.keys(ru).sort());
  });

  test('every message keeps the placeholders of the Russian one', () => {
    for (const key of Object.keys(ru) as MessageKey[]) {
      for (const lang of LANGS) expect([key, placeholders(DICTIONARIES[lang][key])]).toEqual([key, placeholders(ru[key])]);
    }
  });

  test('no message is empty or left untranslated by accident', () => {
    for (const key of Object.keys(ru) as MessageKey[]) {
      expect(ru[key].trim()).not.toBe('');
      expect(en[key].trim()).not.toBe('');
      // English strings have no Cyrillic letters.
      expect([key, /[А-Яа-яЁё]/.test(en[key])]).toEqual([key, false]);
    }
  });

  test('translate fills placeholders and keeps unknown ones', () => {
    expect(translate('ru', 'busy.working', { operation: 'перестроение', seconds: '2.5' })).toBe('Выполняется: перестроение · 2.5 с');
    expect(translate('en', 'busy.working', { operation: 'rebuild', seconds: 3 })).toBe('Running: rebuild · 3 s');
    expect(translate('en', 'notify.opened')).toBe('Opened {name}');
  });
});

describe('hotkeys', () => {
  const key = (k: string, mods: Partial<{ ctrlKey: boolean; metaKey: boolean; shiftKey: boolean; altKey: boolean }> = {}, targetTag?: string) =>
    hotkeyAction({ key: k, ctrlKey: false, metaKey: false, shiftKey: false, altKey: false, ...mods, targetTag });

  test('the bound keys', () => {
    expect(key('z', { ctrlKey: true })).toBe('undo');
    expect(key('Z', { ctrlKey: true, shiftKey: true })).toBe('redo');
    expect(key('y', { metaKey: true })).toBe('redo');
    expect(key('s', { ctrlKey: true })).toBe('save');
    expect(key('o', { ctrlKey: true })).toBe('open');
    expect(key('Escape')).toBe('escape');
    expect(key('Delete')).toBe('delete');
    expect(key('f')).toBe('fit');
    expect(key('F')).toBe('fit');
  });

  test('typing in a field keeps its keys, except Esc', () => {
    expect(key('z', { ctrlKey: true }, 'INPUT')).toBeNull();
    expect(key('f', {}, 'textarea')).toBeNull();
    expect(key('Delete', {}, 'SELECT')).toBeNull();
    expect(key('Escape', {}, 'INPUT')).toBe('escape');
  });

  test('other keys and modifiers are not bound', () => {
    expect(key('f', { shiftKey: true })).toBeNull();
    expect(key('f', { ctrlKey: true })).toBeNull();
    expect(key('s', { ctrlKey: true, altKey: true })).toBeNull();
    expect(key('a')).toBeNull();
  });
});

describe('recent files', () => {
  const entry = (name: string, usedAt: number): RecentEntry => ({
    id: recentId('part', name),
    file: { kind: 'part', name, text: '{}' },
    usedAt,
  });

  test('newest first, one entry per file, limited', () => {
    let list: RecentEntry[] = [];
    list = mergeRecent(list, entry('a', 1), 3);
    list = mergeRecent(list, entry('b', 2), 3);
    list = mergeRecent(list, entry('a', 3), 3);
    expect(list.map((e) => [e.file.name, e.usedAt])).toEqual([
      ['a', 3],
      ['b', 2],
    ]);
    list = mergeRecent(list, entry('c', 4), 3);
    list = mergeRecent(list, entry('d', 5), 3);
    expect(list.map((e) => e.file.name)).toEqual(['d', 'c', 'a']);
  });

  test('overflow lists the oldest entries beyond the limit', () => {
    const list = [entry('a', 1), entry('b', 5), entry('c', 3)];
    expect(overflowRecent(list, 2).map((e) => e.file.name)).toEqual(['a']);
    expect(overflowRecent(list, 5)).toEqual([]);
  });

  test('file kinds from the format field', () => {
    expect(detectKind('{"format":"bendcore-part","version":3}')).toEqual({ kind: 'part' });
    expect(detectKind('{"format":"bendcore-assembly"}')).toEqual({ kind: 'assembly' });
    expect(detectKind('{"format":"bendcore-drawing"}')).toEqual({ kind: 'drawing' });
    expect(detectKind('{"format":"other"}')).toEqual({ error: 'unknown format "other"' });
    expect(detectKind('{}')).toEqual({ error: 'unknown format null' });
    expect('error' in detectKind('ISO-10303-21;')).toBe(true);
  });
});

describe('LatestRunner', () => {
  test('runs one job at a time and only the newest queued one', async () => {
    const runner = new LatestRunner();
    const started: number[] = [];
    const gates: (() => void)[] = [];
    const job = (n: number) => () =>
      new Promise<void>((resolve) => {
        started.push(n);
        gates.push(resolve);
      });
    runner.schedule(job(1));
    runner.schedule(job(2));
    runner.schedule(job(3));
    expect(started).toEqual([1]);
    gates[0]();
    await new Promise((r) => setTimeout(r, 0));
    expect(started).toEqual([1, 3]);
    let settled = false;
    const done = runner.settled().then(() => {
      settled = true;
    });
    await new Promise((r) => setTimeout(r, 0));
    expect(settled).toBe(false);
    gates[1]();
    await done;
    expect(settled).toBe(true);
  });

  test('a failing job does not stop the next one; clear drops the queued job', async () => {
    const runner = new LatestRunner();
    const ran: string[] = [];
    runner.schedule(async () => {
      ran.push('fail');
      throw new Error('x');
    });
    runner.schedule(async () => {
      ran.push('dropped');
    });
    runner.clear();
    await runner.settled();
    runner.schedule(async () => {
      ran.push('next');
    });
    await runner.settled();
    expect(ran).toEqual(['fail', 'next']);
  });
});
