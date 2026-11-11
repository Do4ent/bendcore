/**
 * Entry of the WASM worker (docs/ui/APP_SHELL.md), bundled as wasm-worker.js by the page
 * configurations. The first message names the module and the URL its build output is served
 * from; the worker imports the Emscripten glue from there (it finds the .wasm next to itself),
 * wraps it in the JS API of src/wasm and answers calls with WorkerHost.
 */

import { loadKernel } from '../../wasm/bendcore_kernel_api.mjs';
import { loadModeling } from '../../wasm/bendcore_modeling_api.mjs';
import { loadAssembly } from '../../wasm/bendcore_assembly_api.mjs';
import { loadDrawingKernel } from '../../wasm/bendcore_drawing_api.mjs';
import { InitRequest, Request, Response, serializeError } from './protocol';
import { WorkerHost } from './workerHost';

interface WorkerScope {
  onmessage: ((e: MessageEvent<Request>) => void) | null;
  postMessage(message: Response, transfer: ArrayBuffer[]): void;
}

const scope = self as unknown as WorkerScope;

const GLUE: Record<InitRequest['module'], string> = {
  kernel: 'bendcore_kernel.mjs',
  modeling: 'bendcore_modeling.mjs',
  assembly: 'bendcore_assembly.mjs',
  drawing: 'bendcore_drawing.mjs',
};

export const DRAWING_FONT_FILE = 'DejaVuSans.ttf';

type Factory = (options?: Record<string, unknown>) => Promise<unknown>;

async function load(req: InitRequest): Promise<{ root: object; info: unknown }> {
  const glueUrl = new URL(GLUE[req.module], req.baseUrl).href;
  const glue = (await import(/* webpackIgnore: true */ glueUrl)) as { default: Factory };
  switch (req.module) {
    case 'kernel': {
      const k = await loadKernel(glue.default);
      return { root: k, info: k.info() };
    }
    case 'modeling': {
      const k = await loadModeling(glue.default);
      return { root: k, info: k.info() };
    }
    case 'assembly': {
      const k = await loadAssembly(glue.default);
      return { root: k, info: k.info() };
    }
    case 'drawing': {
      const fontUrl = new URL(DRAWING_FONT_FILE, req.baseUrl).href;
      const response = await fetch(fontUrl);
      if (!response.ok) throw new Error(`font ${fontUrl}: HTTP ${response.status}`);
      const k = await loadDrawingKernel(glue.default);
      k.setFont(new Uint8Array(await response.arrayBuffer()), DRAWING_FONT_FILE);
      return { root: k, info: k.info() };
    }
  }
}

let host: WorkerHost | null = null;

scope.onmessage = (e: MessageEvent<Request>) => {
  const msg = e.data;
  if (msg.type === 'init') {
    load(msg).then(
      ({ root, info }) => {
        host = new WorkerHost(root, (m, t) => scope.postMessage(m, t));
        scope.postMessage({ type: 'ready', info }, []);
      },
      (err: unknown) => scope.postMessage({ type: 'init-failed', error: serializeError(err) }, []),
    );
    return;
  }
  if (!host) {
    scope.postMessage({ type: 'error', id: msg.id, error: { name: 'KernelError', code: 'InvalidInput', detail: 'the module is not loaded yet' } }, []);
    return;
  }
  host.handle(msg);
};
