/**
 * Main-thread side of the WASM worker (docs/ui/APP_SHELL.md).
 *
 *   const client = await WasmClient.start<ModelingKernel>(createWasmWorker(), 'modeling', baseUrl);
 *   const doc = await client.root.createDocument();      // Remote<PartDocumentHandle>
 *   await doc.addFeature(spec); await doc.rebuild();
 *   const mesh = await doc.tessellate('final');          // typed arrays moved, not copied
 *
 * Every method of the remote object returns a Promise; a kernel failure rejects it with the same
 * KernelError (code, detail) the synchronous API throws. cancel() terminates the worker: pending
 * calls reject with KernelError('Cancelled'), the module state of the worker (all documents) is
 * lost, and the client is unusable — the owner starts a new client and reloads its document
 * from a saved snapshot (DocumentSnapshots in the editors).
 */

import { KernelError } from '../../wasm/bendcore_kernel_api.mjs';
import type { KernelErrorCode } from '../../wasm/bendcore_kernel_api.mjs';
import { CONVERTED_SIGNAL, LONG_OPERATIONS, ModuleKind, Request, Response, isRemoteRef } from './protocol';

/** The part of Worker the client uses (a real Worker, or an in-process one in tests). */
export interface WorkerLike {
  postMessage(message: Request): void;
  onmessage: ((e: { data: Response }) => void) | null;
  onerror: ((e: { message?: string }) => void) | null;
  terminate(): void;
}

type AnyFn = (...args: never[]) => unknown;

/** Result of a remote call: class instances (they have dispose) become remote objects. */
export type RemoteResult<R> = R extends { dispose(): void }
  ? Remote<R>
  : R extends { document: infer D; report: infer P }
    ? { document: RemoteResult<D>; report: P }
    : R extends { drawing: infer D; report: infer P }
      ? { drawing: RemoteResult<D>; report: P }
      : R;

/** Asynchronous view of an object in the worker: its methods, each returning a Promise. */
export type Remote<T> = {
  [K in keyof T as T[K] extends AnyFn ? K : never]: T[K] extends (...a: infer A) => infer R
    ? (...a: A) => Promise<RemoteResult<R>>
    : never;
} & { readonly remoteId: number; readonly remoteClass: string };

export interface PendingCall {
  id: number;
  method: string;
  /** Group of a long operation (LONG_OPERATIONS), or null for a short call. */
  operation: string | null;
  startedAt: number;
}

export type ActivityListener = (pending: PendingCall[]) => void;
export type FaultListener = (error: KernelError, module: ModuleKind) => void;

interface Waiting extends PendingCall {
  resolve(v: unknown): void;
  reject(e: unknown): void;
}

export class WasmClient<Root> {
  /** Clients whose worker is running (the shell's idle() waits for all of them). */
  private static readonly running = new Set<WasmClient<unknown>>();

  static live(): WasmClient<unknown>[] {
    return [...WasmClient.running];
  }

  private readonly pending = new Map<number, Waiting>();
  private readonly listeners = new Set<ActivityListener>();
  private nextId = 1;
  private closed: KernelError | null = null;
  readonly root: Remote<Root>;

  private constructor(
    private readonly worker: WorkerLike,
    readonly module: ModuleKind,
    readonly info: unknown,
  ) {
    this.root = this.proxy(0, module) as Remote<Root>;
    WasmClient.running.add(this as WasmClient<unknown>);
    worker.onmessage = (e) => this.receive(e.data);
    worker.onerror = (e) => this.close(new KernelError('WorkerFailed', e.message ?? 'worker error'));
  }

  /** Sends the init message and waits for the module to load in the worker. */
  static start<Root>(worker: WorkerLike, module: ModuleKind, baseUrl: string): Promise<WasmClient<Root>> {
    return new Promise((resolve, reject) => {
      worker.onmessage = (e) => {
        const m = e.data;
        if (m.type === 'ready') resolve(new WasmClient<Root>(worker, module, m.info));
        else if (m.type === 'init-failed') {
          worker.terminate();
          reject(new KernelError(m.error.code as KernelErrorCode, m.error.detail));
        }
      };
      worker.onerror = (e) => {
        worker.terminate();
        reject(new KernelError('WorkerFailed', e.message ?? `worker of ${module} failed to start`));
      };
      worker.postMessage({ type: 'init', module, baseUrl });
    });
  }

  get terminated(): boolean {
    return this.closed !== null;
  }

  pendingCalls(): PendingCall[] {
    return [...this.pending.values()].map(({ id, method, operation, startedAt }) => ({ id, method, operation, startedAt }));
  }

  onActivity(listener: ActivityListener): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  private emit() {
    const list = this.pendingCalls();
    for (const l of this.listeners) l(list);
  }

  /** Low-level call of `method` on worker object `target`. */
  call(target: number, method: string, args: unknown[]): Promise<unknown> {
    if (this.closed) return Promise.reject(this.closed);
    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      this.pending.set(id, {
        id,
        method,
        operation: LONG_OPERATIONS[method] ?? null,
        startedAt: Date.now(),
        resolve,
        reject,
      });
      this.worker.postMessage({ type: 'call', id, target, method, args });
      this.emit();
    });
  }

  private receive(m: Response) {
    if (m.type !== 'result' && m.type !== 'error') return;
    const w = this.pending.get(m.id);
    if (!w) return;
    this.pending.delete(m.id);
    if (m.type === 'result') w.resolve(this.decode(m.value));
    else {
      const error = new KernelError(m.error.code as KernelErrorCode, m.error.detail);
      w.reject(error);
      // A trap in the module: its state is unknown, the client closes (every pending and later
      // call rejects with this error) and the owner is told through onFault().
      if (error.code === 'ModuleFault') this.close(error);
    }
    this.emit();
  }

  private decode(v: unknown): unknown {
    if (v === null || typeof v !== 'object' || ArrayBuffer.isView(v)) return v;
    if (isRemoteRef(v)) return this.proxy(v.__bcRemote, v.className);
    if (Array.isArray(v)) return v.map((x) => this.decode(x));
    const out: Record<string, unknown> = {};
    for (const [k, x] of Object.entries(v)) out[k] = this.decode(x);
    return out;
  }

  private proxy(id: number, className: string): unknown {
    const cache = new Map<string, unknown>();
    return new Proxy(
      {},
      {
        get: (_t, prop) => {
          if (prop === 'remoteId') return id;
          if (prop === 'remoteClass') return className;
          // Not a thenable: awaiting a remote object must not call the worker.
          if (typeof prop !== 'string' || prop === 'then') return undefined;
          let fn = cache.get(prop);
          if (!fn) {
            fn = (...args: unknown[]) => this.call(id, prop, args);
            cache.set(prop, fn);
          }
          return fn;
        },
      },
    );
  }

  private readonly faultListeners = new Set<FaultListener>();

  /** The failure that closed the client when it was a module fault (isModuleFault), else null. */
  get fault(): KernelError | null {
    return this.closed && isModuleFault(this.closed) ? this.closed : null;
  }

  /**
   * Called once when the module fails: a trap in a call (ModuleFault) or the worker died
   * (WorkerFailed). Not called for cancel() and dispose(). Returns the unsubscribe function.
   */
  onFault(listener: FaultListener): () => void {
    this.faultListeners.add(listener);
    return () => this.faultListeners.delete(listener);
  }

  private close(error: KernelError) {
    if (this.closed) return;
    this.closed = error;
    if (isModuleFault(error)) for (const l of this.faultListeners) l(error, this.module);
    WasmClient.running.delete(this as WasmClient<unknown>);
    this.worker.terminate();
    const list = [...this.pending.values()];
    this.pending.clear();
    for (const w of list) w.reject(error);
    this.emit();
  }

  /**
   * Stops the worker at once (also in the middle of a kernel call). Pending calls reject with
   * KernelError('Cancelled'); every object of this client is gone.
   */
  cancel(reason = 'the operation was cancelled'): void {
    this.close(new KernelError('Cancelled', reason));
  }

  /** Ends the worker without an error for pending calls' owners beyond 'Disposed'. */
  dispose(): void {
    this.close(new KernelError('Disposed', 'the module worker was closed'));
  }
}

/**
 * True when `e` says the module itself failed, so its state can no longer be trusted
 * (docs/ui/APP_SHELL.md, "Сбой модуля"): ModuleFault (a WebAssembly trap in a call, protocol.ts
 * isWasmTrap), WorkerFailed (the worker died), or a native kernel error made from a hardware
 * fault by occt_fault_guard (ParseError whose text names SIGSEGV / SIGBUS / SIGFPE inside OCCT).
 */
export function isModuleFault(e: unknown): boolean {
  if (!(e instanceof KernelError)) return false;
  return e.code === 'ModuleFault' || e.code === 'WorkerFailed' || CONVERTED_SIGNAL.test(e.detail);
}

export function isCancelled(e: unknown): boolean {
  return e instanceof KernelError && e.code === 'Cancelled';
}
