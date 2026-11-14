/**
 * Report of a module failure from an editor to the application shell (docs/ui/APP_SHELL.md,
 * "Сбой модуля"). When the editor's worker client closes with a module fault (a WebAssembly trap
 * in a call, the worker died — wasmClient.ts isModuleFault), the shell saves the editor's last
 * completed document as a draft and offers to restart the module (the editor is mounted again
 * with that draft).
 */

import { useEffect, useRef } from 'react';
import type { WasmClient } from '../worker/wasmClient';
import type { ModuleKind } from '../worker/protocol';

export interface ModuleFaultInfo {
  module: ModuleKind;
  /** ModuleFault or WorkerFailed. */
  code: string;
  detail: string;
}

/** Calls `onFault` once when `client` fails; follows the client the editor holds now. */
export function useFaultReport(client: WasmClient<unknown> | null, onFault: ((info: ModuleFaultInfo) => void) | undefined): void {
  const ref = useRef(onFault);
  ref.current = onFault;
  useEffect(() => {
    if (!client) return undefined;
    const report = (e: { code: string; detail: string }, module: ModuleKind) => ref.current?.({ module, code: e.code, detail: e.detail });
    // The client may have failed before this effect ran (a trap in the first call).
    const already = client.fault;
    if (already) {
      report(already, client.module);
      return undefined;
    }
    return client.onFault(report);
  }, [client]);
}
