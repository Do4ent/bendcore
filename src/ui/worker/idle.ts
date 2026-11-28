/**
 * Waits until the page has no worker call in flight for two consecutive animation frames: the
 * last result has arrived and React has rendered it. The test hooks of the editors expose it as
 * idle() (docs/ui/APP_SHELL.md), since every kernel call is asynchronous now.
 */

import type { WasmClient } from './wasmClient';

const frame = () => new Promise<void>((resolve) => requestAnimationFrame(() => resolve()));

export async function waitIdle(clients: () => (WasmClient<unknown> | null | undefined)[], timeoutMs = 120_000): Promise<void> {
  const start = Date.now();
  let quiet = 0;
  while (quiet < 2) {
    if (Date.now() - start > timeoutMs) throw new Error(`the page is still busy after ${timeoutMs} ms`);
    await frame();
    const busy = clients().some((c) => c && !c.terminated && c.pendingCalls().length > 0);
    quiet = busy ? 0 : quiet + 1;
  }
}
