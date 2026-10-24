/**
 * API маршруты для комбинированного 3D смещения трубопровода
 *
 * Endpoints:
 * - POST /api/bendcore/combined-3d-offset/calculate
 * - POST /api/bendcore/combined-3d-offset/validate
 * - POST /api/bendcore/combined-3d-offset/generate-cnc
 * - POST /api/bendcore/combined-3d-offset/export
 */

import { Router, Request, Response } from 'express';

const router = Router();

// ============================================================================
// Интерфейсы
// ============================================================================

interface Point3D {
  x: number;
  y: number;
  z: number;
}

interface Parameters {
  start_point: Point3D;
  end_point: Point3D;
  outer_diameter: number;
  wall_thickness: number;
  bend_radius: number;
  num_bends: number;
  distribution: string;
  inclination_angle: number;
  springback_correction: boolean;
  material: string;
  custom_bend_positions?: number[];
}

interface BendDetail {
  position: Point3D;
  angle_xy: number;
  angle_z: number;
  length: number;
  radius: number;
  springback_correction: number;
  bend_index: number;
}

interface SectionDetail {
  start: Point3D;
  end: Point3D;
  length: number;
  slope_percent: number;
  section_index: number;
}

interface CalculationResult {
  success: boolean;
  error_message: string;
  total_length: number;
  total_distance_3d: number;
  bends: BendDetail[];
  sections: SectionDetail[];
  trajectory_3d: Point3D[];
  cnc_code: string;
  feasibility_score: number;
  warnings: string[];
}

// ============================================================================
// Вспомогательные функции
// ============================================================================

function distance3D(p1: Point3D, p2: Point3D): number {
  const dx = p2.x - p1.x;
  const dy = p2.y - p1.y;
  const dz = p2.z - p1.z;
  return Math.sqrt(dx * dx + dy * dy + dz * dz);
}

function calculateAngles(dx: number, dy: number, dz: number) {
  // Угол в XY плоскости (azimuth)
  const angle_xy = (Math.atan2(dy, dx) * 180) / Math.PI;

  // Угол наклона (elevation)
  const horizontal = Math.sqrt(dx * dx + dy * dy);
  const angle_z = (Math.atan2(dz, horizontal) * 180) / Math.PI;

  return { angle_xy: angle_xy >= 0 ? angle_xy : angle_xy + 360, angle_z };
}

function getSpringbackCorrection(material: string, radius: number): number {
  let baseCorrection = 0.02; // Steel

  if (material === 'aluminum') {
    baseCorrection = 0.03;
  } else if (material === 'copper') {
    baseCorrection = 0.025;
  }

  return baseCorrection * (1.0 + radius / 100.0);
}

function validateParameters(params: Parameters): { valid: boolean; errors: string[] } {
  const errors: string[] = [];
  const MIN_DISTANCE_3D = 10.0;
  const MAX_DISTANCE_3D = 10000.0;
  const MAX_INCLINATION_ANGLE = 85.0;

  // Проверка 3D расстояния
  const dist3D = distance3D(params.start_point, params.end_point);
  if (dist3D < MIN_DISTANCE_3D) {
    errors.push(`3D расстояние слишком малое (< ${MIN_DISTANCE_3D} мм)`);
  }
  if (dist3D > MAX_DISTANCE_3D) {
    errors.push(`3D расстояние слишком большое (> ${MAX_DISTANCE_3D} мм)`);
  }

  // Проверка диаметра
  if (params.outer_diameter <= params.wall_thickness * 2) {
    errors.push('Внешний диаметр должен быть > 2 × толщина стенки');
  }
  if (params.outer_diameter < 1 || params.outer_diameter > 500) {
    errors.push('Внешний диаметр должен быть 1-500 мм');
  }

  // Проверка толщины стенки
  if (params.wall_thickness < 0.1 || params.wall_thickness > 50) {
    errors.push('Толщина стенки должна быть 0.1-50 мм');
  }

  // Проверка количества гибок
  if (params.num_bends < 2 || params.num_bends > 5) {
    errors.push('Количество гибок должно быть 2-5');
  }

  // Проверка распределения
  if (params.distribution !== 'equal' && params.distribution !== 'custom') {
    errors.push("Тип распределения должен быть 'equal' или 'custom'");
  }

  // Проверка угла наклона
  if (Math.abs(params.inclination_angle) > MAX_INCLINATION_ANGLE) {
    errors.push(`Угол наклона должен быть ±${MAX_INCLINATION_ANGLE} градусов`);
  }

  // Проверка материала
  if (!['steel', 'aluminum', 'copper'].includes(params.material)) {
    errors.push("Материал должен быть 'steel', 'aluminum' или 'copper'");
  }

  return { valid: errors.length === 0, errors };
}

// ============================================================================
// POST /api/bendcore/combined-3d-offset/calculate
// ============================================================================

router.post('/calculate', (req: Request, res: Response) => {
  try {
    const params: Parameters = req.body;

    // Валидация параметров
    const validation = validateParameters(params);
    if (!validation.valid) {
      return res.status(400).json({
        success: false,
        error_message: `Ошибка валидации: ${validation.errors[0]}`,
        total_length: 0,
        total_distance_3d: 0,
        bends: [],
        sections: [],
        trajectory_3d: [],
        cnc_code: '',
        feasibility_score: 0,
        warnings: [],
      } as CalculationResult);
    }

    // Расчёт 3D расстояния
    const total_distance_3d = distance3D(params.start_point, params.end_point);

    // Распределение гибок
    const dx = params.end_point.x - params.start_point.x;
    const dy = params.end_point.y - params.start_point.y;
    const dz = params.end_point.z - params.start_point.z;

    const bend_positions: Point3D[] = [];
    if (params.distribution === 'equal') {
      for (let i = 1; i <= params.num_bends; ++i) {
        const t = i / (params.num_bends + 1);
        bend_positions.push({
          x: params.start_point.x + dx * t,
          y: params.start_point.y + dy * t,
          z: params.start_point.z + dz * t,
        });
      }
    } else if (params.distribution === 'custom' && params.custom_bend_positions) {
      for (const t of params.custom_bend_positions) {
        bend_positions.push({
          x: params.start_point.x + dx * t,
          y: params.start_point.y + dy * t,
          z: params.start_point.z + dz * t,
        });
      }
    }

    // Расчёт деталей гибок
    const bends: BendDetail[] = [];
    const { angle_xy, angle_z } = calculateAngles(dx, dy, dz);

    for (let i = 0; i < bend_positions.length; ++i) {
      const bend_pos = bend_positions[i];
      const fdx = bend_pos.x - params.start_point.x;
      const fdy = bend_pos.y - params.start_point.y;
      const fdz = bend_pos.z - params.start_point.z;
      const length = Math.sqrt(fdx * fdx + fdy * fdy + fdz * fdz);

      let radius = params.outer_diameter / 2 + params.wall_thickness;
      let springback_correction = 0;

      if (params.springback_correction) {
        springback_correction = getSpringbackCorrection(params.material, radius);
        radius *= 1 + springback_correction;
      }

      bends.push({
        position: bend_pos,
        angle_xy,
        angle_z,
        length,
        radius,
        springback_correction,
        bend_index: i + 1,
      });
    }

    // Расчёт секций
    const all_points = [params.start_point, ...bend_positions, params.end_point];
    const sections: SectionDetail[] = [];
    let total_length = 0;

    for (let i = 0; i < all_points.length - 1; ++i) {
      const start = all_points[i];
      const end = all_points[i + 1];
      const sdx = end.x - start.x;
      const sdy = end.y - start.y;
      const sdz = end.z - start.z;
      const section_length = Math.sqrt(sdx * sdx + sdy * sdy + sdz * sdz);

      const horizontal = Math.sqrt(sdx * sdx + sdy * sdy);
      const slope_percent = horizontal > 0 ? (Math.abs(sdz) / horizontal) * 100 : 0;

      sections.push({
        start,
        end,
        length: section_length,
        slope_percent,
        section_index: i,
      });

      total_length += section_length;
    }

    // Добавление длины гибок
    for (const bend of bends) {
      const bend_length = bend.radius * Math.PI / 2;
      total_length += bend_length;
    }

    // Траектория для визуализации
    const trajectory_3d = all_points;

    // Расчёт feasibility
    let feasibility_score = 100;
    const min_radius = params.outer_diameter * 0.5;
    for (const bend of bends) {
      if (bend.radius < min_radius) {
        feasibility_score -= 20;
      }
    }
    if (Math.abs(params.inclination_angle) > 60) {
      feasibility_score -= 15;
    }
    if (params.wall_thickness > params.outer_diameter / 3) {
      feasibility_score -= 10;
    }
    if (params.num_bends > 4) {
      feasibility_score -= 5;
    }
    feasibility_score = Math.max(0, feasibility_score);

    // Генерация G-code (заглушка)
    const cnc_code = generateCNCCode(
      params,
      total_distance_3d,
      total_length,
      bends,
      trajectory_3d,
      feasibility_score
    );

    // Предупреждения
    const warnings: string[] = [];
    if (feasibility_score < 60) {
      warnings.push(`Низкая оценка feasibility: ${Math.round(feasibility_score)}%`);
    }
    if (params.inclination_angle > 45) {
      warnings.push('Высокий угол наклона может потребовать специальной оснастки');
    }

    const result: CalculationResult = {
      success: true,
      error_message: '',
      total_length,
      total_distance_3d,
      bends,
      sections,
      trajectory_3d,
      cnc_code,
      feasibility_score,
      warnings,
    };

    res.json(result);
  } catch (error) {
    res.status(500).json({
      success: false,
      error_message: `Исключение: ${error instanceof Error ? error.message : 'Unknown error'}`,
      total_length: 0,
      total_distance_3d: 0,
      bends: [],
      sections: [],
      trajectory_3d: [],
      cnc_code: '',
      feasibility_score: 0,
      warnings: [],
    } as CalculationResult);
  }
});

// ============================================================================
// POST /api/bendcore/combined-3d-offset/validate
// ============================================================================

router.post('/validate', (req: Request, res: Response) => {
  try {
    const params: Parameters = req.body;
    const validation = validateParameters(params);

    res.json({
      valid: validation.valid,
      errors: validation.errors,
    });
  } catch (error) {
    res.status(500).json({
      valid: false,
      errors: [error instanceof Error ? error.message : 'Unknown error'],
    });
  }
});

// ============================================================================
// POST /api/bendcore/combined-3d-offset/generate-cnc
// ============================================================================

router.post('/generate-cnc', (req: Request, res: Response) => {
  try {
    const { params, calculation_result } = req.body;

    const cnc_code = generateCNCCode(
      params,
      calculation_result.total_distance_3d,
      calculation_result.total_length,
      calculation_result.bends,
      calculation_result.trajectory_3d,
      calculation_result.feasibility_score
    );

    res.json({ cnc_code });
  } catch (error) {
    res.status(500).json({
      error: error instanceof Error ? error.message : 'Unknown error',
    });
  }
});

// ============================================================================
// POST /api/bendcore/combined-3d-offset/export
// ============================================================================

router.post('/export', (req: Request, res: Response) => {
  try {
    const { format, result } = req.body;

    let content = '';
    let filename = '';

    if (format === 'nc' || format === 'gcode') {
      content = result.cnc_code;
      filename = 'tube_bend.nc';
    } else if (format === 'json') {
      content = JSON.stringify(result, null, 2);
      filename = 'tube_bend.json';
    } else if (format === 'csv') {
      content = generateCSVReport(result);
      filename = 'tube_bend.csv';
    }

    res.setHeader('Content-Type', 'text/plain; charset=utf-8');
    res.setHeader('Content-Disposition', `attachment; filename="${filename}"`);
    res.send(content);
  } catch (error) {
    res.status(500).json({
      error: error instanceof Error ? error.message : 'Unknown error',
    });
  }
});

// ============================================================================
// Вспомогательные функции для генерации кода
// ============================================================================

function generateCNCCode(
  params: Parameters,
  total_distance_3d: number,
  total_length: number,
  bends: BendDetail[],
  trajectory_3d: Point3D[],
  feasibility_score: number
): string {
  let code = '';
  code += 'G20  ; Дюймы\n';
  code += 'G90  ; Абсолютные координаты\n';
  code += 'G00 Z5.0  ; Поднять инструмент\n\n';

  code += `; Программа гибки трубопровода\n`;
  code += `; Общая длина: ${total_length.toFixed(2)} мм\n`;
  code += `; Feasibility: ${feasibility_score.toFixed(1)}%\n\n`;

  if (trajectory_3d.length > 0) {
    const first = trajectory_3d[0];
    code += `; Перемещение в стартовую позицию\n`;
    code += `G00 X${first.x.toFixed(2)} Y${first.y.toFixed(2)} Z${first.z.toFixed(2)}\n`;
    code += `M03 S500  ; Включить шпиндель\n\n`;
  }

  // Гибки
  for (let i = 0; i < bends.length; ++i) {
    const bend = bends[i];
    code += `; ========== Гибка ${i + 1} ==========\n`;
    code += `; Позиция: (${bend.position.x.toFixed(1)}, ${bend.position.y.toFixed(1)}, ${bend.position.z.toFixed(1)})\n`;
    code += `; Углы: XY=${bend.angle_xy.toFixed(1)}° Z=${bend.angle_z.toFixed(1)}°\n`;
    code += `; Радиус: ${bend.radius.toFixed(2)} мм\n`;
    code += `G01 X${bend.position.x.toFixed(2)} Y${bend.position.y.toFixed(2)} Z${bend.position.z.toFixed(2)} F100  ; Гибка\n\n`;
  }

  // Финальная позиция
  code += `; Перемещение в конечную позицию\n`;
  if (trajectory_3d.length > 0) {
    const last = trajectory_3d[trajectory_3d.length - 1];
    code += `G00 X${last.x.toFixed(2)} Y${last.y.toFixed(2)} Z${last.z.toFixed(2)}\n`;
  }

  code += `M05  ; Выключить шпиндель\n`;
  code += `G00 Z10.0  ; Поднять инструмент\n`;
  code += `M30  ; Конец программы\n`;

  return code;
}

function generateCSVReport(result: CalculationResult): string {
  let csv = 'Combined 3D Offset Report\n\n';

  csv += 'Summary\n';
  csv += `Total Length (mm),${result.total_length.toFixed(2)}\n`;
  csv += `3D Distance (mm),${result.total_distance_3d.toFixed(2)}\n`;
  csv += `Feasibility Score (%),${result.feasibility_score.toFixed(1)}\n`;
  csv += `Number of Bends,${result.bends.length}\n\n`;

  csv += 'Bends\n';
  csv += 'Index,X (mm),Y (mm),Z (mm),Radius (mm),Angle XY (°),Angle Z (°)\n';
  for (const bend of result.bends) {
    csv += `${bend.bend_index},${bend.position.x.toFixed(2)},${bend.position.y.toFixed(2)},${bend.position.z.toFixed(2)},${bend.radius.toFixed(2)},${bend.angle_xy.toFixed(1)},${bend.angle_z.toFixed(1)}\n`;
  }

  csv += '\nSections\n';
  csv += 'Index,Start X,Start Y,Start Z,End X,End Y,End Z,Length (mm),Slope (%)\n';
  for (const section of result.sections) {
    csv += `${section.section_index},${section.start.x.toFixed(2)},${section.start.y.toFixed(2)},${section.start.z.toFixed(2)},${section.end.x.toFixed(2)},${section.end.y.toFixed(2)},${section.end.z.toFixed(2)},${section.length.toFixed(2)},${section.slope_percent.toFixed(1)}\n`;
  }

  return csv;
}

export default router;
