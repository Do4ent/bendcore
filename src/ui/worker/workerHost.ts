/**
 * Worker side of the WASM protocol (protocol.ts): keeps the module API object and the objects
 * its calls return, executes calls in arrival order and posts the results.
 */

import { CallRequest, Response, encodeResult, serializeError } from './protocol';

export type Post = (message: Response, transfer: ArrayBuffer[]) => void;

export class WorkerHost {
  private readonly objects = new Map<number, object>();
  private nextId = 1;

  constructor(root: object, private readonly post: Post) {
    this.objects.set(0, root);
  }

  /** Number of live objects besides the root (documents, shapes). */
  get liveObjects(): number {
    return this.objects.size - 1;
  }

  private register = (obj: object): number => {
    const id = this.nextId++;
    this.objects.set(id, obj);
    return id;
  };

  handle(req: CallRequest): void {
    const target = this.objects.get(req.target);
    try {
      if (!target) throw Object.assign(new Error(`object ${req.target} is not in the worker (disposed)`), { code: 'Disposed', detail: `object ${req.target} is not in the worker (disposed)` });
      const fn = (target as Record<string, unknown>)[req.method];
      if (typeof fn !== 'function' || req.method === 'constructor' || req.method.startsWith('_')) {
        throw Object.assign(new Error(`no method ${req.method}`), { code: 'InvalidInput', detail: `no method ${req.method}` });
      }
      const raw = (fn as (...a: unknown[]) => unknown).apply(target, req.args);
      if (req.method === 'dispose' && req.target !== 0) this.objects.delete(req.target);
      const { value, transfer } = encodeResult(raw, this.register);
      this.post({ type: 'result', id: req.id, value }, transfer);
    } catch (e) {
      this.post({ type: 'error', id: req.id, error: serializeError(e) }, []);
    }
  }
}
