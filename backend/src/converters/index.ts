/**
 * CAD Converters Module - PHASE 2 Production Ready
 *
 * Поддержка форматов: STEP, IGES, DWF, DWFx, SAT
 *
 * PHASE 2 Features:
 * - ISO 10303-21 STEP Parser (AP203, AP214, AP242)
 * - IGES 5.1/5.2/5.3 support
 * - SAT/Parasolid conversion
 * - Async parsing для больших файлов
 * - Unified converter manager
 */

export { STEPConverter } from './StepConverter';
export { IGESConverter } from './IgesConverter';
export { DWFConverter } from './DwfConverter';
export { SATConverter } from './SatConverter';
export { Phase2ConverterManager, converterManager } from './Phase2ConverterManager';

import { STEPConverter } from './StepConverter';
import { IGESConverter } from './IgesConverter';
import { DWFConverter } from './DwfConverter';
import { SATConverter } from './SatConverter';

/**
 * Universal CAD file converter
 */
export class CADConverter {
  /**
   * Auto-detect и parse CAD файл
   */
  static parse(filePath: string): any {
    const ext = filePath.toLowerCase().split('.').pop();

    switch (ext) {
      case 'stp':
      case 'step':
        return STEPConverter.parse(filePath);

      case 'iges':
      case 'igs':
        return IGESConverter.parse(filePath);

      case 'dwf':
      case 'dwfx':
        return DWFConverter.parse(filePath);

      case 'sat':
        return SATConverter.parse(filePath);

      default:
        throw new Error(`Неподдерживаемый формат: ${ext}`);
    }
  }

  /**
   * Convert между форматами
   */
  static convert(
    inputPath: string,
    outputPath: string,
    targetFormat?: string
  ): void {
    const data = this.parse(inputPath);

    if (!data) {
      throw new Error('Не удалось распарсить входной файл');
    }

    const format = targetFormat || outputPath.toLowerCase().split('.').pop();

    switch (format) {
      case 'stp':
      case 'step':
        STEPConverter.export(data, outputPath);
        break;

      case 'iges':
      case 'igs':
        IGESConverter.export(data, outputPath);
        break;

      case 'dwf':
      case 'dwfx':
        DWFConverter.export(data, outputPath, format === 'dwfx' ? 'dwfx' : 'dwf');
        break;

      case 'sat':
        SATConverter.export(data, outputPath);
        break;

      default:
        throw new Error(`Неподдерживаемый целевой формат: ${format}`);
    }
  }

  /**
   * Validate CAD файл
   */
  static validate(filePath: string): { valid: boolean; errors: string[] } {
    const ext = filePath.toLowerCase().split('.').pop();

    switch (ext) {
      case 'stp':
      case 'step':
        return STEPConverter.validate(filePath);

      case 'iges':
      case 'igs':
        return IGESConverter.validate(filePath);

      case 'dwf':
      case 'dwfx':
        return DWFConverter.validate(filePath);

      case 'sat':
        return SATConverter.validate(filePath);

      default:
        return { valid: false, errors: [`Неподдерживаемый формат: ${ext}`] };
    }
  }

  /**
   * Get информацию о файле
   */
  static getFileInfo(filePath: string): any {
    const data = this.parse(filePath);
    const validation = this.validate(filePath);

    return {
      path: filePath,
      format: filePath.toLowerCase().split('.').pop(),
      valid: validation.valid,
      errors: validation.errors,
      header: data?.header || null,
      entityCount: data?.entities?.size || 0,
      geometryCount: (data?.geometry || []).length,
      timestamp: new Date()
    };
  }

  /**
   * List поддерживаемые форматы
   */
  static getSupportedFormats(): string[] {
    return ['step', 'stp', 'iges', 'igs', 'dwf', 'dwfx', 'sat'];
  }

  /**
   * Get информацию о конвертации
   */
  static getConversionInfo(sourceFormat: string, targetFormat: string): any {
    return {
      source: sourceFormat,
      target: targetFormat,
      supported: this.isConversionSupported(sourceFormat, targetFormat),
      dataLoss: this.estimateDataLoss(sourceFormat, targetFormat),
      notes: this.getConversionNotes(sourceFormat, targetFormat)
    };
  }

  private static isConversionSupported(source: string, target: string): boolean {
    const supported = this.getSupportedFormats();
    return supported.includes(source.toLowerCase()) && supported.includes(target.toLowerCase());
  }

  private static estimateDataLoss(source: string, target: string): string {
    // STEP -> * имеет минимальную потерю данных
    // DWF -> STEP может потерять 3D информацию
    if (source.includes('dwf') && !target.includes('dwf')) {
      return 'high';
    }
    return 'low';
  }

  private static getConversionNotes(source: string, target: string): string {
    if (source.includes('dwf') && target.includes('step')) {
      return '2D информация может быть потеряна при конвертации';
    }
    if (source.includes('step') && target.includes('dwf')) {
      return '3D модель будет конвертирована в 2D проекцию';
    }
    return 'Стандартная конвертация';
  }
}

/**
 * Batch converter для множественных файлов
 */
export class BatchCADConverter {
  /**
   * Convert множество файлов
   */
  static convertBatch(
    inputFiles: string[],
    outputDir: string,
    targetFormat: string
  ): { successful: number; failed: number; errors: Map<string, string> } {
    const errors = new Map<string, string>();
    let successful = 0;
    let failed = 0;

    for (const inputFile of inputFiles) {
      try {
        const fileName = inputFile.split('/').pop()?.split('.')[0] || 'file';
        const outputFile = `${outputDir}/${fileName}.${targetFormat}`;

        CADConverter.convert(inputFile, outputFile, targetFormat);
        successful++;
      } catch (error) {
        failed++;
        errors.set(inputFile, String(error));
      }
    }

    return { successful, failed, errors };
  }

  /**
   * Validate батч файлов
   */
  static validateBatch(inputFiles: string[]): {
    valid: number;
    invalid: number;
    results: Map<string, any>;
  } {
    const results = new Map<string, any>();
    let valid = 0;
    let invalid = 0;

    for (const file of inputFiles) {
      const validation = CADConverter.validate(file);
      results.set(file, validation);

      if (validation.valid) {
        valid++;
      } else {
        invalid++;
      }
    }

    return { valid, invalid, results };
  }
}

export default CADConverter;
