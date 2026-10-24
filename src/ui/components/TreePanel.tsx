/**
 * BendCore UI Component: TreePanel
 * Feature tree with expand, select, show/hide, and context menu
 *
 * @component
 * @example
 * const [selectedNode, setSelectedNode] = useState<string | null>(null);
 * <TreePanel
 *   nodes={nodes}
 *   selectedNode={selectedNode}
 *   onSelect={setSelectedNode}
 *   onToggleVisibility={handleVisibility}
 * />
 */

import React, { useState, useCallback, useRef } from 'react';
import './TreePanel.css';

// ============ Types ============

export interface TreeNode {
  id: string;
  name: string;
  type: 'tube' | 'sketch' | 'bend' | 'feature' | 'folder';
  icon?: string;
  visible?: boolean;
  locked?: boolean;
  children?: TreeNode[];
  selected?: boolean;
  expanded?: boolean;
}

interface ContextMenuPosition {
  x: number;
  y: number;
}

interface TreePanelProps {
  nodes: TreeNode[];
  selectedNode?: string | null;
  onSelect?: (nodeId: string | null) => void;
  onToggleVisibility?: (nodeId: string) => void;
  onDelete?: (nodeId: string) => void;
  onRename?: (nodeId: string, newName: string) => void;
  onReorder?: (sourceId: string, targetId: string) => void;
  isDarkMode?: boolean;
  searchTerm?: string;
}

// ============ Type Icons ============

const TYPE_ICONS: Record<string, string> = {
  tube: '⊙',
  sketch: '📐',
  bend: '↯',
  feature: '⚙️',
  folder: '📁',
};

// ============ Component ============

export const TreePanel: React.FC<TreePanelProps> = ({
  nodes,
  selectedNode,
  onSelect,
  onToggleVisibility,
  onDelete,
  onRename,
  isDarkMode = false,
  searchTerm = '',
}) => {
  const [expandedNodes, setExpandedNodes] = useState<Set<string>>(new Set());
  const [contextMenu, setContextMenu] = useState<{
    position: ContextMenuPosition;
    nodeId: string;
  } | null>(null);
  const [renamingId, setRenamingId] = useState<string | null>(null);
  const [renameValue, setRenameValue] = useState('');
  const [draggedNode, setDraggedNode] = useState<string | null>(null);
  const renamingInputRef = useRef<HTMLInputElement>(null);

  // Toggle expand/collapse
  const toggleExpanded = useCallback(
    (nodeId: string) => {
      const newSet = new Set(expandedNodes);
      if (newSet.has(nodeId)) {
        newSet.delete(nodeId);
      } else {
        newSet.add(nodeId);
      }
      setExpandedNodes(newSet);
    },
    [expandedNodes]
  );

  // Handle node selection
  const handleSelect = useCallback(
    (nodeId: string, event?: React.MouseEvent) => {
      if (event?.ctrlKey || event?.metaKey) {
        // Multi-select not implemented yet, but structure is ready
        console.log('Multi-select:', nodeId);
      }
      onSelect?.(nodeId);
    },
    [onSelect]
  );

  // Handle visibility toggle
  const handleVisibilityToggle = useCallback(
    (nodeId: string, event: React.MouseEvent) => {
      event.stopPropagation();
      onToggleVisibility?.(nodeId);
    },
    [onToggleVisibility]
  );

  // Handle context menu
  const handleContextMenu = useCallback(
    (event: React.MouseEvent, nodeId: string) => {
      event.preventDefault();
      event.stopPropagation();
      setContextMenu({
        position: { x: event.clientX, y: event.clientY },
        nodeId,
      });
    },
    []
  );

  // Handle rename start
  const handleRenameStart = useCallback((nodeId: string, currentName: string) => {
    setRenamingId(nodeId);
    setRenameValue(currentName);
    setContextMenu(null);
  }, []);

  // Handle rename end
  const handleRenameEnd = useCallback(
    (nodeId: string) => {
      if (renameValue.trim()) {
        onRename?.(nodeId, renameValue);
      }
      setRenamingId(null);
      setRenameValue('');
    },
    [renameValue, onRename]
  );

  // Handle rename key press
  const handleRenameKeyPress = useCallback(
    (e: React.KeyboardEvent, nodeId: string) => {
      if (e.key === 'Enter') {
        handleRenameEnd(nodeId);
      } else if (e.key === 'Escape') {
        setRenamingId(null);
        setRenameValue('');
      }
    },
    [handleRenameEnd]
  );

  // Focus input when rename starts
  React.useEffect(() => {
    renamingInputRef.current?.focus();
    renamingInputRef.current?.select();
  }, [renamingId]);

  // Handle key navigation
  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent, nodeId: string) => {
      if (e.key === 'Delete' && !renamingId) {
        onDelete?.(nodeId);
      } else if (e.key === 'F2' && !renamingId) {
        const node = findNodeById(nodes, nodeId);
        if (node) {
          handleRenameStart(nodeId, node.name);
        }
      } else if (e.key === 'ArrowRight') {
        e.preventDefault();
        if (!expandedNodes.has(nodeId)) {
          toggleExpanded(nodeId);
        }
      } else if (e.key === 'ArrowLeft') {
        e.preventDefault();
        if (expandedNodes.has(nodeId)) {
          toggleExpanded(nodeId);
        }
      }
    },
    [renamingId, expandedNodes, nodes, onDelete, handleRenameStart, toggleExpanded]
  );

  // Find node by ID
  const findNodeById = (nodeList: TreeNode[], id: string): TreeNode | null => {
    for (const node of nodeList) {
      if (node.id === id) return node;
      if (node.children) {
        const found = findNodeById(node.children, id);
        if (found) return found;
      }
    }
    return null;
  };

  // Filter nodes by search term
  const filterNodes = (nodeList: TreeNode[]): TreeNode[] => {
    if (!searchTerm) return nodeList;

    return nodeList.filter((node) => {
      const nameMatch = node.name.toLowerCase().includes(searchTerm.toLowerCase());
      const childrenMatch = node.children && filterNodes(node.children).length > 0;
      return nameMatch || childrenMatch;
    });
  };

  const filteredNodes = filterNodes(nodes);

  return (
    <div
      className={`tree-panel ${isDarkMode ? 'tree-panel--dark' : ''}`}
      role="region"
      aria-label="Дерево объектов"
    >
      {/* === Search Bar === */}
      {/* TODO: Add search implementation if needed */}

      {/* === Tree List === */}
      <ul className="tree-panel__list" role="tree">
        {filteredNodes.map((node) => (
          <TreeNodeItem
            key={node.id}
            node={node}
            isSelected={selectedNode === node.id}
            isExpanded={expandedNodes.has(node.id)}
            isRenaming={renamingId === node.id}
            renameValue={renamingValue}
            isDragging={draggedNode === node.id}
            onSelect={handleSelect}
            onToggleVisibility={handleVisibilityToggle}
            onToggleExpand={toggleExpanded}
            onContextMenu={handleContextMenu}
            onRenameStart={handleRenameStart}
            onRenameEnd={handleRenameEnd}
            onRenameChange={(value) => setRenameValue(value)}
            onRenameKeyPress={handleRenameKeyPress}
            onKeyDown={handleKeyDown}
            onDragStart={() => setDraggedNode(node.id)}
            onDragEnd={() => setDraggedNode(null)}
            renamingInputRef={renamingInputRef}
          />
        ))}
      </ul>

      {/* === Context Menu === */}
      {contextMenu && (
        <TreeContextMenu
          position={contextMenu.position}
          nodeId={contextMenu.nodeId}
          onRename={(id, name) => {
            handleRenameStart(id, name);
          }}
          onDelete={onDelete}
          onClose={() => setContextMenu(null)}
        />
      )}
    </div>
  );
};

// ============ TreeNodeItem Component ============

interface TreeNodeItemProps {
  node: TreeNode;
  isSelected: boolean;
  isExpanded: boolean;
  isRenaming: boolean;
  renameValue: string;
  isDragging: boolean;
  onSelect: (nodeId: string, event?: React.MouseEvent) => void;
  onToggleVisibility: (nodeId: string, event: React.MouseEvent) => void;
  onToggleExpand: (nodeId: string) => void;
  onContextMenu: (event: React.MouseEvent, nodeId: string) => void;
  onRenameStart: (nodeId: string, currentName: string) => void;
  onRenameEnd: (nodeId: string) => void;
  onRenameChange: (value: string) => void;
  onRenameKeyPress: (e: React.KeyboardEvent, nodeId: string) => void;
  onKeyDown: (e: React.KeyboardEvent, nodeId: string) => void;
  onDragStart: () => void;
  onDragEnd: () => void;
  renamingInputRef: React.RefObject<HTMLInputElement>;
}

const TreeNodeItem: React.FC<TreeNodeItemProps> = ({
  node,
  isSelected,
  isExpanded,
  isRenaming,
  renameValue,
  isDragging,
  onSelect,
  onToggleVisibility,
  onToggleExpand,
  onContextMenu,
  onRenameStart,
  onRenameEnd,
  onRenameChange,
  onRenameKeyPress,
  onKeyDown,
  onDragStart,
  onDragEnd,
  renamingInputRef,
}) => {
  const hasChildren = node.children && node.children.length > 0;

  return (
    <li
      className={`tree-node ${isSelected ? 'tree-node--selected' : ''} ${
        isDragging ? 'tree-node--dragging' : ''
      }`}
      role="treeitem"
      aria-expanded={hasChildren ? isExpanded : undefined}
      aria-selected={isSelected}
    >
      <div
        className="tree-node__content"
        onClick={(e) => onSelect(node.id, e)}
        onContextMenu={(e) => onContextMenu(e, node.id)}
        onKeyDown={(e) => onKeyDown(e, node.id)}
        draggable
        onDragStart={onDragStart}
        onDragEnd={onDragEnd}
        tabIndex={0}
      >
        {/* === Expand/Collapse Button === */}
        {hasChildren && (
          <button
            className={`tree-node__expand ${isExpanded ? 'tree-node__expand--open' : ''}`}
            onClick={(e) => {
              e.stopPropagation();
              onToggleExpand(node.id);
            }}
            aria-label={isExpanded ? 'Свернуть' : 'Развернуть'}
          >
            ▶
          </button>
        )}
        {!hasChildren && <span className="tree-node__expand-placeholder" />}

        {/* === Icon === */}
        <span className="tree-node__icon">
          {node.icon || TYPE_ICONS[node.type] || '⚙️'}
        </span>

        {/* === Name === */}
        {isRenaming ? (
          <input
            ref={renamingInputRef}
            type="text"
            className="tree-node__rename-input"
            value={renameValue}
            onChange={(e) => onRenameChange(e.target.value)}
            onKeyDown={(e) => onRenameKeyPress(e, node.id)}
            onBlur={() => onRenameEnd(node.id)}
            onClick={(e) => e.stopPropagation()}
            aria-label="Новое имя узла"
          />
        ) : (
          <span className="tree-node__name">{node.name}</span>
        )}

        {/* === Visibility Toggle === */}
        <button
          className={`tree-node__visibility ${
            node.visible === false ? 'tree-node__visibility--hidden' : ''
          }`}
          onClick={(e) => onToggleVisibility(node.id, e)}
          aria-label={node.visible === false ? 'Показать' : 'Скрыть'}
          title={node.visible === false ? 'Скрыто' : 'Видимо'}
        >
          {node.visible === false ? '👁‍🗨' : '👁️'}
        </button>

        {/* === Lock Icon === */}
        {node.locked && (
          <span className="tree-node__lock" title="Заблокировано">
            🔒
          </span>
        )}
      </div>

      {/* === Children === */}
      {hasChildren && isExpanded && (
        <ul className="tree-node__children" role="group">
          {node.children!.map((child) => (
            <TreeNodeItem
              key={child.id}
              node={child}
              isSelected={false}
              isExpanded={false}
              isRenaming={false}
              renameValue=""
              isDragging={false}
              onSelect={onSelect}
              onToggleVisibility={onToggleVisibility}
              onToggleExpand={onToggleExpand}
              onContextMenu={onContextMenu}
              onRenameStart={onRenameStart}
              onRenameEnd={onRenameEnd}
              onRenameChange={onRenameChange}
              onRenameKeyPress={onRenameKeyPress}
              onKeyDown={onKeyDown}
              onDragStart={onDragStart}
              onDragEnd={onDragEnd}
              renamingInputRef={renamingInputRef}
            />
          ))}
        </ul>
      )}
    </li>
  );
};

// ============ Context Menu Component ============

interface TreeContextMenuProps {
  position: ContextMenuPosition;
  nodeId: string;
  onRename?: (nodeId: string, currentName: string) => void;
  onDelete?: (nodeId: string) => void;
  onClose: () => void;
}

const TreeContextMenu: React.FC<TreeContextMenuProps> = ({
  position,
  nodeId,
  onRename,
  onDelete,
  onClose,
}) => {
  const handleClick = useCallback(
    (action: () => void) => {
      action();
      onClose();
    },
    [onClose]
  );

  return (
    <div
      className="tree-context-menu"
      style={{
        position: 'fixed',
        top: position.y,
        left: position.x,
      }}
      role="menu"
      onMouseLeave={onClose}
    >
      <button
        className="tree-context-menu__item"
        onClick={() => handleClick(() => onRename?.(nodeId, ''))},
        role="menuitem"
      >
        Переименовать
      </button>
      <button
        className="tree-context-menu__item tree-context-menu__item--danger"
        onClick={() => handleClick(() => onDelete?.(nodeId))}
        role="menuitem"
      >
        Удалить
      </button>
    </div>
  );
};

export default TreePanel;
