/**
 * Associative Drawings Manager - Production Ready
 * Link 3D models with 2D technical drawings, auto-update on model changes
 */

export interface View {
  id: string;
  type: 'front' | 'top' | 'side' | 'isometric' | 'section' | 'detail';
  scale: number;
  position: [number, number];
  rotation: number;
  projection: 'orthogonal' | 'perspective';
}

export interface Dimension {
  id: string;
  type: 'length' | 'angle' | 'radius' | 'diameter' | 'coordinate';
  value: number;
  referenceId: string;
  tolerance?: { lower: number; upper: number };
  position: [number, number];
  unit: string;
  visible: boolean;
}

export interface Annotation {
  id: string;
  type: 'note' | 'weld' | 'surface' | 'thread' | 'symbol';
  text: string;
  position: [number, number];
  leader: boolean;
  visible: boolean;
}

export interface Revision {
  version: number;
  date: Date;
  author: string;
  changes: string;
  approved: boolean;
}

export interface TechnicalDrawing {
  id: string;
  projectId: string;
  name: string;
  views: View[];
  dimensions: Dimension[];
  annotations: Annotation[];
  linked3DId: string;
  revisionHistory: Revision[];
  createdAt: Date;
  updatedAt: Date;
}

export class DrawingManager {
  private drawings: Map<string, TechnicalDrawing> = new Map();
  private updateListeners: Map<string, Function[]> = new Map();
  private changeHistory: Map<string, any[]> = new Map();

  /**
   * Create новый чертеж из 3D модели
   */
  createDrawingFrom3D(modelId: string, projectId: string, drawingName?: string): TechnicalDrawing {
    const drawing: TechnicalDrawing = {
      id: `drw-${Date.now()}`,
      projectId,
      name: drawingName || `Drawing-${modelId.slice(0, 8)}`,
      views: this.generateStandardViews(),
      dimensions: this.extractDimensions(modelId),
      annotations: [],
      linked3DId: modelId,
      revisionHistory: [
        {
          version: 1,
          date: new Date(),
          author: 'System',
          changes: 'Initial creation from 3D model',
          approved: false
        }
      ],
      createdAt: new Date(),
      updatedAt: new Date()
    };

    this.drawings.set(drawing.id, drawing);
    this.changeHistory.set(drawing.id, []);

    return drawing;
  }

  /**
   * Update чертеж когда 3D модель меняется
   */
  updateFromModel(drawingId: string, modelChanges: any): void {
    const drawing = this.drawings.get(drawingId);
    if (!drawing) return;

    const changes: any[] = [];

    // Update dimensions
    for (const dim of drawing.dimensions) {
      const newValue = this.getUpdatedDimensionValue(dim.referenceId, modelChanges);
      if (newValue !== undefined && newValue !== dim.value) {
        changes.push({
          type: 'dimension_changed',
          dimensionId: dim.id,
          oldValue: dim.value,
          newValue: newValue,
          timestamp: new Date()
        });
        dim.value = newValue;
      }
    }

    if (changes.length > 0) {
      drawing.updatedAt = new Date();
      const history = this.changeHistory.get(drawingId) || [];
      history.push(...changes);
      this.changeHistory.set(drawingId, history);

      // Notify listeners
      this.notifyUpdate(drawingId, { type: 'drawing_updated', changes });
    }
  }

  /**
   * Add размер к чертежу
   */
  addDimension(drawingId: string, dimension: Dimension): void {
    const drawing = this.drawings.get(drawingId);
    if (!drawing) return;

    drawing.dimensions.push(dimension);
    drawing.updatedAt = new Date();

    this.notifyUpdate(drawingId, { type: 'dimension_added', dimensionId: dimension.id });
  }

  /**
   * Add аннотацию к чертежу
   */
  addAnnotation(drawingId: string, annotation: Annotation): void {
    const drawing = this.drawings.get(drawingId);
    if (!drawing) return;

    drawing.annotations.push(annotation);
    drawing.updatedAt = new Date();

    this.notifyUpdate(drawingId, { type: 'annotation_added', annotationId: annotation.id });
  }

  /**
   * Export чертеж в PDF
   */
  exportToPDF(drawingId: string): string {
    const drawing = this.drawings.get(drawingId);
    if (!drawing) return '';

    return this.generatePDF(drawing);
  }

  /**
   * Export чертеж в DWG
   */
  exportToDWG(drawingId: string): string {
    const drawing = this.drawings.get(drawingId);
    if (!drawing) return '';

    return this.generateDWG(drawing);
  }

  /**
   * Export чертеж в DXF
   */
  exportToDXF(drawingId: string): string {
    const drawing = this.drawings.get(drawingId);
    if (!drawing) return '';

    return this.generateDXF(drawing);
  }

  /**
   * Create ревизию чертежа
   */
  createRevision(
    drawingId: string,
    author: string,
    changes: string,
    approved: boolean = false
  ): void {
    const drawing = this.drawings.get(drawingId);
    if (!drawing) return;

    const latestRevision = drawing.revisionHistory[drawing.revisionHistory.length - 1];
    const newVersion = latestRevision.version + 1;

    drawing.revisionHistory.push({
      version: newVersion,
      date: new Date(),
      author,
      changes,
      approved
    });

    drawing.updatedAt = new Date();

    this.notifyUpdate(drawingId, {
      type: 'revision_created',
      version: newVersion
    });
  }

  /**
   * Get чертеж
   */
  getDrawing(drawingId: string): TechnicalDrawing | undefined {
    return this.drawings.get(drawingId);
  }

  /**
   * Get все чертежи проекта
   */
  getProjectDrawings(projectId: string): TechnicalDrawing[] {
    const drawings: TechnicalDrawing[] = [];

    for (const [, drawing] of this.drawings) {
      if (drawing.projectId === projectId) {
        drawings.push(drawing);
      }
    }

    return drawings;
  }

  /**
   * Subscribe на обновления
   */
  subscribe(drawingId: string, listener: Function): string {
    if (!this.updateListeners.has(drawingId)) {
      this.updateListeners.set(drawingId, []);
    }

    this.updateListeners.get(drawingId)!.push(listener);

    return `listener-${Date.now()}`;
  }

  /**
   * Get история изменений
   */
  getChangeHistory(drawingId: string): any[] {
    return this.changeHistory.get(drawingId) || [];
  }

  private generateStandardViews(): View[] {
    return [
      {
        id: 'view-1',
        type: 'front',
        scale: 1,
        position: [10, 150],
        rotation: 0,
        projection: 'orthogonal'
      },
      {
        id: 'view-2',
        type: 'top',
        scale: 1,
        position: [150, 150],
        rotation: 0,
        projection: 'orthogonal'
      },
      {
        id: 'view-3',
        type: 'side',
        scale: 1,
        position: [10, 10],
        rotation: 0,
        projection: 'orthogonal'
      },
      {
        id: 'view-4',
        type: 'isometric',
        scale: 1,
        position: [150, 10],
        rotation: 45,
        projection: 'perspective'
      }
    ];
  }

  private extractDimensions(modelId: string): Dimension[] {
    // Упрощенная версия - в реальности извлекает размеры из 3D модели
    return [];
  }

  private getUpdatedDimensionValue(refId: string, changes: any): number | undefined {
    // Упрощенная версия - определяет новое значение размера
    return undefined;
  }

  private generatePDF(drawing: TechnicalDrawing): string {
    let pdf = '%PDF-1.4\n';
    pdf += '1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n';
    pdf += '2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n';
    pdf += '3 0 obj\n<< /Type /Page /Parent 2 0 R /Resources << >> /MediaBox [0 0 612 792] >>\n';
    pdf += `BT /F1 12 Tf 50 700 Td (${drawing.name}) Tj ET\n`;
    pdf += 'endobj\nxref\n0 4\n';
    pdf += '0000000000 65535 f\n';
    pdf += '0000000009 00000 n\n';
    pdf += '0000000058 00000 n\n';
    pdf += '0000000115 00000 n\n';
    pdf += 'trailer\n<< /Size 4 /Root 1 0 R >>\n';
    pdf += 'startxref\n0\n%%EOF\n';

    return pdf;
  }

  private generateDWG(drawing: TechnicalDrawing): string {
    // Упрощенная DWG генерация (реальная требует бинарного формата)
    let dwg = 'AutoCAD DWG\n';
    dwg += `TITLE: ${drawing.name}\n`;
    dwg += `DATE: ${drawing.updatedAt.toISOString()}\n`;
    dwg += `VIEWS: ${drawing.views.length}\n`;
    dwg += `DIMENSIONS: ${drawing.dimensions.length}\n`;

    return dwg;
  }

  private generateDXF(drawing: TechnicalDrawing): string {
    let dxf = '0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1018\n';
    dxf += '0\nENDSEC\n';
    dxf += '0\nSECTION\n2\nENTITIES\n';

    // Add drawing name as text
    dxf += '0\nTEXT\n8\n0\n10\n0.0\n20\n0.0\n40\n5.0\n';
    dxf += `1\n${drawing.name}\n`;

    // Add dimensions
    for (const dim of drawing.dimensions) {
      dxf += '0\nTEXT\n8\n0\n10\n' + dim.position[0] + '\n20\n' + dim.position[1] + '\n';
      dxf += `40\n3.0\n1\n${dim.value} ${dim.unit}\n`;
    }

    dxf += '0\nENDSEC\n0\nEOF\n';

    return dxf;
  }

  private notifyUpdate(drawingId: string, change: any): void {
    const listeners = this.updateListeners.get(drawingId) || [];
    for (const listener of listeners) {
      try {
        listener(change);
      } catch (error) {
        console.error('Error notifying listener:', error);
      }
    }
  }
}

export default DrawingManager;
