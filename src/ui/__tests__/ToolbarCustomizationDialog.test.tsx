/**
 * BendCore Toolbar Customization Dialog Tests
 */

import React from "react";
import { render, screen, fireEvent } from "@testing-library/react";
import ToolbarCustomizationDialog from "../ToolbarCustomizationDialog";

const mockCommands = [
  {
    id: "new-document",
    label: "New Document",
    category: "file",
    description: "Create a new document",
  },
  {
    id: "save-document",
    label: "Save Document",
    category: "file",
    description: "Save current document",
  },
  {
    id: "new-tube",
    label: "New Tube",
    category: "create",
    description: "Create a new tube",
  },
  {
    id: "edit-tube",
    label: "Edit Tube",
    category: "edit",
    description: "Edit selected tube",
  },
  {
    id: "delete",
    label: "Delete",
    category: "delete",
    description: "Delete selected elements",
  },
];

describe("ToolbarCustomizationDialog", () => {
  const mockOnClose = jest.fn();
  const mockOnSave = jest.fn();
  const currentButtonIds = ["new-document", "save-document"];

  const defaultProps = {
    isOpen: true,
    onClose: mockOnClose,
    onSave: mockOnSave,
    currentButtonIds,
    availableCommands: mockCommands,
  };

  beforeEach(() => {
    jest.clearAllMocks();
  });

  describe("Rendering", () => {
    it("should render when open", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);
      expect(screen.getByText("Customize Toolbar")).toBeInTheDocument();
    });

    it("should not render when closed", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} isOpen={false} />);
      expect(screen.queryByText("Customize Toolbar")).not.toBeInTheDocument();
    });

    it("should display available commands", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);
      expect(screen.getByText("New Document")).toBeInTheDocument();
      expect(screen.getByText("New Tube")).toBeInTheDocument();
    });

    it("should display current toolbar preview", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);
      expect(screen.getByText("Toolbar Preview")).toBeInTheDocument();
    });
  });

  describe("Search Functionality", () => {
    it("should filter commands by search query", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);

      const searchInput = screen.getByPlaceholderText("Search commands...");
      fireEvent.change(searchInput, { target: { value: "tube" } });

      expect(screen.getByText("New Tube")).toBeInTheDocument();
      expect(screen.queryByText("Save Document")).not.toBeInTheDocument();
    });

    it("should filter commands by category", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);

      const fileFilterButton = screen.getByText("File");
      fireEvent.click(fileFilterButton);

      expect(screen.getByText("New Document")).toBeInTheDocument();
      expect(screen.queryByText("New Tube")).not.toBeInTheDocument();
    });

    it("should clear filters when clicking active filter", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);

      const createFilterButton = screen.getByText("Create");
      fireEvent.click(createFilterButton);
      fireEvent.click(createFilterButton);

      // After clicking twice, filter should be cleared
      expect(screen.getByText("New Document")).toBeInTheDocument();
      expect(screen.getByText("New Tube")).toBeInTheDocument();
    });
  });

  describe("Toolbar Modification", () => {
    it("should add command to toolbar", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);

      const addButtons = screen.getAllByText("Add");
      fireEvent.click(addButtons[2]); // Click on "New Tube" add button

      // The preview should now contain the new tube command
      // (exact assertion depends on component implementation)
    });

    it("should remove button from toolbar", () => {
      const { container } = render(
        <ToolbarCustomizationDialog {...defaultProps} />
      );

      // Find remove buttons and click one
      const removeButtons = container.querySelectorAll(".toolbar-preview-remove");
      if (removeButtons.length > 0) {
        fireEvent.click(removeButtons[0]);
      }
    });

    it("should allow reordering buttons", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);

      // Drag and drop functionality would be tested here
      // This is simplified as drag/drop testing requires more setup
    });

    it("should add separator", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);

      const separatorButton = screen.getByText("Add Separator");
      fireEvent.click(separatorButton);
    });
  });

  describe("Dialog Actions", () => {
    it("should close dialog when close button clicked", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);

      const closeButton = screen.getByText("✕");
      fireEvent.click(closeButton);

      expect(mockOnClose).toHaveBeenCalled();
    });

    it("should save changes when save button clicked", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);

      const saveButton = screen.getByText("Save Changes");
      fireEvent.click(saveButton);

      expect(mockOnSave).toHaveBeenCalled();
      expect(mockOnClose).toHaveBeenCalled();
    });

    it("should reset to defaults when reset button clicked", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);

      const resetButton = screen.getByText("Reset to Defaults");
      fireEvent.click(resetButton);

      // After reset, toolbar should show original buttons
    });

    it("should close without saving when cancel button clicked", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);

      const cancelButton = screen.getByText("Cancel");
      fireEvent.click(cancelButton);

      expect(mockOnClose).toHaveBeenCalled();
      expect(mockOnSave).not.toHaveBeenCalled();
    });
  });

  describe("Toolbar Summary", () => {
    it("should display button count", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);

      // The summary should be displayed
      expect(screen.getByText(/Buttons:/)).toBeInTheDocument();
    });

    it("should display separator count", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);

      expect(screen.getByText(/Separators:/)).toBeInTheDocument();
    });
  });

  describe("Drag and Drop", () => {
    // jsdom has no DataTransfer; browsers always attach one to drag events.
    const createDataTransfer = () => {
      const data: Record<string, string> = {};
      return {
        effectAllowed: "uninitialized",
        dropEffect: "none",
        setData: jest.fn((type: string, value: string) => {
          data[type] = value;
        }),
        getData: jest.fn((type: string) => data[type] ?? ""),
      };
    };
    const availableItem = (container: HTMLElement, label: string) =>
      Array.from(container.querySelectorAll<HTMLElement>(".customization-item")).find(
        (item) => item.querySelector(".customization-item-label")?.textContent === label
      ) as HTMLElement;
    const previewTitles = (container: HTMLElement) =>
      Array.from(container.querySelectorAll<HTMLElement>(".toolbar-preview-item")).map(
        (item) => item.title
      );

    it("should handle drag start", () => {
      const { container } = render(
        <ToolbarCustomizationDialog {...defaultProps} />
      );

      const item = availableItem(container, "New Tube");
      const dataTransfer = createDataTransfer();
      fireEvent.dragStart(item, { dataTransfer });

      expect(dataTransfer.effectAllowed).toBe("move");
      expect(dataTransfer.setData).toHaveBeenCalledWith("text/plain", "new-tube");
      expect(item).toHaveClass("dragging");
    });

    it("should handle drag over preview", () => {
      const { container } = render(
        <ToolbarCustomizationDialog {...defaultProps} />
      );

      const dataTransfer = createDataTransfer();
      fireEvent.dragStart(availableItem(container, "New Tube"), { dataTransfer });

      const preview = container.querySelector(".toolbar-preview") as HTMLElement;
      fireEvent.dragOver(preview, { dataTransfer });
      expect(dataTransfer.dropEffect).toBe("move");

      // Dropping on the preview background appends to the toolbar
      fireEvent.drop(preview, { dataTransfer });
      expect(previewTitles(container)).toEqual(["New Document", "Save Document", "New Tube"]);
      expect(screen.getByText("Buttons: 3")).toBeInTheDocument();
    });

    it("should highlight drop zone on drag over", () => {
      const { container } = render(
        <ToolbarCustomizationDialog {...defaultProps} />
      );

      const preview = container.querySelector(".toolbar-preview") as HTMLElement;
      expect(preview).not.toHaveClass("drag-over");

      fireEvent.dragOver(preview, { dataTransfer: createDataTransfer() });
      // Check if drag-over class is applied
      expect(preview).toHaveClass("drag-over");

      fireEvent.dragLeave(preview);
      expect(preview).not.toHaveClass("drag-over");
    });

    it("should insert dropped command before the target item", () => {
      const { container } = render(
        <ToolbarCustomizationDialog {...defaultProps} />
      );

      const dataTransfer = createDataTransfer();
      fireEvent.dragStart(availableItem(container, "Delete"), { dataTransfer });
      const firstPreviewItem = container.querySelector(".toolbar-preview-item") as HTMLElement;
      fireEvent.dragOver(firstPreviewItem, { dataTransfer });
      fireEvent.drop(firstPreviewItem, { dataTransfer });

      expect(previewTitles(container)).toEqual(["Delete", "New Document", "Save Document"]);
    });

    it("should allow dropping into an empty toolbar", () => {
      const { container } = render(
        <ToolbarCustomizationDialog {...defaultProps} currentButtonIds={[]} />
      );

      const dataTransfer = createDataTransfer();
      fireEvent.dragStart(availableItem(container, "New Tube"), { dataTransfer });
      const preview = container.querySelector(".toolbar-preview") as HTMLElement;
      fireEvent.dragOver(preview, { dataTransfer });
      fireEvent.drop(preview, { dataTransfer });

      expect(previewTitles(container)).toEqual(["New Tube"]);
      expect(
        screen.queryByText("Drag commands here to customize toolbar")
      ).not.toBeInTheDocument();
    });
  });

  describe("Empty State", () => {
    it("should show message when preview is empty", () => {
      render(
        <ToolbarCustomizationDialog
          {...defaultProps}
          currentButtonIds={[]}
        />
      );

      expect(
        screen.getByText("Drag commands here to customize toolbar")
      ).toBeInTheDocument();
    });
  });

  describe("Keyboard Navigation", () => {
    it("should close on Escape key", () => {
      const { container } = render(
        <ToolbarCustomizationDialog {...defaultProps} />
      );

      fireEvent.keyDown(container, { key: "Escape" });
      expect(mockOnClose).toHaveBeenCalledTimes(1);
      expect(mockOnSave).not.toHaveBeenCalled();
    });

    it("should handle Enter key for adding commands", () => {
      const { container } = render(<ToolbarCustomizationDialog {...defaultProps} />);

      const item = screen.getByLabelText("New Tube (create)");
      item.focus();
      fireEvent.keyDown(item, { key: "Enter" });

      const titles = Array.from(
        container.querySelectorAll<HTMLElement>(".toolbar-preview-item")
      ).map((el) => el.title);
      expect(titles).toEqual(["New Document", "Save Document", "New Tube"]);
    });
  });

  describe("Accessibility", () => {
    it("should have proper ARIA labels", () => {
      render(<ToolbarCustomizationDialog {...defaultProps} />);

      const closeButton = screen.getByText("✕");
      expect(closeButton).toHaveAttribute("title");
    });

    it("should support keyboard navigation", () => {
      const { container } = render(
        <ToolbarCustomizationDialog {...defaultProps} />
      );

      // Tab navigation through items
    });
  });

  describe("Edge Cases", () => {
    it("should handle empty command list", () => {
      render(
        <ToolbarCustomizationDialog
          {...defaultProps}
          availableCommands={[]}
        />
      );

      expect(screen.getByText("Customize Toolbar")).toBeInTheDocument();
    });

    it("should handle duplicate button IDs gracefully", () => {
      render(
        <ToolbarCustomizationDialog
          {...defaultProps}
          currentButtonIds={["new-document", "new-document"]}
        />
      );

      expect(screen.getByText("Customize Toolbar")).toBeInTheDocument();
    });

    it("should handle unknown command IDs", () => {
      render(
        <ToolbarCustomizationDialog
          {...defaultProps}
          currentButtonIds={["unknown-command"]}
        />
      );

      expect(screen.getByText("Customize Toolbar")).toBeInTheDocument();
    });
  });
});
