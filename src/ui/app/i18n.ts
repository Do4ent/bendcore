/**
 * Interface strings of the application in Russian and English (docs/ui/APP_SHELL.md,
 * "Локализация"). Each area has its dictionary pair in src/ui/app/messages/: the shell, the part
 * editor with its feature panels and tree, the sketch editor, the assembly editor, the drawing
 * editor and the STEP viewer. They are merged here into `ru` and `en`.
 *
 * `ru` is the reference dictionary; `en` must have exactly its keys (checked by the types of the
 * per-area dictionaries and by src/ui/app/__tests__/i18nCoverage.test.ts, which also checks that
 * the components hold no interface text of their own). Parameters are written {name} and filled
 * by translate().
 */

import { createContext, useContext } from 'react';
import { shellEn, shellRu } from './messages/shell';
import { partEn, partRu } from './messages/part';
import { sketchEn, sketchRu } from './messages/sketch';
import { assemblyEn, assemblyRu } from './messages/assembly';
import { drawingEn, drawingRu } from './messages/drawing';
import { viewerEn, viewerRu } from './messages/viewer';

export type Lang = 'ru' | 'en';
export const LANGS: Lang[] = ['ru', 'en'];

export const ru = { ...shellRu, ...partRu, ...sketchRu, ...assemblyRu, ...drawingRu, ...viewerRu } as const;

export type MessageKey = keyof typeof ru;

export const en: Record<MessageKey, string> = { ...shellEn, ...partEn, ...sketchEn, ...assemblyEn, ...drawingEn, ...viewerEn };

/** The per-area dictionaries, for the test that no key is defined twice. */
export const AREAS: Record<string, { ru: Record<string, string>; en: Record<string, string> }> = {
  shell: { ru: shellRu, en: shellEn },
  part: { ru: partRu, en: partEn },
  sketch: { ru: sketchRu, en: sketchEn },
  assembly: { ru: assemblyRu, en: assemblyEn },
  drawing: { ru: drawingRu, en: drawingEn },
  viewer: { ru: viewerRu, en: viewerEn },
};

export const DICTIONARIES: Record<Lang, Record<MessageKey, string>> = { ru, en };

/** Parameters {name} of a message. */
export function messageParams(message: string): string[] {
  return [...message.matchAll(/\{(\w+)\}/g)].map((m) => m[1]).sort();
}

function fill(message: string, params: Record<string, string | number>): string {
  return message.replace(/\{(\w+)\}/g, (whole, name: string) => (name in params ? String(params[name]) : whole));
}

export function translate(lang: Lang, key: MessageKey, params: Record<string, string | number> = {}): string {
  return fill(DICTIONARIES[lang][key] ?? ru[key], params);
}

export function isMessageKey(key: string): key is MessageKey {
  return Object.prototype.hasOwnProperty.call(ru, key);
}

export type Translate = (key: MessageKey, params?: Record<string, string | number>) => string;

/**
 * Caption of a name the kernel reports (feature type, parameter, option value, view kind…):
 * the message `prefix + name` when the dictionaries have it, else the name as is (a feature type
 * added to the kernel catalog later shows under its kernel name until it gets a caption).
 */
export function caption(t: Translate, prefix: string, name: string): string {
  const key = prefix + name;
  return isMessageKey(key) ? t(key) : name;
}

export interface I18n {
  lang: Lang;
  t: Translate;
  setLang(lang: Lang): void;
}

const LANG_STORAGE_KEY = 'bendcore.lang';

/** Language chosen earlier in this browser (localStorage may be closed: then Russian). */
export function storedLang(): Lang {
  try {
    const v = window.localStorage.getItem(LANG_STORAGE_KEY);
    return v === 'en' || v === 'ru' ? v : 'ru';
  } catch {
    return 'ru';
  }
}

export function storeLang(lang: Lang): void {
  try {
    window.localStorage.setItem(LANG_STORAGE_KEY, lang);
  } catch {
    // Storage closed (private mode, blocked site data): the choice lasts for this page only.
  }
}

export const I18nContext = createContext<I18n>({
  lang: 'ru',
  t: (key, params) => translate('ru', key, params),
  setLang: () => undefined,
});

export function useI18n(): I18n {
  return useContext(I18nContext);
}
