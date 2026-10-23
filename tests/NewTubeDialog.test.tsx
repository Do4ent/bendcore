/**
 * NewTubeDialog Component Tests
 * Phase 35A: Profile Database and New Tube Dialog
 */

import React from "react";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import NewTubeDialog from "../src/ui/NewTubeDialog";

describe("NewTubeDialog", () => {
  const mockProfiles = [
    {
      id: "manual-half-inch",
      name: "Manual Hand Bender - ½\"",
      category: "Manual Hand Bender",
      manufacturer: "BendTech Pro",
      type: "manual",
      description: "Portable manual hand bender",
      diameters: [
        {
          id: "0.5",
          OD: 0.5,
          R: 1.0,
          S_min: 0.25,
          N: 2,
          B_min: 10.0,
          B_max: 179.0,
          discrete_type: "step",
        },
      ],
    },
    {
      id: "hydraulic-hb16",
      name: "Hydraulic Bender HB-16",
      category: "Hydraulic Bender",
      manufacturer: "Amob",
      type: "hydraulic",
      description: "Hydraulic tube bender",
      diameters: [
        {
          id: "0.5",
          OD: 0.5,
          R: 1.0,
          S_min: 0.25,
          N: 1,
          B_min: 3.0,
          B_max: 179.0,
          discrete_type: "continuous",
        },
        {
          id: "1.0",
          OD: 1.0,
          R: 2.0,
          S_min: 0.5,
          N: 1,
          B_min: 3.0,
          B_max: 179.0,
          discrete_type: "continuous",
        },
      ],
    },
  ];

  beforeEach(() => {
    global.fetch = jest.fn(() =>
      Promise.resolve({
        json: () => Promise.resolve({ profiles: mockProfiles }),
      })
    ) as jest.Mock;
  });

  afterEach(() => {
    jest.clearAllMocks();
  });

  test("renders dialog when isOpen is true", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(screen.getByText("Create New Tube")).toBeInTheDocument();
    });
  });

  test("does not render dialog when isOpen is false", () => {
    const { container } = render(
      <NewTubeDialog isOpen={false} onClose={jest.fn()} />
    );
    expect(container.firstChild).toBeNull();
  });

  test("displays Tab 1: Specification by default", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(screen.getByText("1. Specification")).toBeInTheDocument();
    });
  });

  test("allows filling Tab 1 fields", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(screen.getByText("Tube Specification")).toBeInTheDocument();
    });

    // Select standard
    const standardSelect = screen.getByDisplayValue("ANSI/ASME");
    await userEvent.selectOptions(standardSelect, "DIN");
    expect(standardSelect).toHaveValue("DIN");

    // Select material
    const materialSelect = screen.getByDisplayValue("Steel 1020");
    await userEvent.selectOptions(materialSelect, "ss-304");
    expect(materialSelect).toHaveValue("ss-304");

    // Select tube type
    const typeSelect = screen.getByDisplayValue("Tube");
    await userEvent.selectOptions(typeSelect, "Pipe");
    expect(typeSelect).toHaveValue("Pipe");

    // Fill description
    const descriptionInput = screen.getByPlaceholderText(
      "Enter tube description..."
    ) as HTMLTextAreaElement;
    await userEvent.type(descriptionInput, "Test tube");
    expect(descriptionInput.value).toBe("Test tube");
  });

  test("navigates to Tab 2 when Next is clicked", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(screen.getByText("Tube Specification")).toBeInTheDocument();
    });

    const nextButton = screen.getByText("Next →");
    fireEvent.click(nextButton);

    await waitFor(() => {
      expect(
        screen.getByText("Bending Tool Profile")
      ).toBeInTheDocument();
    });
  });

  test("Tab 2 displays profile and diameter selection", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      const nextButton = screen.getByText("Next →");
      fireEvent.click(nextButton);
    });

    await waitFor(() => {
      expect(
        screen.getByText("Bending Tool Profile")
      ).toBeInTheDocument();
    });

    // Check for profile dropdown
    const profileSelect = screen.getByDisplayValue(
      "Manual Hand Bender - ½\""
    ) as HTMLSelectElement;
    expect(profileSelect).toBeInTheDocument();

    // Diameter should be populated
    const diameterSelect = screen.getByDisplayValue("0.5") as HTMLSelectElement;
    expect(diameterSelect).toBeInTheDocument();

    // Radius should be read-only
    expect(screen.getByText('1"')).toBeInTheDocument();
  });

  test("updates diameter options when profile changes", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      const nextButton = screen.getByText("Next →");
      fireEvent.click(nextButton);
    });

    await waitFor(() => {
      expect(
        screen.getByText("Bending Tool Profile")
      ).toBeInTheDocument();
    });

    // Change to hydraulic profile with multiple diameters
    const profileSelect = screen.getAllByRole("combobox")[0] as HTMLSelectElement;
    await userEvent.selectOptions(profileSelect, "hydraulic-hb16");

    await waitFor(() => {
      // Should now have 2 diameter options
      const diameterSelect = screen.getByDisplayValue(
        "0.5"
      ) as HTMLSelectElement;
      const options = diameterSelect.querySelectorAll("option");
      expect(options.length).toBeGreaterThan(2); // More than just placeholder + 0.5
    });
  });

  test("navigates to Tab 3 from Tab 2", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      const nextButton = screen.getByText("Next →");
      fireEvent.click(nextButton);
    });

    await waitFor(() => {
      expect(
        screen.getByText("Bending Tool Profile")
      ).toBeInTheDocument();
    });

    const nextButton = screen.getByText("Next →");
    fireEvent.click(nextButton);

    await waitFor(() => {
      expect(
        screen.getByText("Blank Configuration")
      ).toBeInTheDocument();
    });
  });

  test("Tab 3 displays blank configuration fields", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    // Navigate to Tab 3
    await waitFor(() => {
      let nextButton = screen.getByText("Next →");
      fireEvent.click(nextButton);
    });

    await waitFor(() => {
      let nextButton = screen.getByText("Next →");
      fireEvent.click(nextButton);
    });

    await waitFor(() => {
      expect(
        screen.getByText("Blank Configuration")
      ).toBeInTheDocument();
    });

    // Check for all blank configuration fields
    expect(screen.getByText("Trimming:")).toBeInTheDocument();
    expect(
      screen.getByText("First Segment Allowance:")
    ).toBeInTheDocument();
    expect(screen.getByText("Last Segment Allowance:")).toBeInTheDocument();
    expect(
      screen.getByText("Automatically calculate trimming allowance")
    ).toBeInTheDocument();
  });

  test("Back button works on Tab 2", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      const nextButton = screen.getByText("Next →");
      fireEvent.click(nextButton);
    });

    await waitFor(() => {
      expect(
        screen.getByText("Bending Tool Profile")
      ).toBeInTheDocument();
    });

    const backButton = screen.getByText("← Back");
    fireEvent.click(backButton);

    await waitFor(() => {
      expect(screen.getByText("Tube Specification")).toBeInTheDocument();
    });
  });

  test("Tab 1 validation prevents advancing with empty material", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(screen.getByText("Tube Specification")).toBeInTheDocument();
    });

    // Don't fill in required fields, just try to advance
    const nextButton = screen.getByText("Next →");
    fireEvent.click(nextButton);

    // Should still be on Tab 1 (validation failed)
    expect(screen.getByText("Tube Specification")).toBeInTheDocument();
  });

  test("Tab 2 validation prevents advancing with invalid wall thickness", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    // Go to Tab 2
    await waitFor(() => {
      const nextButton = screen.getByText("Next →");
      fireEvent.click(nextButton);
    });

    await waitFor(() => {
      expect(
        screen.getByText("Bending Tool Profile")
      ).toBeInTheDocument();
    });

    // Set wall thickness to 0 (invalid)
    const wallThicknessInputs = screen.getAllByPlaceholderText(
      "Wall thickness (inches)"
    );
    const wallInput = wallThicknessInputs[0] as HTMLInputElement;
    await userEvent.clear(wallInput);
    await userEvent.type(wallInput, "0");

    const nextButton = screen.getByText("Next →");
    fireEvent.click(nextButton);

    // Should still be on Tab 2 (validation failed)
    expect(
      screen.getByText("Bending Tool Profile")
    ).toBeInTheDocument();
  });

  test("calls onCreate when Create button is clicked", async () => {
    const onCreate = jest.fn();
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} onCreate={onCreate} />);

    // Go to Tab 3 (all fields should be filled)
    await waitFor(() => {
      const nextButton = screen.getByText("Next →");
      fireEvent.click(nextButton);
    });

    await waitFor(() => {
      const nextButton = screen.getByText("Next →");
      fireEvent.click(nextButton);
    });

    await waitFor(() => {
      expect(screen.getByText("Create ✓")).toBeInTheDocument();
    });

    const createButton = screen.getByText("Create ✓");
    fireEvent.click(createButton);

    await waitFor(() => {
      expect(onCreate).toHaveBeenCalled();
    });
  });

  test("calls onClose when Cancel is clicked", () => {
    const onClose = jest.fn();
    render(<NewTubeDialog isOpen={true} onClose={onClose} />);

    const cancelButton = screen.getByText("Cancel");
    fireEvent.click(cancelButton);

    expect(onClose).toHaveBeenCalled();
  });

  test("displays error messages for invalid fields", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    // Go to Tab 2
    await waitFor(() => {
      const nextButton = screen.getByText("Next →");
      fireEvent.click(nextButton);
    });

    await waitFor(() => {
      expect(
        screen.getByText("Bending Tool Profile")
      ).toBeInTheDocument();
    });

    // Try to go to Tab 3 with invalid wall thickness
    const wallThicknessInputs = screen.getAllByPlaceholderText(
      "Wall thickness (inches)"
    );
    const wallInput = wallThicknessInputs[0] as HTMLInputElement;
    await userEvent.clear(wallInput);
    await userEvent.type(wallInput, "0");

    const nextButton = screen.getByText("Next →");
    fireEvent.click(nextButton);

    // Should see error message
    await waitFor(() => {
      expect(
        screen.getByText(/Wall thickness must be positive/i)
      ).toBeInTheDocument();
    });
  });

  test("allows entering custom description on Tab 1", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(screen.getByText("Tube Specification")).toBeInTheDocument();
    });

    const descriptionInput = screen.getByPlaceholderText(
      "Enter tube description..."
    ) as HTMLTextAreaElement;
    await userEvent.type(descriptionInput, "Custom tube for testing");

    expect(descriptionInput.value).toBe("Custom tube for testing");
  });

  test("checkbox for auto-calculate allowance works", async () => {
    render(<NewTubeDialog isOpen={true} onClose={jest.fn()} />);

    // Navigate to Tab 3
    await waitFor(() => {
      let nextButton = screen.getByText("Next →");
      fireEvent.click(nextButton);
    });

    await waitFor(() => {
      let nextButton = screen.getByText("Next →");
      fireEvent.click(nextButton);
    });

    await waitFor(() => {
      expect(
        screen.getByText("Blank Configuration")
      ).toBeInTheDocument();
    });

    const checkbox = screen.getByRole("checkbox") as HTMLInputElement;
    expect(checkbox.checked).toBe(true);

    fireEvent.click(checkbox);
    expect(checkbox.checked).toBe(false);
  });
});
