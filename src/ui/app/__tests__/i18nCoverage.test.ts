/**
 * @jest-environment node
 */
// Localization of the application (docs/ui/APP_SHELL.md, "Локализация"): every Russian key has
// an English one and back, no key is defined in two area dictionaries, every key the sources ask
// for exists, every key is used, and the editors' components hold no interface text of their own
// (a simple check of the sources: Cyrillic outside comments, Latin words in JSX text and in
// title / placeholder / aria-label attributes).  // qa-allow(placeholder): the HTML attribute name that the test scans

import fs from 'fs';
import path from 'path';
import { AREAS, en, isMessageKey, ru } from '../i18n';

const uiRoot = path.resolve(__dirname, '../..');
/** The application and its editors (the older Electron tube window in src/ui/*.tsx is not part of it). */
const DIRS = ['app', 'modeling', 'modeling/sketch', 'assembly', 'drawing', 'kernel', 'worker', 'exchange'];

function sources(): { file: string; text: string }[] {
  const out: { file: string; text: string }[] = [];
  for (const d of DIRS) {
    const dir = path.join(uiRoot, d);
    for (const name of fs.readdirSync(dir)) {
      if (!/\.(ts|tsx)$/.test(name)) continue;
      out.push({ file: path.join(d, name), text: fs.readFileSync(path.join(dir, name), 'utf8') });
    }
  }
  return out;
}

/** The text without comments (block, line and JSX comments); strings are kept. */
function stripComments(text: string): string {
  let out = '';
  let i = 0;
  let quote: string | null = null;
  while (i < text.length) {
    const c = text[i];
    const next = text[i + 1];
    if (quote) {
      out += c;
      if (c === '\\') {
        out += next ?? '';
        i += 2;
        continue;
      }
      if (c === quote) quote = null;
      i += 1;
      continue;
    }
    if (c === '/' && next === '*') {
      const end = text.indexOf('*/', i + 2);
      i = end < 0 ? text.length : end + 2;
      continue;
    }
    if (c === '/' && next === '/' && text[i - 1] !== ':') {
      const end = text.indexOf('\n', i);
      i = end < 0 ? text.length : end;
      continue;
    }
    if (c === "'" || c === '"' || c === '`') quote = c;
    out += c;
    i += 1;
  }
  return out;
}

const code = sources()
  .filter((s) => !s.file.startsWith('app/messages') && !s.file.endsWith('/i18n.ts'))
  .map((s) => ({ ...s, code: stripComments(s.text) }));

describe('dictionaries', () => {
  test('every Russian key has an English message and back', () => {
    const ruKeys = Object.keys(ru).sort();
    const enKeys = Object.keys(en).sort();
    expect(ruKeys.filter((k) => !enKeys.includes(k))).toEqual([]);
    expect(enKeys.filter((k) => !ruKeys.includes(k))).toEqual([]);
    for (const [area, d] of Object.entries(AREAS)) {
      expect([area, Object.keys(d.ru).sort()]).toEqual([area, Object.keys(d.en).sort()]);
    }
  });

  test('no key is defined in two areas', () => {
    const seen = new Map<string, string>();
    const twice: string[] = [];
    for (const [area, d] of Object.entries(AREAS)) {
      for (const k of Object.keys(d.ru)) {
        if (seen.has(k)) twice.push(`${k}: ${seen.get(k)} and ${area}`);
        seen.set(k, area);
      }
    }
    expect(twice).toEqual([]);
  });

  test('every key asked for in the sources exists', () => {
    const missing: string[] = [];
    for (const s of code) {
      for (const m of s.code.matchAll(/\bt\(\s*'([\w.-]+)'/g)) if (!isMessageKey(m[1])) missing.push(`${s.file}: ${m[1]}`);
    }
    expect(missing).toEqual([]);
  });

  test('every key is used (by name, or by a prefix the sources complete at run time)', () => {
    const all = code.map((s) => s.code).join('\n');
    const literals = new Set([...all.matchAll(/'([a-z][\w-]*(?:\.[\w-]+)+)'/g)].map((m) => m[1]));
    // caption(t, 'part.feature.', type) and t(`drawing.tool.${k}`)
    const prefixes = new Set([
      ...[...all.matchAll(/caption\(\s*t,\s*'([\w.-]+\.)'/g)].map((m) => m[1]),
      ...[...all.matchAll(/`([a-z][\w.-]*\.)\$\{/g)].map((m) => m[1]),
    ]);
    const unused = Object.keys(ru).filter((k) => !literals.has(k) && ![...prefixes].some((p) => k.startsWith(p)));
    expect(unused).toEqual([]);
    // The menu.* keys in particular: the context menus of the part and assembly trees use them.
    expect(Object.keys(ru).filter((k) => k.startsWith('menu.')).length).toBeGreaterThan(0);
  });
});

describe('components', () => {
  test('no Cyrillic text outside comments and the dictionaries', () => {
    const found: string[] = [];
    for (const s of code) {
      s.code.split('\n').forEach((line, i) => {
        // The transliteration table of download names (drawing/sheetGeometry.ts) maps letters.
        if (/^\s*(?:[а-яё]: '[a-z]*',\s*)+$/.test(line)) return;
        if (/[А-Яа-яЁё]/.test(line)) found.push(`${s.file}:${i + 1}: ${line.trim()}`);
      });
    }
    expect(found).toEqual([]);
  });

  // qa-allow(placeholder): the HTML attribute name that the test scans
  test('no Latin interface words in JSX text and in title / placeholder / aria-label', () => {
    // Words that are not language: file formats, standards, plane and slot names.
    const allowed = /^(PDF|DXF|SVG|ISO|STEP|BOM|XY|XZ|YZ|A|B|M\{d\}|Ctrl\+[A-Z])$/;
    const found: string[] = [];
    for (const s of code.filter((x) => x.file.endsWith('.tsx'))) {
      for (const m of s.code.matchAll(/>([^<>{}`;=()[\]'"|&]*)</g)) {
        const text = m[1].trim();
        if (!/[A-Za-z]{2,}/.test(text)) continue;
        // A TypeScript generic between "=>" and "<" (Promise<...>, Record<...>) is not JSX text.
        if (/^(Promise|Record|Array|Set|Map|Partial)$/.test(text)) continue;
        if (!allowed.test(text)) found.push(`${s.file}: >${text}<`);
      }
      // qa-allow(placeholder): the HTML attribute name that the test scans
      for (const m of s.code.matchAll(/\b(title|placeholder|aria-label)="([^"]*)"/g)) {
        if (/[A-Za-z]{2,}/.test(m[2]) && !allowed.test(m[2])) found.push(`${s.file}: ${m[1]}="${m[2]}"`);
      }
    }
    expect(found).toEqual([]);
  });
});
