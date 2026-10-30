/**
 * BendCore Feature Tree Component
 * Left sidebar showing design history and document structure
 * Part of Phase 31 foundational work
 */

import React, { useState, useCallback, useEffect } from "react";
import FeatureTreeItem from "./FeatureTreeItem";
import "./FeatureTree.css";

export interface FeatureNode {
  id: string;
  name: string;
  type: "document" | "body" | "sketch" | "tube" | "port" | "constraint";
  iconColor: string;
  visible: boolean;
  expanded: boolean;
  children?: FeatureNode[];
  metadata?: Record<string, any>;
}

export interface FeatureTreeProps {
  onCommand?: (commandId: string, args?: Record<string, any>) => void;
  onSelectFeature?: (featureId: string) => void;
}

interface ContextMenuPosition {
  x: number;
  y: number;
  featureId: string;
}

const FeatureTree: React.FC<FeatureTreeProps> = ({
  onCommand,
  onSelectFeature,
}) => {
  const [root, setRoot] = useState<FeatureNode | null>(null);
  const [selectedFeatureId, setSelectedFeatureId] = useState<string>("");
  const [contextMenu, setContextMenu] = useState<ContextMenuPosition | null>(null);
  const [editingId, setEditingId] = useState<string | null>(null);
  const [editingName, setEditingName] = useState("");

  // Load feature tree from backend
  useEffect(() => {
    const loadFeatureTree = async () => {
      if (window.electron?.ipc) {
        try {
          const result = await window.electron.ipc.invoke(
            "bendcore:command",
            {
              command: "get-feature-tree",
              args: {},
            }
          );
          const featureResult = result as { success?: boolean; data?: { root?: unknown } } | undefined;
          if (featureResult?.success && featureResult?.data?.root) {
            setRoot(featureResult.data.root as FeatureNode);
          } else {
            // Create default empty document
            setRoot({
              id: "document-root",
              name: "Document",
              type: "document",
              iconColor: "#4A5568",
              visible: true,
              expanded: true,
              children: [],
            });
          }
        } catch (error) {
          console.error("Failed to load feature tree:", error);
          // Create default empty document on error
          setRoot({
            id: "document-root",
            name: "Document",
            type: "document",
            iconColor: "#4A5568",
            visible: true,
            expanded: true,
            children: [],
          });
        }
      }
    };

    loadFeatureTree();
  }, []);

  const handleSelectFeature = useCallback(
    (featureId: string) => {
      setSelectedFeatureId(featureId);
      if (onSelectFeature) {
        onSelectFeature(featureId);
      }
    },
    [onSelectFeature]
  );

  const handleExpandFeature = useCallback(
    (featureId: string, expanded: boolean) => {
      // Update UI
      const updateNode = (node: FeatureNode): void => {
        if (node.id === featureId) {
          node.expanded = expanded;
        }
        if (node.children) {
          node.children.forEach(updateNode);
        }
      };

      if (root) {
        updateNode(root);
        setRoot({ ...root });
      }

      // Notify backend
      if (window.electron?.ipc && onCommand) {
        const cmdId = expanded ? "expand-feature" : "collapse-feature";
        window.electron.ipc.invoke("bendcore:command", {
          command: cmdId,
          args: { featureId },
        });
      }
    },
    [root, onCommand]
  );

  const handleContextMenu = useCallback(
    (event: React.MouseEvent, featureId: string) => {
      event.preventDefault();
      setContextMenu({
        x: event.clientX,
        y: event.clientY,
        featureId,
      });
      handleSelectFeature(featureId);
    },
    [handleSelectFeature]
  );

  const handleRenameStart = useCallback((featureId: string, name: string) => {
    setEditingId(featureId);
    setEditingName(name);
  }, []);

  const handleRenameSave = useCallback(
    (featureId: string, newName: string) => {
      if (!newName.trim()) {
        setEditingId(null);
        return;
      }

      // Update UI
      const updateNode = (node: FeatureNode): void => {
        if (node.id === featureId) {
          node.name = newName;
        }
        if (node.children) {
          node.children.forEach(updateNode);
        }
      };

      if (root) {
        updateNode(root);
        setRoot({ ...root });
      }

      // Notify backend
      if (window.electron?.ipc && onCommand) {
        window.electron.ipc.invoke("bendcore:command", {
          command: "rename-feature",
          args: { featureId, name: newName },
        });
      }

      setEditingId(null);
    },
    [root, onCommand]
  );

  const handleContextMenuAction = useCallback(
    (action: string) => {
      const featureId = contextMenu?.featureId;
      if (!featureId) return;

      switch (action) {
        case "edit":
          // Based on feature type, open appropriate editor
          if (window.electron?.ipc && onCommand) {
            const node = findNode(root, featureId);
            if (node?.type === "sketch") {
              onCommand("edit-sketch");
            } else if (node?.type === "tube") {
              onCommand("edit-tube");
            }
          }
          break;

        case "rename":
          const node = findNode(root, featureId);
          if (node) {
            handleRenameStart(featureId, node.name);
          }
          break;

        case "delete":
          if (window.electron?.ipc && onCommand) {
            onCommand("delete-feature", { featureId });
          }
          // Remove from tree
          const deleteNode = (node: FeatureNode): boolean => {
            if (node.children) {
              const index = node.children.findIndex((c) => c.id === featureId);
              if (index !== -1) {
                node.children.splice(index, 1);
                return true;
              }
              return node.children.some(deleteNode);
            }
            return false;
          };
          if (root) {
            deleteNode(root);
            setRoot({ ...root });
          }
          break;

        case "visibility":
          if (window.electron?.ipc && onCommand) {
            const visNode = findNode(root, featureId);
            onCommand("set-feature-visibility", {
              featureId,
              visible: !visNode?.visible,
            });
          }
          break;
      }

      setContextMenu(null);
    },
    [contextMenu, root, onCommand, handleRenameStart]
  );

  const handleClickOutside = useCallback(() => {
    setContextMenu(null);
  }, []);

  if (!root) {
    return (
      <div className="feature-tree loading">
        <div>Loading feature tree...</div>
      </div>
    );
  }

  return (
    <div className="feature-tree" onClick={handleClickOutside}>
      <div className="feature-tree-header">
        <h3>Feature Tree</h3>
      </div>

      <div className="feature-tree-container">
        <FeatureTreeItem
          node={root}
          selected={selectedFeatureId === root.id}
          editing={editingId === root.id}
          editingName={editingName}
          onSelect={handleSelectFeature}
          onExpand={handleExpandFeature}
          onContextMenu={handleContextMenu}
          onRenameStart={handleRenameStart}
          onRenameSave={handleRenameSave}
          onEditingNameChange={setEditingName}
        />
      </div>

      {/* Context Menu */}
      {contextMenu && (
        <ContextMenu
          x={contextMenu.x}
          y={contextMenu.y}
          onAction={handleContextMenuAction}
          onClose={() => setContextMenu(null)}
        />
      )}
    </div>
  );
};

interface ContextMenuProps {
  x: number;
  y: number;
  onAction: (action: string) => void;
  onClose: () => void;
}

const ContextMenu: React.FC<ContextMenuProps> = ({ x, y, onAction, onClose }) => {
  const items = [
    { label: "Edit", action: "edit", icon: "✎" },
    { label: "Rename", action: "rename", icon: "✐" },
    { label: "Delete", action: "delete", icon: "✗" },
    { label: "Toggle Visibility", action: "visibility", icon: "👁" },
  ];

  return (
    <div
      className="context-menu"
      style={{
        top: `${y}px`,
        left: `${x}px`,
      }}
      onClick={(e) => e.stopPropagation()}
    >
      {items.map((item) => (
        <div
          key={item.action}
          className="context-menu-item"
          onClick={() => {
            onAction(item.action);
            onClose();
          }}
        >
          <span className="icon">{item.icon}</span>
          <span>{item.label}</span>
        </div>
      ))}
    </div>
  );
};

// Helper function
function findNode(node: FeatureNode | null, id: string): FeatureNode | null {
  if (!node) return null;
  if (node.id === id) return node;
  if (node.children) {
    for (const child of node.children) {
      const found = findNode(child, id);
      if (found) return found;
    }
  }
  return null;
}

export default FeatureTree;
