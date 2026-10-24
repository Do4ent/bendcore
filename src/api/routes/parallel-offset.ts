import { Router, Request, Response } from 'express';
import { exec } from 'child_process';
import { promisify } from 'util';
import * as fs from 'fs';
import * as path from 'path';

// Типизация
interface ParallelOffsetParams {
  outer_diameter: number;
  wall_thickness: number;
  bend_angle: number;
  offset_distance: number;
  bend_radius: number;
  material: string;
}

interface CalculationResult {
  total_length: number;
  bend_1_length: number;
  straight_section: number;
  bend_2_length: number;
  bend_angles: [number, number];
  bend_1_pos: [number, number, number];
  bend_2_pos: [number, number, number];
  confidence: number;
  warnings: string;
  is_valid: boolean;
}

interface ValidationError {
  code: string;
  message: string;
  parameter: string;
}

const router = Router();
const execPromise = promisify(exec);

// Стандартные материалы и их свойства
const MATERIAL_PROPERTIES: Record<string, { factor: number; min_bend_radius_factor: number }> = {
  'Steel': { factor: 1.0, min_bend_radius_factor: 2.5 },
  'Stainless Steel': { factor: 1.15, min_bend_radius_factor: 3.0 },
  'Aluminum': { factor: 0.85, min_bend_radius_factor: 2.0 },
  'Copper': { factor: 0.95, min_bend_radius_factor: 2.5 },
  'Titanium': { factor: 1.25, min_bend_radius_factor: 4.0 },
  'Brass': { factor: 0.90, min_bend_radius_factor: 2.3 },
};

/**
 * POST /api/bendcore/parallel-offset/calculate
 * Выполняет расчёт параллельного смещения
 */
router.post('/calculate', async (req: Request, res: Response) => {
  try {
    const params: ParallelOffsetParams = req.body;

    // Базовая валидация
    const validationErrors = validateParams(params);
    if (validationErrors.length > 0) {
      return res.status(400).json({
        success: false,
        errors: validationErrors,
      });
    }

    // Вызов C++ калькулятора через IPC или компилированный модуль
    const result = await calculateParallelOffset(params);

    return res.json({
      success: true,
      data: result,
    });
  } catch (error) {
    console.error('Ошибка при расчёте:', error);
    return res.status(500).json({
      success: false,
      error: 'Ошибка при выполнении расчёта',
    });
  }
});

/**
 * POST /api/bendcore/parallel-offset/validate
 * Валидирует входные параметры без расчёта
 */
router.post('/validate', (req: Request, res: Response) => {
  const params: ParallelOffsetParams = req.body;
  const errors = validateParams(params);

  res.json({
    success: errors.length === 0,
    errors,
    warnings: getWarnings(params),
  });
});

/**
 * GET /api/bendcore/parallel-offset/materials
 * Возвращает список доступных материалов
 */
router.get('/materials', (req: Request, res: Response) => {
  const materials = Object.keys(MATERIAL_PROPERTIES).map(name => ({
    name,
    factor: MATERIAL_PROPERTIES[name].factor,
    min_bend_radius_factor: MATERIAL_PROPERTIES[name].min_bend_radius_factor,
  }));

  res.json({
    success: true,
    materials,
  });
});

/**
 * POST /api/bendcore/parallel-offset/export
 * Экспортирует результаты в PDF, CSV или DXF
 */
router.post('/export', async (req: Request, res: Response) => {
  try {
    const { calculation_result, parameters, format } = req.body;

    if (!['pdf', 'csv', 'dxf'].includes(format)) {
      return res.status(400).json({
        success: false,
        error: 'Неподдерживаемый формат экспорта',
      });
    }

    let fileContent: string;
    let contentType: string;
    let filename: string;

    switch (format) {
      case 'csv':
        fileContent = generateCSV(calculation_result, parameters);
        contentType = 'text/csv';
        filename = `parallel-offset-${new Date().toISOString().slice(0, 10)}.csv`;
        break;

      case 'dxf':
        fileContent = generateDXF(calculation_result, parameters);
        contentType = 'application/dxf';
        filename = `parallel-offset-${new Date().toISOString().slice(0, 10)}.dxf`;
        break;

      case 'pdf':
      default:
        // Генерация PDF (требует библиотеки типа pdfkit)
        fileContent = generatePDF(calculation_result, parameters);
        contentType = 'application/pdf';
        filename = `parallel-offset-${new Date().toISOString().slice(0, 10)}.pdf`;
        break;
    }

    res.setHeader('Content-Type', contentType);
    res.setHeader('Content-Disposition', `attachment; filename="${filename}"`);
    res.send(fileContent);
  } catch (error) {
    console.error('Ошибка при экспорте:', error);
    return res.status(500).json({
      success: false,
      error: 'Ошибка при генерации файла',
    });
  }
});

/**
 * Функция валидации параметров
 */
function validateParams(params: ParallelOffsetParams): ValidationError[] {
  const errors: ValidationError[] = [];

  // Проверка диаметра
  if (!params.outer_diameter || params.outer_diameter < 3.0 || params.outer_diameter > 500.0) {
    errors.push({
      code: 'INVALID_OD',
      message: 'Диаметр должен быть в диапазоне 3-500 мм',
      parameter: 'outer_diameter',
    });
  }

  // Проверка толщины стенки
  if (!params.wall_thickness || params.wall_thickness <= 0 ||
      params.wall_thickness >= params.outer_diameter / 2) {
    errors.push({
      code: 'INVALID_WALL',
      message: 'Некорректная толщина стенки',
      parameter: 'wall_thickness',
    });
  }

  // Проверка угла гибки
  if (!params.bend_angle || params.bend_angle < 5.0 || params.bend_angle > 180.0) {
    errors.push({
      code: 'INVALID_ANGLE',
      message: 'Угол должен быть в диапазоне 5-180°',
      parameter: 'bend_angle',
    });
  }

  // Проверка смещения
  if (!params.offset_distance || params.offset_distance <= 0 ||
      params.offset_distance > params.outer_diameter * 10) {
    errors.push({
      code: 'INVALID_OFFSET',
      message: 'Смещение должно быть положительным и не более 10*OD',
      parameter: 'offset_distance',
    });
  }

  // Проверка радиуса гибки
  if (!params.bend_radius || params.bend_radius <= 0) {
    errors.push({
      code: 'INVALID_RADIUS',
      message: 'Радиус гибки должен быть положительным',
      parameter: 'bend_radius',
    });
  }

  // Проверка минимального радиуса для материала
  const material = params.material || 'Steel';
  const props = MATERIAL_PROPERTIES[material] || MATERIAL_PROPERTIES['Steel'];
  const minRadius = params.outer_diameter * props.min_bend_radius_factor;

  if (params.bend_radius < minRadius) {
    errors.push({
      code: 'RADIUS_TOO_SMALL',
      message: `Радиус гибки слишком мал для ${material} (минимум ${minRadius.toFixed(2)} мм)`,
      parameter: 'bend_radius',
    });
  }

  return errors;
}

/**
 * Функция для получения предупреждений
 */
function getWarnings(params: ParallelOffsetParams): string[] {
  const warnings: string[] = [];

  if (params.offset_distance < params.outer_diameter * 0.1) {
    warnings.push('Смещение очень малое - результаты могут быть неточными');
  }

  if (params.bend_angle < 10 || params.bend_angle > 170) {
    warnings.push('Очень экстремальный угол гибки');
  }

  if (params.bend_radius < params.outer_diameter * 3) {
    warnings.push('Радиус гибки близок к минимально допустимому');
  }

  if (params.offset_distance > params.bend_radius) {
    warnings.push('Смещение больше радиуса гибки - конфигурация может быть сложной');
  }

  return warnings;
}

/**
 * Главная функция расчёта параллельного смещения
 * Вызывает C++ код через IPC или использует встроенную логику
 */
async function calculateParallelOffset(
  params: ParallelOffsetParams
): Promise<CalculationResult> {
  // Здесь была бы интеграция с C++ модулем
  // Пока используем упрощённый расчёт на TypeScript

  const bendAngleRad = (params.bend_angle * Math.PI) / 180;

  // Расчёт K-фактора
  const kFactor = calculateKFactor(params.outer_diameter, params.wall_thickness);

  // Длина первой гибки
  const bend1Length = params.bend_radius * bendAngleRad;

  // Радиус второй гибки с учётом смещения
  const bend2Radius = Math.sqrt(
    params.bend_radius * params.bend_radius +
    params.offset_distance * params.offset_distance
  );
  const bend2Length = bend2Radius * bendAngleRad;

  // Прямой участок
  const straightSection = Math.max(
    0,
    (params.offset_distance * (1 - Math.sin(bendAngleRad))) / Math.cos(bendAngleRad * 0.5)
  );

  // Общая длина с припусками
  const totalLength = (bend1Length + bend2Length) * (1 + kFactor * 0.1) + straightSection;

  // Углы гибок
  const bend1Angle = params.bend_angle;
  const bend2Angle = params.bend_angle * (bend2Radius / params.bend_radius);

  // 3D координаты
  const bend1Pos: [number, number, number] = [0, 0, 0];
  const bend2Pos: [number, number, number] = [
    bend1Length + straightSection,
    params.offset_distance,
    params.offset_distance * (1 - Math.cos(bendAngleRad)),
  ];

  // Уверенность
  let confidence = 95.0;
  if (params.offset_distance < params.outer_diameter * 0.1) {
    confidence -= 5.0;
  }
  if (params.bend_angle < 10.0 || params.bend_angle > 170.0) {
    confidence -= 10.0;
  }
  if (params.offset_distance > params.bend_radius) {
    confidence -= 8.0;
  }

  // Предупреждения
  const warnings = getWarnings(params).join('; ');

  return {
    total_length: totalLength,
    bend_1_length: bend1Length,
    straight_section: straightSection,
    bend_2_length: bend2Length,
    bend_angles: [bend1Angle, bend2Angle],
    bend_1_pos: bend1Pos,
    bend_2_pos: bend2Pos,
    confidence: Math.max(50.0, confidence),
    warnings,
    is_valid: true,
  };
}

/**
 * Расчёт K-фактора
 */
function calculateKFactor(od: number, wall: number): number {
  const ratio = wall / od;

  if (ratio > 0.1) {
    return 0.3; // Толстостенная
  } else if (ratio > 0.05) {
    return 0.4; // Среднестенная
  } else {
    return 0.5; // Тонкостенная
  }
}

/**
 * Генерация CSV файла
 */
function generateCSV(result: CalculationResult, params: ParallelOffsetParams): string {
  const lines = [
    'Результаты расчёта параллельного смещения трубопровода',
    '',
    'Входные параметры:',
    `Внешний диаметр (мм),${params.outer_diameter}`,
    `Толщина стенки (мм),${params.wall_thickness}`,
    `Угол гибки (°),${params.bend_angle}`,
    `Расстояние смещения (мм),${params.offset_distance}`,
    `Радиус гибки (мм),${params.bend_radius}`,
    `Материал,${params.material}`,
    '',
    'Результаты:',
    `Общая длина трубы (мм),${result.total_length.toFixed(2)}`,
    `Длина первой гибки (мм),${result.bend_1_length.toFixed(2)}`,
    `Прямой участок (мм),${result.straight_section.toFixed(2)}`,
    `Длина второй гибки (мм),${result.bend_2_length.toFixed(2)}`,
    `Угол гибки 1 (°),${result.bend_angles[0].toFixed(2)}`,
    `Угол гибки 2 (°),${result.bend_angles[1].toFixed(2)}`,
    `Уверенность расчёта (%),${result.confidence.toFixed(1)}`,
    `Предупреждения,${result.warnings.replace(/,/g, ';')}`,
  ];

  return lines.join('\n');
}

/**
 * Генерация DXF файла
 */
function generateDXF(result: CalculationResult, params: ParallelOffsetParams): string {
  // Основная структура DXF
  const dxf = [
    '0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1021\n0\nENDSEC\n',
    '0\nSECTION\n2\nTABLES\n',
    '0\nTABLE\n2\nLTYPE\n70\n1\n',
    '0\nLTYPE\n2\nCONTINUOUS\n70\n0\n72\n65\n73\n0\n40\n0.0\n',
    '0\nENDTAB\n',
    '0\nTABLE\n2\nLAYER\n70\n1\n',
    '0\nLAYER\n2\nTube\n70\n0\n62\n256\n6\nCONTINUOUS\n',
    '0\nENDTAB\n',
    '0\nENDSEC\n',
    '0\nSECTION\n2\nENTITIES\n',
  ];

  // Линии для трубы
  // Первая гибка
  dxf.push(
    `0\nLINE\n8\nTube\n10\n0\n20\n0\n11\n${result.bend_1_length.toFixed(2)}\n21\n0\n`
  );

  // Прямой участок
  dxf.push(
    `0\nLINE\n8\nTube\n10\n${result.bend_1_length.toFixed(2)}\n20\n0\n11\n${(
      result.bend_1_length + result.straight_section
    ).toFixed(2)}\n21\n${params.offset_distance.toFixed(2)}\n`
  );

  // Вторая гибка
  dxf.push(
    `0\nLINE\n8\nTube\n10\n${(result.bend_1_length + result.straight_section).toFixed(
      2
    )}\n20\n${params.offset_distance.toFixed(2)}\n11\n${(
      result.bend_1_length + result.straight_section + result.bend_2_length
    ).toFixed(2)}\n21\n${params.offset_distance.toFixed(2)}\n`
  );

  dxf.push('0\nENDSEC\n0\nEOF\n');

  return dxf.join('');
}

/**
 * Генерация PDF файла
 */
function generatePDF(result: CalculationResult, params: ParallelOffsetParams): string {
  // Упрощённый PDF (в реальном проекте используется pdfkit)
  const lines = [
    '%PDF-1.4\n',
    '1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n',
    '2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n',
    '3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>\nendobj\n',
    '4 0 obj\n<< /Length 500 >>\nstream\n',
    'BT\n/F1 12 Tf\n50 750 Td\n(Результаты расчёта параллельного смещения) Tj\n',
    '0 -30 Td\n',
    `(OD: ${params.outer_diameter} мм, Стенка: ${params.wall_thickness} мм, Угол: ${params.bend_angle}°) Tj\n`,
    '0 -20 Td\n',
    `(Общая длина: ${result.total_length.toFixed(2)} мм) Tj\n`,
    '0 -15 Td\n',
    `(Уверенность: ${result.confidence.toFixed(1)}%) Tj\n`,
    'ET\n',
    'endstream\nendobj\n',
    '5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n',
    'xref\n0 6\n0000000000 65535 f\n0000000009 00000 n\n0000000074 00000 n\n0000000133 00000 n\n0000000281 00000 n\n0000000846 00000 n\n',
    'trailer\n<< /Size 6 /Root 1 0 R >>\n',
    'startxref\n945\n%%EOF\n',
  ];

  return lines.join('');
}

export default router;
