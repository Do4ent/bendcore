/**
 * Sheet of the drawing editor (docs/ui/DRAWING_UI.md): the kernel's SVG of the sheet under an
 * overlay of the view edges (sheetPicture) for picking, with zoom (wheel, around the cursor) and
 * pan (middle button, or drag on the free sheet in the select tool). In the select tool a view is
 * selected by a click and moved by dragging: the ghost of the view (of the whole projection group
 * for a view in projection relation) follows the pointer, and on release the editor calls
 * moveView() of the kernel.
 */

import React, { useCallback, useEffect, useImperativeHandle, useMemo, useRef, useState } from 'react';
import type { PictureView, Point, SheetPicture } from '../../wasm/bendcore_drawing_api.mjs';
import {
  EdgePick,
  SheetTransform,
  Tool,
  curvePath,
  fitTransform,
  panBy,
  screenToSheet,
  viewAt,
  zoomAt,
} from './sheetGeometry';

export interface SheetCanvasHandle {
  fit(): void;
  transform(): SheetTransform | null;
}

interface Props {
  picture: SheetPicture;
  tool: Tool;
  picks: EdgePick[];
  selectedView: string | null;
  onSelectView(view: string | null): void;
  onEdgeClick(pick: EdgePick): void;
  /** Click on the sheet in the detail and section tools: the sheet point and the view under it. */
  onSheetClick(point: Point, view: PictureView | null): void;
  /** Drag of a view: the new centre of the dragged view's bounds, sheet mm. */
  onMoveView(view: string, x: number, y: number): void;
}

interface Drag {
  kind: 'pan' | 'view';
  view?: string;
  start: Point; // screen px
  startSheet: Point;
  moved: boolean;
  pointerId: number;
}

function kernelSvgInner(svg: string): string {
  // The writer's root element has width/height in mm: the sheet box sets the size on screen.
  return svg
    .replace(/^<\?xml[^>]*>\s*/, '')
    .replace(/<svg([^>]*?)\swidth="[^"]*"\sheight="[^"]*"/, '<svg$1 width="100%" height="100%" preserveAspectRatio="none"');
}

const SheetCanvas = React.forwardRef<SheetCanvasHandle, Props>(function SheetCanvas(props, ref) {
  const { picture, tool, picks, selectedView } = props;
  const box = useRef<HTMLDivElement>(null);
  const [t, setT] = useState<SheetTransform | null>(null);
  const [drag, setDrag] = useState<Drag | null>(null);
  const [ghost, setGhost] = useState<{ views: string[]; dx: number; dy: number } | null>(null);
  const W = picture.width;
  const H = picture.height;

  const fit = useCallback(() => {
    const el = box.current;
    if (!el) return;
    setT(fitTransform(W, H, el.clientWidth, el.clientHeight));
  }, [W, H]);

  useImperativeHandle(ref, () => ({ fit, transform: () => t }), [fit, t]);

  // Fit when the sheet size changes (first picture, other format).
  useEffect(() => {
    fit();
  }, [fit]);

  // Wheel zoom must be a non-passive listener to prevent the page scroll.
  useEffect(() => {
    const el = box.current;
    if (!el) return undefined;
    const onWheel = (e: WheelEvent) => {
      e.preventDefault();
      const r = el.getBoundingClientRect();
      setT((cur) => (cur ? zoomAt(cur, Math.exp(-e.deltaY * 0.0015), e.clientX - r.left, e.clientY - r.top) : cur));
    };
    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  }, []);

  const svgInner = useMemo(() => ({ __html: kernelSvgInner(picture.svg) }), [picture.svg]);

  const local = (e: React.PointerEvent): Point => {
    const r = box.current!.getBoundingClientRect();
    return [e.clientX - r.left, e.clientY - r.top];
  };

  const groupOf = (view: string): string[] => {
    const v = picture.views.find((x) => x.id === view);
    if (!v) return [];
    return v.inGroup ? picture.views.filter((x) => x.inGroup).map((x) => x.id) : [view];
  };

  const onPointerDown = (e: React.PointerEvent) => {
    if (!t) return;
    const s = local(e);
    const p = screenToSheet(t, H, s);
    const target = e.target as Element;
    const viewId = target.getAttribute('data-view-box');
    if (e.button === 1 || (e.button === 0 && tool === 'select' && !viewId)) {
      e.preventDefault();
      box.current!.setPointerCapture(e.pointerId);
      setDrag({ kind: 'pan', start: s, startSheet: p, moved: false, pointerId: e.pointerId });
      return;
    }
    if (e.button !== 0) return;
    if (tool === 'select' && viewId) {
      box.current!.setPointerCapture(e.pointerId);
      props.onSelectView(viewId);
      setDrag({ kind: 'view', view: viewId, start: s, startSheet: p, moved: false, pointerId: e.pointerId });
      return;
    }
    if (tool === 'detail' || tool === 'section') props.onSheetClick(p, viewAt(picture.views, p));
  };

  const onPointerMove = (e: React.PointerEvent) => {
    if (!drag || !t || e.pointerId !== drag.pointerId) return;
    const s = local(e);
    const moved = drag.moved || Math.hypot(s[0] - drag.start[0], s[1] - drag.start[1]) > 3;
    if (drag.kind === 'pan') {
      setT(panBy(t, s[0] - drag.start[0], s[1] - drag.start[1]));
      setDrag({ ...drag, start: s, moved });
      return;
    }
    const p = screenToSheet(t, H, s);
    if (moved) setGhost({ views: groupOf(drag.view!), dx: p[0] - drag.startSheet[0], dy: p[1] - drag.startSheet[1] });
    if (moved !== drag.moved) setDrag({ ...drag, moved });
  };

  const onPointerUp = (e: React.PointerEvent) => {
    if (!drag || e.pointerId !== drag.pointerId) return;
    box.current!.releasePointerCapture(e.pointerId);
    if (drag.kind === 'view' && drag.moved && t) {
      const p = screenToSheet(t, H, local(e));
      const v = picture.views.find((x) => x.id === drag.view);
      if (v) {
        const cx = (v.bounds.minX + v.bounds.maxX) / 2 + p[0] - drag.startSheet[0];
        const cy = (v.bounds.minY + v.bounds.maxY) / 2 + p[1] - drag.startSheet[1];
        props.onMoveView(v.id, cx, cy);
      }
    } else if (drag.kind === 'pan' && !drag.moved && tool === 'select' && e.button === 0) {
      props.onSelectView(null);
    }
    setGhost(null);
    setDrag(null);
  };

  const pickTool = tool === 'dimension' || tool === 'roughness' || tool === 'tolerance' || tool === 'datum';
  const isPicked = (view: string, index: number) => picks.some((p) => p.view === view && p.index === index);

  return (
    <div
      ref={box}
      className={`dr-canvas dr-tool-${tool}${drag?.kind === 'pan' ? ' dr-panning' : ''}`}
      data-testid="sheet-canvas"
      onPointerDown={onPointerDown}
      onPointerMove={onPointerMove}
      onPointerUp={onPointerUp}
      onAuxClick={(e) => e.preventDefault()}
    >
      {t && (
        <div
          className="dr-sheet"
          data-testid="sheet"
          data-zoom={t.zoom}
          style={{ left: t.panX, top: t.panY, width: W * t.zoom, height: H * t.zoom }}
        >
          <div className="dr-sheet-svg" data-testid="sheet-svg" dangerouslySetInnerHTML={svgInner} />
          <svg className="dr-overlay" viewBox={`0 0 ${W} ${H}`} width="100%" height="100%" preserveAspectRatio="none">
            <g transform={`matrix(1 0 0 -1 0 ${H})`}>
              {picture.views.map((v) => (
                <rect
                  key={`box-${v.id}`}
                  data-testid={`view-${v.id}`}
                  data-view-box={v.id}
                  className={`dr-view-box${selectedView === v.id ? ' dr-selected' : ''}`}
                  x={v.bounds.minX - 2}
                  y={v.bounds.minY - 2}
                  width={v.bounds.maxX - v.bounds.minX + 4}
                  height={v.bounds.maxY - v.bounds.minY + 4}
                />
              ))}
              {ghost &&
                picture.views
                  .filter((v) => ghost.views.includes(v.id))
                  .map((v) => (
                    <g key={`ghost-${v.id}`} className="dr-ghost" transform={`translate(${ghost.dx} ${ghost.dy})`}>
                      {v.edges.filter((e) => e.visible).map((e) => (
                        <path key={e.index} d={curvePath(e)} />
                      ))}
                    </g>
                  ))}
              {picture.views.map((v) => (
                <g key={`edges-${v.id}`} className={pickTool ? 'dr-edges dr-pickable' : 'dr-edges'}>
                  {v.edges.map((e) => (
                    <path
                      key={e.index}
                      d={curvePath(e)}
                      data-testid={`edge-${v.id}-${e.index}`}
                      data-view={v.id}
                      data-kind={e.type}
                      data-visible={e.visible ? '1' : '0'}
                      data-points={e.type === 'line' || e.type === 'polyline' ? JSON.stringify(e.points) : undefined}
                      className={`dr-edge${isPicked(v.id, e.index) ? ' dr-picked' : ''}${e.visible ? '' : ' dr-hidden-edge'}`}
                      onPointerDown={
                        pickTool
                          ? (ev) => {
                              if (ev.button !== 0) return;
                              ev.stopPropagation();
                              props.onEdgeClick({ view: v.id, index: e.index, type: e.type });
                            }
                          : undefined
                      }
                    />
                  ))}
                </g>
              ))}
            </g>
          </svg>
        </div>
      )}
    </div>
  );
});

export default SheetCanvas;
