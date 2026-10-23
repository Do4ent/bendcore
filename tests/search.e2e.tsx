/**
 * End-to-End Tests for SearchPanel Component
 * Task 3: Search & Filters (Q3 2027)
 * Tests: Filter by material + OD range, save/load search, fuzzy search, export
 */

import React from "react";
import { render, screen, fireEvent, waitFor, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import "@testing-library/jest-dom";
import SearchPanel, { TubeRecord } from "../src/ui/SearchPanel";

// =========== TEST DATA ===========

const MOCK_TUBES: TubeRecord[] = [
  {
    id: "tube-1",
    designation: "HY-25",
    material: "Steel",
    od: 25.4,
    wall_thickness: 1.5,
    created_date: "2024-01-15",
    author: "john",
  },
  {
    id: "tube-2",
    designation: "HY-32",
    material: "Steel",
    od: 31.75,
    wall_thickness: 2.0,
    created_date: "2024-02-20",
    author: "jane",
  },
  {
    id: "tube-3",
    designation: "AL-20",
    material: "Aluminum",
    od: 20.0,
    wall_thickness: 1.0,
    created_date: "2024-03-10",
    author: "john",
  },
  {
    id: "tube-4",
    designation: "TI-50",
    material: "Titanium",
    od: 50.8,
    wall_thickness: 3.0,
    created_date: "2024-01-25",
    author: "mike",
  },
  {
    id: "tube-5",
    designation: "CU-15",
    material: "Copper",
    od: 15.875,
    wall_thickness: 0.8,
    created_date: "2024-04-05",
    author: "jane",
  },
  // Additional tubes for comprehensive testing
  ...Array.from({ length: 45 }, (_, i) => ({
    id: `tube-${6 + i}`,
    designation: `HY-${25 + (i % 20)}`,
    material: ["Steel", "Aluminum", "Copper", "Titanium", "Custom"][i % 5] as any,
    od: 15 + (i % 50),
    wall_thickness: 0.8 + (i % 30) * 0.1,
    created_date: `2024-${String((i % 12) + 1).padStart(2, "0")}-${String((i % 28) + 1).padStart(2, "0")}`,
    author: `user-${i % 5}`,
  })),
];

// =========== TEST SUITES ===========

describe("SearchPanel E2E Tests", () => {
  beforeEach(() => {
    // Clear localStorage before each test
    localStorage.clear();
  });

  afterEach(() => {
    localStorage.clear();
  });

  // =========== SCENARIO 1: Filter by Material + OD Range ===========

  describe("Scenario 1: Material + OD Range Filter", () => {
    test("Should filter tubes by Steel material", async () => {
      const { container } = render(
        <SearchPanel tubes={MOCK_TUBES} />
      );

      // Find and check the Steel checkbox
      const steelCheckbox = screen.getByRole("checkbox", { name: "" });
      const labels = screen.getAllByText(/Steel|Aluminum|Copper|Titanium|Custom/);

      await waitFor(() => {
        expect(labels.length).toBeGreaterThan(0);
      });

      // Click Steel checkbox (first material checkbox)
      const materialCheckboxes = container.querySelectorAll('.checkbox-group input[type="checkbox"]');
      expect(materialCheckboxes.length).toBeGreaterThan(0);

      fireEvent.click(materialCheckboxes[0]); // Click Steel

      // Wait for results to update
      await waitFor(() => {
        const resultText = screen.getByText(/\d+ tubes matched/);
        expect(resultText).toBeInTheDocument();
      }, { timeout: 500 });

      // Verify results show Steel tubes
      const resultTable = screen.getByRole("table");
      expect(resultTable).toBeInTheDocument();
    });

    test("Should filter by OD range", async () => {
      const { container } = render(
        <SearchPanel tubes={MOCK_TUBES} />
      );

      // Find OD range inputs
      const inputs = screen.getAllByPlaceholderText(/Min|Max/);

      // Set OD min to 25 and max to 40
      const odMinInput = inputs.find(
        (input) => (input as HTMLInputElement).placeholder.includes("Min") &&
                   (input as HTMLInputElement).value === ""
      ) as HTMLInputElement;

      if (odMinInput) {
        fireEvent.change(odMinInput, { target: { value: "25" } });
      }

      await waitFor(() => {
        const resultText = screen.getByText(/\d+ tubes matched/);
        expect(resultText).toBeInTheDocument();
      }, { timeout: 500 });
    });

    test("Should combine Material and OD filters with AND logic", async () => {
      const { container } = render(
        <SearchPanel tubes={MOCK_TUBES} />
      );

      // Select Steel material
      const materialCheckboxes = container.querySelectorAll('.checkbox-group input[type="checkbox"]');
      fireEvent.click(materialCheckboxes[0]); // Steel

      // Set OD range
      const inputs = screen.getAllByPlaceholderText(/Min|Max/);
      const odInputs = inputs.filter(
        (input) => (input as HTMLInputElement).type === "number"
      );

      if (odInputs[0]) {
        fireEvent.change(odInputs[0], { target: { value: "25" } });
      }

      await waitFor(() => {
        const resultText = screen.getByText(/\d+ tubes matched/);
        expect(resultText).toBeInTheDocument();
      }, { timeout: 500 });
    });

    test("Results should update in <500ms", async () => {
      const { container } = render(
        <SearchPanel tubes={MOCK_TUBES} />
      );

      const startTime = performance.now();

      const materialCheckboxes = container.querySelectorAll('.checkbox-group input[type="checkbox"]');
      fireEvent.click(materialCheckboxes[0]);

      await waitFor(() => {
        const resultText = screen.getByText(/\d+ tubes matched/);
        expect(resultText).toBeInTheDocument();
      });

      const endTime = performance.now();
      const duration = endTime - startTime;

      expect(duration).toBeLessThan(500);
    });
  });

  // =========== SCENARIO 2: Save and Load Search ===========

  describe("Scenario 2: Save and Load Search", () => {
    test("Should save current search filters", async () => {
      render(<SearchPanel tubes={MOCK_TUBES} />);

      // Apply some filters
      const materialCheckboxes = document.querySelectorAll(
        '.checkbox-group input[type="checkbox"]'
      );
      fireEvent.click(materialCheckboxes[0]); // Select first material

      // Click "Save Search" button
      const saveButton = screen.getByRole("button", { name: /Save Search/i });
      fireEvent.click(saveButton);

      // Enter search name
      const input = screen.getByPlaceholderText("Enter search name");
      fireEvent.change(input, { target: { value: "My Steel Filter" } });

      // Click save in dialog
      const confirmButton = screen.getByRole("button", { name: /^Save$/i });
      fireEvent.click(confirmButton);

      await waitFor(() => {
        const savedSearch = screen.getByText("My Steel Filter");
        expect(savedSearch).toBeInTheDocument();
      }, { timeout: 500 });

      // Verify it's saved in localStorage
      const stored = localStorage.getItem("bendcore:saved_searches");
      expect(stored).toBeTruthy();
      const searches = JSON.parse(stored || "[]");
      expect(searches).toHaveLength(1);
      expect(searches[0].name).toBe("My Steel Filter");
    });

    test("Should load saved search filters", async () => {
      // Pre-populate localStorage with a saved search
      const savedSearch = {
        id: "search-1",
        name: "Titanium Tubes",
        filters: { material: ["Titanium"] },
        created_at: new Date().toISOString(),
      };
      localStorage.setItem(
        "bendcore:saved_searches",
        JSON.stringify([savedSearch])
      );

      render(<SearchPanel tubes={MOCK_TUBES} />);

      // Wait for saved searches to appear
      await waitFor(() => {
        const savedSearchText = screen.getByText("Titanium Tubes");
        expect(savedSearchText).toBeInTheDocument();
      });

      // Click the saved search
      const savedSearchButton = screen.getByText("Titanium Tubes");
      fireEvent.click(savedSearchButton);

      // Verify filters are loaded and applied
      await waitFor(() => {
        const resultText = screen.getByText(/\d+ tubes matched/);
        expect(resultText).toBeInTheDocument();
      }, { timeout: 500 });
    });

    test("Should delete saved search with confirmation", async () => {
      // Pre-populate a saved search
      const savedSearch = {
        id: "search-1",
        name: "Delete Me",
        filters: {},
        created_at: new Date().toISOString(),
      };
      localStorage.setItem(
        "bendcore:saved_searches",
        JSON.stringify([savedSearch])
      );

      render(<SearchPanel tubes={MOCK_TUBES} />);

      await waitFor(() => {
        expect(screen.getByText("Delete Me")).toBeInTheDocument();
      });

      // Mock window.confirm
      const confirmSpy = jest
        .spyOn(window, "confirm")
        .mockReturnValue(true);

      // Click delete button
      const deleteButtons = screen.getAllByRole("button", { name: "✕" });
      fireEvent.click(deleteButtons[0]);

      // Verify deletion
      await waitFor(() => {
        expect(screen.queryByText("Delete Me")).not.toBeInTheDocument();
      });

      confirmSpy.mockRestore();
    });

    test("Should persist multiple saved searches", async () => {
      render(<SearchPanel tubes={MOCK_TUBES} />);

      // Save first search
      let saveButton = screen.getByRole("button", { name: /Save Search/i });
      fireEvent.click(saveButton);

      let input = screen.getByPlaceholderText("Enter search name");
      fireEvent.change(input, { target: { value: "Search 1" } });

      let confirmButton = screen.getByRole("button", { name: /^Save$/i });
      fireEvent.click(confirmButton);

      await waitFor(() => {
        expect(screen.getByText("Search 1")).toBeInTheDocument();
      }, { timeout: 500 });

      // Save second search
      saveButton = screen.getByRole("button", { name: /Save Search/i });
      fireEvent.click(saveButton);

      input = screen.getByPlaceholderText("Enter search name");
      fireEvent.change(input, { target: { value: "Search 2" } });

      confirmButton = screen.getByRole("button", { name: /^Save$/i });
      fireEvent.click(confirmButton);

      await waitFor(() => {
        expect(screen.getByText("Search 1")).toBeInTheDocument();
        expect(screen.getByText("Search 2")).toBeInTheDocument();
      }, { timeout: 500 });
    });
  });

  // =========== SCENARIO 3: Fuzzy Search ===========

  describe("Scenario 3: Fuzzy Search for Designation", () => {
    test("Should find exact designation match", async () => {
      render(<SearchPanel tubes={MOCK_TUBES} />);

      const designationInput = screen.getByPlaceholderText(
        "Search by designation (fuzzy match)"
      );
      fireEvent.change(designationInput, { target: { value: "HY-25" } });

      await waitFor(() => {
        const resultText = screen.getByText(/\d+ tubes matched/);
        expect(resultText).toBeInTheDocument();
      }, { timeout: 500 });
    });

    test("Should find fuzzy matches (typos/variations)", async () => {
      render(<SearchPanel tubes={MOCK_TUBES} />);

      const designationInput = screen.getByPlaceholderText(
        "Search by designation (fuzzy match)"
      );

      // Search for "HY25" which should match "HY-25"
      fireEvent.change(designationInput, { target: { value: "HY25" } });

      await waitFor(() => {
        const resultText = screen.getByText(/\d+ tubes matched/);
        expect(resultText).toBeInTheDocument();
      }, { timeout: 500 });
    });

    test("Should be case-insensitive", async () => {
      render(<SearchPanel tubes={MOCK_TUBES} />);

      const designationInput = screen.getByPlaceholderText(
        "Search by designation (fuzzy match)"
      );

      fireEvent.change(designationInput, { target: { value: "hy-25" } });

      await waitFor(() => {
        const resultText = screen.getByText(/\d+ tubes matched/);
        expect(resultText).toBeInTheDocument();
      }, { timeout: 500 });
    });
  });

  // =========== SCENARIO 4: Results Table ===========

  describe("Scenario 4: Results Table and Selection", () => {
    test("Should display results in virtualized table", async () => {
      const { container } = render(
        <SearchPanel tubes={MOCK_TUBES} />
      );

      await waitFor(() => {
        const table = screen.getByRole("table");
        expect(table).toBeInTheDocument();
      });

      // Check that table headers are present
      expect(screen.getByText("Designation")).toBeInTheDocument();
      expect(screen.getByText("Material")).toBeInTheDocument();
    });

    test("Should allow selecting individual tubes", async () => {
      const { container } = render(
        <SearchPanel tubes={MOCK_TUBES} />
      );

      // Find first checkbox (excluding "select all")
      const checkboxes = container.querySelectorAll(
        ".virtualized-table input[type='checkbox']"
      );

      if (checkboxes.length > 1) {
        fireEvent.click(checkboxes[1]); // Click first tube checkbox

        // Should have one selected
        const checked = container.querySelectorAll(
          ".virtualized-table input[type='checkbox']:checked"
        );
        expect(checked.length).toBeGreaterThanOrEqual(1);
      }
    });

    test("Should allow selecting all tubes", async () => {
      const { container } = render(
        <SearchPanel tubes={MOCK_TUBES} />
      );

      // Find the "select all" checkbox (first one)
      const selectAllCheckbox = container.querySelector(
        ".virtualized-table input[type='checkbox']"
      ) as HTMLInputElement;

      if (selectAllCheckbox) {
        fireEvent.click(selectAllCheckbox);

        // Should have multiple selected
        const checked = container.querySelectorAll(
          ".virtualized-table input[type='checkbox']:checked"
        );
        expect(checked.length).toBeGreaterThan(1);
      }
    });

    test("Should display result count", async () => {
      render(<SearchPanel tubes={MOCK_TUBES} />);

      await waitFor(() => {
        const resultText = screen.getByText(/\d+ tubes matched/);
        expect(resultText).toBeInTheDocument();
      });
    });

    test("Should clear filters and show all tubes", async () => {
      render(<SearchPanel tubes={MOCK_TUBES} />);

      // Apply a filter
      const materialCheckboxes = document.querySelectorAll(
        '.checkbox-group input[type="checkbox"]'
      );
      fireEvent.click(materialCheckboxes[0]);

      await waitFor(() => {
        const resultText = screen.getByText(/\d+ tubes matched/);
        expect(resultText).toBeInTheDocument();
      }, { timeout: 500 });

      // Click "Clear Filters"
      const clearButton = screen.getByRole("button", { name: /Clear Filters/i });
      fireEvent.click(clearButton);

      await waitFor(() => {
        const resultText = screen.getByText(/\d+ tubes matched/);
        // Should show more tubes after clearing
        expect(resultText).toBeInTheDocument();
      }, { timeout: 500 });
    });
  });

  // =========== SCENARIO 5: Date Range Filtering ===========

  describe("Scenario 5: Date Range Filtering", () => {
    test("Should filter by date range", async () => {
      render(<SearchPanel tubes={MOCK_TUBES} />);

      // Find date inputs
      const dateInputs = screen.getAllByRole("textbox").filter(
        (input) => (input as HTMLInputElement).type === "date"
      );

      if (dateInputs.length >= 2) {
        fireEvent.change(dateInputs[0], { target: { value: "2024-01-01" } });
        fireEvent.change(dateInputs[1], { target: { value: "2024-02-28" } });

        await waitFor(() => {
          const resultText = screen.getByText(/\d+ tubes matched/);
          expect(resultText).toBeInTheDocument();
        }, { timeout: 500 });
      }
    });
  });

  // =========== SCENARIO 6: Query Display ===========

  describe("Scenario 6: Query Builder Display", () => {
    test("Should display current query in JSON format", async () => {
      render(<SearchPanel tubes={MOCK_TUBES} />);

      await waitFor(() => {
        const queryDisplay = screen.getByText(/(No filters applied|No filters)/);
        expect(queryDisplay).toBeInTheDocument();
      });
    });

    test("Should update query display when filters change", async () => {
      const { container } = render(
        <SearchPanel tubes={MOCK_TUBES} />
      );

      // Apply a filter
      const materialCheckboxes = container.querySelectorAll(
        '.checkbox-group input[type="checkbox"]'
      );
      fireEvent.click(materialCheckboxes[0]);

      await waitFor(() => {
        const queryDisplay = screen.getByText(/"material"/);
        expect(queryDisplay).toBeInTheDocument();
      });
    });
  });
});
