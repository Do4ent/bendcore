/**
 * BendCore Sheet Metal Design Calculator
 * Расчёты развёрток, K-факторов, припусков на изгиб
 */

export type MaterialType = 'steel' | 'aluminum' | 'copper' | 'stainless' | 'titanium';

export interface SheetMetalFeature {
  id: string;
  name: string;
  baseThickness: number; // мм
  bendRadius: number; // мм
  bendAngle: number; // градусы (0-180)
  kFactor?: number; // Custom K-factor
  material: MaterialType;
  createdAt: Date;
}

export interface UnfoldResult {
  flatLength: number; // развёрнутая длина (мм)
  bendAllowance: number; // припуск на изгиб (мм)
  minimumBendRadius: number; // минимальный радиус (мм)
  developedLength: number; // итоговая развёрнутая длина (мм)
  kFactorUsed: number; // использованный K-фактор
  materialLoss: number; // потеря на рез (мм)
  spaceNeeded: [number, number]; // требуемое пространство [ширина, длина]
  springbackAngle: number; // угол пружинения (°)
  recommendedPunchRadius: number; // рекомендуемый радиус пуансона (мм)
  processingTime: number; // приблизительное время (сек)
}

/**
 * Таблицы K-факторов для разных материалов и толщин
 * K-фактор = отношение нейтральной оси к толщине материала
 * Используется в формуле: BA = π × (R + t × K) × θ / 180
 */
const K_FACTORS: Record<MaterialType, Record<number, number>> = {
  steel: {
    0.5: 0.31, 0.8: 0.32, 1.0: 0.32, 1.2: 0.32, 1.5: 0.33,
    2.0: 0.35, 2.5: 0.36, 3.0: 0.36, 4.0: 0.37, 5.0: 0.38,
  },
  aluminum: {
    0.5: 0.32, 0.8: 0.33, 1.0: 0.33, 1.2: 0.34, 1.5: 0.35,
    2.0: 0.36, 2.5: 0.37, 3.0: 0.38, 4.0: 0.39, 5.0: 0.40,
  },
  stainless: {
    0.5: 0.33, 0.8: 0.34, 1.0: 0.34, 1.2: 0.35, 1.5: 0.36,
    2.0: 0.37, 2.5: 0.38, 3.0: 0.39, 4.0: 0.40, 5.0: 0.41,
  },
  copper: {
    0.5: 0.30, 0.8: 0.30, 1.0: 0.31, 1.2: 0.31, 1.5: 0.32,
    2.0: 0.33, 2.5: 0.34, 3.0: 0.35, 4.0: 0.36, 5.0: 0.37,
  },
  titanium: {
    0.5: 0.34, 0.8: 0.35, 1.0: 0.35, 1.2: 0.36, 1.5: 0.37,
    2.0: 0.38, 2.5: 0.39, 3.0: 0.40, 4.0: 0.41, 5.0: 0.42,
  },
};

/**
 * Минимальные радиусы изгиба (в толщинах материала t)
 */
const MIN_BEND_RADIUS_MULTIPLIER: Record<MaterialType, number> = {
  steel: 1.0,
  aluminum: 0.8,
  stainless: 1.5,
  copper: 1.2,
  titanium: 2.0,
};

/**
 * Коэффициенты пружинения (springback) в процентах
 */
const SPRINGBACK_FACTORS: Record<MaterialType, number> = {
  steel: 8,
  aluminum: 5,
  stainless: 10,
  copper: 7,
  titanium: 12,
};

/**
 * Рекомендуемые радиусы пуансона для разных толщин
 */
const PUNCH_RADIUS_RECOMMENDATION: Record<MaterialType, Record<number, number>> = {
  steel: {
    0.5: 0.5, 1.0: 1.0, 1.5: 1.5, 2.0: 2.0, 3.0: 3.0, 5.0: 5.0,
  },
  aluminum: {
    0.5: 0.5, 1.0: 1.0, 1.5: 1.5, 2.0: 2.0, 3.0: 3.0, 5.0: 5.0,
  },
  stainless: {
    0.5: 0.8, 1.0: 1.2, 1.5: 2.0, 2.0: 2.5, 3.0: 3.5, 5.0: 5.5,
  },
  copper: {
    0.5: 0.5, 1.0: 1.0, 1.5: 1.5, 2.0: 2.0, 3.0: 3.0, 5.0: 5.0,
  },
  titanium: {
    0.5: 1.0, 1.0: 1.5, 1.5: 2.5, 2.0: 3.0, 3.0: 4.0, 5.0: 6.0,
  },
};

export class SheetMetalCalculator {
  /**
   * Получить K-фактор из таблицы
   * Использует интерполяцию между известными значениями
   */
  static getKFactor(thickness: number, material: MaterialType): number {
    const table = K_FACTORS[material];
    const thicknesses = Object.keys(table).map(Number).sort((a, b) => a - b);

    // Точное совпадение
    if (table[thickness]) {
      return table[thickness];
    }

    // Интерполяция
    for (let i = 0; i < thicknesses.length - 1; i++) {
      if (thickness >= thicknesses[i] && thickness <= thicknesses[i + 1]) {
        const t1 = thicknesses[i];
        const t2 = thicknesses[i + 1];
        const k1 = table[t1];
        const k2 = table[t2];
        const ratio = (thickness - t1) / (t2 - t1);
        return k1 + (k2 - k1) * ratio;
      }
    }

    // Если толщина вне диапазона, используем ближайшее значение
    if (thickness < thicknesses[0]) {
      return table[thicknesses[0]];
    }
    return table[thicknesses[thicknesses.length - 1]];
  }

  /**
   * Получить минимальный радиус изгиба
   */
  static getMinimumBendRadius(thickness: number, material: MaterialType): number {
    return thickness * MIN_BEND_RADIUS_MULTIPLIER[material];
  }

  /**
   * Рассчитать припуск на изгиб (Bend Allowance)
   * BA = π × (R + t × K) × θ / 180
   * где:
   *   R - радиус изгиба
   *   t - толщина материала
   *   K - K-фактор
   *   θ - угол изгиба (градусы)
   */
  static calculateBendAllowance(
    radius: number,
    thickness: number,
    angle: number,
    kFactor: number
  ): number {
    const angleRad = (angle * Math.PI) / 180;
    return (radius + thickness * kFactor) * angleRad;
  }

  /**
   * Рассчитать развёрнутую длину листа
   * DL = L1 + L2 + BA
   */
  static calculateDevelopedLength(
    length1: number,
    length2: number,
    bendAllowance: number
  ): number {
    return length1 + length2 + bendAllowance;
  }

  /**
   * Рассчитать угол пружинения (springback)
   * Материал пытается вернуться в исходную форму после изгиба
   */
  static calculateSpringback(
    bendAngle: number,
    material: MaterialType
  ): number {
    const springbackPercent = SPRINGBACK_FACTORS[material];
    return (bendAngle * springbackPercent) / 100;
  }

  /**
   * Получить рекомендуемый радиус пуансона
   */
  static getRecommendedPunchRadius(thickness: number, material: MaterialType): number {
    const table = PUNCH_RADIUS_RECOMMENDATION[material];
    const thicknesses = Object.keys(table).map(Number).sort((a, b) => a - b);

    if (table[thickness]) {
      return table[thickness];
    }

    // Интерполяция
    for (let i = 0; i < thicknesses.length - 1; i++) {
      if (thickness >= thicknesses[i] && thickness <= thicknesses[i + 1]) {
        const t1 = thicknesses[i];
        const t2 = thicknesses[i + 1];
        const r1 = table[t1];
        const r2 = table[t2];
        const ratio = (thickness - t1) / (t2 - t1);
        return r1 + (r2 - r1) * ratio;
      }
    }

    return table[thicknesses[thicknesses.length - 1]];
  }

  /**
   * Рассчитать приблизительное время обработки (в секундах)
   */
  static calculateProcessingTime(
    thickness: number,
    angle: number,
    bendRadius: number
  ): number {
    // Базовое время в зависимости от толщины
    const baseTime = Math.pow(thickness, 1.5) * 2;
    // Время зависит от угла изгиба
    const angleTime = (angle / 90) * baseTime * 0.5;
    // Время зависит от радиуса (тесные радиусы требуют больше времени)
    const radiusTime = bendRadius < thickness ? baseTime * 0.3 : baseTime * 0.1;
    return baseTime + angleTime + radiusTime;
  }

  /**
   * Полный расчёт развёртки листа
   */
  static unfoldPattern(
    feature: SheetMetalFeature,
    length1: number,
    length2: number
  ): UnfoldResult {
    // Получить K-фактор (использовать custom или из таблицы)
    const kFactor = feature.kFactor ?? this.getKFactor(feature.baseThickness, feature.material);

    // Минимальный радиус
    const minRadius = this.getMinimumBendRadius(feature.baseThickness, feature.material);

    // Проверка, что радиус изгиба не меньше минимального
    if (feature.bendRadius < minRadius) {
      console.warn(
        `⚠️ Радиус изгиба ${feature.bendRadius} меньше минимального ${minRadius.toFixed(2)}мм`
      );
    }

    // Припуск на изгиб
    const bendAllowance = this.calculateBendAllowance(
      feature.bendRadius,
      feature.baseThickness,
      feature.bendAngle,
      kFactor
    );

    // Развёрнутая длина
    const developedLength = this.calculateDevelopedLength(length1, length2, bendAllowance);

    // Пружинение
    const springbackAngle = this.calculateSpringback(feature.bendAngle, feature.material);

    // Рекомендуемый радиус пуансона
    const recommendedPunchRadius = this.getRecommendedPunchRadius(
      feature.baseThickness,
      feature.material
    );

    // Время обработки
    const processingTime = this.calculateProcessingTime(
      feature.baseThickness,
      feature.bendAngle,
      feature.bendRadius
    );

    // Потеря на рез (kerf) - примерно 0.15-0.3 мм в зависимости от толщины
    const materialLoss = Math.min(feature.baseThickness * 0.3, 0.5);

    // Требуемое пространство (с запасом 10%)
    const spaceNeeded: [number, number] = [
      Math.ceil(developedLength * 1.1),
      Math.ceil(length2 * 1.1),
    ];

    return {
      flatLength: parseFloat(developedLength.toFixed(2)),
      bendAllowance: parseFloat(bendAllowance.toFixed(2)),
      minimumBendRadius: parseFloat(minRadius.toFixed(2)),
      developedLength: parseFloat(developedLength.toFixed(2)),
      kFactorUsed: parseFloat(kFactor.toFixed(4)),
      materialLoss: parseFloat(materialLoss.toFixed(2)),
      spaceNeeded,
      springbackAngle: parseFloat(springbackAngle.toFixed(2)),
      recommendedPunchRadius: parseFloat(recommendedPunchRadius.toFixed(2)),
      processingTime: parseFloat(processingTime.toFixed(1)),
    };
  }

  /**
   * Проверить возможность изгиба (DFM - Design for Manufacturing)
   */
  static validateBend(feature: SheetMetalFeature): { valid: boolean; warnings: string[] } {
    const warnings: string[] = [];

    const minRadius = this.getMinimumBendRadius(feature.baseThickness, feature.material);
    if (feature.bendRadius < minRadius) {
      warnings.push(
        `⚠️ Радиус изгиба ${feature.bendRadius}мм меньше минимального ${minRadius.toFixed(2)}мм для материала`
      );
    }

    if (feature.bendAngle < 0 || feature.bendAngle > 180) {
      warnings.push(`❌ Угол изгиба должен быть от 0° до 180°, получено ${feature.bendAngle}°`);
      return { valid: false, warnings };
    }

    if (feature.bendAngle > 170) {
      warnings.push(
        `⚠️ Угол изгиба близок к 180° - может быть сложен в производстве`
      );
    }

    if (feature.bendRadius < feature.baseThickness) {
      warnings.push(
        `⚠️ Радиус изгиба меньше толщины - возможны проблемы с формованием`
      );
    }

    return { valid: warnings.length === 0, warnings };
  }
}
