// Part with document properties and an assembly of two of them, for the end-to-end tests of the
// part document properties (docs/modeling/PART_DOCUMENT.md, "Свойства документа"): made with the
// modeling and assembly APIs of the WebAssembly modules, the way the pages make them.
//
//   Плита опорная АБВГ.715311.001 — box 100 × 60 × 10, 6061-T6, developer, organization, note;
//   «Узел плит» УП.01.000 — two plates side by side.
//
//   import { buildPartProperties } from './part_properties.mjs';
//   const { part, assembly } = buildPartProperties(modelingKernel, assemblyKernel);
export const PLATE = {
  designation: 'АБВГ.715311.001',
  name: 'Плита опорная',
  developer: 'Иванов',
  organization: 'ООО «Ромашка»',
  note: 'Анодировать',
};
export const PLATE_VOLUME = 100 * 60 * 10;  // mm³
export const ALU_DENSITY = 2700;            // kg/m³, al_6061_t6

export function buildPartProperties(modeling, assembly) {
  const doc = modeling.createDocument();
  let part;
  try {
    doc.addFeature({ type: 'Box', id: 'Body', parameters: { length: '100', width: '60', height: '10' } });
    doc.rebuild();
    doc.setDocumentProperties({ ...PLATE, material: 'al_6061_t6' });
    part = doc.save({ material: { id: 'al_6061_t6', density: ALU_DENSITY } });
  } finally {
    doc.dispose();
  }
  const unit = assembly.createDocument('Узел плит');
  try {
    const first = unit.insertPart({ name: 'plate.bcpart', format: 'part-json', data: part, fixed: true });
    unit.insertOccurrence({ definition: first.definition, placement: { position: [0, 80, 0] }, fixed: true });
    const root = unit.state().definitions.find((d) => d.kind === 'assembly');
    unit.setProperties(root.id, { designation: 'УП.01.000', name: 'Узел плит' });
    return { part, assembly: unit.save({ embedGeometry: true }) };
  } finally {
    unit.dispose();
  }
}
