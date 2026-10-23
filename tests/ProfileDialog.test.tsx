/**
 * ProfileDialog Component Tests
 * Phase 35A: Profile Database and New Tube Dialog
 */

import React from "react";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import ProfileDialog from "../src/ui/ProfileDialog";

describe("ProfileDialog", () => {
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
          discrete_values: [10, 20, 30, 45, 60, 90, 120, 150, 179],
        },
      ],
    },
    {
      id: "hydraulic-hb16",
      name: "Hydraulic Bender HB-16",
      category: "Hydraulic Bender",
      manufacturer: "Amob",
      type: "hydraulic",
      description: "Hydraulic tube bender for ½ to 1 inch tubing",
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
    render(<ProfileDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(screen.getByText("Profile Manager")).toBeInTheDocument();
    });
  });

  test("does not render dialog when isOpen is false", () => {
    const { container } = render(<ProfileDialog isOpen={false} onClose={jest.fn()} />);
    expect(container.firstChild).toBeNull();
  });

  test("loads and displays profiles", async () => {
    render(<ProfileDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(
        screen.getByText("Manual Hand Bender - ½\"")
      ).toBeInTheDocument();
      expect(
        screen.getByText("Hydraulic Bender HB-16")
      ).toBeInTheDocument();
    });
  });

  test("displays selected profile details", async () => {
    render(<ProfileDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(
        screen.getByText("Manual Hand Bender - ½\"")
      ).toBeInTheDocument();
    });

    const profileName = screen.getByText("Manual Hand Bender - ½\"");
    fireEvent.click(profileName);

    await waitFor(() => {
      expect(screen.getByText("Manual Hand Bender - ½\"")).toBeInTheDocument();
      expect(screen.getByText("BendTech Pro")).toBeInTheDocument();
    });
  });

  test("filters profiles by search term", async () => {
    render(<ProfileDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(
        screen.getByText("Manual Hand Bender - ½\"")
      ).toBeInTheDocument();
    });

    const searchInput = screen.getByPlaceholderText("Search profiles...");
    await userEvent.type(searchInput, "Hydraulic");

    await waitFor(() => {
      expect(
        screen.getByText("Hydraulic Bender HB-16")
      ).toBeInTheDocument();
      expect(
        screen.queryByText("Manual Hand Bender - ½\"")
      ).not.toBeInTheDocument();
    });
  });

  test("enters edit mode when Edit button is clicked", async () => {
    render(<ProfileDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(screen.getByText("Manual Hand Bender - ½\"")).toBeInTheDocument();
    });

    const editButton = screen.getByText("Edit");
    fireEvent.click(editButton);

    await waitFor(() => {
      expect(screen.getByText("Edit Profile")).toBeInTheDocument();
    });
  });

  test("allows editing profile name", async () => {
    render(<ProfileDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(screen.getByText("Manual Hand Bender - ½\"")).toBeInTheDocument();
    });

    const editButton = screen.getByText("Edit");
    fireEvent.click(editButton);

    await waitFor(() => {
      const nameInputs = screen.getAllByDisplayValue(
        "Manual Hand Bender - ½\""
      );
      expect(nameInputs.length).toBeGreaterThan(0);
    });

    const nameInput = screen.getByDisplayValue(
      "Manual Hand Bender - ½\""
    ) as HTMLInputElement;
    await userEvent.clear(nameInput);
    await userEvent.type(nameInput, "Updated Name");

    expect(nameInput.value).toBe("Updated Name");
  });

  test("validates diameter constraints in edit mode", async () => {
    render(<ProfileDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(screen.getByText("Manual Hand Bender - ½\"")).toBeInTheDocument();
    });

    const editButton = screen.getByText("Edit");
    fireEvent.click(editButton);

    await waitFor(() => {
      expect(screen.getByText("Edit Profile")).toBeInTheDocument();
    });

    // Try to set B_max to less than B_min (invalid)
    const bMaxInputs = screen.getAllByDisplayValue("179");
    const bMaxInput = bMaxInputs[0] as HTMLInputElement;
    await userEvent.clear(bMaxInput);
    await userEvent.type(bMaxInput, "5");

    // Should trigger validation error
    await waitFor(() => {
      expect(
        screen.queryByText(/B_max.*error/i) ||
        bMaxInput.style.borderColor.includes("e74c3c")
      ).toBeTruthy();
    });
  });

  test("calls onClose when Close button is clicked", () => {
    const onClose = jest.fn();
    render(<ProfileDialog isOpen={true} onClose={onClose} />);

    const closeButton = screen.getByText("Close");
    fireEvent.click(closeButton);

    expect(onClose).toHaveBeenCalled();
  });

  test("calls onSave with profile data when Save is clicked in edit mode", async () => {
    const onSave = jest.fn();
    render(<ProfileDialog isOpen={true} onClose={jest.fn()} onSave={onSave} />);

    await waitFor(() => {
      expect(screen.getByText("Manual Hand Bender - ½\"")).toBeInTheDocument();
    });

    const editButton = screen.getByText("Edit");
    fireEvent.click(editButton);

    await waitFor(() => {
      expect(screen.getByText("Edit Profile")).toBeInTheDocument();
    });

    const saveButton = screen.getByText("Save");
    fireEvent.click(saveButton);

    await waitFor(() => {
      expect(onSave).toHaveBeenCalled();
    });
  });

  test("displays diameter table with correct data", async () => {
    render(<ProfileDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(screen.getByText("Manual Hand Bender - ½\"")).toBeInTheDocument();
    });

    // Select profile to view
    const profileName = screen.getByText("Manual Hand Bender - ½\"");
    fireEvent.click(profileName);

    await waitFor(() => {
      expect(screen.getByText("0.5")).toBeInTheDocument(); // OD value
    });
  });

  test("handles cancel in edit mode", async () => {
    render(<ProfileDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(screen.getByText("Manual Hand Bender - ½\"")).toBeInTheDocument();
    });

    const editButton = screen.getByText("Edit");
    fireEvent.click(editButton);

    await waitFor(() => {
      expect(screen.getByText("Edit Profile")).toBeInTheDocument();
    });

    const cancelButton = screen.getByText("Cancel");
    fireEvent.click(cancelButton);

    await waitFor(() => {
      expect(screen.getByText("Profile Manager")).toBeInTheDocument();
    });
  });

  test("displays profile metadata correctly", async () => {
    render(<ProfileDialog isOpen={true} onClose={jest.fn()} />);

    await waitFor(() => {
      expect(
        screen.getByText("Manual Hand Bender - ½\"")
      ).toBeInTheDocument();
    });

    const profileName = screen.getByText("Manual Hand Bender - ½\"");
    fireEvent.click(profileName);

    await waitFor(() => {
      expect(screen.getByText("Manual Hand Bender")).toBeInTheDocument(); // category
      expect(screen.getByText("BendTech Pro")).toBeInTheDocument(); // manufacturer
    });
  });
});
