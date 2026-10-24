import fs from 'fs';

/**
 * Parasolid SAT (ACIS SAT) Format Parser и Exporter
 * Поддержка версий: SAT 7.0, 8.0, 9.0, 10.0
 */

export interface SATEntity {
  type: string;
  id: number;
  data: any;
  version: string;
}

export interface SATData {
  header: {
    version: string;
    appId: string;
    fileType: string;
    timestamp: Date;
    author: string;
  };
  entities: Map<number, SATEntity>;
  bodies: any[];
  geometry: any[];
}

export class SATConverter {
  private static readonly SAT_VERSION = '10.0';
  private static readonly ENTITY_TYPES = [
    'begin', 'end', 'spacer',
    'vertex', 'edge', 'loop', 'face', 'shell', 'solid',
    'lump', 'body', 'transform',
    'cyl_surface', 'cone_surface', 'spline_surface'
  ];

  /**
   * Parse SAT файл
   * @param filePath - Путь к SAT файлу
   * @returns Структурированные данные SAT
   */
  static parse(filePath: string): SATData | null {
    try {
      if (!fs.existsSync(filePath)) {
        throw new Error(`SAT файл не найден: ${filePath}`);
      }

      const content = fs.readFileSync(filePath, 'utf-8');
      return this.parseContent(content);
    } catch (error) {
      console.error(`Ошибка при чтении SAT файла: ${error}`);
      return null;
    }
  }

  /**
   * Parse SAT контента
   */
  static parseContent(content: string): SATData {
    const lines = content.split('\n');

    const data: SATData = {
      header: this.parseHeader(lines),
      entities: new Map(),
      bodies: [],
      geometry: []
    };

    let entityId = 1;
    for (let i = 1; i < lines.length; i++) {
      const line = lines[i].trim();

      if (!line || line.startsWith('//') || line.startsWith('#')) {
        continue;
      }

      if (line === 'End-of-ACIS-data') {
        break;
      }

      const entity = this.parseSATEntity(line, entityId, data.header.version);
      if (entity) {
        data.entities.set(entityId, entity);

        // Extract bodies and geometry
        if (entity.type === 'body') {
          data.bodies.push(entity);
        }

        if (this.isGeometryEntity(entity.type)) {
          data.geometry.push(entity);
        }

        entityId++;
      }
    }

    return data;
  }

  /**
   * Parse SAT entity
   */
  private static parseSATEntity(line: string, id: number, version: string): SATEntity | null {
    const parts = line.split(/\s+/);
    if (parts.length === 0) return null;

    const type = parts[0];

    return {
      type,
      id,
      data: this.parseEntityData(type, parts),
      version
    };
  }

  /**
   * Parse entity данные по типу
   */
  private static parseEntityData(type: string, parts: string[]): any {
    const data: any = {};

    switch (type) {
      case 'vertex':
        data.x = parseFloat(parts[1]);
        data.y = parseFloat(parts[2]);
        data.z = parseFloat(parts[3]);
        break;

      case 'edge':
        data.startVertex = parseInt(parts[1]);
        data.endVertex = parseInt(parts[2]);
        data.curve = parseInt(parts[3]);
        data.sense = parts[4] === 'forward' ? 1 : -1;
        break;

      case 'loop':
        data.face = parseInt(parts[1]);
        data.edges = [];
        for (let i = 2; i < parts.length; i++) {
          const edgeId = parseInt(parts[i]);
          if (!isNaN(edgeId)) {
            data.edges.push(edgeId);
          }
        }
        break;

      case 'face':
        data.shell = parseInt(parts[1]);
        data.surface = parseInt(parts[2]);
        data.sense = parts[3] === 'forward' ? 1 : -1;
        data.loops = [];
        for (let i = 4; i < parts.length; i++) {
          const loopId = parseInt(parts[i]);
          if (!isNaN(loopId)) {
            data.loops.push(loopId);
          }
        }
        break;

      case 'shell':
        data.faces = [];
        for (let i = 1; i < parts.length; i++) {
          const faceId = parseInt(parts[i]);
          if (!isNaN(faceId)) {
            data.faces.push(faceId);
          }
        }
        break;

      case 'body':
        data.lump = parseInt(parts[1]);
        data.transform = parseInt(parts[2]);
        break;

      case 'cyl_surface':
        data.position = {
          x: parseFloat(parts[1]),
          y: parseFloat(parts[2]),
          z: parseFloat(parts[3])
        };
        data.radius = parseFloat(parts[4]);
        break;

      default:
        // Store all parts as generic data
        for (let i = 1; i < parts.length; i++) {
          const val = parts[i];
          const numVal = parseFloat(val);
          data[`param${i}`] = isNaN(numVal) ? val : numVal;
        }
    }

    return data;
  }

  /**
   * Export в SAT формат
   */
  static export(geometry: any, filePath: string): void {
    try {
      const content = this.generateSATContent(geometry);
      fs.writeFileSync(filePath, content, 'utf-8');
      console.log(`SAT файл успешно экспортирован: ${filePath}`);
    } catch (error) {
      throw new Error(`Ошибка экспорта SAT: ${error}`);
    }
  }

  /**
   * Generate SAT контент
   */
  private static generateSATContent(geometry: any): string {
    let content = '';

    // SAT Header
    content += `700 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n`;
    content += `BendCore CAD ACIS SAT File\n`;

    // Begin entity
    content += `begin\n`;

    // Generate vertices (if available)
    if (geometry.vertices && Array.isArray(geometry.vertices)) {
      for (const v of geometry.vertices) {
        content += `vertex ${v.x || 0} ${v.y || 0} ${v.z || 0}\n`;
      }
    }

    // Generate edges (if available)
    if (geometry.edges && Array.isArray(geometry.edges)) {
      for (const e of geometry.edges) {
        content += `edge ${e.v1 || 0} ${e.v2 || 0} ${e.curve || 0} forward\n`;
      }
    }

    // Generate loops (if available)
    if (geometry.loops && Array.isArray(geometry.loops)) {
      for (const loop of geometry.loops) {
        const edgeList = (loop.edges || []).join(' ');
        content += `loop ${loop.face || 0} ${edgeList}\n`;
      }
    }

    // Generate faces (if available)
    if (geometry.faces && Array.isArray(geometry.faces)) {
      for (const f of geometry.faces) {
        const loopList = (f.loops || []).join(' ');
        content += `face ${f.shell || 0} ${f.surface || 0} forward ${loopList}\n`;
      }
    }

    // Generate solids (if available)
    if (geometry.solids && Array.isArray(geometry.solids)) {
      for (const solid of geometry.solids) {
        const faceList = (solid.faces || []).join(' ');
        content += `shell ${faceList}\n`;
      }
    }

    // Generate body
    if (geometry.solids && geometry.solids.length > 0) {
      content += `body 1 0\n`; // Simplified: assume first shell is body
    }

    // End entity
    content += `end\n`;

    // Terminator
    content += `End-of-ACIS-data\n`;

    return content;
  }

  /**
   * Determine if entity represents geometry
   */
  private static isGeometryEntity(type: string): boolean {
    const geometryTypes = [
      'vertex', 'edge', 'loop', 'face', 'shell', 'solid',
      'cyl_surface', 'cone_surface', 'spline_surface',
      'plane_surface', 'sphere_surface'
    ];
    return geometryTypes.includes(type);
  }

  /**
   * Get справку о типе entity
   */
  static getEntityTypeInfo(type: string): string {
    const descriptions: { [key: string]: string } = {
      'vertex': 'Вершина - точка в 3D пространстве',
      'edge': 'Ребро - кривая между двумя вершинами',
      'loop': 'Цикл - замкнутая последовательность ребер',
      'face': 'Грань - поверхность, ограниченная циклами',
      'shell': 'Оболочка - набор граней',
      'solid': 'Твердое тело - ограниченный объем',
      'body': 'Тело - основная единица ACIS',
      'lump': 'Комок - связанный объем',
      'cyl_surface': 'Цилиндрическая поверхность',
      'cone_surface': 'Коническая поверхность',
      'spline_surface': 'Сплайн-поверхность'
    };

    return descriptions[type] || `Неизвестный тип: ${type}`;
  }

  /**
   * Calculate bounds геометрии
   */
  static calculateBounds(satData: SATData): any {
    let minX = Infinity, maxX = -Infinity;
    let minY = Infinity, maxY = -Infinity;
    let minZ = Infinity, maxZ = -Infinity;

    for (const [, entity] of satData.entities) {
      if (entity.type === 'vertex') {
        const { x, y, z } = entity.data;
        minX = Math.min(minX, x);
        maxX = Math.max(maxX, x);
        minY = Math.min(minY, y);
        maxY = Math.max(maxY, y);
        minZ = Math.min(minZ, z);
        maxZ = Math.max(maxZ, z);
      }
    }

    return {
      minX: isFinite(minX) ? minX : 0,
      maxX: isFinite(maxX) ? maxX : 0,
      minY: isFinite(minY) ? minY : 0,
      maxY: isFinite(maxY) ? maxY : 0,
      minZ: isFinite(minZ) ? minZ : 0,
      maxZ: isFinite(maxZ) ? maxZ : 0
    };
  }

  /**
   * Валидация SAT файла
   */
  static validate(filePath: string): { valid: boolean; errors: string[] } {
    const errors: string[] = [];

    try {
      const content = fs.readFileSync(filePath, 'utf-8');
      const lines = content.split('\n');

      // Check header
      if (!lines[0].includes('0')) {
        errors.push('Неверный SAT заголовок');
      }

      // Check for begin and end
      const hasBegin = lines.some(l => l.trim() === 'begin');
      const hasEnd = lines.some(l => l.trim() === 'end');

      if (!hasBegin) {
        errors.push('Не найдена begin команда');
      }

      if (!hasEnd) {
        errors.push('Не найдена end команда');
      }

      // Check for End-of-ACIS-data
      if (!content.includes('End-of-ACIS-data')) {
        errors.push('Отсутствует End-of-ACIS-data маркер');
      }
    } catch (error) {
      errors.push(`Ошибка чтения файла: ${error}`);
    }

    return {
      valid: errors.length === 0,
      errors
    };
  }

  /**
   * Convert между SAT версиями
   */
  static upgradeVersion(satData: SATData, targetVersion: string): SATData {
    const upgraded = { ...satData };
    upgraded.header.version = targetVersion;
    return upgraded;
  }
}
