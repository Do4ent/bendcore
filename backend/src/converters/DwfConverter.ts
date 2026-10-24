import fs from 'fs';
import zlib from 'zlib';

/**
 * DWF/DWFx (Design Web Format) Parser и Exporter
 * Поддержка: DWF 6.01, DWFx (ZIP-based)
 */

export interface DWFData {
  header: {
    version: string;
    source: string;
    timestamp: Date;
  };
  pages: DWFPage[];
  resources: Map<string, any>;
  metadata: Map<string, string>;
}

export interface DWFPage {
  id: string;
  name: string;
  width: number;
  height: number;
  content: string;
}

export class DWFConverter {
  private static readonly DWF_VERSION = '6.01';
  private static readonly DWFX_VERSION = '1.0';

  /**
   * Parse DWF файл
   * @param filePath - Путь к DWF файлу
   * @returns Структурированные данные DWF
   */
  static parse(filePath: string): DWFData | null {
    try {
      if (!fs.existsSync(filePath)) {
        throw new Error(`DWF файл не найден: ${filePath}`);
      }

      const content = fs.readFileSync(filePath);

      // Detect format (DWFx is ZIP, DWF is binary/text)
      if (this.isDWFx(content)) {
        return this.parseDWFx(filePath);
      } else {
        return this.parseDWF(content.toString('utf-8'));
      }
    } catch (error) {
      console.error(`Ошибка при чтении DWF файла: ${error}`);
      return null;
    }
  }

  /**
   * Parse DWF текстовый формат
   */
  private static parseDWF(content: string): DWFData {
    const data: DWFData = {
      header: {
        version: this.DWF_VERSION,
        source: 'BendCore CAD',
        timestamp: new Date()
      },
      pages: [],
      resources: new Map(),
      metadata: new Map()
    };

    // Parse DWF struktura
    const lines = content.split('\n');

    for (let i = 0; i < lines.length; i++) {
      const line = lines[i].trim();

      if (line.startsWith('DWFHEADER')) {
        data.header.version = this.extractVersion(lines[i]);
      }

      if (line.startsWith('PAGE')) {
        const page = this.parsePage(lines, i);
        if (page) data.pages.push(page);
      }

      if (line.startsWith('METADATA')) {
        const [key, value] = line.split('=');
        if (key && value) {
          data.metadata.set(key.trim(), value.trim());
        }
      }
    }

    return data;
  }

  /**
   * Parse DWFx (ZIP-based) формат
   */
  private static parseDWFx(filePath: string): DWFData | null {
    // Упрощенная версия - полная реализация требует обработки ZIP
    return {
      header: {
        version: this.DWFX_VERSION,
        source: 'BendCore CAD',
        timestamp: new Date()
      },
      pages: [],
      resources: new Map(),
      metadata: new Map()
    };
  }

  /**
   * Parse страницу DWF
   */
  private static parsePage(lines: string[], startIndex: number): DWFPage | null {
    const line = lines[startIndex];
    const match = line.match(/PAGE\s*\(\s*'([^']+)'\s*,\s*(\d+)\s*,\s*(\d+)/);

    if (!match) return null;

    return {
      id: `page-${Date.now()}`,
      name: match[1],
      width: parseInt(match[2]),
      height: parseInt(match[3]),
      content: this.extractPageContent(lines, startIndex)
    };
  }

  /**
   * Extract контента страницы
   */
  private static extractPageContent(lines: string[], startIndex: number): string {
    let content = '';
    let depth = 0;

    for (let i = startIndex; i < lines.length; i++) {
      const line = lines[i];

      if (line.includes('(')) depth++;
      if (line.includes(')')) {
        depth--;
        if (depth < 0) break;
      }

      content += line + '\n';
    }

    return content;
  }

  /**
   * Export в DWF формат
   */
  static export(geometry: any, filePath: string, format: 'dwf' | 'dwfx' = 'dwf'): void {
    try {
      if (format === 'dwfx') {
        this.exportDWFx(geometry, filePath);
      } else {
        this.exportDWF(geometry, filePath);
      }
      console.log(`DWF файл успешно экспортирован: ${filePath}`);
    } catch (error) {
      throw new Error(`Ошибка экспорта DWF: ${error}`);
    }
  }

  /**
   * Export в DWF текстовый формат
   */
  private static exportDWF(geometry: any, filePath: string): void {
    let content = this.generateDWFHeader();

    // Add pages
    if (geometry.pages) {
      for (const page of geometry.pages) {
        content += this.generateDWFPage(page);
      }
    } else {
      // Default single page from geometry
      content += this.generateDWFPage({
        name: geometry.name || 'Page1',
        width: geometry.width || 800,
        height: geometry.height || 600,
        objects: geometry.objects || []
      });
    }

    content += 'ENDFILE\n';

    fs.writeFileSync(filePath, content, 'utf-8');
  }

  /**
   * Export в DWFx ZIP формат
   */
  private static exportDWFx(geometry: any, filePath: string): void {
    // Упрощенная реализация
    const content = this.generateDWFxContent(geometry);
    fs.writeFileSync(filePath, content, 'utf-8');
  }

  /**
   * Generate DWF header
   */
  private static generateDWFHeader(): string {
    let header = '';
    header += `DWFHEADER\n`;
    header += `VERSION='${this.DWF_VERSION}'\n`;
    header += `SOURCE='BendCore CAD'\n`;
    header += `DATE='${new Date().toISOString()}'\n`;
    header += `STARTPAGE\n`;

    return header;
  }

  /**
   * Generate DWF страницу
   */
  private static generateDWFPage(page: any): string {
    let content = '';

    content += `PAGE\n`;
    content += `  NAME='${page.name}'\n`;
    content += `  WIDTH=${page.width}\n`;
    content += `  HEIGHT=${page.height}\n`;

    // Add objects
    if (page.objects && Array.isArray(page.objects)) {
      for (const obj of page.objects) {
        content += this.generateDWFObject(obj);
      }
    }

    content += `ENDPAGE\n`;

    return content;
  }

  /**
   * Generate DWF объект
   */
  private static generateDWFObject(obj: any): string {
    let content = '';

    switch (obj.type) {
      case 'line':
        content += `  LINE (${obj.x1},${obj.y1},${obj.x2},${obj.y2})\n`;
        break;

      case 'circle':
        content += `  CIRCLE (${obj.cx},${obj.cy},${obj.radius})\n`;
        break;

      case 'rectangle':
        content += `  RECT (${obj.x},${obj.y},${obj.width},${obj.height})\n`;
        break;

      case 'polygon':
        const points = (obj.points || []).map((p: any) => `${p.x},${p.y}`).join(' ');
        content += `  POLYGON (${points})\n`;
        break;

      case 'text':
        content += `  TEXT ('${obj.text}', ${obj.x}, ${obj.y})\n`;
        break;

      default:
        content += `  OBJECT (${obj.type})\n`;
    }

    return content;
  }

  /**
   * Generate DWFx XML контент
   */
  private static generateDWFxContent(geometry: any): string {
    let xml = '<?xml version="1.0" encoding="UTF-8"?>\n';
    xml += '<DWFx version="' + this.DWFX_VERSION + '">\n';
    xml += '  <Metadata>\n';
    xml += `    <Title>${geometry.name || 'BendCore Model'}</Title>\n`;
    xml += `    <Creator>BendCore CAD</Creator>\n`;
    xml += `    <Created>${new Date().toISOString()}</Created>\n`;
    xml += '  </Metadata>\n';

    xml += '  <Pages>\n';
    if (geometry.pages) {
      for (const page of geometry.pages) {
        xml += this.generateDWFxPage(page);
      }
    }
    xml += '  </Pages>\n';

    xml += '</DWFx>\n';

    return xml;
  }

  /**
   * Generate DWFx страницу
   */
  private static generateDWFxPage(page: any): string {
    let xml = '';

    xml += `    <Page name="${page.name}" width="${page.width}" height="${page.height}">\n`;

    if (page.objects && Array.isArray(page.objects)) {
      for (const obj of page.objects) {
        xml += this.generateDWFxObject(obj);
      }
    }

    xml += '    </Page>\n';

    return xml;
  }

  /**
   * Generate DWFx объект
   */
  private static generateDWFxObject(obj: any): string {
    let xml = '';

    switch (obj.type) {
      case 'line':
        xml += `      <Line x1="${obj.x1}" y1="${obj.y1}" x2="${obj.x2}" y2="${obj.y2}"/>\n`;
        break;

      case 'circle':
        xml += `      <Circle cx="${obj.cx}" cy="${obj.cy}" radius="${obj.radius}"/>\n`;
        break;

      case 'rectangle':
        xml += `      <Rectangle x="${obj.x}" y="${obj.y}" width="${obj.width}" height="${obj.height}"/>\n`;
        break;

      case 'polygon':
        const points = (obj.points || [])
          .map((p: any) => `${p.x},${p.y}`)
          .join(' ');
        xml += `      <Polygon points="${points}"/>\n`;
        break;

      case 'text':
        xml += `      <Text x="${obj.x}" y="${obj.y}">${this.escapeXml(obj.text)}</Text>\n`;
        break;
    }

    return xml;
  }

  /**
   * Escape XML спецсимволы
   */
  private static escapeXml(text: string): string {
    return text
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&apos;');
  }

  /**
   * Detect DWFx (ZIP format)
   */
  private static isDWFx(buffer: Buffer): boolean {
    // ZIP files start with PK\x03\x04
    return buffer[0] === 0x50 && buffer[1] === 0x4b;
  }

  /**
   * Extract version из header
   */
  private static extractVersion(line: string): string {
    const match = line.match(/VERSION='([^']+)'/);
    return match ? match[1] : this.DWF_VERSION;
  }

  /**
   * Валидация DWF файла
   */
  static validate(filePath: string): { valid: boolean; errors: string[] } {
    const errors: string[] = [];

    try {
      const content = fs.readFileSync(filePath, 'utf-8');

      if (
        !content.includes('DWFHEADER') &&
        !content.includes('<?xml') &&
        !this.isDWFx(fs.readFileSync(filePath))
      ) {
        errors.push('Файл не является валидным DWF');
      }
    } catch (error) {
      errors.push(`Ошибка чтения файла: ${error}`);
    }

    return {
      valid: errors.length === 0,
      errors
    };
  }
}
