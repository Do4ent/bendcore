// Shaft unit with a sub-assembly, for the drawing tests of dissolved sub-assemblies
// (docs/ui/DRAWING_UI.md, "Шрифт текста и разборные подсборки"). The shaft unit of
// shaft_unit.mjs is solved by the assembly module; then the bushing and the shaft (placed by the
// mates) become the internal sub-assembly «Вал в сборе» (УВ.02.000) at the same places, and the
// bolts, washers and nuts stay at the top level. The mates are left out: every occurrence keeps
// the solved placement and the sub-assembly is fixed.
//
// Specification by construction (GOST 2.106): «Сборочные единицы» — Вал в сборе (1); standard
// items — bolt × 2, nut × 2, washer × 4 (2…4). With the sub-assembly dissolved: «Детали» —
// Втулка, Вал (1, 2), standard items (3…5), no «Сборочные единицы».
//
//   import { buildNestedUnit } from './nested_unit.mjs';
//   const json = buildNestedUnit(assemblyKernel);           // bendcore-assembly JSON
import { buildShaftUnit } from './shaft_unit.mjs';

export const SUBASSEMBLY = { designation: 'УВ.02.000', name: 'Вал в сборе' };

export function buildNestedUnit(assembly) {
  const doc = JSON.parse(buildShaftUnit(assembly));
  const byName = (n) => doc.occurrences.find((o) => o.name === n);
  const inner = [byName('Втулка'), byName('Вал')];
  if (inner.some((o) => !o)) throw new Error('shaft unit: no bushing or shaft occurrence');
  const defId = Math.max(...doc.definitions.map((d) => d.id)) + 1;
  doc.definitions.push({
    id: defId,
    kind: 'assembly',
    name: SUBASSEMBLY.name,
    properties: { designation: SUBASSEMBLY.designation, name: SUBASSEMBLY.name, material: '' },
    color: null,
    occurrences: inner.map((o) => ({ ...o, fixed: false })),
  });
  const occId = doc.next.occurrence;
  doc.occurrences = [
    {
      color: null,
      definition: defId,
      designation: '',
      fixed: true,
      id: occId,
      name: SUBASSEMBLY.name,
      placement: { position: [0, 0, 0], rotation: [1, 0, 0, 0] },
      suppressed: false,
    },
    ...doc.occurrences.filter((o) => !inner.includes(o)),
  ];
  doc.next.occurrence = occId + 1;
  doc.mates = [];
  return JSON.stringify(doc);
}
