// CAD icons of bendcore, drawn for the ribbon in the style of Tabler Icons (src/ui/icons/README.md):
// a 24×24 grid, the drawing kept inside 2…22, stroke-width 2, round caps and joins, currentColor,
// no fills. Points are drawn as zero-length segments (`h.01`) or small circles; dashed lines as
// separate segments. Licensed under the MIT License of the bendcore project (LICENSE).

/** Section of the icon catalog (web/icons). */
export type CadIconSection = 'sketch' | 'feature' | 'assembly' | 'drawing' | 'general';

export interface CadIcon {
  section: CadIconSection;
  /** Inner SVG markup. */
  body: string;
}

const p = (...d: string[]) => d.map((x) => `<path d="${x}"/>`).join('');
const c = (cx: number, cy: number, r: number) => `<circle cx="${cx}" cy="${cy}" r="${r}"/>`;
const e = (cx: number, cy: number, rx: number, ry: number) => `<ellipse cx="${cx}" cy="${cy}" rx="${rx}" ry="${ry}"/>`;

/** Eight-tooth gear outline around (12, 12), outer radius 9. */
const GEAR =
  'M18.4 10.2L20.9 10.8L20.9 13.2L18.4 13.8L17.7 15.3L19.1 17.5L17.5 19.1L15.3 17.7L13.8 18.4L13.2 20.9L10.8 20.9L10.2 18.4L8.7 17.7L6.5 19.1L4.9 17.5L6.3 15.3L5.6 13.8L3.1 13.2L3.1 10.8L5.6 10.2L6.3 8.7L4.9 6.5L6.5 4.9L8.7 6.3L10.2 5.6L10.8 3.1L13.2 3.1L13.8 5.6L15.3 6.3L17.5 4.9L19.1 6.5L17.7 8.7Z';
/** Small isometric cube with its top-left corner at the origin of the grid (3…15). */
const SMALL_CUBE = p('M9 3l6 3.5v7l-6 3.5l-6 -3.5v-7z', 'M9 10l6 -3.5', 'M9 10v7', 'M9 10l-6 -3.5');
const PLUS_CORNER = p('M16 19h6', 'M19 16v6');

export const CAD_ICONS: Record<string, CadIcon> = {
  // ------------------------------------------------------------------ sketch
  sketch: { section: 'sketch', body: p('M4 20h4l10.5 -10.5a2.8 2.8 0 0 0 -4 -4l-10.5 10.5v4', 'M13.5 6.5l4 4', 'M14 20h6') },
  'sketch-line': { section: 'sketch', body: c(5, 19, 2) + c(19, 5, 2) + p('M6.5 17.5l11 -11') },
  'sketch-rectangle': { section: 'sketch', body: p('M6 6h14v10', 'M18 18h-14v-10') + c(4, 6, 2) + c(20, 18, 2) },
  'sketch-rectangle-center': { section: 'sketch', body: p('M3 6h18v12h-18z', 'M12 12l6 4') + c(12, 12, 1.5) },
  'sketch-circle': { section: 'sketch', body: c(12, 12, 8) + p('M12 12h.01') },
  'sketch-circle-3': { section: 'sketch', body: c(12, 12, 8) + c(12, 4, 1.5) + c(5.1, 16, 1.5) + c(18.9, 16, 1.5) },
  'sketch-arc': { section: 'sketch', body: p('M19 5a14 14 0 0 0 -14 14', 'M19 19h.01') + c(19, 5, 1.5) + c(5, 19, 1.5) },
  'sketch-arc-3': { section: 'sketch', body: p('M4 18a9 9 0 0 1 16 0') + c(4, 18, 1.5) + c(12, 9.5, 1.5) + c(20, 18, 1.5) },
  'sketch-arc-tangent': { section: 'sketch', body: p('M3 19h8', 'M11 19a8 8 0 0 0 8 -8v-6') + c(11, 19, 1.5) },
  'sketch-ellipse': { section: 'sketch', body: e(12, 12, 9, 5.5) + p('M12 12h.01') },
  'sketch-spline': { section: 'sketch', body: p('M3 17c3 -9 6 -9 9 -5s6 4 9 -5') + c(3, 17, 1.5) + c(12, 12, 1.5) + c(21, 7, 1.5) },
  'sketch-point': { section: 'sketch', body: c(12, 12, 2) + p('M12 4v4', 'M12 16v4', 'M4 12h4', 'M16 12h4') },
  'sketch-select': { section: 'sketch', body: p('M6 3l12 9l-6 1l3.5 6.5l-2.5 1.5l-3.5 -6.5l-3.5 4z') },
  'sketch-regions': { section: 'sketch', body: p('M4 4h16v16h-16z', 'M4 10l6 -6', 'M4 16l12 -12', 'M8 20l12 -12', 'M14 20l6 -6') },
  'sketch-trim': { section: 'sketch', body: p('M9 3v18', 'M3 12h6', 'M13 12h2', 'M18 12h3', 'M14 5l4 4', 'M18 5l-4 4') },
  'sketch-extend': { section: 'sketch', body: p('M20 3v18', 'M3 12h7', 'M13 12h1', 'M14 9l3 3l-3 3') },
  'sketch-offset': { section: 'sketch', body: p('M4 21v-9a7 7 0 0 1 7 -7h10', 'M9 21v-9a2 2 0 0 1 2 -2h10') },
  'sketch-mirror': { section: 'sketch', body: p('M12 3v2', 'M12 8v2', 'M12 13v2', 'M12 18v2', 'M9 7l-6 11h6z', 'M15 7l6 11h-6z') },
  'sketch-pattern': { section: 'sketch', body: c(5, 8, 2) + c(12, 8, 2) + c(19, 8, 2) + p('M3 16h18', 'M18 13l3 3l-3 3') },
  'sketch-project': { section: 'sketch', body: p('M3 19l4 -4h14l-4 4z', 'M12 3v9', 'M9 9l3 3l3 -3', 'M7 3h10') },
  'sketch-fillet': { section: 'sketch', body: p('M4 21v-9a8 8 0 0 1 8 -8h9') },
  'sketch-chamfer': { section: 'sketch', body: p('M4 21v-9l8 -8h9') },
  'sketch-construction': { section: 'sketch', body: p('M3 21l2 -2', 'M8 16l2 -2', 'M13 11l2 -2', 'M18 6l2 -2') },
  'sketch-dimension': { section: 'sketch', body: p('M3 7v10', 'M21 7v10', 'M3 12h4', 'M17 12h4', 'M6 10l-3 2l3 2', 'M18 10l3 2l-3 2', 'M9 9h6v6h-6z') },
  'sketch-finish': { section: 'sketch', body: p('M4 20h7', 'M4 20v-7', 'M9 13l3 3l8 -9') },
  'sketch-dxf': { section: 'sketch', body: p('M14 3v4a1 1 0 0 0 1 1h4', 'M17 21h-10a2 2 0 0 1 -2 -2v-14a2 2 0 0 1 2 -2h7l5 5v11a2 2 0 0 1 -2 2', 'M8 17l3 -4l3 2l2 -3') },
  // constraints
  'constraint': { section: 'sketch', body: p('M7 20l10 -16', 'M4 14h16') + c(10.6, 14, 1.5) },
  'constraint-coincident': { section: 'sketch', body: p('M3 20l7 -7', 'M14 13l7 -7', 'M3 4l7 7', 'M14 11l7 7') + c(12, 12, 2) },
  'constraint-horizontal': { section: 'sketch', body: c(4, 12, 2) + c(20, 12, 2) + p('M6 12h12', 'M8 18h8') },
  'constraint-vertical': { section: 'sketch', body: c(12, 4, 2) + c(12, 20, 2) + p('M12 6v12', 'M18 8v8') },
  'constraint-parallel': { section: 'sketch', body: p('M4 20l8 -16', 'M12 20l8 -16') },
  'constraint-perpendicular': { section: 'sketch', body: p('M4 20h16', 'M12 20v-16', 'M12 16h4v4') },
  'constraint-tangent': { section: 'sketch', body: c(12, 14, 6) + p('M3 8h18') },
  'constraint-equal': { section: 'sketch', body: p('M5 9h14', 'M5 15h14') },
  'constraint-symmetric': { section: 'sketch', body: p('M12 3v2', 'M12 8v2', 'M12 14v2', 'M12 19v2') + c(5, 12, 2) + c(19, 12, 2) },
  'constraint-fix': { section: 'sketch', body: c(12, 7, 3) + p('M12 10v6', 'M4 16h16', 'M7 16l-2 4', 'M11 16l-2 4', 'M15 16l-2 4', 'M19 16l-2 4') },
  'constraint-midpoint': { section: 'sketch', body: p('M3 18l18 -12') + c(12, 12, 2) + p('M12 4v2', 'M12 18v2') },
  'constraint-concentric': { section: 'sketch', body: c(12, 12, 8) + c(12, 12, 4) + p('M12 12h.01') },
  'constraint-point-on': { section: 'sketch', body: p('M3 18c4 -10 14 -10 18 -6') + c(12, 10.5, 2) },

  // ------------------------------------------------------------------ features
  feature: { section: 'feature', body: p('M12 3l8 4.5v9l-8 4.5l-8 -4.5v-9z', 'M12 12l8 -4.5', 'M12 12v9', 'M12 12l-8 -4.5') },
  extrude: { section: 'feature', body: p('M3 19l4 -4h14l-4 4z', 'M12 17v-13', 'M9 7l3 -3l3 3') },
  cut: { section: 'feature', body: p('M3 19l4 -4h14l-4 4z', 'M12 3v11', 'M9 11l3 3l3 -3', 'M17 6h4') },
  revolve: { section: 'feature', body: p('M12 3v2', 'M12 8v2', 'M12 13v2', 'M12 18v3', 'M14 3h5v7h-5', 'M16 13.4a8 3 0 1 1 -8 0', 'M6.1 12.4l1.9 1l-1.5 2') },
  hole: { section: 'feature', body: c(12, 12, 6) + p('M12 3v3', 'M12 18v3', 'M3 12h3', 'M18 12h3') },
  'hole-threaded': { section: 'feature', body: c(12, 12, 5) + p('M20 12a8 8 0 1 1 -8 -8', 'M12 3v1', 'M12 20v1', 'M3 12h1') },
  fillet: { section: 'feature', body: p('M4 20v-8a8 8 0 0 1 8 -8h8v16z', 'M12 12h.01') },
  chamfer: { section: 'feature', body: p('M4 20v-9l7 -7h9v16z') },
  shell: { section: 'feature', body: p('M4 4v16h16v-16', 'M8 4v12h8v-12', 'M4 4h4', 'M16 4h4') },
  draft: { section: 'feature', body: p('M4 20h16', 'M7 20l3 -16h4l3 16', 'M7 4v2', 'M7 9v2', 'M7 14v2') },
  rib: { section: 'feature', body: p('M5 3v17h16', 'M5 9l11 11', 'M5 13l7 7') },
  'pattern-linear': { section: 'feature', body: p('M3 8h4v4h-4z', 'M10 8h4v4h-4z', 'M17 8h4v4h-4z', 'M3 18h18', 'M18 15l3 3l-3 3') },
  'pattern-circular': { section: 'feature', body: c(12, 4.5, 2) + c(19.5, 12, 2) + c(12, 19.5, 2) + c(4.5, 12, 2) + p('M12 12h.01') },
  mirror: { section: 'feature', body: p('M12 3v18', 'M9 8l-6 2v8l6 -2z', 'M15 8l6 2v8l-6 -2z') },
  sweep: { section: 'feature', body: p('M5 18c0 -7 5 -12 14 -13') + e(5, 18.5, 3, 1.5) + e(19, 5, 1.5, 3) },
  loft: { section: 'feature', body: e(12, 5, 3, 1.5) + e(12, 18.5, 8, 2.5) + p('M9 5l-5 13.5', 'M15 5l5 13.5') },
  'gear-rim': { section: 'feature', body: p(GEAR) + c(12, 12, 3) },
  boolean: { section: 'feature', body: c(9, 12, 6) + c(15, 12, 6) },
  'body-delete': { section: 'feature', body: p('M10 4l6 3.5v7l-6 3.5l-6 -3.5v-7z', 'M10 11l6 -3.5', 'M10 11v7', 'M16 17l5 5', 'M21 17l-5 5') },
  'body-new': { section: 'feature', body: SMALL_CUBE + PLUS_CORNER },
  'pattern-linear-body': { section: 'feature', body: p('M3 9l3 -2l3 2v4l-3 2l-3 -2z', 'M10 9l3 -2l3 2v4l-3 2l-3 -2z', 'M17 9l2 -1.3', 'M17 9v4', 'M3 19h18', 'M18 17l3 2l-3 2') },
  'pattern-circular-body': { section: 'feature', body: p('M10 2h4v4h-4z', 'M18 10h4v4h-4z', 'M10 18h4v4h-4z', 'M2 10h4v4h-4z', 'M12 12h.01') },
  'mirror-body': { section: 'feature', body: p('M12 3v18', 'M8 7l-3 -2l-3 2v6l3 2l3 -2z', 'M16 7l3 -2l3 2v6l-3 2l-3 -2z') },
  plane: { section: 'feature', body: p('M3 18l5 -12h13l-5 12z') },
  axis: { section: 'feature', body: p('M4 20l3 -3', 'M9 15l.01 -.01', 'M11 13l3 -3', 'M16 8l.01 -.01', 'M18 6l2 -2', 'M15 4h5v5') },
  point: { section: 'feature', body: c(12, 12, 2.5) + p('M5 5l3.5 3.5', 'M19 19l-3.5 -3.5', 'M19 5l-3.5 3.5', 'M5 19l3.5 -3.5') },
  csys: { section: 'feature', body: p('M7 17v-13', 'M7 17h13', 'M7 17l-4 4', 'M5 6l2 -2l2 2', 'M18 15l2 2l-2 2') },
  'parameters-table': { section: 'feature', body: p('M3 5h18v14h-18z', 'M3 10h18', 'M10 5v14', 'M13 14l2 2l3 -3') },
  'reference-mesh': { section: 'feature', body: p('M4 19l8 -15l8 15z', 'M8 11.5h8', 'M12 19l-4 -7.5', 'M12 19l4 -7.5') },
  'mass-properties': { section: 'feature', body: p('M12 4v16', 'M8 20h8', 'M5 7h14', 'M5 7l-3 6h6z', 'M19 7l-3 6h6z') },
  'section-view': { section: 'feature', body: p('M12 3l8 4.5v9l-8 4.5l-8 -4.5v-9z', 'M3 13l18 -4') },
  'sketch-on-plane': { section: 'feature', body: p('M3 19l4 -10h14l-4 10z', 'M9 15l3 -3l2 2l3 -3') },
  'create-drawing': { section: 'feature', body: p('M4 4h16v16h-16z', 'M4 16h16', 'M12 16v4', 'M8 7h4v5h-4z', 'M14 7h2') },
  'selection-filter': { section: 'feature', body: p('M4 5h16l-6 7v6l-4 2v-8z') },
  'variable': { section: 'feature', body: p('M5 18c2 0 2 -12 4 -12', 'M4 11h5', 'M13 9l6 8', 'M19 9l-6 8') },
  tessellate: { section: 'feature', body: p('M4 6h16v12h-16z', 'M4 6l8 12', 'M12 6l8 12', 'M12 6l-8 12', 'M20 6l-8 12') },

  // ------------------------------------------------------------------ assembly
  'insert-part': { section: 'assembly', body: SMALL_CUBE + PLUS_CORNER },
  'insert-subassembly': { section: 'assembly', body: p('M6 3l4 2.3v4.6l-4 2.3l-4 -2.3v-4.6z', 'M13 7l4 2.3v4.6l-4 2.3l-4 -2.3', 'M6 12v4l4 2.3') + PLUS_CORNER },
  'open-component': { section: 'assembly', body: SMALL_CUBE + p('M17 13h4v4', 'M21 13l-5 5') },
  'mate-coincident': { section: 'assembly', body: p('M4 5h16v6h-16z', 'M4 11v6h16v-6', 'M8 8h.01', 'M16 14h.01') },
  'mate-concentric': { section: 'assembly', body: c(12, 12, 8) + c(12, 12, 4) + p('M12 2v4', 'M12 18v4', 'M2 12h4', 'M18 12h4') },
  'mate-distance': { section: 'assembly', body: p('M3 4h18', 'M3 20h18', 'M12 7v10', 'M9 10l3 -3l3 3', 'M9 14l3 3l3 -3') },
  'mate-angle': { section: 'assembly', body: p('M3 20h18', 'M3 20l13 -13', 'M12 20a9 9 0 0 0 -2.6 -6.4') },
  'mate-parallel': { section: 'assembly', body: p('M3 9l4 -5h14l-4 5z', 'M3 20l4 -5h14l-4 5z') },
  'mate-perpendicular': { section: 'assembly', body: p('M3 20l4 -5h14l-4 5z', 'M12 17.5v-15', 'M12 14h3') },
  'mate-tangent': { section: 'assembly', body: c(12, 9, 6) + p('M3 15l4 4h14', 'M3 15h18') },
  'mate-rigid': { section: 'assembly', body: p('M5 11h14v10h-14z', 'M8 11v-4a4 4 0 1 1 8 0v4', 'M12 15v2') },
  'mate-gear': { section: 'assembly', body: c(9, 14, 6) + c(15.4, 7.6, 3) + p('M9 14h.01', 'M15.4 7.6h.01', 'M9 8v-2', 'M3 14h-1', 'M18.4 7.6h2') },
  mate: { section: 'assembly', body: c(8, 12, 5) + c(16, 12, 5) },
  fastener: { section: 'assembly', body: p('M7 3h10v5h-10z', 'M10 8v13h4v-13', 'M10 12l4 -1.5', 'M10 15.5l4 -1.5', 'M10 19l4 -1.5') },
  interference: { section: 'assembly', body: p('M3 3h12v12h-12z', 'M9 9h12v12h-12z', 'M9 13l4 -4', 'M11 15l4 -4') },
  bom: { section: 'assembly', body: p('M3 4h18v16h-18z', 'M3 9h18', 'M3 14.5h18', 'M8 4v16', 'M5.5 11.75h.01', 'M5.5 17.25h.01') },
  explode: { section: 'assembly', body: p('M9 9h6v6h-6z', 'M12 2v4', 'M10 4l2 -2l2 2', 'M12 22v-4', 'M10 20l2 2l2 -2', 'M2 12h4', 'M4 10l-2 2l2 2', 'M22 12h-4', 'M20 10l2 2l-2 2') },
  'pattern-component': { section: 'assembly', body: p('M5 5l3.5 -2l3.5 2v4l-3.5 2l-3.5 -2z', 'M12 13l3.5 -2l3.5 2v4l-3.5 2l-3.5 -2z', 'M4 14v6h6', 'M14 4h6v6') },
  configurations: { section: 'assembly', body: p('M8 3h13v11h-13z', 'M5 6v11h13', 'M2 9v11h13') },
  solve: { section: 'assembly', body: c(12, 12, 9) + p('M8 12l3 3l5 -6') },
  'component-properties': { section: 'assembly', body: SMALL_CUBE + p('M16 16h6', 'M16 19h6', 'M16 22h4') },
  'pick-face': { section: 'assembly', body: p('M3 17l5 -10h13l-5 10z', 'M9 12h7') },
  'pick-edge': { section: 'assembly', body: p('M12 3l8 4.5v9l-8 4.5l-8 -4.5v-9z', 'M20 7.5v9') + c(20, 12, 2) },
  'pick-component': { section: 'assembly', body: p('M12 3l8 4.5v9l-8 4.5l-8 -4.5v-9z', 'M12 12l8 -4.5', 'M12 12v9', 'M12 12l-8 -4.5') + c(12, 12, 2) },
  drag: { section: 'assembly', body: p('M12 3v18', 'M3 12h18', 'M9 6l3 -3l3 3', 'M9 18l3 3l3 -3', 'M6 9l-3 3l3 3', 'M18 9l3 3l-3 3') },

  // ------------------------------------------------------------------ drawing
  'view-base': { section: 'drawing', body: p('M3 4h18v16h-18z', 'M7 8h5v4h5v4h-10z') },
  'view-projected': { section: 'drawing', body: p('M3 9h6v6h-6z', 'M15 9h6v6h-6z', 'M10.5 12h3', 'M12 10l2 2l-2 2') },
  'view-axonometric': { section: 'drawing', body: p('M3 4h18v16h-18z', 'M12 7l5 3v5l-5 3l-5 -3v-5z', 'M12 13l5 -3', 'M12 13v5', 'M12 13l-5 -3') },
  'view-section': { section: 'drawing', body: p('M4 4h16v16h-16z', 'M4 12l8 -8', 'M4 20l16 -16', 'M12 20l8 -8') },
  'view-local-section': { section: 'drawing', body: p('M4 4h16v16h-16z', 'M4 12c3 -2 5 2 8 0s5 -2 8 0', 'M4 17l3 -3.5', 'M8 20l5 -6', 'M14 20l6 -6.5') },
  'view-cross-section': { section: 'drawing', body: c(12, 12, 7) + p('M7.05 16.95l9.9 -9.9', 'M5.06 12.94l7.88 -7.88', 'M11.06 18.94l7.88 -7.88') },
  'view-detail': { section: 'drawing', body: c(7, 7, 4) + c(16, 16, 5) + p('M9.8 9.8l2.7 2.7') },
  'view-auxiliary': { section: 'drawing', body: p('M3 13l5 -5l5 5l-5 5z', 'M14 4h6v6', 'M20 4l-6 6') },
  'view-fit': { section: 'drawing', body: p('M4 8v-4h4', 'M16 4h4v4', 'M20 16v4h-4', 'M8 20h-4v-4', 'M8 9h8v6h-8z') },
  'dimension-linear': { section: 'drawing', body: p('M4 5v15', 'M20 5v15', 'M4 10h16', 'M7 7l-3 3l3 3', 'M17 7l3 3l-3 3') },
  'dimension-chain': { section: 'drawing', body: p('M3 6v14', 'M12 6v14', 'M21 6v14', 'M3 11h18', 'M6 9l-3 2l3 2', 'M18 9l3 2l-3 2', 'M10.5 12.5l3 -3') },
  'dimension-baseline': { section: 'drawing', body: p('M3 4v17', 'M12 12v9', 'M21 6v15', 'M3 15h9', 'M3 8h18', 'M9 13l3 2l-3 2', 'M18 6l3 2l-3 2') },
  'dimension-arrange': { section: 'drawing', body: p('M3 8v13', 'M14 8v13', 'M3 13h11', 'M3 18h11', 'M18 3l1 2l2 1l-2 1l-1 2l-1 -2l-2 -1l2 -1z') },
  'dimension-thread': { section: 'drawing', body: p('M3 13h18v7h-18z', 'M3 15.5h18', 'M5 4v6', 'M19 4v6', 'M5 7h14', 'M8 5l-3 2l3 2', 'M16 5l3 2l-3 2') },
  roughness: { section: 'drawing', body: p('M3 12l4 7l7.5 -15h6.5', 'M5.5 16.2h4.3') },
  'form-tolerance': { section: 'drawing', body: p('M2 8h20v8h-20z', 'M10 8v8', 'M4 14l1.5 -4h3l-1.5 4z', 'M13 12h6') },
  datum: { section: 'drawing', body: p('M8 3h8v7h-8z', 'M12 10v5', 'M6 20l6 -5l6 5z') },
  balloon: { section: 'drawing', body: c(4, 20, 1.5) + p('M5 19l7 -8h9', 'M16 5v5', 'M14.5 6.5l1.5 -1.5') },
  'balloon-arrange': { section: 'drawing', body: p('M14 5h7', 'M14 12h7', 'M14 19h7', 'M5 12l9 -7', 'M5 12h9', 'M5 12l9 7') + c(4, 12, 1.5) },
  'thread-symbol': { section: 'drawing', body: p('M3 7h15l3 3v4l-3 3h-15', 'M3 9.5h15', 'M3 14.5h15', 'M18 7v10') },
  'update-from-model': { section: 'drawing', body: p('M20 11a8 8 0 0 0 -15 -3', 'M4 4v4h4', 'M4 13a8 8 0 0 0 15 3', 'M20 20v-4h-4', 'M10 10h4v4h-4z') },
  'export-pdf': { section: 'drawing', body: p('M14 3v4a1 1 0 0 0 1 1h4', 'M17 21h-10a2 2 0 0 1 -2 -2v-14a2 2 0 0 1 2 -2h7l5 5v11a2 2 0 0 1 -2 2', 'M10 18v-6h2.5a1.5 1.5 0 0 1 0 3h-2.5') },
  'export-dxf': { section: 'drawing', body: p('M14 3v4a1 1 0 0 0 1 1h4', 'M17 21h-10a2 2 0 0 1 -2 -2v-14a2 2 0 0 1 2 -2h7l5 5v11a2 2 0 0 1 -2 2', 'M8 17l3 -4l3 2l2 -3') },
  'export-svg': { section: 'drawing', body: p('M14 3v4a1 1 0 0 0 1 1h4', 'M17 21h-10a2 2 0 0 1 -2 -2v-14a2 2 0 0 1 2 -2h7l5 5v11a2 2 0 0 1 -2 2', 'M8 17c1 -5 3 -5 4 -2s3 3 4 -2') },
  'export-sheet-pdf': { section: 'drawing', body: p('M3 5h12v15h-12z', 'M3 16h12', 'M9 16v4', 'M14 9h7', 'M18 6l3 3l-3 3') },
  'specification': { section: 'drawing', body: p('M5 3h14v18h-14z', 'M5 8h14', 'M9 3v18', 'M5 13h14', 'M5 17h14') },
  sheet: { section: 'drawing', body: p('M3 5h18v14h-18z', 'M5 7h14v10h-14', 'M13 14h6', 'M13 14v3') },
  'technical-requirements': { section: 'drawing', body: p('M5 4h14v16h-14z', 'M8 8h8', 'M8 12h8', 'M8 16h5') },
};
