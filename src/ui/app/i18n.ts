/**
 * Interface strings of the application shell in Russian and English (docs/ui/APP_SHELL.md).
 *
 * `ru` is the reference dictionary; `en` must have exactly its keys (checked by the type and by
 * src/ui/app/__tests__/i18n.test.ts). Placeholders are written {name} and filled by translate().
 */

import { createContext, useContext } from 'react';

export type Lang = 'ru' | 'en';
export const LANGS: Lang[] = ['ru', 'en'];

export const ru = {
  'app.title': 'bendcore',
  'app.language': 'Язык',
  'start.title': 'Начало работы',
  'start.new': 'Создать',
  'start.newPart': 'Новая деталь',
  'start.newAssembly': 'Новая сборка',
  'start.open': 'Открыть',
  'start.openFile': 'Открыть файл…',
  'start.openHint': 'Деталь (.bcpart.json), сборка (.bcasm.json) или чертёж (.bcdrw.json)',
  'start.recent': 'Последние файлы',
  'start.recentEmpty': 'Пока пусто: сохранённые и открытые документы появятся здесь.',
  'start.recentUnavailable': 'Список последних файлов недоступен: хранилище браузера закрыто ({reason}).',
  'start.recentRemove': 'Убрать из списка',
  'start.drafts': 'Несохранённые черновики',
  'start.draftsHint': 'Черновики прошлой сессии сохранены автоматически. Восстановить их?',
  'start.restore': 'Восстановить',
  'start.discard': 'Отбросить',
  'start.savedAt': 'сохранён {time}',
  'tabs.start': 'Начало',
  'tabs.close': 'Закрыть вкладку',
  'tabs.closeUnsaved': 'В «{name}» есть несохранённые изменения. Закрыть вкладку? Черновик будет удалён.',
  'kind.part': 'Деталь',
  'kind.assembly': 'Сборка',
  'kind.drawing': 'Чертёж',
  'action.createDrawing': 'Создать чертёж',
  'action.openComponentPart': 'Открыть деталь компонента',
  'status.ready': 'Готово',
  'status.unsaved': 'Есть несохранённые изменения',
  'status.saved': 'Сохранено',
  'status.draftSaved': 'Черновик сохранён в {time}',
  'status.draftUnavailable': 'Автосохранение недоступно: {reason}',
  'status.noDocument': 'Нет открытого документа',
  'busy.working': 'Выполняется: {operation} · {seconds} с',
  'busy.cancel': 'Прервать',
  'busy.cancelled': 'Операция прервана. Документ восстановлен из снимка после последней завершённой операции; история отмены очищена.',
  'busy.restoreFailed': 'Операция прервана, но восстановить документ не удалось: {reason}',
  'op.rebuild': 'перестроение',
  'op.load': 'загрузка',
  'op.solve': 'решение сборки',
  'op.interference': 'проверка пересечений',
  'op.export': 'экспорт',
  'op.hlr': 'расчёт видов (HLR)',
  'op.other': 'операция ядра',
  'notify.error': 'Ошибка',
  'notify.dismiss': 'Скрыть',
  'notify.opened': 'Открыт файл {name}',
  'notify.unknownFile': 'Не удалось определить тип файла {name}: {reason}',
  'notify.componentNotPart': 'Компонент «{name}» не деталь bendcore (источник: {format}); открыть его как деталь нельзя.',
  'notify.selectComponent': 'Выберите компонент в дереве или во вьюпорте.',
  'menu.edit': 'Изменить',
  'menu.editSketch': 'Изменить эскиз',
  'menu.suppress': 'Погасить',
  'menu.unsuppress': 'Включить',
  'menu.remove': 'Удалить',
  'menu.moveUp': 'Выше',
  'menu.moveDown': 'Ниже',
  'menu.rollbackHere': 'Откатить сюда',
  'menu.fix': 'Закрепить',
  'menu.unfix': 'Освободить',
  'menu.hide': 'Скрыть',
  'menu.show': 'Показать',
  'menu.openPart': 'Открыть деталь',
  'keys.title': 'Горячие клавиши',
  'keys.list': 'Ctrl+Z отменить · Ctrl+Y повторить · Ctrl+S сохранить · Ctrl+O открыть · Esc отмена · Delete удалить · F показать всё',
} as const;

export type MessageKey = keyof typeof ru;

export const en: Record<MessageKey, string> = {
  'app.title': 'bendcore',
  'app.language': 'Language',
  'start.title': 'Get started',
  'start.new': 'New',
  'start.newPart': 'New part',
  'start.newAssembly': 'New assembly',
  'start.open': 'Open',
  'start.openFile': 'Open file…',
  'start.openHint': 'Part (.bcpart.json), assembly (.bcasm.json) or drawing (.bcdrw.json)',
  'start.recent': 'Recent files',
  'start.recentEmpty': 'Nothing yet: saved and opened documents will appear here.',
  'start.recentUnavailable': 'Recent files are unavailable: browser storage is closed ({reason}).',
  'start.recentRemove': 'Remove from the list',
  'start.drafts': 'Unsaved drafts',
  'start.draftsHint': 'Drafts of the previous session were saved automatically. Restore them?',
  'start.restore': 'Restore',
  'start.discard': 'Discard',
  'start.savedAt': 'saved {time}',
  'tabs.start': 'Start',
  'tabs.close': 'Close tab',
  'tabs.closeUnsaved': '"{name}" has unsaved changes. Close the tab? The draft will be deleted.',
  'kind.part': 'Part',
  'kind.assembly': 'Assembly',
  'kind.drawing': 'Drawing',
  'action.createDrawing': 'Create drawing',
  'action.openComponentPart': 'Open component part',
  'status.ready': 'Ready',
  'status.unsaved': 'Unsaved changes',
  'status.saved': 'Saved',
  'status.draftSaved': 'Draft saved at {time}',
  'status.draftUnavailable': 'Autosave is unavailable: {reason}',
  'status.noDocument': 'No open document',
  'busy.working': 'Running: {operation} · {seconds} s',
  'busy.cancel': 'Cancel',
  'busy.cancelled': 'The operation was cancelled. The document was restored from the snapshot taken after the last completed operation; the undo history is cleared.',
  'busy.restoreFailed': 'The operation was cancelled, but the document could not be restored: {reason}',
  'op.rebuild': 'rebuild',
  'op.load': 'loading',
  'op.solve': 'assembly solve',
  'op.interference': 'interference check',
  'op.export': 'export',
  'op.hlr': 'view computation (HLR)',
  'op.other': 'kernel operation',
  'notify.error': 'Error',
  'notify.dismiss': 'Dismiss',
  'notify.opened': 'Opened {name}',
  'notify.unknownFile': 'Cannot tell the type of {name}: {reason}',
  'notify.componentNotPart': 'Component "{name}" is not a bendcore part (source: {format}); it cannot be opened as a part.',
  'notify.selectComponent': 'Select a component in the tree or in the viewport.',
  'menu.edit': 'Edit',
  'menu.editSketch': 'Edit sketch',
  'menu.suppress': 'Suppress',
  'menu.unsuppress': 'Unsuppress',
  'menu.remove': 'Delete',
  'menu.moveUp': 'Move up',
  'menu.moveDown': 'Move down',
  'menu.rollbackHere': 'Roll back here',
  'menu.fix': 'Fix',
  'menu.unfix': 'Float',
  'menu.hide': 'Hide',
  'menu.show': 'Show',
  'menu.openPart': 'Open part',
  'keys.title': 'Keyboard shortcuts',
  'keys.list': 'Ctrl+Z undo · Ctrl+Y redo · Ctrl+S save · Ctrl+O open · Esc cancel · Delete delete · F fit all',
};

export const DICTIONARIES: Record<Lang, Record<MessageKey, string>> = { ru, en };

/** Placeholders {name} of a message. */
export function placeholders(message: string): string[] {
  return [...message.matchAll(/\{(\w+)\}/g)].map((m) => m[1]).sort();
}

export function translate(lang: Lang, key: MessageKey, params: Record<string, string | number> = {}): string {
  const message = DICTIONARIES[lang][key] ?? ru[key];
  return message.replace(/\{(\w+)\}/g, (whole, name: string) => (name in params ? String(params[name]) : whole));
}

export type Translate = (key: MessageKey, params?: Record<string, string | number>) => string;

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
