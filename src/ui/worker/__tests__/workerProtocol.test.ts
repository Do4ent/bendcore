/**
 * @jest-environment node
 */
// Worker protocol (docs/ui/APP_SHELL.md): WasmClient on the main side and WorkerHost on the
// worker side, connected by a real MessageChannel of node (structured clone and transfer lists
// as in a browser worker). The hosted root is a small API with the shapes the module APIs have:
// methods returning plain data, typed arrays (own buffer or a view into a larger "heap"),
// class instances (documents) and KernelError failures.

import { MessageChannel } from 'worker_threads';
import { KernelError } from '../../../wasm/bendcore_kernel_api.mjs';
import { WorkerHost } from '../workerHost';
import { WasmClient, isCancelled } from '../wasmClient';
import type { WorkerLike } from '../wasmClient';
import { encodeResult, serializeError } from '../protocol';
import type { Request, Response } from '../protocol';

const heap = new ArrayBuffer(64);

class Doc {
  private disposed = false;
  constructor(readonly size: number) {}
  dispose() {
    this.disposed = true;
  }
  area(): number {
    if (this.disposed) throw new KernelError('Disposed', 'area() called on a disposed document');
    return this.size * this.size;
  }
  mesh() {
    const positions = new Float32Array([0, 0, 0, this.size, 0, 0, 0, this.size, 0]);
    // A view into a larger buffer, like a typed_memory_view of the WASM heap.
    const spans = new Uint32Array(heap, 8, 3);
    spans.set([7, 0, 3]);
    return { positions, spans, edges: [{ edgeIndex: 0, points: new Float32Array([1, 2, 3]) }] };
  }
}

class Api {
  info() {
    return { version: 'test' };
  }
  createDocument(size: number) {
    if (!(size > 0)) throw new KernelError('InvalidInput', `size must be positive, got ${size}`);
    return new Doc(size);
  }
  loadDocument(size: number) {
    return { document: new Doc(size), report: { migrated: false } };
  }
  /** Never answers in time: the host is told to hold this call (cancel test). */
  slow() {
    return 'late';
  }
}

interface Rig {
  client: WasmClient<Api>;
  posted: Response[];
  hold: Set<string>;
  /** Answers the held calls once `count` of them reached the worker side. */
  release(count: number): Promise<void>;
  close(): void;
}

async function rig(): Promise<Rig> {
  const channel = new MessageChannel();
  const posted: Response[] = [];
  const hold = new Set<string>();
  const held: Request[] = [];
  let host: WorkerHost | null = null;
  channel.port2.on('message', (msg: Request) => {
    if (msg.type === 'init') {
      host = new WorkerHost(new Api(), (m, transfer) => {
        posted.push(m);
        channel.port2.postMessage(m, transfer);
      });
      channel.port2.postMessage({ type: 'ready', info: { version: 'test' } });
      return;
    }
    if (hold.has(msg.method)) held.push(msg);
    else host!.handle(msg);
  });
  let terminated = false;
  const worker: WorkerLike = {
    postMessage: (m) => {
      if (!terminated) channel.port1.postMessage(m);
    },
    onmessage: null,
    onerror: null,
    terminate: () => {
      terminated = true;
    },
  };
  channel.port1.on('message', (data: Response) => worker.onmessage?.({ data }));
  const client = await WasmClient.start<Api>(worker, 'modeling', 'http://test/');
  return {
    client,
    posted,
    hold,
    release: async (count: number) => {
      while (held.length < count) await new Promise((resolve) => setTimeout(resolve, 1));
      held.splice(0).forEach((m) => host!.handle(m));
    },
    close: () => {
      channel.port1.close();
      channel.port2.close();
    },
  };
}

describe('worker protocol', () => {
  let r: Rig;
  beforeEach(async () => {
    r = await rig();
  });
  afterEach(() => r.close());

  test('the client starts with the info of the module', () => {
    expect(r.client.info).toEqual({ version: 'test' });
    expect(r.client.module).toBe('modeling');
  });

  test('plain results and remote documents', async () => {
    expect(await r.client.root.info()).toEqual({ version: 'test' });
    const doc = await r.client.root.createDocument(3);
    expect(doc.remoteClass).toBe('Doc');
    expect(doc.remoteId).toBeGreaterThan(0);
    expect(await doc.area()).toBe(9);
    const loaded = await r.client.root.loadDocument(4);
    expect(loaded.report).toEqual({ migrated: false });
    expect(await loaded.document.area()).toBe(16);
    expect(loaded.document.remoteId).not.toBe(doc.remoteId);
    // Awaiting a remote object does not call the worker (it is not a thenable).
    const same = await Promise.resolve(doc);
    expect(same.remoteId).toBe(doc.remoteId);
  });

  test('typed arrays arrive intact; whole buffers are transferred, heap views copied', async () => {
    const doc = await r.client.root.createDocument(2);
    const mesh = await doc.mesh();
    // Typed arrays come from another realm (the channel): compare by tag, not by constructor.
    expect(Object.prototype.toString.call(mesh.positions)).toBe('[object Float32Array]');
    expect(Object.prototype.toString.call(mesh.spans)).toBe('[object Uint32Array]');
    expect(Array.from(mesh.positions)).toEqual([0, 0, 0, 2, 0, 0, 0, 2, 0]);
    expect(Array.from(mesh.spans)).toEqual([7, 0, 3]);
    expect(Array.from(mesh.edges[0].points)).toEqual([1, 2, 3]);
    // The "heap" stays usable in the worker: it was copied, not detached.
    expect(heap.byteLength).toBe(64);
    // The copy owns its buffer, of exactly its size.
    expect(mesh.spans.buffer.byteLength).toBe(12);
  });

  test('kernel failures are rethrown as KernelError with code and detail', async () => {
    await expect(r.client.root.createDocument(-1)).rejects.toMatchObject({
      name: 'KernelError',
      code: 'InvalidInput',
      detail: 'size must be positive, got -1',
    });
    const err = await r.client.root.createDocument(0).catch((e: unknown) => e);
    expect(err).toBeInstanceOf(KernelError);
  });

  test('dispose releases the worker object; later calls report Disposed', async () => {
    const doc = await r.client.root.createDocument(5);
    await doc.dispose();
    await expect(doc.area()).rejects.toMatchObject({ code: 'Disposed' });
  });

  test('unknown and private methods are refused', async () => {
    const call = r.client.call(0, 'nothing', []);
    await expect(call).rejects.toMatchObject({ code: 'InvalidInput', detail: 'no method nothing' });
    await expect(r.client.call(0, 'constructor', [])).rejects.toMatchObject({ code: 'InvalidInput' });
  });

  test('pending calls are reported to activity listeners with their operation group', async () => {
    const seen: string[][] = [];
    r.client.onActivity((list) => seen.push(list.map((p) => `${p.method}:${p.operation ?? '-'}`)));
    r.hold.add('slow');
    const slow = r.client.call(0, 'slow', []);
    const rebuild = r.client.call(0, 'rebuild', []).catch(() => 'refused');
    expect(r.client.pendingCalls().map((p) => p.method)).toEqual(['slow', 'rebuild']);
    expect(seen[1]).toEqual(['slow:-', 'rebuild:rebuild']);
    await r.release(1);
    expect(await slow).toBe('late');
    expect(await rebuild).toBe('refused');
    expect(r.client.pendingCalls()).toEqual([]);
  });

  test('cancel rejects pending calls with Cancelled and closes the client', async () => {
    r.hold.add('slow');
    const slow = r.client.call(0, 'slow', []);
    expect(WasmClient.live()).toContain(r.client);
    r.client.cancel('stop');
    const err = await slow.catch((e: unknown) => e);
    expect(isCancelled(err)).toBe(true);
    expect(err).toMatchObject({ code: 'Cancelled', detail: 'stop' });
    expect(r.client.terminated).toBe(true);
    expect(WasmClient.live()).not.toContain(r.client);
    await expect(r.client.root.info()).rejects.toMatchObject({ code: 'Cancelled' });
  });
});

describe('protocol helpers', () => {
  test('encodeResult registers class instances and lists each buffer once', () => {
    const registered: object[] = [];
    const shared = new Float64Array([1, 2]);
    const { value, transfer } = encodeResult({ a: shared, b: [shared], doc: new Doc(1) }, (o) => registered.push(o));
    expect(registered).toHaveLength(1);
    expect((value as { doc: unknown }).doc).toEqual({ __bcRemote: 1, className: 'Doc' });
    // The second occurrence of the same buffer is a copy with its own buffer.
    expect(transfer).toHaveLength(2);
    expect(new Set(transfer).size).toBe(2);
  });

  test('serializeError keeps the kernel code and detail', () => {
    expect(serializeError(new KernelError('ParseError', 'line 3'))).toEqual({ name: 'KernelError', code: 'ParseError', detail: 'line 3' });
    expect(serializeError(new TypeError('x is undefined'))).toEqual({ name: 'TypeError', code: 'Error', detail: 'x is undefined' });
    expect(serializeError('text')).toEqual({ name: 'Error', code: 'Error', detail: 'text' });
  });
});
