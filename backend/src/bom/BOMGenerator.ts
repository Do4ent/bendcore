/**
 * Bill of Materials Generator - Production Ready
 * Generate, manage, and export BOMs with cost tracking
 */

export interface BOMItem {
  id: string;
  partNumber: string;
  description: string;
  quantity: number;
  unit: string;
  material: string;
  weight: number;
  unitCost: number;
  totalCost: number;
  supplier: string;
  leadTime: number;
  inStock: number;
  notes: string;
  drawingRef?: string;
}

export interface BOM {
  id: string;
  projectId: string;
  name: string;
  revision: string;
  items: BOMItem[];
  totalCost: number;
  totalWeight: number;
  createdAt: Date;
  updatedAt: Date;
}

export class BOMGenerator {
  private boms: Map<string, BOM> = new Map();

  /**
   * Generate BOM из сборки
   */
  static generateFromAssembly(assembly: any, includeSubassemblies: boolean = true): BOMItem[] {
    const items: BOMItem[] = [];
    const processed = new Set<string>();

    const traverse = (node: any, parent?: any) => {
      if (processed.has(node.id)) return;
      processed.add(node.id);

      if (node.type === 'part') {
        items.push(this.createBOMItem(node));
      }

      if (includeSubassemblies && node.children) {
        for (const childId of node.children) {
          if (typeof childId === 'string') {
            traverse(childId, node);
          } else {
            traverse(childId, node);
          }
        }
      }
    };

    traverse(assembly);

    // Group by part number and sum quantities
    return this.consolidateItems(items);
  }

  /**
   * Create новый BOM
   */
  createBOM(projectId: string, name: string, items: BOMItem[]): BOM {
    const bom: BOM = {
      id: `bom-${Date.now()}`,
      projectId,
      name,
      revision: '1.0',
      items,
      totalCost: BOMGenerator.calculateTotalCost(items),
      totalWeight: BOMGenerator.calculateTotalWeight(items),
      createdAt: new Date(),
      updatedAt: new Date()
    };

    this.boms.set(bom.id, bom);
    return bom;
  }

  /**
   * Add элемент к BOM
   */
  addItem(bomId: string, item: BOMItem): void {
    const bom = this.boms.get(bomId);
    if (!bom) return;

    // Check if item with same part number exists
    const existingIndex = bom.items.findIndex(i => i.partNumber === item.partNumber);

    if (existingIndex >= 0) {
      bom.items[existingIndex].quantity += item.quantity;
      bom.items[existingIndex].totalCost =
        bom.items[existingIndex].quantity * bom.items[existingIndex].unitCost;
    } else {
      item.totalCost = item.quantity * item.unitCost;
      bom.items.push(item);
    }

    bom.totalCost = BOMGenerator.calculateTotalCost(bom.items);
    bom.totalWeight = BOMGenerator.calculateTotalWeight(bom.items);
    bom.updatedAt = new Date();
  }

  /**
   * Remove элемент из BOM
   */
  removeItem(bomId: string, partNumber: string): void {
    const bom = this.boms.get(bomId);
    if (!bom) return;

    bom.items = bom.items.filter(i => i.partNumber !== partNumber);

    bom.totalCost = BOMGenerator.calculateTotalCost(bom.items);
    bom.totalWeight = BOMGenerator.calculateTotalWeight(bom.items);
    bom.updatedAt = new Date();
  }

  /**
   * Calculate итоговую стоимость
   */
  static calculateTotalCost(items: BOMItem[]): number {
    return items.reduce((sum, item) => sum + item.unitCost * item.quantity, 0);
  }

  /**
   * Calculate итоговый вес
   */
  static calculateTotalWeight(items: BOMItem[]): number {
    return items.reduce((sum, item) => sum + item.weight * item.quantity, 0);
  }

  /**
   * Export BOM в CSV
   */
  static exportToCSV(items: BOMItem[]): string {
    let csv =
      'Part Number,Description,Quantity,Unit,Material,Weight (kg),Unit Cost,Total Cost,Supplier,Lead Time (days),In Stock,Notes\n';

    for (const item of items) {
      csv += `${item.partNumber},"${item.description}",${item.quantity},${item.unit},`;
      csv += `${item.material},${item.weight},${item.unitCost},${item.totalCost},`;
      csv += `${item.supplier},${item.leadTime},${item.inStock},"${item.notes}"\n`;
    }

    return csv;
  }

  /**
   * Export BOM в JSON
   */
  static exportToJSON(bom: BOM): string {
    return JSON.stringify(
      {
        id: bom.id,
        projectId: bom.projectId,
        name: bom.name,
        revision: bom.revision,
        totalCost: bom.totalCost,
        totalWeight: bom.totalWeight,
        createdAt: bom.createdAt.toISOString(),
        updatedAt: bom.updatedAt.toISOString(),
        items: bom.items
      },
      null,
      2
    );
  }

  /**
   * Export BOM в Excel (простой CSV)
   */
  static exportToExcel(bomId: string, bom: BOM): string {
    let excel = 'BOM Report\n';
    excel += `Project: ${bom.projectId}\n`;
    excel += `Name: ${bom.name}\n`;
    excel += `Revision: ${bom.revision}\n`;
    excel += `Total Cost: $${bom.totalCost.toFixed(2)}\n`;
    excel += `Total Weight: ${bom.totalWeight.toFixed(2)} kg\n\n`;

    excel += this.exportToCSV(bom.items);

    return excel;
  }

  /**
   * Get BOM
   */
  getBOM(bomId: string): BOM | undefined {
    return this.boms.get(bomId);
  }

  /**
   * Get все BOMs для проекта
   */
  getProjectBOMs(projectId: string): BOM[] {
    const boms: BOM[] = [];

    for (const [, bom] of this.boms) {
      if (bom.projectId === projectId) {
        boms.push(bom);
      }
    }

    return boms;
  }

  /**
   * Duplicate BOM
   */
  duplicateBOM(bomId: string, newName: string): BOM | null {
    const original = this.boms.get(bomId);
    if (!original) return null;

    const newItems = original.items.map(item => ({ ...item }));

    const newBOM: BOM = {
      id: `bom-${Date.now()}`,
      projectId: original.projectId,
      name: newName,
      revision: '1.0',
      items: newItems,
      totalCost: original.totalCost,
      totalWeight: original.totalWeight,
      createdAt: new Date(),
      updatedAt: new Date()
    };

    this.boms.set(newBOM.id, newBOM);
    return newBOM;
  }

  /**
   * Get статистику BOM
   */
  getStatistics(bomId: string): any {
    const bom = this.boms.get(bomId);
    if (!bom) return null;

    const uniqueSuppliers = new Set(bom.items.map(i => i.supplier)).size;
    const maxLeadTime = Math.max(...bom.items.map(i => i.leadTime));
    const averageCostPerUnit =
      bom.items.length > 0 ? bom.totalCost / bom.items.reduce((sum, i) => sum + i.quantity, 0) : 0;

    return {
      totalItems: bom.items.length,
      totalQuantity: bom.items.reduce((sum, i) => sum + i.quantity, 0),
      totalCost: bom.totalCost,
      totalWeight: bom.totalWeight,
      uniqueSuppliers,
      maxLeadTime,
      averageCostPerUnit,
      inStockCount: bom.items.filter(i => i.inStock > 0).length
    };
  }

  private static createBOMItem(node: any): BOMItem {
    return {
      id: node.id,
      partNumber: node.partNumber || `PART-${node.id.slice(0, 8)}`,
      description: node.name || node.description || 'Unknown',
      quantity: 1,
      unit: node.unit || 'pcs',
      material: node.material || 'Unknown',
      weight: node.weight || 0,
      unitCost: node.cost || 0,
      totalCost: node.cost || 0,
      supplier: node.supplier || 'TBD',
      leadTime: node.leadTime || 5,
      inStock: node.inStock || 0,
      notes: node.notes || ''
    };
  }

  private static consolidateItems(items: BOMItem[]): BOMItem[] {
    const consolidated = new Map<string, BOMItem>();

    for (const item of items) {
      if (consolidated.has(item.partNumber)) {
        const existing = consolidated.get(item.partNumber)!;
        existing.quantity += item.quantity;
        existing.totalCost = existing.quantity * existing.unitCost;
      } else {
        consolidated.set(item.partNumber, { ...item });
      }
    }

    return Array.from(consolidated.values()).sort((a, b) =>
      a.partNumber.localeCompare(b.partNumber)
    );
  }
}

export default BOMGenerator;
