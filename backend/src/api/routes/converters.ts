/**
 * CAD Converters API Routes - Production Ready
 * REST endpoints for file conversion and format handling
 */

import express, { Router, Request, Response } from 'express';
import multer from 'multer';
import fs from 'fs';
import path from 'path';
import { CADConverter, BatchCADConverter } from '../../converters';

const router = Router();
const upload = multer({ dest: '/tmp/uploads' });

/**
 * POST /converters/parse
 * Parse CAD файл и вернуть структурированные данные
 */
router.post('/parse', upload.single('file'), async (req: Request, res: Response) => {
  try {
    if (!req.file) {
      return res.status(400).json({ error: 'No file provided' });
    }

    const filePath = req.file.path;
    const data = CADConverter.parse(filePath);

    if (!data) {
      return res.status(400).json({ error: 'Failed to parse file' });
    }

    res.json({
      success: true,
      format: req.file.originalname.split('.').pop(),
      header: data.header,
      entityCount: data.entities?.size || 0,
      geometryCount: (data.geometry || []).length,
      timestamp: new Date()
    });

    // Cleanup temp file
    fs.unlink(filePath, (err) => {
      if (err) console.error('Failed to delete temp file:', err);
    });
  } catch (error) {
    res.status(500).json({ error: String(error) });
  }
});

/**
 * POST /converters/convert
 * Convert между форматами
 */
router.post('/convert', upload.single('file'), async (req: Request, res: Response) => {
  try {
    const { targetFormat } = req.body;
    if (!req.file || !targetFormat) {
      return res.status(400).json({ error: 'Missing file or target format' });
    }

    const inputPath = req.file.path;
    const outputPath = `/tmp/converted/model.${targetFormat}`;

    // Ensure output directory exists
    const outputDir = path.dirname(outputPath);
    if (!fs.existsSync(outputDir)) {
      fs.mkdirSync(outputDir, { recursive: true });
    }

    CADConverter.convert(inputPath, outputPath, targetFormat);

    const fileContent = fs.readFileSync(outputPath);
    res.setHeader('Content-Type', 'application/octet-stream');
    res.setHeader('Content-Disposition', `attachment; filename="model.${targetFormat}"`);
    res.send(fileContent);

    // Cleanup
    fs.unlink(inputPath, () => {});
    fs.unlink(outputPath, () => {});
  } catch (error) {
    res.status(500).json({ error: String(error) });
  }
});

/**
 * POST /converters/validate
 * Validate CAD файл
 */
router.post('/validate', upload.single('file'), async (req: Request, res: Response) => {
  try {
    if (!req.file) {
      return res.status(400).json({ error: 'No file provided' });
    }

    const validation = CADConverter.validate(req.file.path);

    res.json({
      valid: validation.valid,
      format: req.file.originalname.split('.').pop(),
      errors: validation.errors
    });

    fs.unlink(req.file.path, () => {});
  } catch (error) {
    res.status(500).json({ error: String(error) });
  }
});

/**
 * GET /converters/formats
 * Get поддерживаемые форматы
 */
router.get('/formats', (req: Request, res: Response) => {
  res.json({
    supported: CADConverter.getSupportedFormats(),
    converters: [
      {
        name: 'STEP',
        extensions: ['stp', 'step'],
        description: 'ISO 10303-21 STEP format',
        standard: 'ISO 10303'
      },
      {
        name: 'IGES',
        extensions: ['iges', 'igs'],
        description: 'Initial Graphics Exchange Specification',
        standard: 'ANSI Y14.26M'
      },
      {
        name: 'DWF',
        extensions: ['dwf', 'dwfx'],
        description: 'Design Web Format',
        versions: ['6.01', '1.0']
      },
      {
        name: 'SAT',
        extensions: ['sat'],
        description: 'Parasolid ACIS format',
        standard: 'ACIS'
      }
    ]
  });
});

/**
 * GET /converters/info/:format
 * Get информацию о формате
 */
router.get('/info/:format', (req: Request, res: Response) => {
  const { format } = req.params;
  const formatInfo = CADConverter.getSupportedFormats().includes(format.toLowerCase());

  if (!formatInfo) {
    return res.status(404).json({ error: `Format ${format} not supported` });
  }

  const infoMap: { [key: string]: any } = {
    step: {
      name: 'STEP',
      fullName: 'Standard for the Exchange of Product Data',
      versions: ['AP203', 'AP214', 'AP242'],
      description: 'Comprehensive 3D CAD format with geometry and metadata',
      dataLoss: 'Minimal',
      compatibility: ['SolidWorks', 'Inventor', 'FreeCAD', 'CATIA']
    },
    iges: {
      name: 'IGES',
      fullName: 'Initial Graphics Exchange Specification',
      versions: ['5.1', '5.2', '5.3'],
      description: 'Vector-based format supporting curves and surfaces',
      dataLoss: 'Low-Medium',
      compatibility: ['AutoCAD', 'SolidWorks', 'CATIA']
    },
    dwf: {
      name: 'DWF',
      fullName: 'Design Web Format',
      versions: ['6.01', 'DWFx 1.0'],
      description: '2D format for web sharing and markup',
      dataLoss: 'High (3D to 2D)',
      compatibility: ['AutoCAD', 'Revit', 'ArcGIS']
    },
    sat: {
      name: 'SAT',
      fullName: 'Parasolid SAT',
      versions: ['7.0', '8.0', '9.0', '10.0'],
      description: 'ACIS solid modeling format',
      dataLoss: 'Low',
      compatibility: ['Parasolid', 'NX', 'Solid Edge']
    }
  };

  res.json(infoMap[format.toLowerCase()] || { error: 'Format not found' });
});

/**
 * POST /converters/batch
 * Batch convert множество файлов
 */
router.post('/batch', upload.array('files'), async (req: Request, res: Response) => {
  try {
    const { targetFormat } = req.body;

    if (!req.files || !targetFormat) {
      return res.status(400).json({ error: 'Missing files or target format' });
    }

    const inputFiles = (req.files as Express.Multer.File[]).map((f) => f.path);
    const outputDir = '/tmp/batch_converted';

    if (!fs.existsSync(outputDir)) {
      fs.mkdirSync(outputDir, { recursive: true });
    }

    const result = BatchCADConverter.convertBatch(inputFiles, outputDir, targetFormat);

    res.json({
      successful: result.successful,
      failed: result.failed,
      total: inputFiles.length,
      errors: Array.from(result.errors.entries()).map(([file, error]) => ({
        file,
        error
      }))
    });

    // Cleanup input files
    for (const file of inputFiles) {
      fs.unlink(file, () => {});
    }
  } catch (error) {
    res.status(500).json({ error: String(error) });
  }
});

/**
 * GET /converters/compatibility/:source/:target
 * Check совместимость конвертации
 */
router.get('/compatibility/:source/:target', (req: Request, res: Response) => {
  const { source, target } = req.params;

  const info = CADConverter.getConversionInfo(source, target);

  res.json({
    source,
    target,
    ...info
  });
});

/**
 * POST /converters/batch-validate
 * Batch validate файлы
 */
router.post('/batch-validate', upload.array('files'), async (req: Request, res: Response) => {
  try {
    if (!req.files) {
      return res.status(400).json({ error: 'No files provided' });
    }

    const files = (req.files as Express.Multer.File[]).map((f) => f.path);
    const result = BatchCADConverter.validateBatch(files);

    const detailed = Array.from(result.results.entries()).map(([file, validation]) => ({
      file: path.basename(file),
      valid: validation.valid,
      errors: validation.errors
    }));

    res.json({
      valid: result.valid,
      invalid: result.invalid,
      total: files.length,
      files: detailed
    });

    // Cleanup
    for (const file of files) {
      fs.unlink(file, () => {});
    }
  } catch (error) {
    res.status(500).json({ error: String(error) });
  }
});

/**
 * GET /converters/stats
 * Get статистику конвертера
 */
router.get('/stats', (req: Request, res: Response) => {
  res.json({
    supportedFormats: CADConverter.getSupportedFormats().length,
    formats: CADConverter.getSupportedFormats(),
    conversionPairs: CADConverter.getSupportedFormats().length ** 2,
    version: '1.0.0',
    lastUpdated: new Date()
  });
});

export default router;
