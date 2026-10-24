/**
 * DWG/DXF File Handler for BendCore
 * Handles import/export of AutoCAD formats
 */

export interface DXFEntity {
  type: string;
  layer: string;
  x?: number;
  y?: number;
  x1?: number;
  y1?: number;
  x2?: number;
  y2?: number;
  radius?: number;
  [key: string]: any;
}

export interface DWGFile {
  fileName: string;
  version: string;
  layers: string[];
  entities: DXFEntity[];
  metadata: {
    createdAt: Date;
    author?: string;
    description?: string;
  };
}

export class DWGDXFHandler {
  /**
   * Parse DXF file content
   */
  static parseDXF(content: string): DWGFile {
    const lines = content.split('\n');
    const entities: DXFEntity[] = [];
    const layers: Set<string> = new Set();

    let currentEntity: Partial<DXFEntity> = {};
    let i = 0;

    while (i < lines.length) {
      const code = parseInt(lines[i]);
      const value = lines[i + 1]?.trim();

      if (code === 0) {
        if (Object.keys(currentEntity).length > 0) {
          entities.push(currentEntity as DXFEntity);
        }
        currentEntity = { type: value };
      } else if (code === 8) {
        currentEntity.layer = value;
        layers.add(value);
      } else if (code === 10) {
        currentEntity.x = parseFloat(value);
      } else if (code === 20) {
        currentEntity.y = parseFloat(value);
      } else if (code === 11) {
        currentEntity.x1 = parseFloat(value);
      } else if (code === 21) {
        currentEntity.y1 = parseFloat(value);
      } else if (code === 12) {
        currentEntity.x2 = parseFloat(value);
      } else if (code === 22) {
        currentEntity.y2 = parseFloat(value);
      } else if (code === 40) {
        currentEntity.radius = parseFloat(value);
      }

      i += 2;
    }

    return {
      fileName: 'imported-drawing',
      version: '2021',
      layers: Array.from(layers),
      entities,
      metadata: {
        createdAt: new Date(),
      },
    };
  }

  /**
   * Generate DXF file content
   */
  static generateDXF(entities: DXFEntity[], layers: string[]): string {
    const lines: string[] = [];

    // DXF Header
    lines.push('0', 'SECTION', '2', 'HEADER', '9', '$ACADVER', '1', 'AC1021');
    lines.push('9', '$EXTMIN', '10', '0.0', '20', '0.0');
    lines.push('9', '$EXTMAX', '10', '1000.0', '20', '1000.0');
    lines.push('0', 'ENDSEC');

    // Layers section
    lines.push('0', 'SECTION', '2', 'TABLES');
    lines.push('0', 'TABLE', '2', 'LAYER', '70', '');

    layers.forEach((layer) => {
      lines.push('0', 'LAYER');
      lines.push('2', layer);
      lines.push('70', '0');
      lines.push('62', String(Math.floor(Math.random() * 256)));
    });

    lines.push('0', 'ENDTAB', '0', 'ENDTAB', '0', 'ENDSEC');

    // Entities section
    lines.push('0', 'SECTION', '2', 'ENTITIES');

    entities.forEach((entity) => {
      lines.push('0', entity.type);
      lines.push('8', entity.layer || 'Default');

      if (entity.type === 'LINE') {
        lines.push('10', String(entity.x || 0));
        lines.push('20', String(entity.y || 0));
        lines.push('11', String(entity.x1 || 0));
        lines.push('21', String(entity.y1 || 0));
      } else if (entity.type === 'CIRCLE') {
        lines.push('10', String(entity.x || 0));
        lines.push('20', String(entity.y || 0));
        lines.push('40', String(entity.radius || 10));
      } else if (entity.type === 'ARC') {
        lines.push('10', String(entity.x || 0));
        lines.push('20', String(entity.y || 0));
        lines.push('40', String(entity.radius || 10));
      }
    });

    lines.push('0', 'ENDSEC', '0', 'EOF');

    return lines.join('\n');
  }

  /**
   * Export to DWG (simplified - returns DXF-compatible format)
   */
  static exportDWG(entities: DXFEntity[], layers: string[], fileName: string): string {
    const dxfContent = this.generateDXF(entities, layers);
    // In real implementation, would use dwg library
    return dxfContent;
  }

  /**
   * Import from file
   */
  static async importFile(file: File): Promise<DWGFile> {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();

      reader.onload = (e) => {
        try {
          const content = e.target?.result as string;
          if (file.name.endsWith('.dxf')) {
            resolve(this.parseDXF(content));
          } else if (file.name.endsWith('.dwg')) {
            // For DWG, would need dwg library
            resolve(this.parseDXF(content));
          } else {
            reject(new Error('Unsupported file format'));
          }
        } catch (error) {
          reject(error);
        }
      };

      reader.onerror = () => reject(new Error('File read error'));
      reader.readAsText(file);
    });
  }

  /**
   * Export file
   */
  static exportFile(
    entities: DXFEntity[],
    layers: string[],
    fileName: string,
    format: 'dxf' | 'dwg' = 'dxf'
  ): Blob {
    const content =
      format === 'dwg'
        ? this.exportDWG(entities, layers, fileName)
        : this.generateDXF(entities, layers);

    return new Blob([content], {
      type: 'application/octet-stream',
    });
  }
}

// React Hook для DWG/DXF
export const useDWGDXFHandler = () => {
  const importDXF = async (file: File) => {
    try {
      const dwgFile = await DWGDXFHandler.importFile(file);
      return dwgFile;
    } catch (error) {
      console.error('Import error:', error);
      throw error;
    }
  };

  const exportDXF = (
    entities: DXFEntity[],
    layers: string[],
    fileName: string
  ) => {
    const blob = DWGDXFHandler.exportFile(
      entities,
      layers,
      fileName,
      'dxf'
    );
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `${fileName}.dxf`;
    link.click();
    URL.revokeObjectURL(url);
  };

  const exportDWG = (
    entities: DXFEntity[],
    layers: string[],
    fileName: string
  ) => {
    const blob = DWGDXFHandler.exportFile(
      entities,
      layers,
      fileName,
      'dwg'
    );
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `${fileName}.dwg`;
    link.click();
    URL.revokeObjectURL(url);
  };

  return { importDXF, exportDXF, exportDWG };
};
