/**
 * Drawing editor page (docs/ui/DRAWING_UI.md): the drawing document of bendcore_drawing_wasm made
 * from a part or assembly document — sheets with the title block, views (base, projected,
 * axonometric, section, detail), dimensions by picked 2D edges, roughness, form tolerances,
 * datums, technical requirements, update from the model with a report of changed and dangling
 * dimensions, the specification of an assembly, export to PDF/DXF/SVG and save/open.
 *
 * Every action is a call of the drawing kernel; the sheet on screen is the kernel's SVG of the
 * sheet and the pickable edges are the kernel's 2D view edges. window.bendcoreDrawingEditor
 * exposes the page state (read-only) for end-to-end tests.
 */

import React, { useCallback, useEffect, useRef, useState } from 'react';
import type {
  DrawingDescription,
  DrawingHandle,
  DrawingKernel,
  GeometricCharacteristic,
  PictureView,
  Point,
  SheetInfo,
  SheetPicture,
  SpecificationTable,
  StalenessReport,
  StandardView,
  UpdateReport,
  ViewSpec,
} from '../../wasm/bendcore_drawing_api.mjs';
import SheetCanvas, { SheetCanvasHandle } from './SheetCanvas';
import { defaultDrawingBaseUrl, loadDrawingFrom } from './drawingLoader';
import {
  CHARACTERISTICS,
  DIMENSION_TYPE_LABELS,
  SCALES,
  SHEET_SIDES,
  STANDARD_VIEW_LABELS,
  VIEW_KIND_LABELS,
} from './labels';
import {
  EdgePick,
  PickState,
  Tool,
  addPick,
  canDimensionSingle,
  downloadName,
  emptyPicks,
  sheetToViewPoint,
  viewPointToModel,
} from './sheetGeometry';
import './DrawingEditor.css';

type Variable = { name: string; expression: string; value: number | null; error: string | null };

export interface DrawingEditorState {
  status: 'loading' | 'ready' | 'failed';
  error: { code: string; message: string } | null;
  message: string;
  sheet: string | null;
  tool: Tool;
  description: DrawingDescription | null;
  staleness: StalenessReport | null;
  report: UpdateReport | null;
  picks: EdgePick[];
}

declare global {
  interface Window {
    bendcoreDrawingEditor?: DrawingEditorState;
  }
}

const scaleText = ([n, d]: [number, number]) => (n === 5 && d === 2 ? '2,5:1' : n === 2 && d === 5 ? '1:2,5' : `${n}:${d}`);

function download(bytes: Uint8Array | string, name: string, type: string) {
  const blob = new Blob([typeof bytes === 'string' ? bytes : bytes.slice().buffer], { type });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = name;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function errorOf(e: unknown): { code: string; message: string } {
  if (e && typeof e === 'object' && 'code' in e && 'detail' in e) {
    return { code: String((e as { code: string }).code), message: String((e as { detail: string }).detail) };
  }
  return { code: 'Error', message: e instanceof Error ? e.message : String(e) };
}

const STANDARD_PROJECTED: StandardView[] = ['top', 'left', 'right', 'bottom', 'rear'];

export default function DrawingEditorApp(): JSX.Element {
  const [kernel, setKernel] = useState<DrawingKernel | null>(null);
  const [status, setStatus] = useState<DrawingEditorState['status']>('loading');
  const [error, setError] = useState<DrawingEditorState['error']>(null);
  const [message, setMessage] = useState('Загрузка ядра чертежей…');
  const drawingRef = useRef<DrawingHandle | null>(null);
  const [desc, setDesc] = useState<DrawingDescription | null>(null);
  const [picture, setPicture] = useState<SheetPicture | null>(null);
  const [sheetId, setSheetId] = useState<string | null>(null);
  const [tool, setTool] = useState<Tool>('select');
  const [pickState, setPickState] = useState<PickState>(emptyPicks);
  const [selectedView, setSelectedView] = useState<string | null>(null);
  const [variables, setVariables] = useState<Variable[]>([]);
  const [staleness, setStaleness] = useState<StalenessReport | null>(null);
  const [report, setReport] = useState<UpdateReport | null>(null);
  const [tab, setTab] = useState<'sheet' | 'specification'>('sheet');
  const [spec, setSpec] = useState<{ table: SpecificationTable | null; reason: string | null }>({ table: null, reason: null });
  const [modelName, setModelName] = useState('');
  const canvas = useRef<SheetCanvasHandle>(null);

  // Tool options.
  const [baseStandard, setBaseStandard] = useState<StandardView>('front');
  const [dimOffset, setDimOffset] = useState('10');
  const [rough, setRough] = useState({ method: 'materialRemoval', parameter: 'Ra', value: '3,2' });
  const [tol, setTol] = useState<{ characteristic: GeometricCharacteristic; tolerance: string; datums: string }>({
    characteristic: 'parallelism',
    tolerance: '0,02',
    datums: 'А',
  });
  const [datumLetter, setDatumLetter] = useState('А');
  const [detailRadius, setDetailRadius] = useState('10');
  const [sectionSide, setSectionSide] = useState('leftOf');

  useEffect(() => {
    let cancelled = false;
    loadDrawingFrom(defaultDrawingBaseUrl())
      .then((k) => {
        if (cancelled) return;
        setKernel(k);
        setStatus('ready');
        setMessage('Откройте документ детали или сборки (JSON)');
      })
      .catch((e) => {
        if (cancelled) return;
        setStatus('failed');
        setError(errorOf(e));
        setMessage('Ядро чертежей не загрузилось');
      });
    return () => {
      cancelled = true;
    };
  }, []);

  const sheet: SheetInfo | null = desc?.sheets.find((s) => s.id === sheetId) ?? null;

  const refresh = useCallback(
    (wantedSheet?: string | null) => {
      const d = drawingRef.current;
      if (!d) return;
      const description = d.describe();
      setDesc(description);
      const id =
        wantedSheet && description.sheets.some((s) => s.id === wantedSheet) ? wantedSheet : description.sheets[0]?.id ?? null;
      setSheetId(id);
      setPicture(id ? d.sheetPicture(id) : null);
      if (description.model.kind === 'part') setVariables(d.variables());
      else setVariables([]);
      setStaleness(d.checkModel());
      try {
        setSpec({ table: d.specification(), reason: null });
      } catch (e) {
        setSpec({ table: null, reason: errorOf(e).message });
      }
    },
    [],
  );

  // Runs a kernel action; any failure is shown in the status line and the page is refreshed.
  const run = useCallback(
    (label: string, action: (d: DrawingHandle) => unknown, keepPicks = false) => {
      const d = drawingRef.current;
      if (!d) return;
      let next = sheetId;
      try {
        const result = action(d) as { warnings?: string[]; switchTo?: string } | undefined;
        if (result && typeof result.switchTo === 'string') next = result.switchTo;
        setError(null);
        const warnings = result && Array.isArray(result.warnings) && result.warnings.length ? ` (${result.warnings.join('; ')})` : '';
        setMessage(label + warnings);
      } catch (e) {
        setError(errorOf(e));
        setMessage(`${label}: не выполнено`);
      }
      if (!keepPicks) setPickState(emptyPicks);
      refresh(next);
    },
    [refresh, sheetId],
  );

  const openDrawing = (make: (k: DrawingKernel) => DrawingHandle, name: string) => {
    if (!kernel) return;
    try {
      const d = make(kernel);
      drawingRef.current?.dispose();
      drawingRef.current = d;
      setModelName(name);
      setReport(null);
      setSelectedView(null);
      setPickState(emptyPicks);
      setTool('select');
      setError(null);
      setMessage(`Открыто: ${name}`);
      refresh(null);
    } catch (e) {
      setError(errorOf(e));
      setMessage(`Не удалось открыть ${name}`);
    }
  };

  const onOpenModel = async (files: FileList | null) => {
    if (!files || files.length === 0) return;
    const file = files[0];
    const text = await file.text();
    let kind: 'part' | 'assembly' = 'part';
    try {
      kind = JSON.parse(text).format === 'bendcore-assembly' ? 'assembly' : 'part';
    } catch {
      // The kernel reports the parse error.
    }
    const base = file.name.replace(/(\.part|\.assembly)?\.json$/i, '');
    openDrawing(
      (k) =>
        kind === 'assembly'
          ? k.createFromAssembly(text, { path: file.name })
          : k.createFromPart(text, { path: file.name, properties: { name: base } }),
      file.name,
    );
  };

  const onOpenDrawing = async (files: FileList | null) => {
    if (!files || files.length < 2) {
      setError({ code: 'InvalidInput', message: 'Выберите два файла: чертёж (.drawing.json) и его модель' });
      return;
    }
    const texts = await Promise.all(Array.from(files).map((f) => f.text().then((t) => ({ name: f.name, t }))));
    const isDrawing = (t: string) => {
      try {
        return JSON.parse(t).format === 'bendcore-drawing';
      } catch {
        return false;
      }
    };
    const drawingFile = texts.find((x) => isDrawing(x.t));
    const modelFile = texts.find((x) => x !== drawingFile);
    if (!drawingFile || !modelFile) {
      setError({ code: 'InvalidInput', message: 'Среди файлов нет документа чертежа "bendcore-drawing"' });
      return;
    }
    const loaded: { report: UpdateReport | null } = { report: null };
    openDrawing(
      (k) => {
        const r = k.load(drawingFile.t, modelFile.t);
        loaded.report = r.report;
        return r.drawing;
      },
      drawingFile.name,
    );
    if (loaded.report) setReport(loaded.report);
  };

  const onSave = () => {
    const d = drawingRef.current;
    if (!d || !desc) return;
    const designation = sheet?.titleShown?.designation ?? '';
    download(d.save(), downloadName(designation, '', 'drawing.json'), 'application/json');
    download(d.saveModel(), downloadName(designation, '', desc.model.kind === 'part' ? 'part.json' : 'assembly.json'), 'application/json');
    setMessage('Сохранено: документ чертежа и модель');
  };

  const onExport = (format: 'pdf' | 'dxf' | 'svg') => {
    const d = drawingRef.current;
    if (!d || !sheet) return;
    try {
      const designation = sheet.titleShown?.designation ?? '';
      if (format === 'pdf') {
        download(d.exportPdf(), downloadName(designation, '', 'pdf'), 'application/pdf');
      } else {
        const bytes = d.exportSheet(sheet.id, format);
        download(bytes, downloadName(designation, `_${sheet.id}`, format), format === 'dxf' ? 'application/dxf' : 'image/svg+xml');
      }
      setError(null);
      setMessage(`Экспорт ${format.toUpperCase()} выполнен`);
    } catch (e) {
      setError(errorOf(e));
      setMessage(`Экспорт ${format.toUpperCase()}: не выполнен`);
    }
  };

  // ---- picks of the edge tools

  const createFromPicks = useCallback(
    (state: PickState) => {
      if (!sheetId || state.picks.length === 0) return;
      const view = state.picks[0].view;
      const indices = state.picks.map((p) => p.index);
      if (tool === 'dimension') {
        run('Размер добавлен', (d) => d.addDimension(sheetId, view, indices, { offset: Number(dimOffset) || 10 }));
      } else if (tool === 'roughness') {
        run('Шероховатость добавлена', (d) =>
          d.addRoughness(sheetId, view, indices[0], { method: rough.method as 'materialRemoval', parameter: rough.parameter, value: rough.value }),
        );
      } else if (tool === 'tolerance') {
        const datums = tol.datums.split(/[\s,;]+/).filter(Boolean);
        run('Допуск добавлен', (d) =>
          d.addTolerance(sheetId, view, indices[0], { characteristic: tol.characteristic, tolerance: tol.tolerance, datums }),
        );
      } else if (tool === 'datum') {
        run('База добавлена', (d) => d.addDatum(sheetId, view, indices[0], datumLetter));
      }
    },
    [sheetId, tool, run, dimOffset, rough, tol, datumLetter],
  );

  const onEdgeClick = (pick: EdgePick) => {
    const next = addPick(pickState, tool, pick);
    if (next.message) setMessage(next.message);
    if (next.ready) {
      createFromPicks(next);
    } else {
      setPickState(next);
    }
  };

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if ((e.target as HTMLElement)?.tagName === 'INPUT' || (e.target as HTMLElement)?.tagName === 'TEXTAREA') return;
      if (e.key === 'Escape') {
        setPickState(emptyPicks);
        setTool('select');
      } else if (e.key === 'Enter' && tool === 'dimension' && canDimensionSingle(pickState)) {
        createFromPicks(pickState);
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [tool, pickState, createFromPicks]);

  const onSheetClick = (p: Point, view: PictureView | null) => {
    if (!sheetId) return;
    if (!view) {
      setMessage('Щёлкните по виду');
      return;
    }
    const vp = sheetToViewPoint(view, p);
    if (tool === 'detail') {
      run('Выносной элемент добавлен', (d) =>
        d.addView(sheetId, { kind: 'detail', parent: view.id, detailCenter: vp, detailRadius: Number(detailRadius) || 10 }),
      );
    } else if (tool === 'section') {
      const spec: ViewSpec = { kind: 'section', parent: view.id, planePoint: viewPointToModel(view, vp), markView: view.id };
      if (sectionSide !== 'none') spec.side = sectionSide as ViewSpec['side'];
      run('Разрез добавлен', (d) => d.addView(sheetId, spec));
    }
  };

  const switchTool = (t: Tool) => {
    setTool(t);
    setPickState(emptyPicks);
  };

  const onUpdate = () => {
    const d = drawingRef.current;
    if (!d) return;
    try {
      const r = d.updateFromModel();
      setReport(r);
      setError(null);
      setMessage(
        r.modelChanged
          ? `Чертёж обновлён: ревизия ${r.revision}, изменено размеров ${r.stale.length}, висячих ${r.dangling.length}`
          : 'Модель не изменилась',
      );
    } catch (e) {
      setError(errorOf(e));
      setMessage('Обновление по модели: не выполнено');
    }
    refresh(sheetId);
  };

  // State for tests.
  useEffect(() => {
    window.bendcoreDrawingEditor = {
      status,
      error,
      message,
      sheet: sheetId,
      tool,
      description: desc,
      staleness,
      report,
      picks: pickState.picks,
    };
  }, [status, error, message, sheetId, tool, desc, staleness, report, pickState]);

  const hasBase = !!sheet?.views.some((v) => v.kind === 'base');
  const selected = sheet?.views.find((v) => v.id === selectedView) ?? null;
  const drawing = drawingRef.current;

  return (
    <div className="dr-root">
      <header className="dr-toolbar">
        <strong className="dr-title">bendcore · чертёж</strong>
        <label className="dr-file">
          Открыть модель…
          <input type="file" accept=".json,application/json" data-testid="open-model" onChange={(e) => void onOpenModel(e.target.files)} disabled={!kernel} />
        </label>
        <label className="dr-file">
          Открыть чертёж…
          <input type="file" accept=".json,application/json" multiple data-testid="open-drawing" onChange={(e) => void onOpenDrawing(e.target.files)} disabled={!kernel} />
        </label>
        <button onClick={onSave} disabled={!drawing}>Сохранить</button>
        <span className="dr-sep" />
        <button onClick={() => onExport('pdf')} disabled={!drawing} data-testid="export-pdf">PDF</button>
        <button onClick={() => onExport('dxf')} disabled={!drawing} data-testid="export-dxf">DXF</button>
        <button onClick={() => onExport('svg')} disabled={!drawing} data-testid="export-svg">SVG</button>
        <span className="dr-sep" />
        {(
          [
            ['select', 'Выбор'],
            ['dimension', 'Размер'],
            ['roughness', 'Шероховатость'],
            ['tolerance', 'Допуск формы'],
            ['datum', 'База'],
            ['section', 'Разрез'],
            ['detail', 'Выносной элемент'],
          ] as [Tool, string][]
        ).map(([t, l]) => (
          <button key={t} className={tool === t ? 'dr-active' : ''} onClick={() => switchTool(t)} disabled={!drawing} data-testid={`tool-${t}`}>
            {l}
          </button>
        ))}
        <span className="dr-sep" />
        <button onClick={() => canvas.current?.fit()} disabled={!picture}>Вписать</button>
      </header>

      <div className="dr-main">
        <aside className="dr-left">
          {sheet && desc && drawing && (
            <>
              <h2>Листы</h2>
              <div className="dr-sheet-tabs">
                {desc.sheets.map((s) => (
                  <button key={s.id} className={s.id === sheetId ? 'dr-active' : ''} onClick={() => refresh(s.id)}>
                    {s.id} · {s.name}
                  </button>
                ))}
                <button onClick={() => run('Лист добавлен', (d) => ({ switchTo: d.addSheet({}) }))} title="Добавить лист А3" data-testid="add-sheet">+</button>
              </div>
              <div className="dr-row">
                <label>
                  Формат
                  <select
                    value={sheet.format}
                    data-testid="sheet-format"
                    onChange={(e) => run('Формат листа изменён', (d) => d.setSheet(sheet.id, { format: e.target.value as SheetInfo['format'] }))}
                  >
                    {['A0', 'A1', 'A2', 'A3', 'A4'].map((f) => (
                      <option key={f} value={f}>{f.replace('A', 'А')}</option>
                    ))}
                  </select>
                </label>
                <label>
                  <select
                    value={sheet.orientation}
                    data-testid="sheet-orientation"
                    onChange={(e) => run('Ориентация изменена', (d) => d.setSheet(sheet.id, { orientation: e.target.value as SheetInfo['orientation'] }))}
                  >
                    <option value="landscape">альбомная</option>
                    <option value="portrait">книжная</option>
                  </select>
                </label>
                <label>
                  Масштаб
                  <select
                    value={sheet.scale ? sheet.scale.ratio.join(':') : ''}
                    data-testid="sheet-scale"
                    onChange={(e) =>
                      run('Масштаб листа изменён', (d) =>
                        d.setSheet(sheet.id, { scale: e.target.value ? (e.target.value.split(':').map(Number) as [number, number]) : null }),
                      )
                    }
                  >
                    <option value="">авто ({sheet.computedScale?.label})</option>
                    {SCALES.map((s) => (
                      <option key={s.join(':')} value={s.join(':')}>{scaleText(s)}</option>
                    ))}
                  </select>
                </label>
              </div>
              {desc.sheets.length > 1 && (
                <button onClick={() => run('Лист удалён', (d) => d.removeSheet(sheet.id))}>Удалить лист {sheet.id}</button>
              )}

              <h2>Виды</h2>
              {!hasBase && (
                <div className="dr-row">
                  <select value={baseStandard} onChange={(e) => setBaseStandard(e.target.value as StandardView)}>
                    {(Object.keys(STANDARD_VIEW_LABELS) as StandardView[]).map((s) => (
                      <option key={s} value={s}>вид {STANDARD_VIEW_LABELS[s]}</option>
                    ))}
                  </select>
                  <button data-testid="add-base" onClick={() => run('Главный вид добавлен', (d) => d.addView(sheet.id, { kind: 'base', standard: baseStandard }))}>
                    Главный вид
                  </button>
                </div>
              )}
              {hasBase && (
                <div className="dr-buttons">
                  {STANDARD_PROJECTED.map((s) => (
                    <button key={s} data-testid={`add-${s}`} onClick={() => run(`Вид ${STANDARD_VIEW_LABELS[s]} добавлен`, (d) => d.addView(sheet.id, { kind: 'projected', standard: s }))}>
                      Вид {STANDARD_VIEW_LABELS[s]}
                    </button>
                  ))}
                  <button data-testid="add-isometric" onClick={() => run('Изометрия добавлена', (d) => d.addView(sheet.id, { kind: 'axonometric', axonometry: 'isometric' }))}>
                    Изометрия
                  </button>
                  <button data-testid="add-dimetric" onClick={() => run('Диметрия добавлена', (d) => d.addView(sheet.id, { kind: 'axonometric', axonometry: 'dimetric' }))}>
                    Диметрия
                  </button>
                </div>
              )}
              <ul className="dr-list" data-testid="view-list">
                {sheet.views.map((v) => (
                  <li key={v.id} className={v.id === selectedView ? 'dr-selected' : ''} onClick={() => setSelectedView(v.id)}>
                    <span>{v.id}</span> {VIEW_KIND_LABELS[v.kind]}
                    {v.label ? ` ${v.label}` : ''} <span className="dr-muted">{v.computedScale?.label}</span>
                  </li>
                ))}
              </ul>
              {selected && (
                <div className="dr-box" data-testid="view-properties">
                  <div>
                    {selected.id}: {VIEW_KIND_LABELS[selected.kind]}
                    {selected.inGroup ? ' (в проекционной связи)' : ''}
                  </div>
                  {!selected.inGroup && (
                    <label>
                      Масштаб вида
                      <select
                        value={selected.scale ? selected.scale.ratio.join(':') : ''}
                        onChange={(e) =>
                          run('Масштаб вида изменён', (d) =>
                            d.setView(sheet.id, selected.id, { scale: e.target.value ? (e.target.value.split(':').map(Number) as [number, number]) : null }),
                          )
                        }
                      >
                        <option value="">авто ({selected.computedScale?.label})</option>
                        {SCALES.map((s) => (
                          <option key={s.join(':')} value={s.join(':')}>{scaleText(s)}</option>
                        ))}
                      </select>
                    </label>
                  )}
                  <label className="dr-check">
                    <input type="checkbox" checked={selected.hidden} onChange={(e) => run('Невидимые линии', (d) => d.setView(sheet.id, selected.id, { hidden: e.target.checked }))} />
                    невидимые линии
                  </label>
                  {selected.position && (
                    <button onClick={() => run('Положение — автоматически', (d) => d.setView(sheet.id, selected.id, { position: null }))}>Автоматическое положение</button>
                  )}
                  <button
                    onClick={() =>
                      run('Вид удалён', (d) => {
                        d.removeView(sheet.id, selected.id);
                        setSelectedView(null);
                      })
                    }
                  >
                    Удалить вид
                  </button>
                </div>
              )}

              <h2>Инструмент</h2>
              <div className="dr-box" data-testid="tool-options">
                {tool === 'select' && <p className="dr-muted">Щелчок — выбор вида, перетаскивание — перемещение (вид в проекционной связи двигает всю группу), колесо — масштаб, средняя кнопка или перетаскивание листа — панорама.</p>}
                {tool === 'dimension' && (
                  <>
                    <p className="dr-muted">Два ребра вида: расстояние (вертикальные — горизонтальный размер, горизонтальные — вертикальный) или угол; окружность/дуга — выберите и нажмите «Размер» (Enter).</p>
                    <label>
                      Отступ, мм <input value={dimOffset} onChange={(e) => setDimOffset(e.target.value)} size={4} />
                    </label>
                    <button disabled={!canDimensionSingle(pickState)} onClick={() => createFromPicks(pickState)}>Размер</button>
                    <div className="dr-muted" data-testid="picks">Выбрано рёбер: {pickState.picks.length}</div>
                  </>
                )}
                {tool === 'roughness' && (
                  <>
                    <select value={rough.method} onChange={(e) => setRough({ ...rough, method: e.target.value })}>
                      <option value="materialRemoval">с удалением материала</option>
                      <option value="noMaterialRemoval">без удаления материала</option>
                      <option value="unspecified">вид обработки не устанавливается</option>
                    </select>
                    <div className="dr-row">
                      <input value={rough.parameter} onChange={(e) => setRough({ ...rough, parameter: e.target.value })} size={4} />
                      <input value={rough.value} onChange={(e) => setRough({ ...rough, value: e.target.value })} size={6} />
                    </div>
                    <p className="dr-muted">Щёлкните по прямолинейному ребру.</p>
                  </>
                )}
                {tool === 'tolerance' && (
                  <>
                    <select value={tol.characteristic} onChange={(e) => setTol({ ...tol, characteristic: e.target.value as GeometricCharacteristic })}>
                      {CHARACTERISTICS.map(([c, l]) => (
                        <option key={c} value={c}>{l}</option>
                      ))}
                    </select>
                    <label>
                      Допуск <input value={tol.tolerance} onChange={(e) => setTol({ ...tol, tolerance: e.target.value })} size={6} />
                    </label>
                    <label>
                      Базы <input value={tol.datums} onChange={(e) => setTol({ ...tol, datums: e.target.value })} size={6} />
                    </label>
                    <p className="dr-muted">Щёлкните по прямолинейному ребру.</p>
                  </>
                )}
                {tool === 'datum' && (
                  <label>
                    Буква базы <input value={datumLetter} onChange={(e) => setDatumLetter(e.target.value)} size={3} />
                  </label>
                )}
                {tool === 'detail' && (
                  <label>
                    Радиус, мм модели <input value={detailRadius} onChange={(e) => setDetailRadius(e.target.value)} size={4} />
                    <span className="dr-muted"> — щёлкните центр на виде</span>
                  </label>
                )}
                {tool === 'section' && (
                  <label>
                    Положение разреза
                    <select value={sectionSide} onChange={(e) => setSectionSide(e.target.value)}>
                      {SHEET_SIDES.map(([s, l]) => (
                        <option key={s} value={s}>{l}</option>
                      ))}
                      <option value="none">вне проекционной связи</option>
                    </select>
                    <span className="dr-muted"> — щёлкните точку секущей плоскости на виде</span>
                  </label>
                )}
              </div>

              <h2>Аннотации</h2>
              <ul className="dr-list" data-testid="annotation-list">
                {sheet.dimensions.map((d) => (
                  <li key={d.id} className={d.state === 'dangling' ? 'dr-dangling' : ''} data-testid={`dimension-${d.id}`} data-value={d.value}>
                    <span>{d.id}</span> {DIMENSION_TYPE_LABELS[d.type]} <strong>{d.text}</strong>
                    {d.state === 'dangling' && <span className="dr-muted"> висячий: {d.reason}</span>}
                    <input
                      className="dr-tol"
                      placeholder="поле"
                      defaultValue={d.tolerance ?? ''}
                      title="Поле допуска ISO 286 (H7, h6…)"
                      onBlur={(e) => {
                        if ((e.target.value || null) !== d.tolerance) run('Допуск размера изменён', (h) => h.setDimension(sheet.id, d.id, { tolerance: e.target.value || null }));
                      }}
                    />
                    <button title="Удалить" onClick={() => run('Размер удалён', (h) => h.removeAnnotation(sheet.id, d.id))}>×</button>
                  </li>
                ))}
                {[...sheet.roughness.map((r) => ({ ...r, label: `шероховатость ${r.spec.parameter ?? ''} ${r.spec.value ?? ''}` })),
                  ...sheet.tolerances.map((t) => ({ ...t, label: `${CHARACTERISTICS.find((c) => c[0] === t.characteristic)?.[1]} ${t.tolerance} ${t.datums.join(' ')}` })),
                  ...sheet.datums.map((b) => ({ ...b, label: `база ${b.letter}` }))].map((a) => (
                  <li key={a.id} className={a.state === 'dangling' ? 'dr-dangling' : ''}>
                    <span>{a.id}</span> {a.label}
                    <button title="Удалить" onClick={() => run('Знак удалён', (h) => h.removeAnnotation(sheet.id, a.id))}>×</button>
                  </li>
                ))}
              </ul>
              <TechnicalRequirements
                key={`${sheet.id}-${sheet.technicalRequirements.join('|')}`}
                items={sheet.technicalRequirements}
                onApply={(items) => run('Технические требования', (d) => d.setSheet(sheet.id, { technicalRequirements: items }))}
              />
              <label className="dr-check">
                <input
                  type="checkbox"
                  checked={!!sheet.roughnessRemainder}
                  onChange={(e) =>
                    run('Знак «остальное»', (d) => d.setSheet(sheet.id, { roughnessRemainder: e.target.checked ? { value: rough.value, parameter: rough.parameter } : null }))
                  }
                />
                шероховатость остальных поверхностей ({rough.parameter} {rough.value})
              </label>
            </>
          )}
        </aside>

        <section className="dr-center">
          <div className="dr-tabs">
            <button className={tab === 'sheet' ? 'dr-active' : ''} onClick={() => setTab('sheet')}>Лист</button>
            <button className={tab === 'specification' ? 'dr-active' : ''} onClick={() => setTab('specification')} data-testid="tab-specification">
              Спецификация
            </button>
            {sheet && <span className="dr-muted">{sheet.name} · масштаб {sheet.computedScale?.label} · {modelName}</span>}
          </div>
          {tab === 'sheet' && picture && (
            <SheetCanvas
              ref={canvas}
              picture={picture}
              tool={tool}
              picks={pickState.picks}
              selectedView={selectedView}
              onSelectView={setSelectedView}
              onEdgeClick={onEdgeClick}
              onSheetClick={onSheetClick}
              onMoveView={(view, x, y) => run('Вид перемещён', (d) => d.moveView(sheetId!, view, x, y))}
            />
          )}
          {tab === 'sheet' && !picture && <div className="dr-empty">{status === 'loading' ? 'Загрузка…' : 'Нет открытого чертежа'}</div>}
          {tab === 'specification' && (
            <div className="dr-spec" data-testid="specification">
              {spec.table ? (
                <table>
                  <thead>
                    <tr><th>Формат</th><th>Зона</th><th>Поз.</th><th>Обозначение</th><th>Наименование</th><th>Кол.</th><th>Примечание</th></tr>
                  </thead>
                  <tbody>
                    {spec.table.sections.flatMap((s) => [
                      <tr key={`h-${s.section}`} className="dr-spec-section"><td colSpan={7}>{s.title}</td></tr>,
                      ...s.items.map((it, i) => (
                        <tr key={`${s.section}-${i}`}>
                          <td>{it.format}</td><td>{it.zone}</td><td>{it.position || ''}</td><td>{it.designation}</td><td>{it.name}</td><td>{it.quantity ?? ''}</td><td>{it.note}</td>
                        </tr>
                      )),
                    ])}
                  </tbody>
                </table>
              ) : (
                <p className="dr-muted">{spec.reason ?? 'Нет открытого чертежа'}</p>
              )}
              {desc?.model.kind === 'assembly' && sheet && (
                <label className="dr-check">
                  <input type="checkbox" checked={sheet.partsList} onChange={(e) => run('Перечень составных частей', (d) => d.setSheet(sheet.id, { partsList: e.target.checked }))} />
                  перечень составных частей на листе
                </label>
              )}
            </div>
          )}
        </section>

        <aside className="dr-right">
          {sheet && desc && drawing && (
            <>
              <h2>Модель</h2>
              {variables.length > 0 && (
                <table className="dr-vars" data-testid="variables">
                  <tbody>
                    {variables.map((v) => (
                      <tr key={v.name}>
                        <td>{v.name}</td>
                        <td>
                          <input
                            key={`${v.name}=${v.expression}`}
                            defaultValue={v.expression}
                            data-testid={`variable-${v.name}`}
                            onKeyDown={(e) => {
                              if (e.key === 'Enter') (e.target as HTMLInputElement).blur();
                            }}
                            onBlur={(e) => {
                              if (e.target.value !== v.expression) run(`Параметр ${v.name} изменён`, (d) => d.setVariable(v.name, e.target.value));
                            }}
                          />
                        </td>
                        <td className="dr-muted">{v.value ?? v.error}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              )}
              {desc.model.kind === 'part' && (
                <ModelPropertiesForm
                  key={JSON.stringify(desc.model.properties)}
                  properties={desc.model.properties}
                  onApply={(p) =>
                    run('Свойства детали', (d) => {
                      const r = d.setModelProperties(p);
                      setReport(r);
                    })
                  }
                />
              )}

              <h2>Обновление по модели</h2>
              <div className={`dr-box ${staleness?.modelChanged ? 'dr-stale' : ''}`} data-testid="update-panel">
                <div data-testid="model-state">
                  {staleness?.modelChanged
                    ? `Модель изменилась: видов к пересчёту ${staleness.staleViews.length}, размеров ${staleness.dimensions.length}`
                    : 'Чертёж соответствует модели'}
                </div>
                <button data-testid="update-from-model" onClick={onUpdate}>Обновить по модели</button>
                {report && (
                  <div className="dr-report" data-testid="update-report">
                    <div>
                      Ревизия {report.revision}; видов рассчитано {report.viewsComputed}, из кэша {report.viewsFromCache}
                    </div>
                    <div>Изменённые размеры ({report.stale.length}):</div>
                    <ul data-testid="report-stale">
                      {report.stale.map((s) => (
                        <li key={s}>{s}</li>
                      ))}
                    </ul>
                    <div>Висячие ({report.dangling.length}):</div>
                    <ul data-testid="report-dangling">
                      {report.dangling.map((s) => (
                        <li key={s} className="dr-dangling">{s}</li>
                      ))}
                    </ul>
                    {report.warnings.length > 0 && <div className="dr-muted">{report.warnings.join('; ')}</div>}
                  </div>
                )}
              </div>

              <h2>Основная надпись</h2>
              <TitleBlockForm
                key={`${sheet.id}-${JSON.stringify(sheet.title)}`}
                sheet={sheet}
                onApply={(title) => run('Основная надпись изменена', (d) => d.setSheet(sheet.id, { title }))}
              />
            </>
          )}
        </aside>
      </div>

      <footer className={`dr-status${error ? ' dr-error' : ''}`} data-testid="status" role={error ? 'alert' : 'status'}>
        {message}
        {error ? ` — ${error.code}: ${error.message}` : ''}
      </footer>
    </div>
  );
}

function TechnicalRequirements(props: { items: string[]; onApply(items: string[]): void }) {
  const [text, setText] = useState(props.items.join('\n'));
  return (
    <div className="dr-box">
      <div>Технические требования (по пункту в строке)</div>
      <textarea rows={3} value={text} onChange={(e) => setText(e.target.value)} data-testid="technical-requirements" />
      <button onClick={() => props.onApply(text.split('\n').map((s) => s.trim()).filter(Boolean))}>Применить</button>
    </div>
  );
}

function ModelPropertiesForm(props: {
  properties: DrawingDescription['model']['properties'];
  onApply(p: Partial<DrawingDescription['model']['properties']>): void;
}) {
  const [p, setP] = useState({ ...props.properties, density: props.properties.density === null ? '' : String(props.properties.density) });
  return (
    <div className="dr-box dr-form">
      <label>Обозначение <input value={p.designation} onChange={(e) => setP({ ...p, designation: e.target.value })} data-testid="model-designation" /></label>
      <label>Наименование <input value={p.name} onChange={(e) => setP({ ...p, name: e.target.value })} /></label>
      <label>Материал <input value={p.material} onChange={(e) => setP({ ...p, material: e.target.value })} /></label>
      <label>Плотность, кг/м³ <input value={p.density} onChange={(e) => setP({ ...p, density: e.target.value })} size={6} /></label>
      <button onClick={() => props.onApply({ ...p, density: p.density.trim() === '' ? null : Number(p.density) })}>Применить</button>
    </div>
  );
}

const TITLE_ROWS: [keyof Pick<SheetInfo['title'], 'designation' | 'name' | 'material' | 'mass' | 'litera' | 'organization'>, string][] = [
  ['designation', 'Обозначение'],
  ['name', 'Наименование'],
  ['material', 'Материал'],
  ['mass', 'Масса'],
  ['litera', 'Литера'],
  ['organization', 'Организация'],
];
const SIGNATURES: [keyof Pick<SheetInfo['title'], 'developer' | 'checker' | 'techControl' | 'normControl' | 'approver'>, string][] = [
  ['developer', 'Разраб.'],
  ['checker', 'Пров.'],
  ['techControl', 'Т.контр.'],
  ['normControl', 'Н.контр.'],
  ['approver', 'Утв.'],
];

function TitleBlockForm(props: { sheet: SheetInfo; onApply(title: SheetInfo['title']): void }) {
  const [t, setT] = useState(props.sheet.title);
  const shown = props.sheet.titleShown;
  return (
    <div className="dr-box dr-form" data-testid="title-block">
      {TITLE_ROWS.map(([k, l]) => (
        <label key={k}>
          {l}
          <input
            value={t[k]}
            placeholder={shown && k in shown ? String((shown as Record<string, unknown>)[k] ?? '') : ''}
            onChange={(e) => setT({ ...t, [k]: e.target.value })}
            data-testid={`title-${k}`}
          />
        </label>
      ))}
      {SIGNATURES.map(([k, l]) => (
        <label key={k}>
          {l}
          <input value={t[k].surname} placeholder="фамилия" onChange={(e) => setT({ ...t, [k]: { ...t[k], surname: e.target.value } })} data-testid={`title-${k}`} />
          <input value={t[k].date} placeholder="дата" size={5} onChange={(e) => setT({ ...t, [k]: { ...t[k], date: e.target.value } })} />
        </label>
      ))}
      <button onClick={() => props.onApply(t)} data-testid="title-apply">Применить</button>
    </div>
  );
}
