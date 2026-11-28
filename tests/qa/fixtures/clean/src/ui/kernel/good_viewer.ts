// Фикстура самотеста check_no_stubs: законные конструкции TypeScript, похожие на заглушки.
export interface KernelApi {
  volume(handle: number): number;
  dispose(handle: number): void;
}

export abstract class Tool {
  abstract apply(face: number): boolean;
  constructor(protected readonly name: string) {}
  // Свойство без параметров с постоянным значением — не заглушка.
  isModal(): boolean { return false; }
  get label(): string { return this.name; }
}

export function faceArea(api: KernelApi, face: number): number {
  return api.volume(face) / 2;
}

export function attach(el: HTMLElement, cb: () => void) {
  // Пустой обработчик как значение по умолчанию и пустой catch — не объявленная функциональность.
  el.addEventListener('click', () => {});
  const off = el.onclick ?? (() => {});
  try {
    cb();
  } catch {}
  return off;
}

export const emptyTemplate = '{}';
export const hint = `function f(x) {}`;
export const input = { placeholder: 'STEP file', value: '' };
// Пустой объект как начальное состояние — данные, а не тело функции.
export const initialSelection = {};
