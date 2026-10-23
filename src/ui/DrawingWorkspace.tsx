/**
 * Чертёжная среда (7): лист с видами, развёрткой и таблицами по данным ядра (drawing.part),
 * переключение стандарта, формата, набора видов, режима размеров и конца подачи; выпуск (7.7):
 * проверки перед выпуском, «Не для производства», выпущенный чертёж только для чтения,
 * снять выпуск — роль «Выпускающий» или администратор.
 *
 * Ассоциативные виды: история версий, откат, сравнение версий.
 * 2D Sketches (v7.9): SketchEditor интеграция для добавления аннотаций на чертежи.
 * Слои и видимость (Q2 2027, Task 3): LayersPanel для управления видимостью слоёв.
 */
import React, { useEffect, useMemo, useState } from 'react';
import { CoreClient } from './core/coreClient';
import { drawingToSvg, STANDARD_NAME } from './core/drawingSvg';
import { t } from './core/i18n';
import type { AssociativeDrawing, AssociativityStatus, DrawingData, DrawingVersion, Feed, PathItem, ReleaseCheckResult, Sketch2D, SketchElement, SketchDimension, LayerVisibility } from './core/types';
import DrawingVersionPanel from './DrawingVersionPanel';
import SketchEditor from './SketchEditor';
import LayersPanel from './LayersPanel';
import CommentsPanel, { type Comment } from './CommentsPanel';
import './panels.css';

export interface DrawingTube {
  path: PathItem[];
  od: number;
  wall?: number;
  title: Record<string, string>;
  unfold?: Record<string, unknown>;
  /** Технологические или геометрические ошибки трубы (4.5). */
  hasErrors?: boolean;
}

export interface DrawingWorkspaceProps {
  tube: DrawingTube;
  client?: CoreClient;
  /** Роль пользователя: снять выпуск может «Выпускающий» или «Администратор». */
  role?: string;
  user?: string;
  strictness?: 'strict' | 'soft';
  /** Сколько видов устарело и сколько ссылок в размерах потеряно (из модели документа). */
  staleViews?: number;
  lostRefs?: number;
}

type Standard = DrawingData['standard'];

const FORMATS: Record<Standard, string[]> = {
  GOST: ['A4', 'A3', 'A2', 'A1', 'A0'],
  ISO: ['A4', 'A3', 'A2', 'A1', 'A0'],
  DIN: ['A4', 'A3', 'A2', 'A1', 'A0'],
  ANSI: ['ANSI A', 'ANSI B', 'ANSI C', 'ANSI D', 'ANSI E'],
};

export const DrawingWorkspace: React.FC<DrawingWorkspaceProps> = ({
  tube,
  client,
  role = '',
  user = '',
  strictness = 'strict',
  staleViews = 0,
  lostRefs = 0,
}) => {
  const core = useMemo(() => client ?? new CoreClient(), [client]);
  const [standard, setStandard] = useState<Standard>('GOST');
  const [format, setFormat] = useState('A3');
  const [preset, setPreset] = useState<'minimal' | 'full'>('minimal');
  const [dims, setDims] = useState<'S' | 'L' | 'table'>('S');
  const [feed, setFeed] = useState<Feed>('A');
  const [sheets, setSheets] = useState<DrawingData[]>([]);
  const [active, setActive] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [released, setReleased] = useState<{ by: string; date: string } | null>(null);
  const [check, setCheck] = useState<ReleaseCheckResult | null>(null);

  // Ассоциативные виды
  const [associativeDrawing, setAssociativeDrawing] = useState<AssociativeDrawing | null>(null);
  const [associativityStatus, setAssociativityStatus] = useState<AssociativityStatus>('not_applicable');

  // 2D Sketches (v7.9)
  const [sketches, setSketches] = useState<Sketch2D[]>([]);
  const [showSketchEditor, setShowSketchEditor] = useState(false);
  const [editingSketch, setEditingSketch] = useState<Sketch2D | null>(null);
  const [sketchError, setSketchError] = useState<string | null>(null);

  // Слои и видимость (Q2 2027, Task 3)
  const [layerVisibility, setLayerVisibility] = useState<LayerVisibility>({ layer_visibility: {} });
  const [showLayersPanel, setShowLayersPanel] = useState(true);

  // Комментарии и замечания (Q2 2027, Task 4)
  const [comments, setComments] = useState<Comment[]>([]);
  const [showCommentsPanel, setShowCommentsPanel] = useState(true);

  useEffect(() => {
    let alive = true;
    core
      .drawingPart({
        path: tube.path,
        od: tube.od,
        wall: tube.wall ?? 0,
        standard,
        format,
        preset,
        dims,
        title: tube.title,
        has_errors: !!tube.hasErrors,
        unfold: { ...(tube.unfold ?? {}), feed },
      })
      .then((r) => {
        if (!alive) return;
        setSheets(r.sheets);
        setActive((a) => Math.min(a, r.sheets.length - 1));
        setError(null);
      })
      .catch((e: Error) => alive && setError(e.message));
    return () => {
      alive = false;
    };
  }, [core, tube, standard, format, preset, dims, feed]);

  // Загрузить ассоциативный чертёж (версионирование)
  useEffect(() => {
    let alive = true;
    const loadAssociative = async () => {
      try {
        const assoc = await core.makeAssociativeDrawing({
          path: tube.path,
          od: tube.od,
          wall: tube.wall ?? 0,
          standard,
          format,
          preset,
          dims,
          title: tube.title,
          has_errors: !!tube.hasErrors,
          unfold: { ...(tube.unfold ?? {}), feed },
          precision: 1,
        });
        if (alive) {
          setAssociativeDrawing(assoc);
          setAssociativityStatus(assoc.state.status);
        }
      } catch (e) {
        if (alive) {
          console.warn('Ошибка загрузки ассоциативного чертежа:', e);
          setAssociativityStatus('broken');
        }
      }
    };
    loadAssociative();
    return () => {
      alive = false;
    };
  }, [core, tube, standard, format, preset, dims, feed]);

  const sheet = sheets[active];
  const svg = useMemo(
    () => (sheet ? drawingToSvg(sheet, { stamp: released ? { released: true, ...released } : { released: false } }) : ''),
    [sheet, released],
  );
  const readOnly = released !== null;

  const askRelease = async () => {
    try {
      setCheck(
        await core.releaseCheck({
          tech_or_geom_errors: !!tube.hasErrors,
          lost_dim_refs: lostRefs,
          stale_views: staleViews,
          strictness,
          role,
        }),
      );
    } catch (e) {
      setError((e as Error).message);
    }
  };
  const doRelease = () => {
    setReleased({ by: user, date: new Date().toISOString().slice(0, 10) });
    setCheck(null);
  };
  const canUnrelease = role === 'Выпускающий' || role === 'Администратор';

  const changeStandard = (s: Standard) => {
    setStandard(s);
    if (!FORMATS[s].includes(format)) setFormat(s === 'ANSI' ? 'ANSI B' : 'A3');
  };

  // Handlers for sketches
  const handleAddSketch = () => {
    setEditingSketch(null);
    setShowSketchEditor(true);
    setSketchError(null);
  };

  const handleSketchSave = async (elements: SketchElement[], dimensions: SketchDimension[]) => {
    try {
      const sketchName = `Sketch_${active + 1}_${Date.now()}`;
      const result = await core.addSketch({
        sheet_number: active + 1,
        name: sketchName,
        elements,
        dimensions,
        author: user,
      });

      if (result.valid) {
        const newSketch: Sketch2D = {
          id: `sketch_${Date.now()}`,
          sheet_number: active + 1,
          name: sketchName,
          elements,
          dimensions,
          visible: true,
          locked: false,
          created_by: user,
          created_at: new Date().toISOString(),
          modified_at: new Date().toISOString(),
        };
        setSketches((prev) => [...prev, newSketch]);
        setShowSketchEditor(false);
        setError(null);
      } else {
        setSketchError(result.errors?.join('; ') ?? 'Sketch validation failed');
      }
    } catch (e) {
      setSketchError((e as Error).message);
    }
  };

  const handleSketchCancel = () => {
    setShowSketchEditor(false);
    setEditingSketch(null);
    setSketchError(null);
  };

  // Слои и видимость (Q2 2027, Task 3)
  const handleToggleLayer = (layerType: number, visible: boolean) => {
    setLayerVisibility((prev) => ({
      layer_visibility: {
        ...prev.layer_visibility,
        [layerType]: visible,
      },
    }));
    // localStorage сохранение (Task 3.9)
    if (sheet) {
      try {
        const key = `bendcore:drawing:layers:${sheet.number}`;
        localStorage.setItem(key, JSON.stringify(layerVisibility));
      } catch (e) {
        console.warn('Failed to save layer visibility to localStorage:', e);
      }
    }
  };

  const handleShowAllLayers = () => {
    const allVisible: Record<number, boolean> = {};
    for (let i = 0; i <= 4; i++) {
      allVisible[i] = true;
    }
    setLayerVisibility({ layer_visibility: allVisible });
  };

  const handleHideAllLayers = () => {
    const allHidden: Record<number, boolean> = {};
    for (let i = 0; i <= 4; i++) {
      allHidden[i] = false;
    }
    setLayerVisibility({ layer_visibility: allHidden });
  };

  // Комментарии (Q2 2027, Task 4)
  const handleAddComment = async (sheet: number, x: number, y: number, text: string) => {
    try {
      const result = await core.call('drawing.add_comment', {
        sheet_id: sheet,
        x,
        y,
        text,
        author: user || 'anonymous',
      });
      if (result.ok && result.data?.id) {
        const newComment: Comment = {
          id: result.data.id,
          sheet_number: sheet,
          position: [x, y],
          text,
          author: user || 'anonymous',
          created_at: new Date().toISOString(),
          modified_at: new Date().toISOString(),
          status: 'open',
          replies: [],
        };
        setComments((prev) => [...prev, newComment]);
      }
    } catch (e) {
      setError((e as Error).message);
    }
  };

  const handleReplyToComment = (commentId: string, text: string) => {
    setComments((prev) =>
      prev.map((c) =>
        c.id === commentId
          ? {
              ...c,
              replies: [
                ...c.replies,
                {
                  id: `reply_${Date.now()}`,
                  text,
                  author: user || 'anonymous',
                  timestamp: new Date().toISOString(),
                },
              ],
              modified_at: new Date().toISOString(),
            }
          : c
      )
    );
  };

  const handleChangeCommentStatus = (commentId: string, status: string) => {
    setComments((prev) =>
      prev.map((c) =>
        c.id === commentId
          ? {
              ...c,
              status: status as 'open' | 'resolved' | 'needs_rework' | 'archived',
              modified_at: new Date().toISOString(),
            }
          : c
      )
    );
  };

  const handleDeleteComment = (commentId: string) => {
    setComments((prev) => prev.filter((c) => c.id !== commentId));
  };

  // Загрузить состояние видимости из localStorage при изменении листа
  useEffect(() => {
    if (sheet) {
      try {
        const key = `bendcore:drawing:layers:${sheet.number}`;
        const saved = localStorage.getItem(key);
        if (saved) {
          setLayerVisibility(JSON.parse(saved));
        } else {
          // По умолчанию все слои видимы
          setLayerVisibility({
            layer_visibility: {
              0: true,
              1: true,
              2: true,
              3: true,
              4: true,
            },
          });
        }
      } catch (e) {
        console.warn('Failed to load layer visibility from localStorage:', e);
      }
    }
  }, [sheet?.number]);

  return (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 280px', height: '100%', gap: '8px', padding: '8px' }}>
      <section className="bc-panel" aria-label={t('draw.title')} style={{ height: '100%' }}>
        <div className="bc-header" style={{ flexWrap: 'wrap' }}>
          <h2>
            {t('draw.title')} {tube.title['Обозначение'] ?? ''}
            {readOnly && (
              <span className="bc-badge released" style={{ marginLeft: 8 }} title={t('draw.readOnly')}>
                {t('draw.released')} {released.date}
              </span>
            )}
          </h2>
        <label className="bc-field">
          {t('draw.standard')}
          <select value={standard} disabled={readOnly} onChange={(e) => changeStandard(e.target.value as Standard)}>
            {(Object.keys(STANDARD_NAME) as Standard[]).map((s) => (
              <option key={s} value={s}>
                {STANDARD_NAME[s]}
              </option>
            ))}
          </select>
        </label>
        <label className="bc-field">
          {t('draw.format')}
          <select value={format} disabled={readOnly} onChange={(e) => setFormat(e.target.value)}>
            {FORMATS[standard].map((f) => (
              <option key={f}>{f}</option>
            ))}
          </select>
        </label>
        <label className="bc-field">
          {t('draw.preset')}
          <select value={preset} disabled={readOnly} onChange={(e) => setPreset(e.target.value as 'minimal' | 'full')}>
            <option value="minimal">{t('draw.minimal')}</option>
            <option value="full">{t('draw.full')}</option>
          </select>
        </label>
        <label className="bc-field">
          {t('draw.dims')}
          <select value={dims} disabled={readOnly} onChange={(e) => setDims(e.target.value as 'S' | 'L' | 'table')}>
            <option value="S">{t('draw.dimsS')}</option>
            <option value="L">{t('draw.dimsL')}</option>
            <option value="table">{t('draw.dimsTable')}</option>
          </select>
        </label>
        <div className="bc-field">
          {t('draw.feed')}
          <div className="bc-seg">
            {(['A', 'B'] as Feed[]).map((x) => (
              <button key={x} aria-pressed={feed === x} disabled={readOnly} onClick={() => setFeed(x)}>
                {x === 'A' ? 'А' : 'Б'}
              </button>
            ))}
          </div>
        </div>
        {!readOnly && (
          <button className="bc-btn" onClick={handleAddSketch} disabled={!sheet}>
            + {t('sketch.add') ?? 'Add Sketch'}
          </button>
        )}
        {readOnly ? (
          <button
            className="bc-btn"
            disabled={!canUnrelease}
            title={canUnrelease ? '' : 'Выпускающий или администратор'}
            onClick={() => setReleased(null)}
          >
            {t('draw.unrelease')}
          </button>
        ) : (
          <button className="bc-btn primary" onClick={askRelease} disabled={!sheet}>
            {t('draw.release')}
          </button>
        )}
      </div>
      {error && <div className="bc-banner error" style={{ margin: 12 }}>{error}</div>}
      {sheet && sheet.warnings.length > 0 && (
        <div className="bc-banner warn" style={{ margin: '12px 12px 0' }}>
          {sheet.warnings.join('; ')}
        </div>
      )}
      {sheets.length > 1 && (
        <div className="bc-tabs" role="tablist" style={{ padding: '8px 12px 0' }}>
          {sheets.map((s, i) => (
            <button key={s.number} role="tab" aria-selected={i === active} onClick={() => setActive(i)}>
              Лист {s.number}
            </button>
          ))}
        </div>
      )}
      <div className="bc-sheet-host" dangerouslySetInnerHTML={{ __html: svg }} />

      {check && (
        <div className="bc-modal-backdrop" role="dialog" aria-modal="true" aria-label={t('draw.release')}>
          <div className="bc-panel bc-modal" style={{ width: 'min(520px, calc(100vw - 32px))' }}>
            <div className="bc-header">
              <h2>{t('draw.release')}</h2>
            </div>
            <div className="bc-body">
              {check.not_for_production && <div className="bc-banner error">{t('draw.notForProduction')}</div>}
              {check.blocking.length > 0 && (
                <div className="bc-banner error">
                  <strong>{t('draw.blocking')}:</strong>
                  <ul style={{ margin: '4px 0 0 18px', padding: 0 }}>
                    {check.blocking.map((b) => (
                      <li key={b}>{b}</li>
                    ))}
                  </ul>
                </div>
              )}
              {check.warnings.length > 0 && (
                <div className="bc-banner warn">
                  <strong>{t('draw.warnings')}:</strong>
                  <ul style={{ margin: '4px 0 0 18px', padding: 0 }}>
                    {check.warnings.map((w) => (
                      <li key={w}>{w}</li>
                    ))}
                  </ul>
                </div>
              )}
              {check.can_release && check.warnings.length === 0 && (
                <p className="bc-note">После выпуска чертёж станет только для чтения.</p>
              )}
            </div>
            <div className="bc-footer">
              <button className="bc-btn" onClick={() => setCheck(null)}>
                {t('common.cancel')}
              </button>
              {check.can_release && (
                <button className="bc-btn primary" onClick={doRelease}>
                  {check.offer_update_and_release ? t('draw.updateAndRelease') : t('draw.release')}
                </button>
              )}
            </div>
          </div>
        </div>
      )}
      </section>

      <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', width: '280px', height: '100%' }}>
        {showLayersPanel && sheet && (
          <LayersPanel
            layers={sheet.layers}
            layerVisibility={layerVisibility}
            onToggleLayer={handleToggleLayer}
            onShowAll={handleShowAllLayers}
            onHideAll={handleHideAllLayers}
            client={core}
            darkMode={false}
          />
        )}

        {associativeDrawing && (
          <DrawingVersionPanel
            tubeId={tube.id ?? 'unknown'}
            currentRevision={associativeDrawing.versions[0]?.revision ?? 1}
            versions={associativeDrawing.versions}
            status={associativityStatus}
            onRestore={async (revision) => {
              console.log('Откат к версии', revision);
            }}
            onCompare={async (rev1, rev2) => {
              console.log('Сравнение версий', rev1, 'и', rev2);
            }}
          />
        )}
      </div>

      {/* Sketch Editor Modal */}
      {showSketchEditor && (
        <div
          className="bc-modal-backdrop"
          role="dialog"
          aria-modal="true"
          aria-label="Sketch Editor"
          style={{ position: 'fixed', top: 0, left: 0, right: 0, bottom: 0, backgroundColor: 'rgba(0,0,0,0.5)', zIndex: 2000 }}
        >
          <div
            className="bc-panel bc-modal"
            style={{
              width: 'min(1000px, calc(100vw - 32px))',
              height: 'min(700px, calc(100vh - 32px))',
              margin: 'auto',
              display: 'flex',
              flexDirection: 'column',
            }}
          >
            {sketchError && <div className="bc-banner error" style={{ margin: 12 }}>{sketchError}</div>}
            <SketchEditor
              sheetNumber={active + 1}
              sketchName={editingSketch?.name ?? `Sketch_${active + 1}`}
              initialElements={editingSketch?.elements}
              initialDimensions={editingSketch?.dimensions}
              onSave={handleSketchSave}
              onCancel={handleSketchCancel}
              isDarkMode={false}
            />
          </div>
        </div>
      )}

      {/* Sketch List Panel */}
      {sketches.length > 0 && (
        <div
          className="bc-panel"
          style={{
            position: 'absolute',
            right: 300,
            top: 100,
            width: 200,
            maxHeight: 300,
            overflowY: 'auto',
            zIndex: 1000,
          }}
        >
          <div className="bc-header">
            <h4 style={{ margin: 0 }}>Sketches ({sketches.length})</h4>
          </div>
          <div style={{ padding: '8px' }}>
            {sketches
              .filter((s) => s.sheet_number === active + 1)
              .map((sketch) => (
                <div
                  key={sketch.id}
                  style={{
                    padding: '6px',
                    marginBottom: '4px',
                    backgroundColor: '#f5f5f5',
                    borderRadius: '4px',
                    fontSize: '12px',
                    cursor: 'pointer',
                  }}
                  onClick={() => {
                    setEditingSketch(sketch);
                    setShowSketchEditor(true);
                  }}
                >
                  {sketch.name} ({sketch.elements.length + sketch.dimensions.length} items)
                </div>
              ))}
          </div>
        </div>
      )}
    </div>
  );
};

export default DrawingWorkspace;
