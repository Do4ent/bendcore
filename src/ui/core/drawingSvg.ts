/**
 * Отрисовка листа чертежа (ответ drawing.part) в SVG.
 * Координаты ядра — мм листа с осью Y вверх; в SVG ось Y вниз, поэтому y → H − y.
 * Чистая функция без DOM: используется и в интерфейсе, и для печати/экспорта.
 */
import type { DrawingData, DrawingTable, Geometry, LineStyle, Vec2 } from './types';

export interface ReleaseStamp {
  released: boolean;
  by?: string;
  date?: string;
}

interface Ctx {
  H: number;
  out: string[];
  italic: boolean;
}

const STYLE: Record<LineStyle, { w: number; dash?: string }> = {
  contour: { w: 0.5 },
  thin: { w: 0.18 },
  axis: { w: 0.18, dash: '8 2 1.5 2' },
  cut: { w: 0.5, dash: '8 2 1.5 2' },
  hidden: { w: 0.25, dash: '3 1.5' },
};

/** Названия стандартов документа (6.11). */
export const STANDARD_NAME: Record<DrawingData['standard'], string> = {
  GOST: 'ГОСТ (ЕСКД)',
  ISO: 'ISO',
  DIN: 'DIN',
  ANSI: 'ANSI',
};

export function escapeXml(s: string): string {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

const n = (v: number) => (Math.round(v * 1000) / 1000).toString();

function line(ctx: Ctx, pts: Vec2[], style: LineStyle, closed = false): void {
  const s = STYLE[style];
  const d = pts.map((p, i) => `${i ? 'L' : 'M'}${n(p[0])} ${n(ctx.H - p[1])}`).join(' ') + (closed ? ' Z' : '');
  ctx.out.push(
    `<path d="${d}" fill="none" stroke="currentColor" stroke-width="${s.w}"${s.dash ? ` stroke-dasharray="${s.dash}"` : ''} stroke-linecap="round" stroke-linejoin="round"/>`,
  );
}

function text(
  ctx: Ctx,
  p: Vec2,
  value: string,
  h: number,
  anchor: 'start' | 'middle' | 'end' = 'middle',
  angleDeg = 0,
  extra = '',
): void {
  const size = h / 0.7;  // высота прописной ≈ 0,7 кегля
  const x = n(p[0]);
  const y = n(ctx.H - p[1]);
  const rot = angleDeg ? ` transform="rotate(${n(-angleDeg)} ${x} ${y})"` : '';
  ctx.out.push(
    `<text x="${x}" y="${y}" font-size="${n(size)}" text-anchor="${anchor}"${ctx.italic ? ' font-style="italic"' : ''}${rot}${extra}>${escapeXml(value)}</text>`,
  );
}

function rect(ctx: Ctx, x0: number, y0: number, x1: number, y1: number, w = 0.5): void {
  line(ctx, [[x0, y0], [x1, y0], [x1, y1], [x0, y1]], w >= 0.5 ? 'contour' : 'thin', true);
}

function arrow(ctx: Ctx, tip: Vec2, dir: Vec2): void {
  // Стрелка длиной 2,5 мм с углом ~20° (ГОСТ 2.307).
  const L = 2.5;
  const W = 0.45;
  const b: Vec2 = [tip[0] - dir[0] * L, tip[1] - dir[1] * L];
  const nrm: Vec2 = [-dir[1], dir[0]];
  const p1: Vec2 = [b[0] + nrm[0] * W, b[1] + nrm[1] * W];
  const p2: Vec2 = [b[0] - nrm[0] * W, b[1] - nrm[1] * W];
  ctx.out.push(
    `<path d="M${n(tip[0])} ${n(ctx.H - tip[1])} L${n(p1[0])} ${n(ctx.H - p1[1])} L${n(p2[0])} ${n(ctx.H - p2[1])} Z" fill="currentColor" stroke="none"/>`,
  );
}

function renderGeometry(ctx: Ctx, g: Geometry, origin: Vec2, scale: number): void {
  const P = (p: Vec2): Vec2 => [origin[0] + scale * p[0], origin[1] + scale * p[1]];
  for (const h of g.hatches) {
    const pts = h.outline.map(P);
    const d = pts.map((p, i) => `${i ? 'L' : 'M'}${n(p[0])} ${n(ctx.H - p[1])}`).join(' ') + ' Z';
    ctx.out.push(`<path d="${d}" fill="url(#hatch45)" stroke="none"/>`);
  }
  for (const l of g.lines) line(ctx, l.pts.map(P), l.style, l.closed);
  for (const c of g.circles) {
    const p = P(c.c);
    const s = STYLE[c.style];
    ctx.out.push(
      `<circle cx="${n(p[0])}" cy="${n(ctx.H - p[1])}" r="${n(c.r * scale)}" fill="none" stroke="currentColor" stroke-width="${s.w}"/>`,
    );
  }
  for (const d of g.dims) {
    const a = P(d.a);
    const b = P(d.b);
    const len = Math.hypot(b[0] - a[0], b[1] - a[1]);
    if (len < 1e-6) continue;
    const u: Vec2 = [(b[0] - a[0]) / len, (b[1] - a[1]) / len];
    const nl: Vec2 = [-u[1], u[0]];
    const off = d.offset + d.model_offset * scale;
    const da: Vec2 = [a[0] + nl[0] * off, a[1] + nl[1] * off];
    const db: Vec2 = [b[0] + nl[0] * off, b[1] + nl[1] * off];
    // Выносные линии: от точки с зазором 1 мм до 2 мм за размерную линию.
    const ext = (p: Vec2): Vec2[] => [
      [p[0] + nl[0] * 1, p[1] + nl[1] * 1],
      [p[0] + nl[0] * (off + 2), p[1] + nl[1] * (off + 2)],
    ];
    line(ctx, ext(a), 'thin');
    line(ctx, ext(b), 'thin');
    line(ctx, [da, db], 'thin');
    arrow(ctx, da, [-u[0], -u[1]]);
    arrow(ctx, db, u);
    // Текст над размерной линией (со стороны выноса) и читаемый: не вверх ногами.
    let ang = (Math.atan2(u[1], u[0]) * 180) / Math.PI;
    let lift = 1;  // базовая линия текста от размерной линии, мм
    if (ang > 90 || ang <= -90) {
      ang += ang > 0 ? -180 : 180;
      lift = 1 + 2.5;  // при развороте глифы растут к линии — поднимаем базу на высоту текста
    }
    const tpos: Vec2 = [(da[0] + db[0]) / 2 + nl[0] * lift, (da[1] + db[1]) / 2 + nl[1] * lift];
    text(ctx, tpos, d.reference ? `${d.text}*` : d.text, 2.5, 'middle', ang);
  }
  for (const t of g.texts) {
    const p = P(t.p);
    const q: Vec2 = [p[0] + t.shift[0], p[1] + t.shift[1]];
    if (t.frame) {
      const w = Math.max(t.h * 1.2, t.text.length * t.h * 0.75 + 1.5);
      const hh = t.h + 2;
      ctx.out.push(
        `<rect x="${n(q[0] - w / 2)}" y="${n(ctx.H - q[1] - hh / 2)}" width="${n(w)}" height="${n(hh)}" fill="var(--sheet-bg, #fff)" stroke="currentColor" stroke-width="0.25"/>`,
      );
      text(ctx, [q[0], q[1] - t.h / 2], t.text, t.h, 'middle');
    } else {
      text(ctx, q, t.text, t.h, t.anchor);
    }
  }
}

function columnWidths(t: DrawingTable, width: number): number[] {
  const cols = t.header.length;
  const est = t.header.map((h, i) => Math.max(h.length, ...t.rows.map((r) => (r[i] ?? '').length), 2));
  const sum = est.reduce((a, b) => a + b, 0);
  return est.map((e) => (width * e) / sum).slice(0, cols);
}

/** Высота таблицы — как в ядре (drawing.cpp, table_height). */
export function tableHeight(t: DrawingTable): number {
  return 8 + 7 + (t.note ? 6 : 0) + 6 * t.rows.length;
}

function renderTable(ctx: Ctx, t: DrawingTable, topLeft: Vec2, width: number): void {
  const x0 = topLeft[0];
  let y = topLeft[1];
  text(ctx, [x0 + width / 2, y - 6], t.title, 3.5);
  y -= 8;
  const widths = columnWidths(t, width);
  const h = tableHeight(t) - 8;
  rect(ctx, x0, y - h, x0 + width, y, 0.5);
  // Шапка; строка примечания («R = 40») — на всю ширину, без разделителей столбцов.
  const noteH = t.note ? 6 : 0;
  let x = x0;
  widths.forEach((w, i) => {
    text(ctx, [x + w / 2, y - 5], t.header[i], 2.5);
    if (i) {
      line(ctx, [[x, y], [x, y - 7]], 'thin');
      line(ctx, [[x, y - 7 - noteH], [x, y - h]], 'thin');
    }
    x += w;
  });
  y -= 7;
  line(ctx, [[x0, y], [x0 + width, y]], 'contour');
  if (t.note) {
    text(ctx, [x0 + 2, y - 4.5], t.note, 2.5, 'start');
    y -= 6;
    line(ctx, [[x0, y], [x0 + width, y]], 'thin');
  }
  for (const row of t.rows) {
    let cx = x0;
    row.forEach((cell, i) => {
      const w = widths[i] ?? 0;
      text(ctx, [cx + w / 2, y - 4.5], cell, 2.5);
      cx += w;
    });
    y -= 6;
    line(ctx, [[x0, y], [x0 + width, y]], 'thin');
  }
}

function renderTitleBlock(ctx: Ctx, d: DrawingData, stamp: ReleaseStamp): void {
  const tb = d.title_block;
  const W = tb.x1 - tb.x0;
  const Ht = tb.y1 - tb.y0;
  rect(ctx, tb.x0, tb.y0, tb.x1, tb.y1, 0.5);
  const get = (k: string) => d.title[k] ?? '';
  if (Ht < 20) {
    // Последующий лист (ГОСТ 2.104, форма 2а): обозначение и номер листа.
    line(ctx, [[tb.x1 - 10, tb.y0], [tb.x1 - 10, tb.y1]], 'contour');
    text(ctx, [(tb.x0 + tb.x1 - 10) / 2, tb.y0 + Ht / 2 - 2.5], get('Обозначение'), 5);
    text(ctx, [tb.x1 - 5, tb.y1 - 4], 'Лист', 2);
    text(ctx, [tb.x1 - 5, tb.y0 + 2], get('Лист'), 3.5);
    return;
  }
  // Сетка: правая часть — обозначение (верх), наименование и материал; справа — масштаб, лист.
  const xr = tb.x0 + W * 0.35;  // слева — подписи (Разработал, Проверил…)
  const xs = tb.x1 - 50;        // колонка масштаба и листов
  const y1 = tb.y1 - Ht * 0.28;
  const y2 = tb.y0 + Ht * 0.4;
  line(ctx, [[xr, tb.y0], [xr, tb.y1]], 'contour');
  line(ctx, [[xr, y1], [tb.x1, y1]], 'contour');
  line(ctx, [[xs, tb.y0], [xs, y1]], 'contour');
  line(ctx, [[xr, y2], [xs, y2]], 'contour');
  line(ctx, [[xs, (y1 + tb.y0) / 2], [tb.x1, (y1 + tb.y0) / 2]], 'thin');
  line(ctx, [[xs + 25, tb.y0], [xs + 25, y1]], 'thin');

  text(ctx, [(xr + tb.x1) / 2, (y1 + tb.y1) / 2 - 2.5], get('Обозначение'), 5);
  text(ctx, [(xr + xs) / 2, (y1 + y2) / 2 - 2], get('Наименование'), 5);
  text(ctx, [(xr + xs) / 2, (y2 + tb.y0) / 2 - 1.5], get('Материал'), 3.5);
  text(ctx, [xs + 12.5, y1 - 4], 'Масштаб', 2);
  text(ctx, [xs + 12.5, (y1 + tb.y0) / 2 + 2], get('Масштаб'), 3.5);
  text(ctx, [xs + 37.5, y1 - 4], 'Лист', 2);
  text(ctx, [xs + 37.5, (y1 + tb.y0) / 2 + 2], get('Лист'), 3.5);
  text(ctx, [xs + 12.5, tb.y0 + 6], 'Стандарт', 2);
  text(ctx, [xs + 12.5, tb.y0 + 2], get('Стандарт') || STANDARD_NAME[d.standard], 2.5);
  text(ctx, [xs + 37.5, tb.y0 + 6], 'Листов', 2);
  text(ctx, [xs + 37.5, tb.y0 + 2], get('Листов'), 2.5);

  const rows = ['Разработал', 'Проверил', 'Т. контроль', 'Н. контроль', 'Утвердил'];
  const rh = Ht / rows.length;
  rows.forEach((r, i) => {
    const yy = tb.y1 - rh * (i + 1);
    if (i) line(ctx, [[tb.x0, yy + rh], [xr, yy + rh]], 'thin');
    text(ctx, [tb.x0 + 2, yy + rh / 2 - 1], r, 2, 'start');
    text(ctx, [tb.x0 + 26, yy + rh / 2 - 1], get(r), 2.5, 'start');
  });

  if (stamp.released) {
    const label = `ВЫПУЩЕН ${stamp.date ?? ''} ${stamp.by ?? ''}`.trim();
    text(ctx, [(xr + xs) / 2, y1 + 2], label, 2.5, 'middle', 0, ' fill="var(--ok, #1a7f37)"');
  }
}

export interface SvgOptions {
  stamp?: ReleaseStamp;
  /** Подсветить вид (по id) — для выбора в интерфейсе. */
  highlightView?: string;
}

export function drawingToSvg(d: DrawingData, opt: SvgOptions = {}): string {
  const W = d.format.w;
  const H = d.format.h;
  const ctx: Ctx = { H, out: [], italic: d.standard === 'GOST' };
  ctx.out.push(
    `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${W} ${H}" width="${W}mm" height="${H}mm" font-family="'Arial Narrow', Arial, sans-serif" style="color: var(--ink, #111); background: var(--sheet-bg, #fff)">`,
  );
  ctx.out.push(
    '<defs><pattern id="hatch45" width="2" height="2" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><line x1="0" y1="0" x2="0" y2="2" stroke="currentColor" stroke-width="0.18"/></pattern></defs>',
  );
  ctx.out.push(`<rect x="0" y="0" width="${W}" height="${H}" fill="var(--sheet-bg, #fff)"/>`);
  rect(ctx, d.border.x0, d.border.y0, d.border.x1, d.border.y1, 0.5);

  for (const v of d.views) {
    const hl = opt.highlightView === v.id;
    ctx.out.push(`<g data-view="${escapeXml(v.id)}"${hl ? ' style="color: var(--accent, #1F6FC5)"' : ''}>`);
    renderGeometry(ctx, v.geom, v.origin, v.scale);
    if (v.label) {
      const top = v.origin[1] + v.scale * v.geom.bbox.hi[1];
      if (v.kind === 'unfold') {
        // Над стопкой размеров развёртки, у левого края (подпись «Подача с конца …» — по центру).
        const above = v.geom.dims.filter((dm) => dm.a[1] > 0).reduce((m, dm) => Math.max(m, dm.offset), 0);
        text(ctx, [v.origin[0] + v.scale * v.geom.bbox.lo[0], top + above + 9], v.label, 3.5, 'start');
      } else {
        const cx = v.origin[0] + (v.scale * (v.geom.bbox.lo[0] + v.geom.bbox.hi[0])) / 2;
        text(ctx, [cx, top + 12], v.label, 3.5);
      }
    }
    ctx.out.push('</g>');
  }
  for (const t of d.tables) renderTable(ctx, t.table, t.top_left, t.width);
  renderTitleBlock(ctx, d, opt.stamp ?? { released: false });

  if (d.not_for_production) {
    text(ctx, [(d.border.x0 + d.title_block.x0) / 2, d.border.y1 - 10], 'НЕ ДЛЯ ПРОИЗВОДСТВА', 7, 'middle', 0,
      ' fill="var(--danger, #D93A3A)" font-weight="bold"');
  }
  ctx.out.push('</svg>');
  return ctx.out.join('');
}
