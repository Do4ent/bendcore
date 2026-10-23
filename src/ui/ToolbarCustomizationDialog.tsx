/**
 * BendCore Toolbar Customization Dialog
 * Allows users to customize toolbar buttons, reorder them, and preview changes
 */

import React, { useState, useCallback, useMemo } from "react";
import "./ToolbarCustomization.css";

interface Command {
  id: string;
  label: string;
  category: string;
  description?: string;
  icon?: string;
}

interface ToolbarCustomizationDialogProps {
  isOpen: boolean;
  onClose: () => void;
  onSave: (buttonIds: string[]) => void;
  currentButtonIds: string[];
  availableCommands: Command[];
}

const COMMAND_CATEGORIES = [
  { id: "create", label: "Create", color: "var(--icon-create-green)" },
  { id: "edit", label: "Edit", color: "var(--icon-edit-orange)" },
  { id: "delete", label: "Delete", color: "var(--icon-delete-red)" },
  { id: "file", label: "File", color: "var(--icon-neutral-gray)" },
  { id: "tools", label: "Tools", color: "var(--icon-auxiliary-brown)" },
  { id: "analyze", label: "Analyze", color: "var(--icon-update-yellow)" },
  { id: "import", label: "Import", color: "var(--icon-import-cyan)" },
  { id: "view", label: "View", color: "var(--icon-freeze-blue)" },
  { id: "sketch", label: "Sketch", color: "var(--icon-links-purple)" },
];

const ToolbarCustomizationDialog: React.FC<ToolbarCustomizationDialogProps> = ({
  isOpen,
  onClose,
  onSave,
  currentButtonIds,
  availableCommands,
}) => {
  const [searchQuery, setSearchQuery] = useState("");
  const [selectedCategories, setSelectedCategories] = useState<Set<string>>(
    new Set()
  );
  const [toolbarItems, setToolbarItems] = useState<string[]>(currentButtonIds);
  const [draggedItem, setDraggedItem] = useState<string | null>(null);
  const [dragSource, setDragSource] = useState<"available" | "toolbar" | null>(
    null
  );

  // Filter available commands
  const filteredCommands = useMemo(() => {
    return availableCommands.filter((cmd) => {
      const matchesSearch =
        cmd.label.toLowerCase().includes(searchQuery.toLowerCase()) ||
        cmd.description?.toLowerCase().includes(searchQuery.toLowerCase());

      const matchesCategory =
        selectedCategories.size === 0 ||
        selectedCategories.has(cmd.category);

      return matchesSearch && matchesCategory;
    });
  }, [searchQuery, selectedCategories, availableCommands]);

  // Get command by ID
  const getCommand = useCallback(
    (id: string) => {
      if (id === "separator") {
        return { id: "separator", label: "---", category: "utility" };
      }
      return availableCommands.find((cmd) => cmd.id === id);
    },
    [availableCommands]
  );

  // Handle category filter
  const toggleCategory = (categoryId: string) => {
    const newCategories = new Set(selectedCategories);
    if (newCategories.has(categoryId)) {
      newCategories.delete(categoryId);
    } else {
      newCategories.add(categoryId);
    }
    setSelectedCategories(newCategories);
  };

  // Handle drag start
  const handleDragStart = (
    e: React.DragEvent,
    itemId: string,
    source: "available" | "toolbar"
  ) => {
    setDraggedItem(itemId);
    setDragSource(source);
    e.dataTransfer.effectAllowed = "move";
  };

  // Handle drag over
  const handleDragOver = (e: React.DragEvent) => {
    e.preventDefault();
    e.dataTransfer.dropEffect = "move";
  };

  // Handle drop in toolbar
  const handleDropToolbar = (e: React.DragEvent, targetIndex: number) => {
    e.preventDefault();
    if (!draggedItem) return;

    if (dragSource === "available") {
      // Add from available commands
      const newItems = [...toolbarItems];
      newItems.splice(targetIndex, 0, draggedItem);
      setToolbarItems(newItems);
    } else if (dragSource === "toolbar") {
      // Reorder within toolbar
      const newItems = [...toolbarItems];
      const currentIndex = newItems.indexOf(draggedItem);
      if (currentIndex !== -1) {
        newItems.splice(currentIndex, 1);
        const insertIndex =
          currentIndex < targetIndex ? targetIndex - 1 : targetIndex;
        newItems.splice(insertIndex, 0, draggedItem);
        setToolbarItems(newItems);
      }
    }

    setDraggedItem(null);
    setDragSource(null);
  };

  // Handle remove from toolbar
  const handleRemoveFromToolbar = (itemId: string) => {
    setToolbarItems(toolbarItems.filter((id) => id !== itemId));
  };

  // Handle add button
  const handleAddCommand = (commandId: string) => {
    if (!toolbarItems.includes(commandId)) {
      setToolbarItems([...toolbarItems, commandId]);
    }
  };

  // Handle save
  const handleSave = () => {
    onSave(toolbarItems);
    onClose();
  };

  // Handle reset to defaults
  const handleReset = () => {
    setToolbarItems(currentButtonIds);
  };

  if (!isOpen) return null;

  return (
    <div className="toolbar-customization-dialog">
      <div className="toolbar-customization-dialog-content">
        {/* Header */}
        <div className="toolbar-customization-header">
          <h2 className="toolbar-customization-title">Customize Toolbar</h2>
          <button
            className="toolbar-customization-close"
            onClick={onClose}
            title="Close"
          >
            ✕
          </button>
        </div>

        {/* Body */}
        <div className="toolbar-customization-body">
          {/* Available Commands Panel */}
          <div className="customization-panel">
            <div className="customization-panel-header">Available Commands</div>

            {/* Search */}
            <div className="customization-panel-search">
              <input
                type="text"
                className="customization-search-input"
                placeholder="Search commands..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
              />
            </div>

            {/* Filters */}
            <div className="customization-filter-buttons">
              {COMMAND_CATEGORIES.map((cat) => (
                <button
                  key={cat.id}
                  className={`filter-button ${
                    selectedCategories.has(cat.id) ? "active" : ""
                  }`}
                  onClick={() => toggleCategory(cat.id)}
                  title={cat.label}
                >
                  {cat.label}
                </button>
              ))}
            </div>

            {/* Commands List */}
            <div className="customization-items">
              {filteredCommands.map((cmd) => (
                <div
                  key={cmd.id}
                  className={`customization-item ${
                    draggedItem === cmd.id && dragSource === "available"
                      ? "dragging"
                      : ""
                  }`}
                  draggable
                  onDragStart={(e) =>
                    handleDragStart(e, cmd.id, "available")
                  }
                  onDragEnd={() => {
                    setDraggedItem(null);
                    setDragSource(null);
                  }}
                >
                  <div
                    className="customization-item-icon"
                    style={{
                      color: COMMAND_CATEGORIES.find(
                        (c) => c.id === cmd.category
                      )?.color || "var(--icon-neutral-gray)",
                    }}
                  >
                    ■
                  </div>
                  <div className="customization-item-label">{cmd.label}</div>
                  <div className="customization-item-category">
                    {cmd.category}
                  </div>
                  <button
                    className="customization-item-action"
                    onClick={() => handleAddCommand(cmd.id)}
                    title="Add to toolbar"
                  >
                    Add
                  </button>
                </div>
              ))}
            </div>
          </div>

          {/* Toolbar Preview Panel */}
          <div className="toolbar-preview-panel">
            <div className="toolbar-preview-title">Toolbar Preview</div>

            <div className="toolbar-preview">
              {toolbarItems.length === 0 ? (
                <div
                  style={{
                    width: "100%",
                    display: "flex",
                    alignItems: "center",
                    justifyContent: "center",
                    color: "var(--text-secondary, #666666)",
                    fontStyle: "italic",
                  }}
                >
                  Drag commands here to customize toolbar
                </div>
              ) : (
                toolbarItems.map((itemId, index) => {
                  const cmd = getCommand(itemId);
                  return (
                    <div
                      key={`${itemId}-${index}`}
                      className={`toolbar-preview-item ${
                        itemId === "separator" ? "separator" : ""
                      } ${
                        draggedItem === itemId && dragSource === "toolbar"
                          ? "dragging"
                          : ""
                      }`}
                      draggable
                      onDragStart={(e) =>
                        handleDragStart(e, itemId, "toolbar")
                      }
                      onDragEnd={() => {
                        setDraggedItem(null);
                        setDragSource(null);
                      }}
                      onDragOver={handleDragOver}
                      onDrop={(e) => handleDropToolbar(e, index)}
                      title={cmd?.label || ""}
                    >
                      {itemId !== "separator" && (
                        <>
                          <div className="toolbar-preview-item-icon">
                            {cmd && (
                              <div
                                style={{
                                  width: "24px",
                                  height: "24px",
                                  display: "flex",
                                  alignItems: "center",
                                  justifyContent: "center",
                                  color: COMMAND_CATEGORIES.find(
                                    (c) => c.id === cmd.category
                                  )?.color || "var(--icon-neutral-gray)",
                                  fontSize: "12px",
                                }}
                              >
                                ■
                              </div>
                            )}
                          </div>
                          <button
                            className="toolbar-preview-remove"
                            onClick={() => handleRemoveFromToolbar(itemId)}
                            title="Remove from toolbar"
                          >
                            ×
                          </button>
                        </>
                      )}
                    </div>
                  );
                })
              )}
            </div>

            {/* Add Separator Button */}
            <div style={{ marginBottom: "12px" }}>
              <button
                className="customization-button"
                onClick={() => setToolbarItems([...toolbarItems, "separator"])}
              >
                Add Separator
              </button>
            </div>

            {/* Preview Stats */}
            <div
              style={{
                padding: "12px",
                background: "var(--bg-secondary, #f5f5f5)",
                borderRadius: "4px",
                fontSize: "13px",
                color: "var(--text-secondary, #666666)",
                flex: 1,
                overflow: "auto",
              }}
            >
              <div>
                <strong>Toolbar Summary:</strong>
              </div>
              <div>Buttons: {toolbarItems.filter((id) => id !== "separator").length}</div>
              <div>Separators: {toolbarItems.filter((id) => id === "separator").length}</div>
              <div style={{ marginTop: "8px", fontSize: "12px" }}>
                <strong>Tips:</strong>
                <ul style={{ margin: "4px 0", paddingLeft: "16px" }}>
                  <li>Drag commands to add them to the toolbar</li>
                  <li>Drag buttons in the preview to reorder</li>
                  <li>Click × to remove buttons</li>
                  <li>Use separators to group related commands</li>
                </ul>
              </div>
            </div>
          </div>
        </div>

        {/* Footer */}
        <div className="toolbar-customization-footer">
          <button className="customization-button" onClick={handleReset}>
            Reset to Defaults
          </button>
          <button className="customization-button" onClick={onClose}>
            Cancel
          </button>
          <button className="customization-button primary" onClick={handleSave}>
            Save Changes
          </button>
        </div>
      </div>
    </div>
  );
};

export default ToolbarCustomizationDialog;
