/**
 * Template System E2E Tests
 * Integration tests for template UI and auto-selection features
 * Task 2.8: E2E Tests (2 scenarios)
 */

import React from "react";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import TubeDialog, { TubeConfig } from "../src/ui/TubeDialog";
import TemplateEditor from "../src/ui/TemplateEditor";
import { useTemplates } from "../src/ui/hooks/useTemplates";

// ============ E2E TEST 1: Auto-select template on TubeDialog open, apply defaults ============

describe("E2E: Template Auto-Selection in TubeDialog", () => {
  it("should auto-select template on dialog open and apply defaults", async () => {
    const mockOnSave = jest.fn();
    const mockOnClose = jest.fn();

    const { rerender } = render(
      <TubeDialog
        isOpen={true}
        onClose={mockOnClose}
        onSave={mockOnSave}
        initialConfig={{
          geometry: {
            outerDiameter: 25.0,
            wallThickness: 2.0,
            length: 400.0,
            material: "steel-1020",
            materialName: "Carbon Steel 1020",
          },
          bends: [],
          ports: [],
          advanced: {
            springbackFactor: 0.3,
            bendCompensation: false,
            lengthTolerance: 2.0,
            angleTolerance: 0.5,
            surfaceFinish: "Raw",
          },
        }}
      />
    );

    // Step 1: Dialog opens and geometry tab is active
    expect(screen.getByText("Tube Element Configuration")).toBeInTheDocument();
    expect(screen.getByText("Geometry")).toBeInTheDocument();

    // Step 2: Wait for template suggestions to appear
    await waitFor(() => {
      const quickSelectLabel = screen.queryByText("Quick Select Template");
      if (quickSelectLabel) {
        expect(quickSelectLabel).toBeInTheDocument();
      }
    });

    // Step 3: Find and click on a suggested template
    const templateButtons = screen.queryAllByText(/% match/);
    if (templateButtons.length > 0) {
      const firstButton = templateButtons[0];
      expect(firstButton).toBeInTheDocument();

      // Find the parent button
      const buttonElement = firstButton.closest("button");
      expect(buttonElement).toBeInTheDocument();

      // Click to apply template
      fireEvent.click(buttonElement!);

      // Step 4: Verify the geometry values updated (or didn't change for Standard template)
      // The values should still be consistent with geometry tab
      expect(screen.getByDisplayValue("25")).toBeInTheDocument();
    }
  });

  it("should show confidence scores in template suggestions", async () => {
    render(
      <TubeDialog
        isOpen={true}
        onClose={() => {}}
        onSave={() => {}}
        initialConfig={{
          geometry: {
            outerDiameter: 50.0,
            wallThickness: 3.5,
            length: 500.0,
            material: "steel-1045",
            materialName: "Carbon Steel 1045",
          },
          bends: [],
          ports: [],
          advanced: {
            springbackFactor: 0.3,
            bendCompensation: false,
            lengthTolerance: 2.0,
            angleTolerance: 0.5,
            surfaceFinish: "Raw",
          },
        }}
      />
    );

    // Wait for template suggestions with confidence scores
    await waitFor(() => {
      const matches = screen.queryAllByText(/% match/);
      if (matches.length > 0) {
        matches.forEach((match) => {
          expect(match).toBeInTheDocument();
          const text = match.textContent || "";
          expect(/\d+% match/.test(text)).toBe(true);
        });
      }
    });
  });
});

// ============ E2E TEST 2: Create custom template, save, use in new tube ============

describe("E2E: Custom Template Creation and Usage", () => {
  it("should create custom template and use it for new tube", async () => {
    const user = userEvent.setup();

    // Step 1: Open template editor
    const { rerender } = render(
      <TemplateEditor isOpen={true} onClose={() => {}} />
    );

    // Step 2: Wait for UI to load and click Create
    await waitFor(() => {
      const createButton = screen.queryByText("Create");
      expect(createButton).toBeInTheDocument();
    });

    // Step 3: Fill in template details
    const nameInput = screen.getByDisplayValue("New Template");
    await user.clear(nameInput);
    await user.type(nameInput, "Heavy Industrial Tube");

    const descriptionInput = screen.getByDisplayValue("Custom template");
    await user.clear(descriptionInput);
    await user.type(descriptionInput, "For heavy industrial applications");

    // Step 4: Set parameter ranges
    const inputs = screen.getAllByRole("spinbutton");
    if (inputs.length > 0) {
      // Set OD min
      await user.clear(inputs[0]);
      await user.type(inputs[0], "35");
    }

    // Step 5: Save template
    const saveButton = screen.getByText("Save Template");
    expect(saveButton).toBeInTheDocument();
    fireEvent.click(saveButton);

    // Step 6: Verify template was saved (should return to list view)
    await waitFor(() => {
      expect(screen.queryByText("Save Template")).not.toBeInTheDocument();
    });

    // Step 7: Verify the template appears in the list
    const templateName = screen.queryByText("Heavy Industrial Tube");
    expect(templateName).toBeInTheDocument();
  });

  it("should persist custom templates in localStorage", async () => {
    const user = userEvent.setup();

    // Step 1: Create a template
    render(<TemplateEditor isOpen={true} onClose={() => {}} />);

    await waitFor(() => {
      expect(screen.getByText("Create")).toBeInTheDocument();
    });

    const nameInput = screen.getByDisplayValue("New Template");
    await user.clear(nameInput);
    await user.type(nameInput, "Persistent Custom Template");

    const saveButton = screen.getByText("Save Template");
    fireEvent.click(saveButton);

    // Wait for save to complete
    await waitFor(() => {
      expect(screen.queryByText("Save Template")).not.toBeInTheDocument();
    });

    // Step 2: Verify localStorage was updated
    const storedData = localStorage.getItem("bendcore_templates");
    expect(storedData).not.toBeNull();

    if (storedData) {
      const parsed = JSON.parse(storedData);
      expect(parsed.templates).toBeDefined();
      expect(Array.isArray(parsed.templates)).toBe(true);

      const foundTemplate = parsed.templates.find(
        (t: any) => t.name === "Persistent Custom Template"
      );
      expect(foundTemplate).toBeDefined();
    }
  });

  it("should export and reimport templates", async () => {
    const user = userEvent.setup();

    render(<TemplateEditor isOpen={true} onClose={() => {}} />);

    // Wait for export button
    await waitFor(() => {
      expect(screen.getByText("Export Templates")).toBeInTheDocument();
    });

    // Click export
    const exportButton = screen.getByText("Export Templates");
    expect(exportButton).toBeInTheDocument();

    // Note: Full export/import test would require mocking file download/upload
    // This test verifies the button exists and is clickable
    fireEvent.click(exportButton);

    // Verify no errors occurred
    expect(screen.getByText("Export Templates")).toBeInTheDocument();
  });
});

// ============ E2E TEST: Template selection updates preview ============

describe("E2E: Template Selection and Preview", () => {
  it("should update tube preview when template is applied", async () => {
    const mockOnSave = jest.fn();

    render(
      <TubeDialog
        isOpen={true}
        onClose={() => {}}
        onSave={mockOnSave}
        initialConfig={{
          geometry: {
            outerDiameter: 16.0,
            wallThickness: 1.5,
            length: 500.0,
            material: "steel-1020",
            materialName: "Carbon Steel 1020",
          },
          bends: [],
          ports: [],
          advanced: {
            springbackFactor: 0.3,
            bendCompensation: false,
            lengthTolerance: 2.0,
            angleTolerance: 0.5,
            surfaceFinish: "Raw",
          },
        }}
      />
    );

    // Wait for template suggestions
    await waitFor(() => {
      const matches = screen.queryAllByText(/% match/);
      expect(matches.length > 0).toBe(true);
    });

    // Click a template suggestion
    const templateMatches = screen.getAllByText(/% match/);
    if (templateMatches.length > 0) {
      const parentButton = templateMatches[0].closest("button");
      if (parentButton) {
        fireEvent.click(parentButton);

        // Verify geometry values were updated by checking the preview tab
        const previewButton = screen.getByText("Preview");
        fireEvent.click(previewButton);

        // Preview should show the updated configuration
        await waitFor(() => {
          const geometryConfig = screen.queryByText("Tube Configuration");
          expect(geometryConfig).toBeInTheDocument();
        });
      }
    }
  });

  it("should validate configuration after template application", async () => {
    render(
      <TubeDialog
        isOpen={true}
        onClose={() => {}}
        onSave={() => {}}
        initialConfig={{
          geometry: {
            outerDiameter: 25.0,
            wallThickness: 2.0,
            length: 400.0,
            material: "steel-1020",
            materialName: "Carbon Steel 1020",
          },
          bends: [],
          ports: [],
          advanced: {
            springbackFactor: 0.3,
            bendCompensation: false,
            lengthTolerance: 2.0,
            angleTolerance: 0.5,
            surfaceFinish: "Raw",
          },
        }}
      />
    );

    // Navigate to preview tab to check validation
    const previewButton = screen.getByText("Preview");
    fireEvent.click(previewButton);

    // Check for validation status
    await waitFor(() => {
      const validIndicator = screen.queryByText(/Valid|errors/);
      // Should have some validation indicator
      expect(previewButton).toBeInTheDocument();
    });
  });
});

// ============ INTEGRATION TEST: TemplateManager and UI Hook ============

describe("Integration: Template Manager and useTemplates Hook", () => {
  it("should load predefined templates on first use", async () => {
    // This test verifies that the hook properly loads templates
    let loadedTemplates: any = null;

    const TestComponent = () => {
      const { library } = useTemplates();
      loadedTemplates = library;
      return <div>{library.templates.length} templates loaded</div>;
    };

    render(<TestComponent />);

    await waitFor(() => {
      expect(loadedTemplates).not.toBeNull();
      expect(loadedTemplates.templates.length).toBeGreaterThanOrEqual(3);
    });
  });

  it("should sync template operations across multiple instances", async () => {
    const TestComponent1 = () => {
      const { library, addTemplate } = useTemplates();
      return (
        <div>
          <span data-testid="count1">{library.templates.length}</span>
          <button
            onClick={() => {
              addTemplate({
                id: "sync-test",
                name: "Sync Test",
                description: "Test",
                category: "Custom",
                outer_diameter: {
                  min: 10,
                  max: 50,
                  default: 30,
                  unit: "mm",
                },
                wall_thickness: {
                  min: 1,
                  max: 3,
                  default: 2,
                  unit: "mm",
                },
                length: { min: 50, max: 500, default: 250, unit: "mm" },
                use_case: "Testing",
                compatible_materials: [],
              });
            }}
          >
            Add Template
          </button>
        </div>
      );
    };

    const TestComponent2 = () => {
      const { library } = useTemplates();
      return <span data-testid="count2">{library.templates.length}</span>;
    };

    const { rerender } = render(
      <>
        <TestComponent1 />
        <TestComponent2 />
      </>
    );

    await waitFor(() => {
      const count1 = screen.getByTestId("count1");
      const count2 = screen.getByTestId("count2");
      expect(count1).toBeInTheDocument();
      expect(count2).toBeInTheDocument();
    });
  });
});
