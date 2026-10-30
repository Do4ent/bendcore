/**
 * Assembly Hierarchy Manager - Production Ready
 * Multi-level assembly management with in-context editing support
 */

export interface AssemblyNode {
  id: string;
  name: string;
  type: 'part' | 'assembly' | 'subassembly';
  parent?: string;
  children: string[];
  position: [number, number, number];
  rotation: [number, number, number];
  scale: [number, number, number];
  visible: boolean;
  locked: boolean;
  metadata: Map<string, any>;
  constraints: string[];
  references: string[];
}

export interface AssemblyConstraint {
  id: string;
  type: 'coincident' | 'parallel' | 'perpendicular' | 'distance' | 'angle';
  entity1: string;
  entity2: string;
  value?: number;
  tolerance?: number;
}

export interface AssemblyReference {
  id: string;
  sourceId: string;
  targetId: string;
  type: string;
  data: any;
}

export class AssemblyHierarchyManager {
  private nodes: Map<string, AssemblyNode> = new Map();
  private depth: Map<string, number> = new Map();
  private constraints: Map<string, AssemblyConstraint> = new Map();
  private references: Map<string, AssemblyReference> = new Map();

  /**
   * Add part/assembly к иерархии
   */
  addPart(id: string, parentId: string | null, part: any): void {
    if (this.nodes.has(id)) {
      throw new Error(`Part с ID ${id} уже существует`);
    }

    const node: AssemblyNode = {
      id,
      name: part.name || `Part-${id.slice(0, 8)}`,
      type: part.type || 'part',
      parent: parentId || undefined,
      children: [],
      position: part.position || [0, 0, 0],
      rotation: part.rotation || [0, 0, 0],
      scale: part.scale || [1, 1, 1],
      visible: part.visible !== false,
      locked: part.locked || false,
      metadata: new Map(Object.entries(part.metadata || {})),
      constraints: [],
      references: []
    };

    this.nodes.set(id, node);

    // Update parent
    if (parentId) {
      const parent = this.nodes.get(parentId);
      if (parent) {
        parent.children.push(id);
      } else {
        throw new Error(`Parent с ID ${parentId} не найден`);
      }
    }

    this.updateDepth(id);
  }

  /**
   * Add constraint между двумя entities
   */
  addConstraint(constraint: AssemblyConstraint): void {
    if (!this.nodes.has(constraint.entity1) || !this.nodes.has(constraint.entity2)) {
      throw new Error('One or both entities not found');
    }

    this.constraints.set(constraint.id, constraint);

    // Add constraint references to nodes
    const node1 = this.nodes.get(constraint.entity1);
    const node2 = this.nodes.get(constraint.entity2);

    if (node1) node1.constraints.push(constraint.id);
    if (node2) node2.constraints.push(constraint.id);
  }

  /**
   * Add reference между parts
   */
  addReference(sourceId: string, targetId: string, type: string, data: any = {}): string {
    const referenceId = `ref-${Date.now()}-${Math.random().toString(36).slice(2, 9)}`;

    const reference: AssemblyReference = {
      id: referenceId,
      sourceId,
      targetId,
      type,
      data
    };

    this.references.set(referenceId, reference);

    const source = this.nodes.get(sourceId);
    const target = this.nodes.get(targetId);

    if (source) source.references.push(referenceId);
    if (target) target.references.push(referenceId);

    return referenceId;
  }

  /**
   * In-context editing: edit sub-assembly within parent context
   */
  editInContext(partId: string): any {
    const part = this.nodes.get(partId);
    if (!part || !part.parent) {
      throw new Error(`Part ${partId} не найдена или не имеет parent`);
    }

    const parent = this.nodes.get(part.parent);
    if (!parent) {
      throw new Error(`Parent assembly not found`);
    }

    return {
      part,
      parent,
      context: {
        visibleParts: this.getVisibleParts(part.parent),
        hiddenParts: this.getHiddenParts(part.parent),
        constraints: this.getConstraintsForPart(partId),
        references: this.getReferencesForPart(partId),
        siblings: parent.children
          .filter(childId => childId !== partId)
          .map(childId => this.nodes.get(childId))
      }
    };
  }

  /**
   * Get flattened list of all parts
   */
  getAllParts(parentId?: string): AssemblyNode[] {
    const parts: AssemblyNode[] = [];
    const roots = parentId ? [parentId] : this.getRootNodes();

    const traverse = (nodeIds: string[]) => {
      for (const id of nodeIds) {
        const node = this.nodes.get(id);
        if (!node) continue;

        parts.push(node);
        traverse(node.children);
      }
    };

    traverse(roots);
    return parts;
  }

  /**
   * Calculate maximum depth of assembly
   */
  getMaxDepth(): number {
    let max = 0;
    for (const d of this.depth.values()) {
      max = Math.max(max, d);
    }
    return max;
  }

  /**
   * Validate hierarchy (max depth, circular refs, etc)
   */
  validate(): { valid: boolean; issues: string[] } {
    const issues: string[] = [];

    // Check depth limit (max 20 levels)
    if (this.getMaxDepth() > 20) {
      issues.push('Глубина сборки превышает максимум 20 уровней');
    }

    // Check circular references
    for (const [id] of this.nodes) {
      if (this.hasCircularReference(id, new Set())) {
        issues.push(`Циклическая ссылка найдена у ${id}`);
      }
    }

    // Check for orphaned nodes
    for (const [id, node] of this.nodes) {
      if (node.parent && !this.nodes.has(node.parent)) {
        issues.push(`Orphaned node ${id} (parent не существует)`);
      }

      for (const childId of node.children) {
        if (!this.nodes.has(childId)) {
          issues.push(`Orphaned child reference in ${id}`);
        }
      }
    }

    // Check constraint consistency
    for (const [, constraint] of this.constraints) {
      if (!this.nodes.has(constraint.entity1) || !this.nodes.has(constraint.entity2)) {
        issues.push(`Invalid constraint ${constraint.id}`);
      }
    }

    return {
      valid: issues.length === 0,
      issues
    };
  }

  /**
   * Get все constraints для part
   */
  private getConstraintsForPart(partId: string): AssemblyConstraint[] {
    const constraints: AssemblyConstraint[] = [];

    for (const [, constraint] of this.constraints) {
      if (constraint.entity1 === partId || constraint.entity2 === partId) {
        constraints.push(constraint);
      }
    }

    return constraints;
  }

  /**
   * Get все references для part
   */
  private getReferencesForPart(partId: string): AssemblyReference[] {
    const references: AssemblyReference[] = [];

    for (const [, reference] of this.references) {
      if (reference.sourceId === partId || reference.targetId === partId) {
        references.push(reference);
      }
    }

    return references;
  }

  /**
   * Get visible parts
   */
  private getVisibleParts(parentId: string): string[] {
    return this.getAllParts(parentId)
      .filter(p => p.visible)
      .map(p => p.id);
  }

  /**
   * Get hidden parts
   */
  private getHiddenParts(parentId: string): string[] {
    return this.getAllParts(parentId)
      .filter(p => !p.visible)
      .map(p => p.id);
  }

  /**
   * Update part position in 3D space
   */
  updatePosition(partId: string, position: [number, number, number]): void {
    const node = this.nodes.get(partId);
    if (node) {
      node.position = position;
    }
  }

  /**
   * Update part rotation
   */
  updateRotation(partId: string, rotation: [number, number, number]): void {
    const node = this.nodes.get(partId);
    if (node) {
      node.rotation = rotation;
    }
  }

  /**
   * Toggle visibility
   */
  toggleVisibility(partId: string): boolean {
    const node = this.nodes.get(partId);
    if (node) {
      node.visible = !node.visible;
      return node.visible;
    }
    throw new Error(`Part ${partId} not found`);
  }

  /**
   * Toggle lock state
   */
  toggleLock(partId: string): boolean {
    const node = this.nodes.get(partId);
    if (node) {
      node.locked = !node.locked;
      return node.locked;
    }
    throw new Error(`Part ${partId} not found`);
  }

  /**
   * Get part by ID
   */
  getPart(partId: string): AssemblyNode | undefined {
    return this.nodes.get(partId);
  }

  /**
   * Remove part and children
   */
  removePart(partId: string): string[] {
    const removed: string[] = [];
    const node = this.nodes.get(partId);

    if (!node) {
      throw new Error(`Part ${partId} not found`);
    }

    // Remove children first
    for (const childId of node.children) {
      removed.push(...this.removePart(childId));
    }

    // Remove constraints
    const constraintsToRemove: string[] = [];
    for (const [id, constraint] of this.constraints) {
      if (constraint.entity1 === partId || constraint.entity2 === partId) {
        constraintsToRemove.push(id);
      }
    }
    for (const id of constraintsToRemove) {
      this.constraints.delete(id);
    }

    // Remove references
    const referencesToRemove: string[] = [];
    for (const [id, reference] of this.references) {
      if (reference.sourceId === partId || reference.targetId === partId) {
        referencesToRemove.push(id);
      }
    }
    for (const id of referencesToRemove) {
      this.references.delete(id);
    }

    // Remove from parent
    if (node.parent) {
      const parent = this.nodes.get(node.parent);
      if (parent) {
        parent.children = parent.children.filter(id => id !== partId);
      }
    }

    // Remove the node itself
    this.nodes.delete(partId);
    this.depth.delete(partId);

    removed.push(partId);
    return removed;
  }

  /**
   * Export hierarchy as tree structure
   */
  exportAsTree(rootId?: string): any {
    const root = rootId
      ? this.nodes.get(rootId)
      : this.getRootNodes().length === 1
        ? this.nodes.get(this.getRootNodes()[0])
        : null;

    if (!root) {
      return null;
    }

    return this.buildTree(root);
  }

  /**
   * Export hierarchy as flat list
   */
  exportAsFlat(): any[] {
    const items = [];

    for (const [, node] of this.nodes) {
      items.push({
        ...node,
        metadata: Object.fromEntries(node.metadata)
      });
    }

    return items;
  }

  /**
   * Get статистику сборки
   */
  getStatistics(): any {
    const totalParts = this.nodes.size;
    const parts = Array.from(this.nodes.values()).filter(n => n.type === 'part').length;
    const assemblies = Array.from(this.nodes.values()).filter(n => n.type !== 'part').length;
    const totalConstraints = this.constraints.size;
    const totalReferences = this.references.size;

    return {
      totalParts,
      parts,
      assemblies,
      maxDepth: this.getMaxDepth(),
      totalConstraints,
      totalReferences,
      timestamp: new Date()
    };
  }

  private updateDepth(nodeId: string, d: number = 1): void {
    this.depth.set(nodeId, d);
    const node = this.nodes.get(nodeId);
    if (node && node.parent) {
      this.updateDepth(node.parent, d + 1);
    }
  }

  private getRootNodes(): string[] {
    const roots: string[] = [];
    for (const [id, node] of this.nodes) {
      if (!node.parent) roots.push(id);
    }
    return roots;
  }

  private hasCircularReference(nodeId: string, visited: Set<string>): boolean {
    if (visited.has(nodeId)) return true;
    visited.add(nodeId);

    const node = this.nodes.get(nodeId);
    if (!node || !node.parent) return false;

    return this.hasCircularReference(node.parent, new Set(visited));
  }

  private buildTree(node: AssemblyNode): any {
    return {
      id: node.id,
      name: node.name,
      type: node.type,
      position: node.position,
      rotation: node.rotation,
      visible: node.visible,
      locked: node.locked,
      children: node.children
        .map(childId => this.nodes.get(childId))
        .filter((n): n is AssemblyNode => n !== undefined)
        .map(child => this.buildTree(child))
    };
  }
}

export default AssemblyHierarchyManager;
