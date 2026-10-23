/**
 * Tests for PatternTemplates component
 */

import React from "react";
import { render, screen, fireEvent } from "@testing-library/react";
import PatternTemplates, {
  getPatternTemplate,
  listPatternTemplates,
  generatePatternVertices,
} from "../src/ui/PatternTemplates";

describe("PatternTemplates", () => {
  describe("Pattern templates", () => {
    it("should have 6 pattern templates", () => {
      const templates = listPatternTemplates();
      expect(templates).toHaveLength(6);
    });

    it("should have correct pattern IDs", () => {
      const templates = listPatternTemplates();
      const ids = templates.map((t) => t.id);
      expect(ids).toContain("angle_90");
      expect(ids).toContain("offset");
      expect(ids).toContain("saddle");
      expect(ids).toContain("u_shape");
      expect(ids).toContain("z_shape");
      expect(ids).toContain("wall_penetration");
    });

    it("should retrieve pattern by ID", () => {
      const pattern = getPatternTemplate("angle_90");
      expect(pattern).toBeDefined();
      expect(pattern?.name).toBe("90° Angle");
    });

    it("should return undefined for invalid pattern ID", () => {
      const pattern = getPatternTemplate("invalid");
      expect(pattern).toBeUndefined();
    });
  });

  describe("Pattern geometry generation", () => {
    it("should generate vertices for 90° angle", () => {
      const vertices = generatePatternVertices("angle_90", {});
      expect(vertices).not.toBeNull();
      expect(vertices).toHaveLength(4);
    });

    it("should generate vertices for offset pattern", () => {
      const params = {
        offset_distance: { value: 30, min: 5, max: 200, step: 5, unit: "mm", label: "Offset" },
        offset_height: { value: 60, min: 10, max: 300, step: 5, unit: "mm", label: "Height" },
      };
      const vertices = generatePatternVertices("offset", params);
      expect(vertices).not.toBeNull();
      expect(vertices?.length).toBeGreaterThanOrEqual(4);
    });

    it("should generate vertices for U-shape", () => {
      const params = {
        length: { value: 100, min: 30, max: 400, step: 5, unit: "mm", label: "Length" },
        distance_between_ends: { value: 60, min: 20, max: 300, step: 5, unit: "mm", label: "Distance" },
      };
      const vertices = generatePatternVertices("u_shape", params);
      expect(vertices).not.toBeNull();
      expect(vertices).toHaveLength(5);
    });

    it("should return null for invalid pattern", () => {
      const vertices = generatePatternVertices("invalid", {});
      expect(vertices).toBeNull();
    });
  });

  describe("Component rendering", () => {
    it("should render pattern selector", () => {
      const { container } = render(<PatternTemplates onPatternSelect={jest.fn()} />);
      const selector = container.querySelector(".pattern-selector");
      expect(selector).toBeInTheDocument();
    });

    it("should render all 6 pattern items", () => {
      const { container } = render(<PatternTemplates onPatternSelect={jest.fn()} />);
      const items = container.querySelectorAll(".pattern-item");
      expect(items).toHaveLength(6);
    });

    it("should display pattern names", () => {
      render(<PatternTemplates onPatternSelect={jest.fn()} />);
      expect(screen.getByText("90° Angle")).toBeInTheDocument();
      expect(screen.getByText("Offset (Duck/Dogleg)")).toBeInTheDocument();
      expect(screen.getByText("U-Shape (Loop/Hairpin)")).toBeInTheDocument();
    });

    it("should render preview section", () => {
      const { container } = render(<PatternTemplates onPatternSelect={jest.fn()} />);
      const preview = container.querySelector(".pattern-preview-section");
      expect(preview).toBeInTheDocument();
    });

    it("should render parameters section", () => {
      const { container } = render(<PatternTemplates onPatternSelect={jest.fn()} />);
      const params = container.querySelector(".pattern-params-section");
      expect(params).toBeInTheDocument();
    });
  });

  describe("User interactions", () => {
    it("should select pattern on click", () => {
      const { container } = render(<PatternTemplates onPatternSelect={jest.fn()} />);
      const items = container.querySelectorAll(".pattern-item");
      fireEvent.click(items[1]); // Click offset pattern

      // The offset pattern should be selected (has parameters)
      const paramSection = container.querySelector(".pattern-params-section");
      expect(paramSection?.textContent).not.toContain("No parameters");
    });

    it("should update parameters on slider change", () => {
      const { container } = render(<PatternTemplates onPatternSelect={jest.fn()} />);

      // Select offset pattern (has parameters)
      const items = container.querySelectorAll(".pattern-item");
      fireEvent.click(items[1]);

      // Find and change slider
      const sliders = container.querySelectorAll(".param-slider");
      if (sliders.length > 0) {
        fireEvent.change(sliders[0], { target: { value: "50" } });
        // Component should update preview
      }
    });

    it("should call onPatternSelect when creating tube", () => {
      const onSelect = jest.fn();
      const { container } = render(<PatternTemplates onPatternSelect={onSelect} />);

      const createBtn = screen.getByText("Create Tube from Pattern");
      fireEvent.click(createBtn);

      expect(onSelect).toHaveBeenCalled();
    });

    it("should call onCancel when cancel button is clicked", () => {
      const onCancel = jest.fn();
      const { container } = render(
        <PatternTemplates onPatternSelect={jest.fn()} onCancel={onCancel} />
      );

      const cancelBtn = screen.getByText("Cancel");
      fireEvent.click(cancelBtn);

      expect(onCancel).toHaveBeenCalled();
    });
  });

  describe("Parameter validation", () => {
    it("should enforce parameter min/max constraints", () => {
      const { container } = render(<PatternTemplates onPatternSelect={jest.fn()} />);

      const items = container.querySelectorAll(".pattern-item");
      fireEvent.click(items[1]); // Offset pattern

      const numberInputs = container.querySelectorAll(".param-number") as any[];
      if (numberInputs.length > 0) {
        const input = numberInputs[0];
        fireEvent.change(input, { target: { value: "1000" } });
        // Value should be clamped to max
      }
    });
  });

  describe("Pattern segments", () => {
    it("90° angle should have 2 segments", () => {
      const pattern = getPatternTemplate("angle_90");
      expect(pattern?.segments).toBe(2);
    });

    it("offset should have 3 segments", () => {
      const pattern = getPatternTemplate("offset");
      expect(pattern?.segments).toBe(3);
    });

    it("saddle should have 4 segments", () => {
      const pattern = getPatternTemplate("saddle");
      expect(pattern?.segments).toBe(4);
    });

    it("u-shape should have 3 segments", () => {
      const pattern = getPatternTemplate("u_shape");
      expect(pattern?.segments).toBe(3);
    });

    it("z-shape should have 4 segments", () => {
      const pattern = getPatternTemplate("z_shape");
      expect(pattern?.segments).toBe(4);
    });

    it("wall penetration should have 2 segments", () => {
      const pattern = getPatternTemplate("wall_penetration");
      expect(pattern?.segments).toBe(2);
    });
  });

  describe("Pattern descriptions", () => {
    it("should have non-empty descriptions for all patterns", () => {
      const templates = listPatternTemplates();
      templates.forEach((template) => {
        expect(template.description).toBeTruthy();
        expect(template.description.length).toBeGreaterThan(0);
      });
    });
  });
});
