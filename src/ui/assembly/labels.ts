// Texts of the assembly page that depend on numbers (docs/ui/ASSEMBLY_UI.md). Captions of mate
// types, mate states, geometry and fastener kinds are messages of
// src/ui/app/messages/assembly.ts (assembly.mateType.*, assembly.mateStatus.*,
// assembly.geometry.*, assembly.fastenerKind.*), looked up with caption().

import { translate } from '../app/i18n';
import type { Lang, Translate } from '../app/i18n';

const RU: Translate = (key, params) => translate('ru', key, params);

/**
 * "1 степень свободы", "2 степени свободы", "5 степеней свободы" (Russian plural forms by the
 * last two digits); English: "1 degree of freedom", "2 degrees of freedom".
 */
export function dofText(n: number, t: Translate = RU, lang: Lang = 'ru'): string {
  if (lang === 'en') return t(n === 1 ? 'assembly.dof.one' : 'assembly.dof.many', { n });
  const mod10 = n % 10;
  const mod100 = n % 100;
  if (mod10 === 1 && mod100 !== 11) return t('assembly.dof.one', { n });
  if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) return t('assembly.dof.few', { n });
  return t('assembly.dof.many', { n });
}

/** Mass in kg with three significant decimals, or a dash when unknown. */
export function massText(kg: number | null | undefined): string {
  if (kg === null || kg === undefined) return '—';
  if (kg >= 1) return kg.toFixed(3);
  return kg.toPrecision(3);
}
