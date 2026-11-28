/**
 * @jest-environment node
 */
// Kernel messages in Russian (docs/ui/APP_SHELL.md, "Сообщения ядра").
//
// Coverage: every message the C++ sources of src/modeling, src/assembly_v2, src/drawing_v2, src/kernel
// and src/dxf_reader.cpp generate (ErrorCode::X with a text, markError, messages/warnings of feature
// nodes, resolution details, messages built by local helpers such as invalid("..."), bad("...") —
// kernelMessageSources.ts) matches a template of KERNEL_TEMPLATES (a message the user can meet) or is
// listed in kernelMessagesOther.json with the reason why the user never meets it (an internal message:
// the localized title of its code and the original text in the details). The list is exact: a new kernel
// message fails the test until it gets a template or an entry with a reason; an entry that a template
// covers or that no source generates any more fails it too.
//   BENDCORE_UPDATE_KERNEL_MESSAGES=1 npx jest src/ui/app/__tests__/kernelMessages.test.ts
// rewrites the list (new entries get the reason TODO that the test refuses until it is replaced).

import fs from 'fs';
import path from 'path';
import { en, ru, translate } from '../i18n';
import type { MessageKey, Translate } from '../i18n';
import { KERNEL_TEMPLATES, localizeKernelError, localizeKernelMessage, matchKernelText, patternRegExp } from '../kernelMessages';
import { messagesOfSource, skeletonsOf } from '../kernelMessageSources';
import type { KernelMessageSource } from '../kernelMessageSources';

const repo = path.resolve(__dirname, '../../../..');
const OTHER_FILE = path.resolve(__dirname, '../kernelMessagesOther.json');

/** Internal messages: "Code\tskeleton" -> key of the reason why the user never meets it. */
function readInternal(): { reasons: Record<string, string>; entries: Record<string, string> } {
  return JSON.parse(fs.readFileSync(OTHER_FILE, 'utf8'));
}

const tRu: Translate = (key: MessageKey, params) => translate('ru', key, params);
const tEn: Translate = (key: MessageKey, params) => translate('en', key, params);

function walk(dir: string): string[] {
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap((e) => {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) return walk(p);
    return /\.(cpp|hpp|h)$/.test(e.name) ? [p] : [];
  });
}

function messagesOf(paths: string[]): KernelMessageSource[] {
  const out: KernelMessageSource[] = [];
  for (const rel of paths) {
    const abs = path.join(repo, rel);
    const files = fs.statSync(abs).isDirectory() ? walk(abs) : [abs];
    for (const f of files) out.push(...messagesOfSource(fs.readFileSync(f, 'utf8'), path.relative(repo, f)));
  }
  return out;
}

/** The areas whose messages must be covered (the task of the coverage test). */
const COVERED = ['src/modeling', 'src/assembly_v2', 'src/drawing_v2', 'src/kernel', 'src/dxf_reader.cpp', 'src/wasm'];
/** Further kernel sources a template may be written for (kernel core, DXF reader, bindings). */
const EXTRA: string[] = [];

const entryOf = (m: KernelMessageSource) => `${m.code}\t${m.skeleton}`;
/** A skeleton as a text: each value is one field character. */
const sample = (skeleton: string) => skeleton.replace(/\{\}/g, '§');

const loose = KERNEL_TEMPLATES.map(([id, pattern]) => ({ id, re: patternRegExp(pattern, true).re }));
const templateOf = (skeleton: string) => loose.find((l) => l.re.test(sample(skeleton)))?.id ?? null;

describe('message extraction from C++', () => {
  test('literals, values, ternaries and helpers', () => {
    expect(skeletonsOf('"radius " + std::to_string(r) + " is too large"')).toEqual(['radius {} is too large']);
    expect(skeletonsOf('std::string("slot \'") + slot + "\' is empty"')).toEqual(["slot '{}' is empty"]);
    expect(skeletonsOf('flag ? "the bodies do not intersect" : "the operation removes the whole body"')).toEqual([
      'the bodies do not intersect',
      'the operation removes the whole body',
    ]);
    expect(skeletonsOf('occtMessage("fillet", e.what())')).toEqual(['OCCT exception in fillet: {}']);
    expect(skeletonsOf('error')).toEqual([]);
    expect(skeletonsOf('"a" "b" + x')).toEqual(['ab{}']);
    const found = messagesOfSource(
      [
        '// return Error{ErrorCode::InvalidInput, "commented out"};',
        'return Result::failure(ErrorCode::OperationFailed, "cut removes " + n + " bodies");',
        'markError("broken reference " + where + ": " + detail);',
        "if (c == ',') return Error{ErrorCode::ParseError, \"comma\"};",
      ].join('\n'),
      'x.cpp',
    );
    // Local helpers that build the error from a text: every call is a message of the helper's code.
    const helpers = messagesOfSource(
      [
        'Error invalid(std::string m) { return Error{ErrorCode::InvalidInput, std::move(m)}; }',
        '[[noreturn]] void bad(const std::string& message) { throw FormatError(message); }',
        'auto a = invalid("no sheet \'" + sheet + "\'");',
        'if (!ok) bad("missing field \'" + name + "\'");',
      ].join('\n'),
      'h.cpp',
    );
    expect(helpers.map((m) => [m.code, m.skeleton])).toEqual([
      ['InvalidInput', "no sheet '{}'"],
      ['ParseError', "missing field '{}'"],
    ]);
    expect(found.map((m) => [m.code, m.skeleton, m.where])).toEqual([
      ['OperationFailed', 'cut removes {} bodies', 'x.cpp:2'],
      ['ParseError', 'comma', 'x.cpp:4'],
      ['message', 'broken reference {}: {}', 'x.cpp:3'],
    ]);
  });
});

describe('coverage of the kernel messages', () => {
  const covered = messagesOf(COVERED);
  const extra = messagesOf(EXTRA);

  test('the sources generate hundreds of messages of every main code', () => {
    const codes = new Set(covered.map((m) => m.code));
    for (const c of ['InvalidInput', 'OperationFailed', 'ParseError', 'IoError', 'Unsupported', 'Fault', 'message']) {
      expect([c, codes.has(c)]).toEqual([c, true]);
    }
    expect(new Set(covered.map(entryOf)).size).toBeGreaterThan(500);
  });

  test('every message has a template or is listed as internal with a reason', () => {
    const uncovered = [...new Set(covered.filter((m) => templateOf(m.skeleton) === null).map(entryOf))].sort();
    if (process.env.BENDCORE_UPDATE_KERNEL_MESSAGES) {
      // Keeps the reasons of the entries that stay; a new entry gets the reason TODO that the test below refuses.
      const old = readInternal();
      const entries: Record<string, string> = {};
      for (const e of uncovered) entries[e] = old.entries[e] ?? 'TODO';
      fs.writeFileSync(OTHER_FILE, JSON.stringify({ reasons: old.reasons, entries }, null, 1) + '\n');
    }
    const { reasons, entries } = readInternal();
    const listed = Object.keys(entries);
    const where = new Map(covered.map((m) => [entryOf(m), m.where]));
    // New messages without a template: add a template (visible to the user) or an entry with a reason (internal).
    expect(uncovered.filter((e) => !listed.includes(e)).map((e) => `${e}  (${where.get(e)})`)).toEqual([]);
    // Entries no source generates any more, or that a template covers now.
    expect(listed.filter((e) => !uncovered.includes(e))).toEqual([]);
    // Every entry names a reason; every reason is explained and used.
    expect(Object.entries(entries).filter(([, r]) => !(r in reasons)).map(([e, r]) => `${e}: ${r}`)).toEqual([]);
    expect(Object.keys(reasons).filter((r) => !Object.values(entries).includes(r))).toEqual([]);
    for (const [key, text] of Object.entries(reasons)) expect([key, text.length >= 60]).toEqual([key, true]);
  });

  test('all messages the user can meet have a template: 100 % of the visible ones', () => {
    const all = new Set(covered.map(entryOf));
    const internal = new Set(Object.keys(readInternal().entries));
    const visible = [...all].filter((e) => !internal.has(e));
    const withTemplate = visible.filter((e) => templateOf(e.split('\t')[1]) !== null);
    expect(withTemplate.length).toBe(visible.length);
    // The visible messages are the large majority: the internal list stays the exception.
    expect(visible.length / all.size).toBeGreaterThan(0.8);
  });

  test('every template matches a message of the kernel sources', () => {
    const all = [...covered, ...extra];
    // A message is also met as the nested part of a prefix message ("configuration 'A': <message>").
    const tails = (skeleton: string) => skeleton.split(': ').map((_, i, parts) => parts.slice(i).join(': '));
    const samples = [...new Set(all.flatMap((m) => tails(m.skeleton).map(sample)))];
    const dead = KERNEL_TEMPLATES.filter(([id]) => !samples.some((s) => loose.find((l) => l.id === id)!.re.test(s)));
    expect(dead.map(([id, p]) => `${id}: ${p}`)).toEqual([]);
  });

  test('every template has a Russian text without kernel English and an English text', () => {
    const latin = /[A-Za-z]{3,}/g;
    const allowed = new Set(['OCCT', 'DXF', 'INSUNITS', 'ASCII', 'bendcore', 'json', 'step', 'stp', 'brep', 'sketch', 'JSON', 'WebAssembly', 'ISO',
      // Names of file formats and fonts, as the user sees them in the interface.
      'true', 'false', 'sin', 'gost', 'truetype', 'FFFD', 'EOF', 'zlib', 'STEP', 'IGES', 'ZIP', 'BRep', 'LWPOLYLINE', 'POLYLINE', 'SECTION', 'ENDSEC', 'SEQEND', 'LAYER', 'solid', 'CRC', 'RRGGBB', 'Rmax', 'PDF', 'CSV', 'STL', 'TrueType', 'DejaVu', 'Sans', 'Liberation', 'BENDCORE', 'DRAWING', 'FONT', 'base', 'R2000']);
    for (const [id, pattern] of KERNEL_TEMPLATES) {
      const key = `kernel.msg.${id}` as MessageKey;
      expect([id, key in ru, key in en]).toEqual([id, true, true]);
      const names = [...pattern.matchAll(/\{(\w+)(?::\w+)?\}/g)].map((m) => m[1]);
      const text = translate('ru', key, Object.fromEntries(names.map((n) => [n, '§'])));
      expect([id, (text.match(latin) ?? []).filter((w) => !allowed.has(w))]).toEqual([id, []]);
      expect([id, /\{\w+\}/.test(text)]).toEqual([id, false]);
    }
  });
});

describe('template parameters', () => {
  const names = (text: string) => [...new Set([...text.matchAll(/\{(\w+)\}/g)].map((m) => m[1]))].sort();

  test('the Russian and the English text of a template use the same parameters, all taken from the pattern', () => {
    const bad: string[] = [];
    for (const [id, pattern] of KERNEL_TEMPLATES) {
      const key = `kernel.msg.${id}` as MessageKey;
      const fields = names(pattern.replace(/\{(\w+):\w+\}/g, '{$1}'));
      const inRu = names(ru[key as keyof typeof ru] as string);
      const inEn = names(en[key as keyof typeof en] as string);
      if (inRu.join() !== inEn.join()) bad.push(`${id}: ru {${inRu}} en {${inEn}}`);
      for (const n of [...inRu, ...inEn]) if (!fields.includes(n)) bad.push(`${id}: {${n}} is not a field of the pattern`);
    }
    expect(bad).toEqual([]);
  });

  test('template ids and patterns are unique', () => {
    expect(new Set(KERNEL_TEMPLATES.map(([id]) => id)).size).toBe(KERNEL_TEMPLATES.length);
    expect(new Set(KERNEL_TEMPLATES.map(([, p]) => p)).size).toBe(KERNEL_TEMPLATES.length);
  });
});

describe('localization', () => {
  test('the main operation errors are shown in Russian', () => {
    const cases: [string, string, RegExp][] = [
      ['InvalidInput', 'an open-contour sketch has no regions to select', /^Контур эскиза не замкнут/],
      ['InvalidInput', 'the sketch profile has no material region', /^В профиле эскиза нет замкнутой области$/],
      [
        'OperationFailed',
        'fillet radius 30.000000 failed: OCCT exception in BRepFilletAPI_MakeFillet::Build: BRep_API: command not done; failed edges: #3 (contour status Error)',
        /^Скругление радиусом 30 не построено: радиус, вероятно, слишком велик/,
      ],
      ['OperationFailed', 'the bodies do not intersect', /^Тела не пересекаются$/],
      ['OperationFailed', 'OCCT exception in bodyBoolean: Standard_ConstructionError', /^Сбой геометрического ядра OCCT в операции bodyBoolean$/],
      ['ParseError', 'not JSON: [json.exception.parse_error.101] parse error', /^Файл не является документом JSON$/],
      ['ParseError', 'drawing: not a bendcore-drawing file', /^Файл не является чертежом bendcore$/],
      ['Unsupported', 'format version 9 is newer than the supported 3', /^Версия формата 9 новее поддерживаемой \(3\)$/],
      ['InvalidInput', "sketch 'Profile' has no closed profile", /^Эскиз «Profile» не содержит замкнутого контура$/],
      ['InvalidInput', 'fillet needs an existing body', /^Скругление: нужно уже построенное тело$/],
      ['InvalidInput', 'the DXF file has no $INSUNITS; choose the drawing units', /^В файле DXF нет единиц/],
      // Views and sections: the sheet and view prefix with the nested reason.
      ['OperationFailed', "sheet 'S1', view 'V2': section: OCCT failure: BRepAlgoAPI_Cut", /^Лист «S1», вид «V2»: разрез: сбой геометрического ядра OCCT$/],
      ['message', "sheet 'S1': views 'F' and 'V3' overlap", /^Лист «S1»: виды «F» и «V3» перекрываются$/],
      ['message', "sheet 'S1': view 'V3' at 1:1 does not fit its cell; set its position on the sheet",
        /^Лист «S1»: вид «V3» в масштабе 1:1 не помещается в свою ячейку — задайте его положение на листе$/],
      ['InvalidInput', 'stepped section: two plane points at the same position along the step axis', /^Ступенчатый разрез: две точки/],
      // Dimensions and positions.
      ['message', 'edge #4 resolved to 2 elements', /^Ссылке edge #4 соответствуют 2 элементов$/],
      ['message', "sheet 'S1', view 'V1': no assembly part is visible for positions", /^Лист «S1», вид «V1»: не видно ни одной детали/],
      // Assembly: mates, fasteners, patterns, loading of files.
      ['InvalidInput', 'occurrence 7 is not a top-level occurrence', /^occurrence 7 не является вхождением верхнего уровня$/],
      ['InvalidInput', 'hole at (15, 30, 20): screw is too short: grip 25 mm, shank ends 3.5 mm inside the nut face', /винт короток: пакет 25 мм/],
      ['InvalidInput', 'circular pattern needs count >= 2', /^Круговому массиву нужно не меньше двух экземпляров$/],
      ['IoError', "cannot read subassembly 'frame.json': cannot open 'frame.json'", /^Не удалось прочитать подсборку «frame\.json»: не удаётся открыть «frame\.json»$/],
      ['ParseError', 'cache: invalid base64', /^Кэш формы в файле повреждён/],
      // Part operations.
      ['OperationFailed', 'rib does not reach the body on either side', /^Ребро жёсткости не достаёт до тела ни с одной стороны$/],
      ['InvalidInput', "hole #2: hole point is outside the face", /^Отверстие #2: /],
      ['OperationFailed', 'thin wall: BRepOffsetAPI_MakeThickSolid failed', /^Тонкая стенка не построена/],
      ['message', "face 'Extrude:F3' was split into 2 parts", /^Элемент типа «грань» «Extrude:F3» разделён на 2 частей$/],
      // Messages built by local helpers, files, drawing dimensions, imports.
      ['ParseError', "missing field 'sheets'", /^В файле нет обязательного поля «sheets»$/],
      ['ParseError', "'scale' must be an integer in [1, 100]", /^Поле «scale» должно быть целым числом от 1 до 100$/],
      ['InvalidInput', "tolerance class 'Hh7': mixed letter case", /^Поле допуска «Hh7»: буквы разного регистра$/],
      ['InvalidInput', "fit 'H7': expected hole/shaft, e.g. H7/s6", /^Посадка «H7»: ожидается запись «отверстие\/вал»/],
      ['OperationFailed', "face #3 'Extrude:F1' does not exist in the model (2 faces)", /^Элемента «грань #3 'Extrude:F1'» нет в модели: всего элементов типа «грань» — 2$/],
      ['InvalidInput', 'dimension is dangling: edge #4 is not shown in the view', /^Размер потерял привязку: элемент «ребро #4» не показан на виде$/],
      ['InvalidInput', 'technical requirements, item 2: GOST 2.316: column width must be 0..185 mm', /^Технические требования, пункт 2: ГОСТ 2\.316: ширина столбца/],
      ['ParseError', "DXF line 12: group 10: expected a number, got 'x'", /^DXF, строка 12: группа 10: ожидалось число, получено «x»$/],
      ['ParseError', 'zip: CRC-32 mismatch in 3D/3dmodel.model', /^Архив ZIP: контрольная сумма CRC-32 записи «3D\/3dmodel\.model» не совпадает$/],
      ['InvalidInput', "configuration 'A': unknown material 'M9'", /^[Ии]сполнение «A»: неизвестный материал «M9»$/],
      ['InvalidInput', 'gear.hand must be right or left', /^Направление зубьев должно быть правым или левым$/],
    ];
    for (const [code, original, expected] of cases) {
      const m = localizeKernelMessage(tRu, code, original);
      expect([original, m.text]).toEqual([original, expect.stringMatching(expected)]);
      expect(m.original).toBe(original);
      expect(m.code).toBe(code);
    }
  });

  test('a broken reference: the nested resolution detail is localized too', () => {
    const m = localizeKernelMessage(tRu, 'message', "broken reference edges[0] 'Extrude:E3': edge 'Extrude:E3' was removed by feature 'Fillet1'");
    expect(m.template).toBe('brokenReference');
    expect(m.text).toBe("Ссылка edges[0] 'Extrude:E3' потеряна: элемент типа «ребро» «Extrude:E3» удалён операцией «Fillet1»");
    // An unknown detail: the reference template without it.
    const kind = localizeKernelMessage(tRu, 'message', "broken reference face[0] 'X': reference is a edge, slot expects a face");
    expect(kind.text).toBe("Ссылка face[0] 'X' потеряна: ссылка указывает на элемент типа «ребро», а нужен элемент типа «грань»");
    const raw = localizeKernelMessage(tRu, 'message', "broken reference face[0] 'X': some detail of a future kernel");
    expect(raw.template).toBe('brokenReferenceRaw');
    expect(raw.text).toBe("Ссылка face[0] 'X' потеряна");
  });

  test('unknown text: the title of the code, the original kept; no word-by-word translation', () => {
    const m = localizeKernelError(tRu, { code: 'OperationFailed', detail: 'BRepPrimAPI_MakeRevol failed somewhere new' });
    expect(m.text).toBeNull();
    expect(m.title).toBe('Операция не выполнена');
    expect(m.original).toBe('BRepPrimAPI_MakeRevol failed somewhere new');
    expect(localizeKernelError(tRu, { code: 'Fault', detail: 'SIGSEGV in OCCT' }).title).toBe('Сбой в ядре');
    expect(localizeKernelError(tRu, { code: 'Strange', detail: 'x' }).title).toBe('Ошибка ядра');
    expect(localizeKernelError(tRu, new Error('boom'))).toMatchObject({ code: 'other', text: null, original: 'boom' });
    // A term outside the dictionary does not match the template.
    expect(matchKernelText('teleport needs an existing body', tRu)).toBeNull();
  });

  test('in English the template text is the kernel wording', () => {
    for (const original of ['the bodies do not intersect', "sketch 'S' has no closed profile", 'fillet needs an existing body']) {
      expect(localizeKernelMessage(tEn, 'InvalidInput', original).text).toBe(original);
    }
    // A model element of a drawing reference keeps its English kind in English.
    const ref = localizeKernelMessage(tEn, 'OperationFailed', "edge #4 'E1' is not shown in the view");
    expect(ref.text).toBe("edge #4 'E1' is not shown in the view");
    // A template that leaves out a value (the OCCT reason) has its own English text; the original is in the details.
    const f = localizeKernelMessage(tEn, 'OperationFailed', 'fillet radius 2.500000 failed: x');
    expect(f.text).toBe('fillet radius 2.5 failed: the radius is probably too large for the edges or their faces');
    expect(f.original).toBe('fillet radius 2.500000 failed: x');
  });
});
