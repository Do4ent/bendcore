/**
 * BendCore Feature Tree Item Component
 * Individual tree node rendering
 */

import React from "react";
import { FeatureNode } from "./FeatureTree";

export interface FeatureTreeItemProps {
  node: FeatureNode;
  selected: boolean;
  editing: boolean;
  editingName: string;
  onSelect: (featureId: string) => void;
  onExpand: (featureId: string, expanded: boolean) => void;
  onContextMenu: (event: React.MouseEvent, featureId: string) => void;
  onRenameStart: (featureId: string, name: string) => void;
  onRenameSave: (featureId: string, newName: string) => void;
  onEditingNameChange: (name: string) => void;
}

const FeatureTreeItem: React.FC<FeatureTreeItemProps> = ({
  node,
  selected,
  editing,
  editingName,
  onSelect,
  onExpand,
  onContextMenu,
  onRenameStart,
  onRenameSave,
  onEditingNameChange,
}) => {
  const hasChildren = node.children && node.children.length > 0;
  const typeIcon = getTypeIcon(node.type);
  const typeLabel = getTypeLabel(node.type);

  const handleToggleExpand = (e: React.MouseEvent) => {
    e.stopPropagation();
    onExpand(node.id, !node.expanded);
  };

  const handleSelect = (e: React.MouseEvent) => {
    e.stopPropagation();
    onSelect(node.id);
  };

  const handleContextMenu = (e: React.MouseEvent) => {
    onContextMenu(e, node.id);
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === "Enter") {
      onRenameSave(node.id, editingName);
    } else if (e.key === "Escape") {
      onEditingNameChange(node.name);
    }
  };

  return (
    <div className="feature-tree-item">
      <div
        className={`feature-tree-item-content ${selected ? "selected" : ""}`}
        onClick={handleSelect}
        onContextMenu={handleContextMenu}
      >
        {/* Expand/Collapse Toggle */}
        {hasChildren && (
          <button
            className={`expand-toggle ${node.expanded ? "expanded" : ""}`}
            onClick={handleToggleExpand}
            title={node.expanded ? "Collapse" : "Expand"}
          >
            ▶
          </button>
        )}
        {!hasChildren && <div className="expand-toggle-placeholder" />}

        {/* Visibility Toggle */}
        <button
          className={`visibility-toggle ${node.visible ? "visible" : "hidden"}`}
          onClick={(e) => {
            e.stopPropagation();
            onContextMenu(e, node.id);
          }}
          title={node.visible ? "Hide" : "Show"}
        >
          {node.visible ? "👁" : "⊘"}
        </button>

        {/* Icon */}
        <div
          className="feature-icon"
          style={{ backgroundColor: node.iconColor }}
          title={typeLabel}
        >
          {typeIcon}
        </div>

        {/* Name */}
        {editing ? (
          <input
            type="text"
            className="feature-name-input"
            value={editingName}
            onChange={(e) => onEditingNameChange(e.target.value)}
            onBlur={() => onRenameSave(node.id, editingName)}
            onKeyDown={handleKeyDown}
            autoFocus
            onClick={(e) => e.stopPropagation()}
          />
        ) : (
          <span
            className="feature-name"
            onDoubleClick={(e) => {
              e.stopPropagation();
              onRenameStart(node.id, node.name);
            }}
          >
            {node.name}
          </span>
        )}
      </div>

      {/* Children */}
      {hasChildren && node.expanded && (
        <div className="feature-tree-children">
          {node.children!.map((child) => (
            <FeatureTreeItem
              key={child.id}
              node={child}
              selected={selected && child.id === node.id}
              editing={editing && child.id === node.id}
              editingName={editingName}
              onSelect={onSelect}
              onExpand={onExpand}
              onContextMenu={onContextMenu}
              onRenameStart={onRenameStart}
              onRenameSave={onRenameSave}
              onEditingNameChange={onEditingNameChange}
            />
          ))}
        </div>
      )}
    </div>
  );
};

function getTypeIcon(type: string): string {
  const icons: Record<string, string> = {
    document: "📄",
    body: "📦",
    sketch: "✏️",
    tube: "⚙️",
    port: "⭕",
    constraint: "🔗",
  };
  return icons[type] || "•";
}

function getTypeLabel(type: string): string {
  const labels: Record<string, string> = {
    document: "Document",
    body: "Body",
    sketch: "Sketch",
    tube: "Tube Element",
    port: "Connection Port",
    constraint: "Constraint",
  };
  return labels[type] || type;
}

export default FeatureTreeItem;
