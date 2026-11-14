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
  const add = (code: string, index: number, expr: string) => {
    const line = text.slice(0, index).split('\n').length;
    for (const skeleton of skeletonsOf(expr)) out.push({ code, skeleton, where: `${file}:${line}` });
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
  MESSAGE_CALLS.lastIndex = 0;
  while ((m = MESSAGE_CALLS.exec(text)) !== null) add('message', m.index, expressionAt(text, MESSAGE_CALLS.lastIndex));
  return out;
}
