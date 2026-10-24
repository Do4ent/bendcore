/**
 * Comprehensive Bending Tables - Production Ready
 * Material properties, K-factors, bend forces, tooling recommendations
 */

export const BENDING_TABLES = {
  materials: {
    steel: {
      name: 'Сталь (ASTM A36)',
      density: 7850,
      elasticModulus: 210000,
      yieldStrength: 250,
      ultimateStrength: 400,
      thickness_range: [0.5, 5.0],
      elongation: 23,
      hardness: 'B70'
    },
    aluminum: {
      name: 'Алюминий 6061',
      density: 2700,
      elasticModulus: 69000,
      yieldStrength: 275,
      ultimateStrength: 310,
      thickness_range: [0.5, 4.0],
      elongation: 17,
      hardness: 'B95'
    },
    stainless: {
      name: 'Нержавеющая сталь 304',
      density: 8000,
      elasticModulus: 193000,
      yieldStrength: 170,
      ultimateStrength: 515,
      thickness_range: [0.5, 3.0],
      elongation: 70,
      hardness: 'B80'
    },
    copper: {
      name: 'Медь (Cu-ETP)',
      density: 8960,
      elasticModulus: 117000,
      yieldStrength: 70,
      ultimateStrength: 200,
      thickness_range: [0.5, 2.5],
      elongation: 45,
      hardness: 'B35'
    },
    brass: {
      name: 'Латунь (CuZn)',
      density: 8400,
      elasticModulus: 103000,
      yieldStrength: 160,
      ultimateStrength: 370,
      thickness_range: [0.5, 3.0],
      elongation: 30,
      hardness: 'B65'
    },
    titanium: {
      name: 'Титан Grade 2',
      density: 4510,
      elasticModulus: 103000,
      yieldStrength: 345,
      ultimateStrength: 435,
      thickness_range: [0.5, 2.0],
      elongation: 20,
      hardness: 'C32'
    }
  },

  k_factors: {
    steel: [
      { thickness: 0.5, k_factor: 0.31 },
      { thickness: 1.0, k_factor: 0.32 },
      { thickness: 1.5, k_factor: 0.33 },
      { thickness: 2.0, k_factor: 0.34 },
      { thickness: 3.0, k_factor: 0.35 },
      { thickness: 5.0, k_factor: 0.37 }
    ],
    aluminum: [
      { thickness: 0.5, k_factor: 0.32 },
      { thickness: 1.0, k_factor: 0.33 },
      { thickness: 1.5, k_factor: 0.34 },
      { thickness: 2.0, k_factor: 0.35 },
      { thickness: 3.0, k_factor: 0.36 },
      { thickness: 4.0, k_factor: 0.37 }
    ],
    stainless: [
      { thickness: 0.5, k_factor: 0.30 },
      { thickness: 1.0, k_factor: 0.31 },
      { thickness: 1.5, k_factor: 0.32 },
      { thickness: 2.0, k_factor: 0.33 },
      { thickness: 3.0, k_factor: 0.34 }
    ],
    copper: [
      { thickness: 0.5, k_factor: 0.33 },
      { thickness: 1.0, k_factor: 0.34 },
      { thickness: 1.5, k_factor: 0.35 },
      { thickness: 2.0, k_factor: 0.36 }
    ],
    brass: [
      { thickness: 0.5, k_factor: 0.32 },
      { thickness: 1.0, k_factor: 0.33 },
      { thickness: 1.5, k_factor: 0.34 },
      { thickness: 2.0, k_factor: 0.35 }
    ],
    titanium: [
      { thickness: 0.5, k_factor: 0.31 },
      { thickness: 1.0, k_factor: 0.32 },
      { thickness: 1.5, k_factor: 0.33 },
      { thickness: 2.0, k_factor: 0.34 }
    ]
  },

  min_bend_radius: {
    steel: { ratio: 1.0, note: '1t минимум' },
    aluminum: { ratio: 0.8, note: '0.8t минимум' },
    stainless: { ratio: 1.5, note: '1.5t минимум' },
    copper: { ratio: 1.2, note: '1.2t минимум' },
    brass: { ratio: 1.0, note: '1t минимум' },
    titanium: { ratio: 1.5, note: '1.5t минимум' }
  },

  bend_force_tables: {
    steel: [
      { thickness: 0.5, width: 100, force: 15, unit: 'тонны' },
      { thickness: 1.0, width: 100, force: 30, unit: 'тонны' },
      { thickness: 1.5, width: 100, force: 45, unit: 'тонны' },
      { thickness: 2.0, width: 100, force: 60, unit: 'тонны' },
      { thickness: 3.0, width: 100, force: 90, unit: 'тонны' },
      { thickness: 5.0, width: 100, force: 150, unit: 'тонны' }
    ],
    aluminum: [
      { thickness: 0.5, width: 100, force: 8, unit: 'тонны' },
      { thickness: 1.0, width: 100, force: 16, unit: 'тонны' },
      { thickness: 1.5, width: 100, force: 24, unit: 'тонны' },
      { thickness: 2.0, width: 100, force: 32, unit: 'тонны' },
      { thickness: 3.0, width: 100, force: 48, unit: 'тонны' }
    ],
    stainless: [
      { thickness: 0.5, width: 100, force: 18, unit: 'тонны' },
      { thickness: 1.0, width: 100, force: 36, unit: 'тонны' },
      { thickness: 1.5, width: 100, force: 54, unit: 'тонны' },
      { thickness: 2.0, width: 100, force: 72, unit: 'тонны' }
    ]
  },

  tool_recommendations: {
    'steel_0.5': { punch: 'P-4', die: 'D-8', pressure: 50, capacity: 25 },
    'steel_1.0': { punch: 'P-6', die: 'D-12', pressure: 80, capacity: 50 },
    'steel_1.5': { punch: 'P-8', die: 'D-16', pressure: 120, capacity: 75 },
    'steel_2.0': { punch: 'P-10', die: 'D-20', pressure: 150, capacity: 100 },
    'aluminum_0.5': { punch: 'P-4-AL', die: 'D-8-AL', pressure: 30, capacity: 15 },
    'aluminum_1.0': { punch: 'P-6-AL', die: 'D-12-AL', pressure: 50, capacity: 30 },
    'stainless_0.5': { punch: 'P-4-SS', die: 'D-8-SS', pressure: 60, capacity: 30 },
    'stainless_1.0': { punch: 'P-6-SS', die: 'D-12-SS', pressure: 100, capacity: 60 }
  },

  bend_angles: {
    '45': { type: '45 градусов', toolOffset: 0.5 },
    '90': { type: '90 градусов', toolOffset: 1.0 },
    '135': { type: '135 градусов', toolOffset: 0.5 },
    '180': { type: '180 градусов', toolOffset: 2.0 }
  },

  standards: {
    DIN: { spec: 'DIN 6935', description: 'Правила гибки листовых материалов', year: 2012 },
    ISO: { spec: 'ISO 13715', description: 'Технические спецификации', year: 2015 },
    AISC: { spec: 'AISC Manual', description: 'Конструкционная сталь', year: 2016 },
    AWS: { spec: 'AWS D1.1', description: 'Сварка конструкционной стали', year: 2020 }
  }
};

export class BendingTableManager {
  /**
   * Get K-factor для материала и толщины
   */
  static getKFactor(material: string, thickness: number): number {
    const table = BENDING_TABLES.k_factors[material as keyof typeof BENDING_TABLES.k_factors];
    if (!table) {
      console.warn(`K-factor таблица для ${material} не найдена`);
      return 0.33; // Default value
    }

    // Linear interpolation
    for (let i = 0; i < table.length - 1; i++) {
      const t1 = table[i].thickness;
      const t2 = table[i + 1].thickness;

      if (thickness >= t1 && thickness <= t2) {
        const k1 = table[i].k_factor;
        const k2 = table[i + 1].k_factor;
        const ratio = (thickness - t1) / (t2 - t1);
        return k1 + (k2 - k1) * ratio;
      }
    }

    // Extrapolate if beyond range
    if (thickness < table[0].thickness) {
      return table[0].k_factor;
    }
    return table[table.length - 1].k_factor;
  }

  /**
   * Get минимальный радиус гибки
   */
  static getMinBendRadius(material: string, thickness: number): number {
    const info = BENDING_TABLES.min_bend_radius[material as keyof typeof BENDING_TABLES.min_bend_radius];
    if (!info) {
      return thickness; // Default: 1t
    }
    return thickness * info.ratio;
  }

  /**
   * Calculate bent длину трубы (расчет развертки)
   */
  static calculateBentLength(
    radius: number,
    angle: number,
    thickness: number,
    material: string
  ): number {
    const kFactor = this.getKFactor(material, thickness);
    const angleRad = (angle * Math.PI) / 180;
    const neutralRadius = radius + kFactor * thickness;
    return neutralRadius * angleRad;
  }

  /**
   * Get силу гибки
   */
  static getBendForce(material: string, thickness: number, width: number): number {
    const forces = BENDING_TABLES.bend_force_tables[material as keyof typeof BENDING_TABLES.bend_force_tables];
    if (!forces) {
      return 0;
    }

    // Find closest thickness
    let closestForce = forces[0];
    for (const f of forces) {
      if (Math.abs(f.thickness - thickness) < Math.abs(closestForce.thickness - thickness)) {
        closestForce = f;
      }
    }

    // Scale by width (linear approximation)
    return closestForce.force * (width / 100);
  }

  /**
   * Get рекомендацию по инструменту
   */
  static getToolRecommendation(material: string, thickness: number): any {
    const thicknessStr = thickness.toFixed(1);
    const key = `${material}_${thicknessStr}`;

    return (
      BENDING_TABLES.tool_recommendations[key as keyof typeof BENDING_TABLES.tool_recommendations] ||
      BENDING_TABLES.tool_recommendations[`${material}_1.0` as keyof typeof BENDING_TABLES.tool_recommendations]
    );
  }

  /**
   * Get свойства материала
   */
  static getMaterialProperties(material: string): any {
    return BENDING_TABLES.materials[material as keyof typeof BENDING_TABLES.materials] || null;
  }

  /**
   * Get стандарт
   */
  static getStandard(standardId: string): any {
    return BENDING_TABLES.standards[standardId as keyof typeof BENDING_TABLES.standards] || null;
  }

  /**
   * Validate гибки (can this material/thickness be bent?)
   */
  static validateBending(
    thickness: number,
    bendRadius: number,
    material: string,
    angle: number
  ): { valid: boolean; issues: string[] } {
    const issues: string[] = [];

    // Check thickness range
    const matProps = this.getMaterialProperties(material);
    if (matProps) {
      const [minThick, maxThick] = matProps.thickness_range;
      if (thickness < minThick || thickness > maxThick) {
        issues.push(
          `Толщина ${thickness}мм вне диапазона ${minThick}-${maxThick}мм для ${material}`
        );
      }
    }

    // Check bend radius
    const minRadius = this.getMinBendRadius(material, thickness);
    if (bendRadius < minRadius) {
      issues.push(`Радиус гибки ${bendRadius}мм меньше минимума ${minRadius}мм`);
    }

    // Check angle validity
    if (angle < 1 || angle > 180) {
      issues.push(`Угол ${angle}° должен быть между 1° и 180°`);
    }

    return {
      valid: issues.length === 0,
      issues
    };
  }

  /**
   * Get все доступные материалы
   */
  static getAvailableMaterials(): string[] {
    return Object.keys(BENDING_TABLES.materials);
  }

  /**
   * Calculate развертку (flat blank) для согнутой трубы
   */
  static calculateFlatBlank(
    radius: number,
    angle: number,
    thickness: number,
    material: string,
    straightSections: { length: number }[] = []
  ): number {
    let totalLength = 0;

    // Add bent section
    totalLength += this.calculateBentLength(radius, angle, thickness, material);

    // Add straight sections
    for (const section of straightSections) {
      totalLength += section.length;
    }

    // Account for spring-back (приблизительно)
    const springback = 1 - Math.cos((3 * Math.PI) / 180); // 3° typical springback
    totalLength *= 1 + springback;

    return totalLength;
  }
}

export default BendingTableManager;
