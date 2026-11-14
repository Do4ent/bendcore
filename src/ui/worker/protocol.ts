/**
 * Message protocol between the main thread and the WASM worker (docs/ui/APP_SHELL.md).
 *
 * The worker hosts one module API object (Kernel, ModelingKernel, AssemblyKernel, DrawingKernel)
 * as object 0 and every class instance a call returns (documents, shapes) under a new object id.
 * The main thread calls methods by (object id, method name, arguments); plain results are
 * structured-cloned, the buffers of typed arrays are transferred, class instances come back as
 * references, kernel failures come back as { code, detail } and are rethrown as KernelError.
 */

export type ModuleKind = 'kernel' | 'modeling' | 'assembly' | 'drawing';

export interface InitRequest {
  type: 'init';
  module: ModuleKind;
  /** Directory URL (trailing slash) serving the Emscripten glue, the .wasm and the font. */
  baseUrl: string;
}

export interface CallRequest {
  type: 'call';
  id: number;
  target: number;
  method: string;
  args: unknown[];
}

export type Request = InitRequest | CallRequest;

export interface SerializedError {
  name: string;
  /** KernelError code; "Error" for a JavaScript error without a kernel code. */
  code: string;
  detail: string;
}

export interface ReadyResponse {
  type: 'ready';
  info: unknown;
}

export interface InitFailedResponse {
  type: 'init-failed';
  error: SerializedError;
}

export interface ResultResponse {
  type: 'result';
  id: number;
  value: unknown;
}

export interface ErrorResponse {
  type: 'error';
  id: number;
  error: SerializedError;
}

export type Response = ReadyResponse | InitFailedResponse | ResultResponse | ErrorResponse;

/** Reference to an object kept in the worker. */
export interface RemoteRef {
  __bcRemote: number;
  className: string;
}

export function isRemoteRef(v: unknown): v is RemoteRef {
  return typeof v === 'object' && v !== null && typeof (v as RemoteRef).__bcRemote === 'number';
}

/**
 * True for a WebAssembly trap or an Emscripten abort: WebAssembly.RuntimeError ("memory access
 * out of bounds", "null function or function signature mismatch", "integer divide by zero",
 * "unreachable", "Aborted(…)"). WASM has no signals, so this is where a native SIGSEGV / SIGBUS /
 * SIGFPE of OCCT ends up (docs/kernel/KERNEL_CORE.md, occt_fault_guard); the module instance is
 * not usable afterwards.
 */
export function isWasmTrap(e: unknown): boolean {
  const runtimeError = (globalThis as { WebAssembly?: { RuntimeError?: new () => Error } }).WebAssembly?.RuntimeError;
  if (runtimeError && e instanceof runtimeError) return true;
  // Errors of another realm (a test sandbox) are not instances of this realm's classes.
  return typeof e === 'object' && e !== null && (e as { name?: unknown }).name === 'RuntimeError' && typeof (e as { message?: unknown }).message === 'string';
}

/**
 * Error codes that mean the module itself failed and its state can no longer be trusted
 * (docs/ui/APP_SHELL.md, "Сбой модуля"): ModuleFault — a WebAssembly trap in a call (isWasmTrap);
 * Fault — kernel ErrorCode::Fault, a native hardware fault (SIGSEGV / SIGBUS / SIGFPE inside OCCT)
 * caught by occt_fault_guard (docs/kernel/KERNEL_CORE.md). The worker host refuses every later
 * call after either and the client closes.
 */
export const MODULE_FAULT_CODES: readonly string[] = ['ModuleFault', 'Fault'];

export function serializeError(e: unknown): SerializedError {
  if (isWasmTrap(e)) {
    return { name: 'KernelError', code: 'ModuleFault', detail: `WebAssembly trap: ${(e as Error).message}` };
  }
  if (e && typeof e === 'object' && 'code' in e && 'detail' in e) {
    const ke = e as { name?: string; code: unknown; detail: unknown };
    return { name: String(ke.name ?? 'KernelError'), code: String(ke.code), detail: String(ke.detail) };
  }
  if (e instanceof Error) return { name: e.name, code: 'Error', detail: e.message };
  return { name: 'Error', code: 'Error', detail: String(e) };
}

function isPlain(v: object): boolean {
  const proto = Object.getPrototypeOf(v);
  return proto === Object.prototype || proto === null;
}

/**
 * Prepares a result for postMessage: class instances are registered (register returns the id)
 * and replaced by RemoteRef; typed arrays that own their whole buffer are listed for transfer,
 * views into a larger buffer (e.g. the WASM heap) are copied first so the heap is never detached.
 */
export function encodeResult(
  value: unknown,
  register: (obj: object) => number,
): { value: unknown; transfer: ArrayBuffer[] } {
  const transfer: ArrayBuffer[] = [];
  const seen = new Set<ArrayBuffer>();
  const walk = (v: unknown): unknown => {
    if (v === null || typeof v !== 'object') return v;
    if (ArrayBuffer.isView(v)) {
      let view = v as ArrayBufferView & { slice(): ArrayBufferView };
      const buffer = view.buffer as ArrayBuffer;
      const whole = view.byteOffset === 0 && view.byteLength === buffer.byteLength;
      if (!whole || seen.has(buffer) || !(buffer instanceof ArrayBuffer)) view = view.slice() as typeof view;
      const own = view.buffer as ArrayBuffer;
      if (!seen.has(own)) {
        seen.add(own);
        transfer.push(own);
      }
      return view;
    }
    if (v instanceof ArrayBuffer) return v.slice(0);
    if (Array.isArray(v)) return v.map(walk);
    if (!isPlain(v)) {
      return { __bcRemote: register(v), className: (v as { constructor?: { name?: string } }).constructor?.name ?? 'Object' };
    }
    const out: Record<string, unknown> = {};
    for (const [k, x] of Object.entries(v)) out[k] = walk(x);
    return out;
  };
  return { value: walk(value), transfer };
}

/** Methods whose duration is shown by the progress indicator (docs/ui/APP_SHELL.md). */
export const LONG_OPERATIONS: Record<string, string> = {
  rebuild: 'rebuild',
  loadDocument: 'load',
  readStep: 'load',
  importStep: 'load',
  importIges: 'load',
  importModel: 'load',
  solve: 'solve',
  dragEnd: 'solve',
  insertPart: 'solve',
  interference: 'interference',
  exportStep: 'export',
  exportFile: 'export',
  addView: 'hlr',
  setView: 'hlr',
  updateFromModel: 'hlr',
  createFromPart: 'hlr',
  createFromAssembly: 'hlr',
  load: 'hlr',
  sheetPicture: 'hlr',
  exportPdf: 'export',
  exportSheet: 'export',
};
