/**
 * Progress indicator of kernel calls in the worker (docs/ui/APP_SHELL.md): shown while a call
 * has been running for more than DELAY_MS; names the operation group (rebuild, assembly solve,
 * HLR…) and the elapsed time, and offers to cancel it. The kernel reports no fraction done, so
 * the indicator is indeterminate.
 */

import React, { useEffect, useState } from 'react';
import type { PendingCall, WasmClient } from '../worker/wasmClient';
import { useI18n } from './i18n';
import type { MessageKey } from './i18n';

export const DELAY_MS = 300;

export interface BusyIndicatorProps {
  client: WasmClient<unknown> | null;
  /** Cancels the running call (terminates the worker); the editor restores its document. */
  onCancel?: () => void;
}

const BusyIndicator: React.FC<BusyIndicatorProps> = ({ client, onCancel }) => {
  const { t } = useI18n();
  const [pending, setPending] = useState<PendingCall[]>([]);
  const [now, setNow] = useState(Date.now());

  useEffect(() => {
    if (!client) return undefined;
    setPending(client.pendingCalls());
    return client.onActivity(setPending);
  }, [client]);

  useEffect(() => {
    if (pending.length === 0) return undefined;
    const timer = window.setInterval(() => setNow(Date.now()), 100);
    return () => window.clearInterval(timer);
  }, [pending.length]);

  const oldest = pending.reduce<PendingCall | null>((a, c) => (!a || c.startedAt < a.startedAt ? c : a), null);
  if (!oldest || now - oldest.startedAt < DELAY_MS) return null;
  const longOne = pending.find((p) => p.operation) ?? oldest;
  const operation = t((`op.${longOne.operation ?? 'other'}`) as MessageKey);
  return (
    <span className="app-busy" role="status" data-testid="busy-indicator" data-operation={longOne.operation ?? 'other'}>
      <span className="app-busy-spinner" aria-hidden="true" />
      {t('busy.working', { operation, seconds: ((now - oldest.startedAt) / 1000).toFixed(1) })}
      {onCancel && (
        <button type="button" data-testid="busy-cancel" onClick={onCancel}>
          {t('busy.cancel')}
        </button>
      )}
    </span>
  );
};

export default BusyIndicator;
