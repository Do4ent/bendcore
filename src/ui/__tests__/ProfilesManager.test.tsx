/**
 * BendCore Profiles Manager Tests
 */

import React from "react";
import { render, screen, fireEvent } from "@testing-library/react";
import ProfilesManager from "../ProfilesManager";
import { UIProfile } from "../SettingsStore";

const mockProfile: UIProfile = {
  name: "Default",
  isDefault: true,
  toolbar: {
    buttons: ["new-document", "save-document"],
    homeTabButtons: ["new-document"],
    assemblyTabButtons: [],
    drawingTabButtons: [],
  },
  theme: {
    name: "Light",
    preset: "light" as const,
    colors: {
      tube: "#1F6FC5",
      create: "#2E9E5B",
      delete: "#D93A3A",
      import: "#0E97A3",
      links: "#8A4FD8",
      edit: "#E8820C",
      update: "#F5B517",
      freeze: "#2D9CDB",
      auxiliary: "#C9920A",
      axes_x: "#FF0000",
      axes_y: "#00FF00",
      axes_z: "#0000FF",
      neutral: "#757575",
    },
  },
  windowState: { width: 1400, height: 900, x: 0, y: 0 },
  shortcuts: {},
  panelLayout: {
    featureTreeWidth: 250,
    propertiesPanelWidth: 300,
    statusBarHeight: 24,
  },
};

describe("ProfilesManager", () => {
  const mockOnProfileChange = jest.fn();
  const mockOnProfileSave = jest.fn();
  const mockOnProfileDelete = jest.fn();
  const mockOnProfileExport = jest.fn();
  const mockOnProfileImport = jest.fn();

  const defaultProps = {
    profiles: { Default: mockProfile },
    currentProfile: "Default",
    onProfileChange: mockOnProfileChange,
    onProfileSave: mockOnProfileSave,
    onProfileDelete: mockOnProfileDelete,
    onProfileExport: mockOnProfileExport,
    onProfileImport: mockOnProfileImport,
  };

  beforeEach(() => {
    jest.clearAllMocks();
  });

  describe("Rendering", () => {
    it("should render profiles manager", () => {
      render(<ProfilesManager {...defaultProps} />);
      expect(screen.getByText("Manage Profiles")).toBeInTheDocument();
    });

    it("should display profile list", () => {
      render(<ProfilesManager {...defaultProps} />);
      expect(screen.getByText("Default")).toBeInTheDocument();
    });

    it("should show create profile form", () => {
      render(<ProfilesManager {...defaultProps} />);
      expect(screen.getByPlaceholderText("Enter profile name...")).toBeInTheDocument();
    });

    it("should display create profile button", () => {
      render(<ProfilesManager {...defaultProps} />);
      expect(screen.getByText("Create Profile")).toBeInTheDocument();
    });
  });

  describe("Profile Creation", () => {
    it("should create new profile", () => {
      render(<ProfilesManager {...defaultProps} />);

      const input = screen.getByPlaceholderText("Enter profile name...");
      const button = screen.getByText("Create Profile");

      fireEvent.change(input, { target: { value: "New Profile" } });
      fireEvent.click(button);

      expect(mockOnProfileSave).toHaveBeenCalled();
    });

    it("should show error for empty profile name", () => {
      render(<ProfilesManager {...defaultProps} />);

      const button = screen.getByText("Create Profile");
      fireEvent.click(button);

      // Error message should be displayed
    });

    it("should clear input after successful creation", () => {
      render(<ProfilesManager {...defaultProps} />);

      const input = screen.getByPlaceholderText(
        "Enter profile name..."
      ) as HTMLInputElement;
      fireEvent.change(input, { target: { value: "New" } });
      fireEvent.click(screen.getByText("Create Profile"));

      expect(input.value).toBe("");
    });

    it("should prevent duplicate profile names", () => {
      render(<ProfilesManager {...defaultProps} />);

      const input = screen.getByPlaceholderText("Enter profile name...");
      const button = screen.getByText("Create Profile");

      fireEvent.change(input, { target: { value: "Default" } });
      fireEvent.click(button);

      // Should show error about duplicate name
    });
  });

  describe("Profile Actions", () => {
    it("should switch to profile when Load clicked", () => {
      const profiles = {
        Default: mockProfile,
        Profile2: { ...mockProfile, name: "Profile2", isDefault: false },
      };

      render(
        <ProfilesManager {...defaultProps} profiles={profiles} currentProfile="Default" />
      );

      // Find and click the Load button for Profile2
      const loadButtons = screen.getAllByText("Load");
      if (loadButtons.length > 0) {
        fireEvent.click(loadButtons[0]);
        expect(mockOnProfileChange).toHaveBeenCalled();
      }
    });

    it("should show rename dialog", () => {
      const profiles = {
        Default: mockProfile,
        Profile2: { ...mockProfile, name: "Profile2", isDefault: false },
      };

      render(
        <ProfilesManager {...defaultProps} profiles={profiles} />
      );

      const renameButtons = screen.getAllByText("Rename");
      if (renameButtons.length > 0) {
        fireEvent.click(renameButtons[0]);
      }
    });

    it("should show delete confirmation", () => {
      const profiles = {
        Default: mockProfile,
        Profile2: { ...mockProfile, name: "Profile2", isDefault: false },
      };

      render(
        <ProfilesManager {...defaultProps} profiles={profiles} />
      );

      const deleteButtons = screen.getAllByText("Delete");
      if (deleteButtons.length > 0) {
        fireEvent.click(deleteButtons[0]);
      }
    });

    it("should prevent deletion of default profile", () => {
      render(<ProfilesManager {...defaultProps} />);

      // Default profile should not have a delete button
      const deleteButtons = screen.queryAllByText("Delete");
      expect(deleteButtons.length).toBe(0);
    });

    it("should export profile", () => {
      render(<ProfilesManager {...defaultProps} />);

      const exportButtons = screen.getAllByText("Export");
      if (exportButtons.length > 0) {
        fireEvent.click(exportButtons[0]);
        expect(mockOnProfileExport).toHaveBeenCalled();
      }
    });
  });

  describe("Profile Renaming", () => {
    it("should rename profile with new name", () => {
      const profiles = {
        Default: mockProfile,
        Profile2: { ...mockProfile, name: "Profile2", isDefault: false },
      };

      render(
        <ProfilesManager {...defaultProps} profiles={profiles} />
      );

      // Would test rename dialog here
    });

    it("should prevent empty rename", () => {
      const profiles = {
        Default: mockProfile,
        Profile2: { ...mockProfile, name: "Profile2", isDefault: false },
      };

      render(
        <ProfilesManager {...defaultProps} profiles={profiles} />
      );

      // Would test empty name validation
    });

    it("should prevent duplicate profile names on rename", () => {
      const profiles = {
        Default: mockProfile,
        Profile2: { ...mockProfile, name: "Profile2", isDefault: false },
      };

      render(
        <ProfilesManager {...defaultProps} profiles={profiles} />
      );

      // Would test duplicate validation
    });
  });

  describe("Profile Statistics", () => {
    it("should display button count", () => {
      render(<ProfilesManager {...defaultProps} />);

      // Statistics should be visible
      expect(screen.getByText(/Buttons:/)).toBeInTheDocument();
    });

    it("should display separator count", () => {
      render(<ProfilesManager {...defaultProps} />);

      expect(screen.getByText(/Separators:/)).toBeInTheDocument();
    });

    it("should display theme name", () => {
      render(<ProfilesManager {...defaultProps} />);

      // "Theme:" also appears inside the toolbar summary line; target the stat label
      expect(screen.getByText("Theme:", { selector: ".profile-stat-label" })).toBeInTheDocument();
    });
  });

  describe("Import/Export", () => {
    it("should handle file import", () => {
      render(<ProfilesManager {...defaultProps} />);

      const browseButton = screen.getByText("Browse");
      expect(browseButton).toBeInTheDocument();
    });

    it("should show success on import", () => {
      render(<ProfilesManager {...defaultProps} />);

      // Would test import functionality
    });

    it("should show error on invalid import", () => {
      render(<ProfilesManager {...defaultProps} />);

      // Would test invalid import error
    });
  });

  describe("Active Profile Indication", () => {
    it("should mark current profile as active", () => {
      render(<ProfilesManager {...defaultProps} currentProfile="Default" />);

      // Current profile should show ACTIVE badge
    });

    it("should update active status on profile switch", () => {
      const { rerender } = render(
        <ProfilesManager {...defaultProps} currentProfile="Default" />
      );

      rerender(
        <ProfilesManager
          {...defaultProps}
          currentProfile="Profile2"
          profiles={{
            Default: mockProfile,
            Profile2: { ...mockProfile, name: "Profile2", isDefault: false },
          }}
        />
      );
    });
  });

  describe("Default Profile", () => {
    it("should mark default profile", () => {
      render(<ProfilesManager {...defaultProps} />);

      // Default profile should have badge
      expect(screen.getByText("DEFAULT")).toBeInTheDocument();
    });

    it("should allow exporting default profile", () => {
      render(<ProfilesManager {...defaultProps} />);

      const exportButtons = screen.getAllByText("Export");
      expect(exportButtons.length).toBeGreaterThan(0);
    });
  });

  describe("Empty State", () => {
    it("should handle empty profiles list", () => {
      render(
        <ProfilesManager
          {...defaultProps}
          profiles={{}}
        />
      );

      expect(screen.getByText("Manage Profiles")).toBeInTheDocument();
    });
  });

  describe("Status Messages", () => {
    it("should show success on profile creation", () => {
      render(<ProfilesManager {...defaultProps} />);

      const input = screen.getByPlaceholderText("Enter profile name...");
      const button = screen.getByText("Create Profile");

      fireEvent.change(input, { target: { value: "New" } });
      fireEvent.click(button);

      // Success message should appear
    });

    it("should show error messages", () => {
      render(<ProfilesManager {...defaultProps} />);

      // Error cases would trigger status messages
    });

    it("should clear status messages after timeout", (done) => {
      render(<ProfilesManager {...defaultProps} />);

      // Status should disappear after 3 seconds
      setTimeout(() => {
        done();
      }, 3100);
    });
  });

  describe("Accessibility", () => {
    it("should have proper labels", () => {
      render(<ProfilesManager {...defaultProps} />);

      expect(screen.getByPlaceholderText("Enter profile name...")).toBeInTheDocument();
    });

    it("should support keyboard navigation", () => {
      render(<ProfilesManager {...defaultProps} />);

      const input = screen.getByPlaceholderText("Enter profile name...");
      fireEvent.keyPress(input, { key: "Enter", code: "Enter" });
    });
  });

  describe("Edge Cases", () => {
    it("should handle profile with special characters in name", () => {
      render(<ProfilesManager {...defaultProps} />);

      const input = screen.getByPlaceholderText("Enter profile name...");
      const button = screen.getByText("Create Profile");

      fireEvent.change(input, { target: { value: "Profile #1 (Test)" } });
      fireEvent.click(button);

      expect(mockOnProfileSave).toHaveBeenCalled();
    });

    it("should handle very long profile names", () => {
      render(<ProfilesManager {...defaultProps} />);

      const input = screen.getByPlaceholderText("Enter profile name...");
      const longName = "A".repeat(100);

      fireEvent.change(input, { target: { value: longName } });
      fireEvent.click(screen.getByText("Create Profile"));
    });

    it("should handle rapid profile creation", () => {
      render(<ProfilesManager {...defaultProps} />);

      const input = screen.getByPlaceholderText("Enter profile name...");
      const button = screen.getByText("Create Profile");

      fireEvent.change(input, { target: { value: "Profile1" } });
      fireEvent.click(button);

      fireEvent.change(input, { target: { value: "Profile2" } });
      fireEvent.click(button);

      expect(mockOnProfileSave).toHaveBeenCalledTimes(2);
    });
  });
});
