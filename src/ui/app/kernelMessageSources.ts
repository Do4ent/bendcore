/**
 * Reads the error messages the C++ kernel generates from its sources (docs/ui/APP_SHELL.md,
 * "Сообщения ядра"). Used by the coverage test of the message templates (kernelMessages.ts):
 * every message of src/modeling, src/assembly_v2 and src/drawing_v2 either matches a template
 * or is listed as "other" (localized title by code, original text in the details).
 *
 * A message is the expression after `ErrorCode::<Code>,` — `Result::failure(ErrorCode::X, "radius "
 * + r + " too large")`, `Error{ErrorCode::X, ...}`, `fail(ErrorCode::X, ...)` — or the argument of
 * `markError(...)` / `messages.push_back(...)` / `warnings.push_back(...)` (messages of a feature
 * node) or the value assigned to a `.detail` (resolution details of references), code "message". Its skeleton is the string literals with `{}` where a value (a number, a
 * name, a nested message) is concatenated; `cond ? "a" : "b"` gives one skeleton per branch;
 * helpers that wrap a message are expanded (HELPERS). Expressions without a literal pass another
 * message through (`error`, `e.what()`) and are not messages of their own.
 */

export interface KernelMessageSource {
  /** ErrorCode name, "message" for a feature node message, "forwarded" for a code passed on */
  code: string;
  /** literal text with {} for each concatenated value */
  skeleton: string;
  /** file:line of the first occurrence */
  where: string;
}

/** Source without comments; string and character literals kept, line numbers preserved. */
export function stripComments(src: string): string {
  let out = '';
  let i = 0;
  while (i < src.length) {
    const c = src[i];
    if (c === '"' || (c === "'" && !/[0-9A-Fa-f]/.test(src[i - 1] ?? ''))) {
      let j = i + 1;
      while (j < src.length && src[j] !== c && src[j] !== '\n') j += src[j] === '\\' ? 2 : 1;
      out += src.slice(i, j + 1);
      i = j + 1;
    } else if (src.startsWith('//', i)) {
      while (i < src.length && src[i] !== '\n') i++;
    } else if (src.startsWith('/*', i)) {
      const j = src.indexOf('*/', i + 2);
      const end = j < 0 ? src.length : j + 2;
      out += src.slice(i, end).replace(/[^\n]/g, ' ');
      i = end;
    } else {
      out += c;
      i++;
    }
  }
  return out;
}

function unescape(lit: string): string {
  return lit.replace(/\\(.)/g, (_, ch: string) => (ch === 'n' ? '\n' : ch === 't' ? '\t' : ch));
}

/** Index after the literal starting at i (a quote). */
function skipLiteral(s: string, i: number): number {
  const q = s[i];
  let j = i + 1;
  while (j < s.length && s[j] !== q) j += s[j] === '\\' ? 2 : 1;
  return j + 1;
}

const OPEN = '([{';
const CLOSE = ')]}';

/** Text of the expression starting at `from`, up to the `,` / `)` / `}` / `;` that ends it. */
function expressionAt(src: string, from: number): string {
  let depth = 0;
  let i = from;
  while (i < src.length) {
    const c = src[i];
    if (c === '"') {
      i = skipLiteral(src, i);
      continue;
    }
    if (OPEN.includes(c)) depth++;
    else if (CLOSE.includes(c)) {
      if (depth === 0) break;
      depth--;
    } else if ((c === ',' || c === ';') && depth === 0) break;
    i++;
  }
  return src.slice(from, i);
}

/** Splits at top-level occurrences of `sep` (outside literals and brackets). */
function splitTop(expr: string, sep: (s: string, i: number) => number): string[] {
  const parts: string[] = [];
  let depth = 0;
  let start = 0;
  let i = 0;
  while (i < expr.length) {
    const c = expr[i];
    if (c === '"') {
      i = skipLiteral(expr, i);
      continue;
    }
    if (c === "'" && /^'(\\.|[^\\'])'/.test(expr.slice(i))) {
      i += expr[i + 1] === '\\' ? 4 : 3;
      continue;
    }
    if (OPEN.includes(c)) depth++;
    else if (CLOSE.includes(c)) depth--;
    else if (depth === 0) {
      const n = sep(expr, i);
      if (n > 0) {
        parts.push(expr.slice(start, i));
        i += n;
        start = i;
        continue;
      }
    }
    i++;
  }
  parts.push(expr.slice(start));
  return parts;
}

/** Top-level `cond ? a : b`: [a, b], or null. */
function ternary(expr: string): [string, string] | null {
  const q = splitTop(expr, (s, i) => (s[i] === '?' ? 1 : 0));
  if (q.length < 2) return null;
  const rest = q.slice(1).join('?');
  const branches = splitTop(rest, (s, i) => (s[i] === ':' && s[i + 1] !== ':' && s[i - 1] !== ':' ? 1 : 0));
  if (branches.length < 2) return null;
  return [branches[0], branches.slice(1).join(':')];
}

/** `name(args)` covering the whole expression: [name, args], or null. */
function call(expr: string): [string, string[]] | null {
  const m = /^([A-Za-z_][\w:]*)\s*\(/.exec(expr);
  if (!m) return null;
  let depth = 0;
  for (let i = m[0].length - 1; i < expr.length; i++) {
    const c = expr[i];
    if (c === '"') {
      i = skipLiteral(expr, i) - 1;
      continue;
    }
    if (OPEN.includes(c)) depth++;
    else if (CLOSE.includes(c)) {
      depth--;
      if (depth === 0) {
        if (expr.slice(i + 1).trim() !== '') return null;
        const inner = expr.slice(m[0].length, i);
        return [m[1], splitTop(inner, (s, k) => (s[k] === ',' ? 1 : 0))];
      }
    }
  }
  return null;
}

const join = (parts: string[]): string => parts.join('').replace(/(\{\})+/g, '{}');

/**
 * Helpers of the kernel that build a message from their arguments (by their definitions):
 * occtMessage (src/modeling/features/feature_common.cpp), failureText of dress_up.cpp (the head,
 * optionally followed by "; failed edges: ..."), std::string(...) conversions.
 */
const HELPERS: Record<string, (args: string[][]) => string[]> = {
  occtMessage: (a) => cross([['OCCT exception in '], a[0] ?? ['{}'], [': {}']]),
  failureText: (a) => a[0] ?? ['{}'],
  'std::string': (a) => a[0] ?? ['{}'],
  'std::string_view': (a) => a[0] ?? ['{}'],
};

function cross(parts: string[][]): string[] {
  let out = [''];
  for (const alts of parts) out = out.flatMap((o) => alts.map((a) => o + a));
  return [...new Set(out.map((s) => join([s])))];
}

/** Skeletons of an expression; [] when it has no literal. */
export function skeletonsOf(expression: string): string[] {
  const out = alternatives(expression.trim());
  return out.filter((s) => s.replace(/\{\}/g, '').trim().length > 0);
}

function alternatives(expr: string): string[] {
  if (expr === '') return ['{}'];
  const t = ternary(expr);
  if (t) return [...new Set([...alternatives(t[0].trim()), ...alternatives(t[1].trim())])];
  const terms = splitTop(expr, (s, i) => (s[i] === '+' && s[i + 1] !== '+' && s[i - 1] !== '+' && s[i + 1] !== '=' ? 1 : 0));
  if (terms.length > 1) return cross(terms.map((x) => alternatives(x.trim())));
  if (/^"/.test(expr)) {
    // Adjacent literals "a" "b" (and a trailing suffix such as a user-defined literal).
    const lits = [...expr.matchAll(/"((?:[^"\\]|\\.)*)"/g)].map((m) => unescape(m[1]));
    return [lits.join('')];
  }
  if (expr.startsWith('(') && expr.endsWith(')')) {
    const inner = call(`x${expr}`);
    if (inner && inner[1].length === 1) return alternatives(inner[1][0].trim());
  }
  const c = call(expr);
  if (c) {
    const helper = Object.prototype.hasOwnProperty.call(HELPERS, c[0]) ? HELPERS[c[0]] : undefined;
    if (helper) return helper(c[1].map((a) => alternatives(a.trim())));
  }
  return ['{}'];
}

const MESSAGE_CALLS = /(?:markError|messages\s*\.\s*(?:push_back|emplace_back)|warnings\s*\.\s*(?:push_back|emplace_back))\s*\(|\.detail\s*=(?!=)/g;

/** Messages of one C++ source file. */
export function messagesOfSource(src: string, file: string): KernelMessageSource[] {
  const text = stripComments(src);
  const out: KernelMessageSource[] = [];
  // `const std::string prefix = "configuration '" + name + "': ";` — a local text variable with a literal
  // is substituted where it is a whole term of a message (`prefix + "unknown material"`).
  const locals = [...text.matchAll(/\bstd::string\s+(\w+)\s*=\s*([^;]*"[^;]*);/g)].map((d) => ({ name: d[1], value: d[2], at: d.index ?? 0 }));
  const withLocals = (expr: string, index: number): string => {
    let e = expr;
    for (const name of new Set(locals.map((l) => l.name))) {
      const def = locals.filter((l) => l.name === name && l.at < index && index - l.at < 2500).pop();
      if (!def) continue;
      e = splitTop(e, (str, i) => (str[i] === '+' ? 1 : 0))
        .map((term) => (term.trim() === name ? `(${def.value})` : term))
        .join('+');
    }
    return e;
  };
  const add = (code: string, index: number, expr: string) => {
    const line = text.slice(0, index).split('\n').length;
    for (const skeleton of skeletonsOf(withLocals(expr, index))) out.push({ code, skeleton, where: `${file}:${line}` });
  };
  const re = /ErrorCode::(\w+)\s*,/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(text)) !== null) add(m[1], m.index, expressionAt(text, re.lastIndex));
  // A code passed on from another result with a new text: fail(r.error().code, "sweep path: " + ...).
  const forwarded = /\.error\(\)\.code\s*,/g;
  while ((m = forwarded.exec(text)) !== null) add('forwarded', m.index, expressionAt(text, forwarded.lastIndex));
  // WASM bindings: failure("Code", text) and InputError(text) (reported as InvalidInput).
  const named = /\bfailure\(\s*"(\w+)"\s*,/g;
  while ((m = named.exec(text)) !== null) add(m[1], m.index, expressionAt(text, named.lastIndex));
  const input = /\bInputError\(/g;
  while ((m = input.exec(text)) !== null) add('InvalidInput', m.index, expressionAt(text, input.lastIndex));
  // Local helpers that build the error from a text: `Error invalid(std::string m) { return
  // Error{ErrorCode::InvalidInput, ...}; }`, `[[noreturn]] void bad(const std::string& m) { throw
  // FormatError(m); }`, `Error parseError(size_t line, const std::string& what) { return Error{...,
  // "DXF line " + to_string(line) + ": " + what}; }` — every call `invalid("...")` is a message of the
  // helper's code, with the call's argument in place of the helper's text parameter.
  for (const [name, h] of helpersOf(text)) {
    const call = new RegExp(`(?<![\\w.>:])${name}\\(`, 'g');
    while ((m = call.exec(text)) !== null) {
      if (/(?:Error|void)\s+$/.test(text.slice(Math.max(0, m.index - 12), m.index))) continue; // the definition
      const args = splitTop(argumentsAt(text, call.lastIndex), (str, i) => (str[i] === ',' ? 1 : 0));
      // bad(ErrorCode::Fault, "text"): the text is the last argument.
      const arg = h.index >= 0 && args.length > h.index ? args[h.index] : args[args.length - 1];
      if (h.body === null) add(h.code, m.index, arg);
      else add(h.code, m.index, h.body.replace(new RegExp(`(?<![\\w"])${h.param}(?![\\w"])`, 'g'), `(${arg})`));
    }
  }
  MESSAGE_CALLS.lastIndex = 0;
  while ((m = MESSAGE_CALLS.exec(text)) !== null) add('message', m.index, expressionAt(text, MESSAGE_CALLS.lastIndex));
  return out;
}

/** Text between the parenthesis that opens at `from` (just after it) and its closing one. */
function argumentsAt(text: string, from: number): string {
  let depth = 0;
  let i = from;
  while (i < text.length) {
    const c = text[i];
    if (c === '"') {
      i = skipLiteral(text, i);
      continue;
    }
    if (OPEN.includes(c)) depth++;
    else if (CLOSE.includes(c)) {
      if (depth === 0) break;
      depth--;
    }
    i++;
  }
  return text.slice(from, i);
}

interface Helper {
  code: string;
  /** index of the text parameter among the arguments (-1: the last argument) */
  index: number;
  /** expression of the message in the helper body, with the parameter name; null: the parameter itself */
  body: string | null;
  param: string;
}

/** Error-building helpers defined in a file: name -> code and message expression (a thrown parse failure is ParseError). */
function helpersOf(text: string): Map<string, Helper> {
  const out = new Map<string, Helper>();
  const make = /\bError\s+(\w+)\s*\(([^()]*)\)\s*\{\s*return\s+Error\s*\{\s*ErrorCode::(\w+)\s*,\s*([^;]*?)\s*\}\s*;\s*\}/g;
  let m: RegExpExecArray | null;
  while ((m = make.exec(text)) !== null) {
    const params = m[2].split(',').map((p) => p.trim()).filter((p) => p !== '');
    const index = params.findIndex((p) => /std::string/.test(p));
    if (index < 0) continue;
    const param = /(\w+)\s*$/.exec(params[index])?.[1] ?? '';
    const moved = new RegExp(`^std::move\\(${param}\\)$|^${param}$`).test(m[4]);
    out.set(m[1], { code: m[3], index, body: moved ? null : m[4], param });
  }
  const thrown = /\bvoid\s+(\w+)\s*\(\s*const\s+std::string&\s*\w*\s*\)\s*\{\s*throw\s+(?:FormatError|ParseFailure)\b/g;
  while ((m = thrown.exec(text)) !== null) out.set(m[1], { code: 'ParseError', index: -1, body: null, param: '' });
  return out;
}
