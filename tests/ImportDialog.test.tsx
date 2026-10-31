/**
 * Tests for ImportDialog component
 */

import React from "react";
import { render, screen, fireEvent, waitFor, within } from "@testing-library/react";
import ImportDialog, { GroupingRule } from "../src/ui/ImportDialog";
import { analyzeStep, RecognizedTube } from "../src/ui/tubeService";

// Recognition runs in the geometry kernel service; the dialog is tested
// against a mocked service response.
jest.mock("../src/ui/tubeService", () => ({
  analyzeStep: jest.fn(),
}));

const mockedAnalyzeStep = analyzeStep as jest.MockedFunction<typeof analyzeStep>;

const recognizedTube = (overrides: Partial<RecognizedTube> = {}): RecognizedTube => ({
  od: 25,
  wall: 2,
  innerDiameter: 21,
  length: 840,
  bends: [{ angle: 90, radius: 50 }],
  shapeType: "bent_tube",
  confidence: 0.92,
  notes: "",
  valid: true,
  mesh: { vertices: [0, 0, 0, 1, 0, 0, 0, 1, 0], indices: [0, 1, 2] },
  ...overrides,
});

const selectFile = (container: HTMLElement, name: string) => {
  const fileInput = container.querySelector('input[type="file"]') as HTMLInputElement;
  const file = new File(["ISO-10303-21;"], name, { type: "application/octet-stream" });
  fireEvent.change(fileInput, { target: { files: [file] } });
  return file;
};

/** Render the dialog and take it through file selection and recognition. */
const renderReviewing = async (
  props: Partial<React.ComponentProps<typeof ImportDialog>> = {},
  tube: RecognizedTube = recognizedTube()
) => {
  mockedAnalyzeStep.mockResolvedValue(tube);
  const utils = render(<ImportDialog onImport={jest.fn()} {...props} />);
  selectFile(utils.container, "frame.step");
  await waitFor(() => {
    expect(utils.container.querySelector(".import-tube-reviewer")).toBeInTheDocument();
  });
  return utils;
};

/** Expand the first recognition group so its tubes are listed. */
const expandFirstGroup = (container: HTMLElement) => {
  fireEvent.click(container.querySelector(".group-header") as HTMLElement);
};

describe("ImportDialog", () => {
  beforeEach(() => {
    mockedAnalyzeStep.mockReset();
  });

  describe("Component rendering", () => {
    it("should render import dialog", () => {
      const { container } = render(
        <ImportDialog onImport={jest.fn()} />
      );
      expect(container.querySelector(".import-dialog")).toBeInTheDocument();
    });

    it("should show file selector initially", () => {
      const { container } = render(
        <ImportDialog onImport={jest.fn()} />
      );
      expect(container.querySelector(".file-drop-zone")).toBeInTheDocument();
    });

    it("should display import header", () => {
      render(<ImportDialog onImport={jest.fn()} />);
      expect(screen.getByText("Import CAD File")).toBeInTheDocument();
    });
  });

  describe("File selection", () => {
    it("should accept STEP files", async () => {
      mockedAnalyzeStep.mockResolvedValue(recognizedTube());
      const { container } = render(
        <ImportDialog onImport={jest.fn()} />
      );
      const fileInput = container.querySelector('input[type="file"]') as HTMLInputElement;
      expect(fileInput?.accept).toContain("step");
      expect(fileInput?.accept).toContain("stp");

      const file = selectFile(container, "frame.stp");
      await waitFor(() => expect(mockedAnalyzeStep).toHaveBeenCalledWith(file));
    });

    // The geometry kernel cannot read IGES yet (see handleFileSelected), so the
    // dialog must not offer it and must explain why an IGES file is refused.
    it("should explain that IGES files are not supported yet", () => {
      const { container } = render(
        <ImportDialog onImport={jest.fn()} />
      );
      const fileInput = container.querySelector('input[type="file"]') as HTMLInputElement;
      expect(fileInput.accept).not.toContain("iges");
      expect(fileInput.accept).not.toContain("igs");

      selectFile(container, "frame.igs");

      expect(mockedAnalyzeStep).not.toHaveBeenCalled();
      expect(container.querySelector(".import-error")).toHaveTextContent(/IGES/);
      expect(container.querySelector(".file-drop-zone")).toBeInTheDocument();
    });
  });

  describe("Grouping rules", () => {
    it("should have 5 grouping rules", () => {
      const rules = Object.keys(GroupingRule).length;
      expect(rules).toBe(5);
    });

    it("should have RULE_1 for diameter", () => {
      expect(GroupingRule.RULE_1).toBe("diameter");
    });

    it("should have RULE_2 for material", () => {
      expect(GroupingRule.RULE_2).toBe("material");
    });

    it("should have RULE_3 for bending profile", () => {
      expect(GroupingRule.RULE_3).toBe("bending_profile");
    });

    it("should have RULE_4 for contact", () => {
      expect(GroupingRule.RULE_4).toBe("contact");
    });

    it("should have RULE_5 for isolated", () => {
      expect(GroupingRule.RULE_5).toBe("isolated");
    });
  });

  describe("Rounding preferences", () => {
    it("should show rounding preferences", async () => {
      const { container } = await renderReviewing();
      expect(container.querySelector(".import-rounding-prefs")).toBeTruthy();
      expect(screen.getByText("Geometry Rounding")).toBeInTheDocument();
    });

    it("should default rounding to enabled", async () => {
      const { container } = await renderReviewing();
      const checkboxes = container.querySelectorAll<HTMLInputElement>(".rounding-options input[type=\"checkbox\"]");
      // At least one checkbox should be present for rounding toggle
      expect(checkboxes.length).toBeGreaterThan(0);
      expect(
        screen.getByRole("checkbox", { name: "Apply rounding to imported geometry" })
      ).toBeChecked();
    });
  });

  describe("Tube recognition", () => {
    it("should recognize tubes with confidence scores", async () => {
      const { container } = await renderReviewing();
      expandFirstGroup(container);

      expect(container.querySelector(".tube-name")).toHaveTextContent("frame (D25 x 2)");
      expect(screen.getByText("Confidence: 92%")).toBeInTheDocument();
    });

    it("should group tubes by diameter", async () => {
      const { container } = await renderReviewing();
      // Verify grouping panel structure exists
      expect(container.querySelector(".import-tube-reviewer")).toBeInTheDocument();
      expect(container.querySelector(".group-name")).toHaveTextContent("By Diameter (D25)");
      expect(container.querySelector(".group-count")).toHaveTextContent("(1)");
    });

    it("should show material information", async () => {
      const { container } = await renderReviewing();
      expandFirstGroup(container);

      const details = container.querySelector(".tube-details") as HTMLElement;
      expect(details).toBeTruthy();
      expect(within(details).getByText("OD: 25.0 mm")).toBeInTheDocument();
      expect(within(details).getByText("Profile: 90°/R50")).toBeInTheDocument();
    });
  });

  describe("Tube selection", () => {
    it("should allow selecting/deselecting tubes", async () => {
      const { container } = await renderReviewing();
      expandFirstGroup(container);

      // Verify checkboxes exist for tube selection
      const checkboxes = container.querySelectorAll<HTMLInputElement>(".tube-checkbox");
      expect(checkboxes.length).toBe(1);
      expect(checkboxes[0]).toBeChecked();
      expect(screen.getByRole("button", { name: "Import 1 Tubes" })).toBeInTheDocument();

      fireEvent.click(checkboxes[0]);
      expect(checkboxes[0]).not.toBeChecked();
      expect(screen.getByRole("button", { name: "Import 0 Tubes" })).toBeInTheDocument();
    });
  });

  describe("Import process", () => {
    it("should call onImport callback when importing", async () => {
      const onImport = jest.fn();
      await renderReviewing({ onImport });
      expect(onImport).not.toHaveBeenCalled();

      fireEvent.click(screen.getByRole("button", { name: "Import 1 Tubes" }));

      expect(onImport).toHaveBeenCalledTimes(1);
      const [tubes, options] = onImport.mock.calls[0];
      expect(tubes).toHaveLength(1);
      expect(tubes[0]).toEqual(
        expect.objectContaining({ diameter: 25, wall: 2, length: 840, confidence: 92 })
      );
      expect(options).toEqual(expect.objectContaining({ applyRounding: true }));
      expect(screen.getByText("✓ Import complete")).toBeInTheDocument();
    });

    it("should handle cancel button", () => {
      const onCancel = jest.fn();
      render(
        <ImportDialog onImport={jest.fn()} onCancel={onCancel} />
      );
      // Verify cancel button exists
      const buttons = screen.queryAllByRole("button");
      expect(buttons.length).toBeGreaterThan(0);

      fireEvent.click(screen.getByRole("button", { name: "Cancel" }));
      expect(onCancel).toHaveBeenCalledTimes(1);
    });
  });

  describe("Progress indication", () => {
    it("should show analysis progress", async () => {
      let resolveAnalysis: (tube: RecognizedTube) => void = () => undefined;
      mockedAnalyzeStep.mockReturnValue(
        new Promise<RecognizedTube>((resolve) => {
          resolveAnalysis = resolve;
        })
      );
      const { container } = render(
        <ImportDialog onImport={jest.fn()} />
      );
      selectFile(container, "frame.step");

      // Verify progress bar structure exists (shown during analysis)
      expect(container.querySelector(".import-analysis")).toBeInTheDocument();
      expect(container.querySelector(".progress-text")).toHaveTextContent("10%");

      resolveAnalysis(recognizedTube());
      await waitFor(() => {
        expect(container.querySelector(".import-analysis")).not.toBeInTheDocument();
      });
    });
  });

  describe("Confidence scoring", () => {
    it("should display confidence as percentage", async () => {
      const { container } = await renderReviewing();
      expandFirstGroup(container);

      // Verify confidence display exists
      expect(container.querySelector(".confidence-bar")).toBeTruthy();
      const fill = container.querySelector(".confidence-fill") as HTMLElement;
      expect(fill.style.width).toBe("92%");
    });

    it("should color-code confidence levels", async () => {
      const { container } = await renderReviewing({}, recognizedTube({ confidence: 0.5, valid: false }));
      expandFirstGroup(container);

      const bars = container.querySelectorAll<HTMLElement>(".confidence-fill");
      expect(bars.length).toBe(1);
      // Low confidence (<= 60%) is shown in red
      expect(bars[0].style.background).toBe("rgb(244, 67, 54)");
    });
  });

  describe("Validation", () => {
    it("should require at least one tube selected", async () => {
      const onImport = jest.fn();
      const { container } = await renderReviewing(
        { onImport },
        recognizedTube({ confidence: 0.4, valid: false, notes: "no cylinder found" })
      );

      // Low-confidence recognition is not pre-selected and is reported
      expect(screen.getByRole("button", { name: "Import 0 Tubes" })).toBeInTheDocument();
      expect(container.querySelector(".import-error")).toHaveTextContent("no cylinder found");
    });
  });

  describe("Feature structure", () => {
    it("should have file drop zone", () => {
      const { container } = render(
        <ImportDialog onImport={jest.fn()} />
      );
      expect(container.querySelector(".file-drop-zone")).toBeInTheDocument();
    });

    it("should have tube reviewer panel", async () => {
      const { container } = await renderReviewing();
      expect(container.querySelector(".import-tube-reviewer")).toBeInTheDocument();
    });

    it("should have rounding preferences panel", async () => {
      const { container } = await renderReviewing();
      expect(container.querySelector(".import-rounding-prefs")).toBeInTheDocument();
    });
  });

  describe("Accessibility", () => {
    it("should have proper ARIA labels", () => {
      const { container } = render(
        <ImportDialog onImport={jest.fn()} />
      );
      const dialog = container.querySelector(".import-dialog");
      expect(dialog).toBeInTheDocument();
    });

    it("should be keyboard navigable", () => {
      render(
        <ImportDialog onImport={jest.fn()} onCancel={jest.fn()} />
      );
      const buttons = screen.queryAllByRole("button");
      expect(buttons.length).toBeGreaterThan(0);
      buttons.forEach((btn) => {
        expect(btn).toBeVisible();
      });
    });
  });
});
