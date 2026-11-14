/**
 * DXF import panel of the sketch editor (docs/ui/SKETCH_EDITOR.md, "Импорт DXF"). The chosen file
 * is sent to the kernel (ModelingKernel.dxfSketch in the modeling worker) on every change of the
 * layers, units, tolerance or fixing: the kernel answers with the layers of the file, the counts
 * and the open ends of what would be imported. "Insert" asks the kernel for the same import
 * appended to the current sketch data and hands the result to the editor, which solves it.
 */

import React, { useEffect, useRef, useState } from 'react';
import type { DxfSketchImport, ModelingKernel, SketchData } from '../../../wasm/bendcore_modeling_api.mjs';
import type { Remote } from '../../worker/wasmClient';
import { useI18n } from '../../app/i18n';
import { DXF_UNITS, DxfImportForm, dxfOptions, dxfSummary, dxfTolerance, initialDxfForm, toggleLayer, unitCaption } from './dxfImport';
import { KernelMessageText } from '../../app/KernelMessageText';

export interface DxfImportPanelProps {
  kernel: Remote<ModelingKernel>;
  /** current data of the sketch (the import is appended to it) */
  current: () => SketchData;
  /** the import, appended to the current data */
  onInsert: (result: DxfSketchImport) => void;
  onClose: () => void;
}

interface Loaded {
  name: string;
  bytes: Uint8Array;
}

const DxfImportPanel: React.FC<DxfImportPanelProps> = ({ kernel, current, onInsert, onClose }) => {
  const { t } = useI18n();
  const [file, setFile] = useState<Loaded | null>(null);
  const [form, setForm] = useState<DxfImportForm>(initialDxfForm);
  const [preview, setPreview] = useState<DxfSketchImport | null>(null);
  const [error, setError] = useState<unknown>(null);
  const [busy, setBusy] = useState(false);
  const seq = useRef(0);
  const inputRef = useRef<HTMLInputElement>(null);

  // Preview: the kernel's answer for the file and the form.
  useEffect(() => {
    if (!file) return;
    const n = ++seq.current;
    kernel.dxfSketch(file.bytes, dxfOptions(form)).then(
      (r) => {
        if (n !== seq.current) return;
        setPreview(r);
        setError(null);
      },
      (e: unknown) => {
        if (n !== seq.current) return;
        setPreview(null);
        setError(e);
      },
    );
  }, [kernel, file, form]);

  const choose = async (f: File) => {
    const bytes = new Uint8Array(await f.arrayBuffer());
    setForm(initialDxfForm());
    setPreview(null);
    setFile({ name: f.name, bytes });
  };

  const insert = async () => {
    if (!file) return;
    setBusy(true);
    try {
      const r = await kernel.dxfSketch(file.bytes, { ...dxfOptions(form), into: current() });
      setError(null);
      onInsert(r);
    } catch (e) {
      setError(e);
    } finally {
      setBusy(false);
    }
  };

  const units = form.units ?? (preview ? preview.units : null);
  const toleranceOk = dxfTolerance(form) !== null;
  return (
    <fieldset className="pm-group sk-tools sk-dxf" data-testid="sketch-dxf-panel">
      <legend>{t('sketch.dxf.title')}</legend>
      <input
        ref={inputRef}
        type="file"
        accept=".dxf"
        data-testid="sketch-dxf-file"
        onChange={(e) => {
          const f = e.target.files?.[0];
          if (f) void choose(f);
          e.target.value = '';
        }}
      />
      {file && preview && (
        <>
          <div className="sk-dxf-line">{t('sketch.dxf.file', { name: file.name, version: preview.version || '—' })}</div>
          <div className="sk-dxf-layers" data-testid="sketch-dxf-layers">
            <div className="sk-dxf-line">{t('sketch.dxf.layers')}</div>
            {preview.layers.map((l) => (
              <label key={l.name} className="sk-check">
                <input
                  type="checkbox"
                  data-testid={`sketch-dxf-layer-${l.name}`}
                  checked={form.layers ? form.layers.includes(l.name) : l.selected}
                  onChange={(e) => setForm(toggleLayer(form, preview, l.name, e.target.checked))}
                />
                {t('sketch.dxf.layer', { name: l.name, curves: l.curves })} {l.frozen ? t('sketch.dxf.frozen') : ''}
                {l.off ? t('sketch.dxf.off') : ''}
              </label>
            ))}
          </div>
          <label className="sk-check">
            {t('sketch.dxf.units')}{' '}
            <select
              data-testid="sketch-dxf-units"
              value={form.units === null && preview.fileUnits !== 0 ? 'file' : String(units)}
              onChange={(e) => setForm({ ...form, units: e.target.value === 'file' ? null : Number(e.target.value) })}
            >
              {preview.fileUnits !== 0 && (
                <option value="file">
                  {unitCaption(t, preview.fileUnits)} {t('sketch.dxf.unitsFromFile', { code: preview.fileUnits })}
                </option>
              )}
              {DXF_UNITS.map((u) => (
                <option key={u} value={String(u)}>
                  {unitCaption(t, u)}
                </option>
              ))}
            </select>
          </label>
          {preview.fileUnits === 0 && form.units === null && <div className="sk-message">{t('sketch.dxf.unitsMissing')}</div>}
          <label className="sk-check">
            {t('sketch.dxf.tolerance')}{' '}
            <input
              type="text"
              size={8}
              data-testid="sketch-dxf-tolerance"
              value={form.tolerance}
              onChange={(e) => setForm({ ...form, tolerance: e.target.value })}
            />
          </label>
          {!toleranceOk && <div className="sk-message">{t('sketch.dxf.badTolerance')}</div>}
          <label className="sk-check">
            <input type="checkbox" data-testid="sketch-dxf-fixed" checked={form.fixed} onChange={(e) => setForm({ ...form, fixed: e.target.checked })} />
            {t('sketch.dxf.fixed')}
          </label>
          <ul className="sk-dxf-summary" data-testid="sketch-dxf-summary" data-open-ends={preview.openEnds.length} data-curves={preview.entities.length}>
            {dxfSummary(preview, t).map((s, i) => (
              <li key={i}>{s}</li>
            ))}
          </ul>
        </>
      )}
      {error !== null && (
        <div className="sk-message" role="alert" data-testid="sketch-dxf-error">
          <KernelMessageText error={error} />
        </div>
      )}
      <div className="pm-panel-buttons">
        <button
          type="button"
          className="pm-primary"
          data-testid="sketch-dxf-insert"
          disabled={!preview || preview.entities.length === 0 || busy || !toleranceOk}
          onClick={() => void insert()}
        >
          {t('sketch.dxf.insert')}
        </button>
        <button type="button" data-testid="sketch-dxf-close" onClick={onClose}>
          {t('sketch.dxf.close')}
        </button>
      </div>
    </fieldset>
  );
};

export default DxfImportPanel;
