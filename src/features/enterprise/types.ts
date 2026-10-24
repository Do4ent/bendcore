/**
 * ЭТАП 4: ENTERPRISE ФУНКЦИИ
 * Типы для автоматизации маршрутизации труб и BOM
 */

export interface PipeFitting {
  id: string;
  type: 'elbow' | 'tee' | 'coupling' | 'reducer' | 'cap' | 'flange';
  angle: number;
  size: number;
  cost: number;
}

export interface PipeRoute {
  id: string;
  name: string;
  startPoint: [number, number, number];
  endPoint: [number, number, number];
  diameter: number;
  material: string;
  fittings: PipeFitting[];
  length: number;
  bendCount: number;
  cost: number;
}

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
  leadTime: number;  // В днях
}

export interface BillOfMaterials {
  id: string;
  projectName: string;
  items: BOMItem[];
  totalCost: number;
  totalWeight: number;
  createdAt: Date;
}

export interface EnterpriseState {
  routes: PipeRoute[];
  boms: BillOfMaterials[];
}
