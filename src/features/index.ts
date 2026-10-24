/**
 * BendCore CAD - ПОЛНАЯ РЕАЛИЗАЦИЯ ВСЕХ 4 ЭТАПОВ
 *
 * ЭТАП 1: Базовое моделирование (Pad, Pocket, Fillet, Chamfer, Mirror, Pattern)
 * ЭТАП 2: Продвинутое моделирование (Loft, Sweep, Surface)
 * ЭТАП 3: Сборка и симуляция (Assembly, FEA Simulator Mock)
 * ЭТАП 4: Enterprise функции (Pipe Routing, BOM)
 */

// ЭТАП 1: Базовое моделирование
export * from './modeling';

// ЭТАП 2: Продвинутое моделирование
export * from './advanced';

// ЭТАП 3: Сборка и симуляция
export * from './assembly';
export * from './simulation';

// ЭТАП 4: Enterprise функции
export * from './enterprise';

// Главная панель со всеми функциями
export { BendCoreFeaturesApp } from './BendCoreFeaturesApp';
