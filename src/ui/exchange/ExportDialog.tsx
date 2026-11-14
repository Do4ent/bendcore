/**
 * Export dialog of the part, assembly and imported-model editors (docs/ui/DATA_EXCHANGE.md):
 * format (STEP, IGES, STL, 3MF) and its options; "Save file" asks the editor's kernel module for
 * the bytes (exportFile) and downloads them. Errors of the module stay in the dialog.
 */

import React, { useState } from 'react';
import { caption, useI18n } from '../app/i18n';
import { downloadText } from '../app/editorHost';
import {
  DEFAULT_EXPORT_FORM,
  EXPORT_FORMATS,
  MEDIA_TYPES,
  exportFileName,
  exportOptions,
  isMeshFormat,
} from './exchange';
import type { ExportForm, ExportFormat, ExportOptions } from './exchange';
import './Exchange.css';

export interface ExportDialogProps {
  /** Document name; the file is named after it with the extension of the format. */
  name: string;
  /** Kernel call that writes the file (part, assembly or imported shape). */
  onExport: (format: ExportFormat, options: ExportOptions) => Promise<Uint8Array>;
  onClose: () => void;
  /** After a successful download: file name and size, for the editor's status line. */
  onSaved?: (fileName: string, size: number) => void;
  /** 3MF entries of this build are stored, not deflated (kernel exchangeInfo). */
  zipStored?: boolean;
}

/** Last form of the session, so that a second export starts from the same settings. */
let lastForm: ExportForm = DEFAULT_EXPORT_FORM;

const SCHEMAS: ExportForm['schema'][] = ['AP214', 'AP242'];
const UNITS: ExportForm['unit'][] = ['mm', 'm', 'inch'];
const IGES_MODES: ExportForm['igesMode'][] = ['brep', 'faces'];
const STL_FORMATS: ExportForm['stlFormat'][] = ['binary', 'ascii'];

const ExportDialog: React.FC<ExportDialogProps> = ({ name, onExport, onClose, onSaved, zipStored }) => {
  const { t } = useI18n();
  const [form, setFormState] = useState<ExportForm>(lastForm);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const checked = exportOptions(form, name);
  const fieldError = 'error' in checked ? checked.error : null;

  const setForm = (patch: Partial<ExportForm>) => {
    setFormState((f) => {
      const next = { ...f, ...patch };
      lastForm = next;
      return next;
    });
    setError(null);
  };

  const save = async () => {
    if (!('options' in checked)) return;
    setBusy(true);
    setError(null);
    try {
      const bytes = await onExport(form.format, checked.options);
      const fileName = exportFileName(name, form.format);
      downloadText(fileName, bytes, MEDIA_TYPES[form.format]);
      onSaved?.(fileName, bytes.length);
      onClose();
    } catch (e) {
      const ke = e as { code?: string; detail?: string; message?: string };
      setError(ke.code ? `${ke.code}: ${ke.detail ?? ke.message}` : String(ke.message ?? e));
    } finally {
      setBusy(false);
    }
  };

  const rangeText = fieldError ? t('exchange.rangeError', { min: fieldError.min, max: fieldError.max }) : '';

  return (
    <div className="ex-backdrop" role="presentation" onMouseDown={(e) => e.target === e.currentTarget && !busy && onClose()}>
      <div className="ex-dialog" role="dialog" aria-modal="true" data-testid="export-dialog">
        <h2>{t('exchange.exportTitle', { name })}</h2>
        <label className="ex-row">
          <span>{t('exchange.format')}</span>
          <select value={form.format} onChange={(e) => setForm({ format: e.target.value as ExportFormat })} data-testid="export-format">
            {EXPORT_FORMATS.map((f) => (
              <option key={f} value={f}>
                {caption(t, 'exchange.format.', f)}
              </option>
            ))}
          </select>
        </label>
        {form.format === 'step' && (
          <label className="ex-row">
            <span>{t('exchange.schema')}</span>
            <select value={form.schema} onChange={(e) => setForm({ schema: e.target.value as ExportForm['schema'] })} data-testid="export-schema">
              {SCHEMAS.map((s) => (
                <option key={s} value={s}>
                  {s}
                </option>
              ))}
            </select>
          </label>
        )}
        {form.format === 'iges' && (
          <label className="ex-row">
            <span>{t('exchange.igesMode')}</span>
            <select value={form.igesMode} onChange={(e) => setForm({ igesMode: e.target.value as ExportForm['igesMode'] })} data-testid="export-iges-mode">
              {IGES_MODES.map((m) => (
                <option key={m} value={m}>
                  {caption(t, 'exchange.igesMode.', m)}
                </option>
              ))}
            </select>
          </label>
        )}
        {!isMeshFormat(form.format) && (
          <label className="ex-row">
            <span>{t('exchange.unit')}</span>
            <select value={form.unit} onChange={(e) => setForm({ unit: e.target.value as ExportForm['unit'] })} data-testid="export-unit">
              {UNITS.map((u) => (
                <option key={u} value={u}>
                  {caption(t, 'exchange.unit.', u)}
                </option>
              ))}
            </select>
          </label>
        )}
        {form.format === 'stl' && (
          <label className="ex-row">
            <span>{t('exchange.stlFormat')}</span>
            <select value={form.stlFormat} onChange={(e) => setForm({ stlFormat: e.target.value as ExportForm['stlFormat'] })} data-testid="export-stl-format">
              {STL_FORMATS.map((f) => (
                <option key={f} value={f}>
                  {caption(t, 'exchange.stlFormat.', f)}
                </option>
              ))}
            </select>
          </label>
        )}
        {isMeshFormat(form.format) && (
          <>
            <label className="ex-row">
              <span>{t('exchange.deflection')}</span>
              <input
                type="number"
                step="0.01"
                value={form.linearDeflection}
                onChange={(e) => setForm({ linearDeflection: e.target.value })}
                aria-invalid={fieldError?.field === 'linearDeflection'}
                data-testid="export-deflection"
              />
            </label>
            <label className="ex-row">
              <span>{t('exchange.angle')}</span>
              <input
                type="number"
                step="1"
                value={form.angularDeflectionDeg}
                onChange={(e) => setForm({ angularDeflectionDeg: e.target.value })}
                aria-invalid={fieldError?.field === 'angularDeflectionDeg'}
                data-testid="export-angle"
              />
            </label>
            <p className="ex-hint">{t('exchange.meshHint')}</p>
            {form.format === '3mf' && zipStored && <p className="ex-hint">{t('exchange.zipStored')}</p>}
          </>
        )}
        {fieldError && (
          <p className="ex-error" role="alert" data-testid="export-range-error">
            {rangeText}
          </p>
        )}
        {error && (
          <p className="ex-error" role="alert" data-testid="export-error">
            {error}
          </p>
        )}
        <div className="ex-buttons">
          <button type="button" className="ex-primary" onClick={() => void save()} disabled={busy || fieldError !== null} data-testid="export-save">
            {t('exchange.save')}
          </button>
          <button type="button" onClick={onClose} disabled={busy} data-testid="export-cancel">
            {t('exchange.cancel')}
          </button>
        </div>
      </div>
    </div>
  );
};

export default ExportDialog;
