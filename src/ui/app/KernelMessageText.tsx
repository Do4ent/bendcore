/**
 * A kernel error or message in the interface language (docs/ui/APP_SHELL.md, "Сообщения ядра"):
 * the localized text of its template, or the localized title of its code; the code and the
 * kernel's original text are always in the details.
 */

import React from 'react';
import { useI18n } from './i18n';
import { localizeKernelError, localizeKernelMessage } from './kernelMessages';
import './KernelMessageText.css';

export interface KernelMessageTextProps {
  /** a thrown value (KernelError { code, detail }, Error, ...) */
  error?: unknown;
  /** or a code and a text (a message of a feature node: code "message") */
  code?: string | null;
  message?: string;
}

export const KernelMessageText: React.FC<KernelMessageTextProps> = ({ error, code = null, message }) => {
  const { t } = useI18n();
  const m = message !== undefined ? localizeKernelMessage(t, code, message) : localizeKernelError(t, error);
  return (
    <span className="kernel-message" data-code={m.code} data-template={m.template ?? ''}>
      <span className="kernel-message-text">{m.text ?? m.title}</span>{' '}
      <details className="kernel-message-details">
        <summary>{t('kernel.details')}</summary>
        <code>
          {m.code}: {m.original}
        </code>
      </details>
    </span>
  );
};

export default KernelMessageText;
