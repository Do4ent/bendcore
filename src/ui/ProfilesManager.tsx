/**
 * BendCore Profiles Manager Component
 * Create, load, delete, and manage UI profiles
 */

import React, { useState, useCallback } from "react";
import { UIProfile, SettingsData } from "./SettingsStore";
import "./ProfilesManager.css";

interface ProfilesManagerProps {
  profiles: Record<string, UIProfile>;
  currentProfile: string;
  onProfileChange: (profileName: string) => void;
  onProfileSave: (profile: UIProfile) => void;
  onProfileDelete: (profileName: string) => void;
  onProfileExport: (profileName: string) => void;
  onProfileImport: (profileData: UIProfile) => void;
}

interface DialogState {
  type: "none" | "create" | "rename" | "delete" | "confirm";
  profileName: string;
  newName: string;
}

const ProfilesManager: React.FC<ProfilesManagerProps> = ({
  profiles,
  currentProfile,
  onProfileChange,
  onProfileSave,
  onProfileDelete,
  onProfileExport,
  onProfileImport,
}) => {
  const [profileName, setProfileName] = useState("");
  const [dialog, setDialog] = useState<DialogState>({
    type: "none",
    profileName: "",
    newName: "",
  });
  const [status, setStatus] = useState<{
    type: "success" | "error" | "info" | null;
    message: string;
  }>({ type: null, message: "" });
  const [fileInputLabel, setFileInputLabel] = useState("Choose file...");

  // Show status message
  const showStatus = (
    type: "success" | "error" | "info",
    message: string
  ) => {
    setStatus({ type, message });
    setTimeout(() => setStatus({ type: null, message: "" }), 3000);
  };

  // Create new profile
  const handleCreateProfile = () => {
    if (!profileName.trim()) {
      showStatus("error", "Profile name cannot be empty");
      return;
    }

    if (profiles[profileName]) {
      showStatus("error", "Profile already exists");
      return;
    }

    const currentProfileData = profiles[currentProfile];
    const newProfile: UIProfile = {
      ...JSON.parse(JSON.stringify(currentProfileData)),
      name: profileName,
      isDefault: false,
    };

    onProfileSave(newProfile);
    setProfileName("");
    showStatus("success", `Profile "${profileName}" created`);
  };

  // Switch to profile
  const handleSwitchProfile = (name: string) => {
    onProfileChange(name);
    showStatus("info", `Switched to profile "${name}"`);
  };

  // Open rename dialog
  const openRenameDialog = (name: string) => {
    setDialog({
      type: "rename",
      profileName: name,
      newName: name,
    });
  };

  // Rename profile
  const handleRenameProfile = () => {
    if (!dialog.newName.trim()) {
      showStatus("error", "Profile name cannot be empty");
      return;
    }

    if (dialog.newName === dialog.profileName) {
      setDialog({ type: "none", profileName: "", newName: "" });
      return;
    }

    if (profiles[dialog.newName]) {
      showStatus("error", "Profile name already exists");
      return;
    }

    const profile = profiles[dialog.profileName];
    const renamedProfile: UIProfile = {
      ...JSON.parse(JSON.stringify(profile)),
      name: dialog.newName,
    };

    // Delete old and save new
    onProfileDelete(dialog.profileName);
    onProfileSave(renamedProfile);

    if (currentProfile === dialog.profileName) {
      onProfileChange(dialog.newName);
    }

    setDialog({ type: "none", profileName: "", newName: "" });
    showStatus("success", `Profile renamed to "${dialog.newName}"`);
  };

  // Open delete confirmation
  const openDeleteDialog = (name: string) => {
    if (name === "Default") {
      showStatus("error", "Cannot delete default profile");
      return;
    }
    setDialog({
      type: "delete",
      profileName: name,
      newName: "",
    });
  };

  // Confirm delete
  const handleDeleteProfile = () => {
    if (currentProfile === dialog.profileName) {
      onProfileChange("Default");
    }
    onProfileDelete(dialog.profileName);
    setDialog({ type: "none", profileName: "", newName: "" });
    showStatus("success", `Profile "${dialog.profileName}" deleted`);
  };

  // Export profile
  const handleExportProfile = (name: string) => {
    onProfileExport(name);
    showStatus("success", `Profile "${name}" exported`);
  };

  // Import profile
  const handleImportProfile = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (event) => {
      try {
        const imported = JSON.parse(event.target?.result as string);
        if (imported.name && imported.toolbar && imported.theme) {
          onProfileImport(imported);
          showStatus("success", `Profile "${imported.name}" imported`);
        } else {
          showStatus("error", "Invalid profile file");
        }
      } catch (error) {
        showStatus("error", "Failed to import profile");
      }
    };
    reader.readAsText(file);

    // Reset file input
    if (e.target) {
      e.target.value = "";
      setFileInputLabel("Choose file...");
    }
  };

  const profileList = Object.values(profiles);

  return (
    <div className="profiles-manager">
      <h2 className="profiles-manager-title">Manage Profiles</h2>

      {/* Status */}
      {status.type && (
        <div className={`profile-status ${status.type}`}>
          <span className="status-icon">
            {status.type === "success" ? "✓" : status.type === "error" ? "✕" : "ℹ"}
          </span>
          {status.message}
        </div>
      )}

      {/* Create Profile Form */}
      <div className="create-profile-form">
        <h3 style={{ margin: "0 0 12px 0", fontSize: "14px", fontWeight: "600" }}>
          Create New Profile
        </h3>
        <div className="form-group">
          <label className="form-label">Profile Name</label>
          <input
            type="text"
            className="form-input"
            placeholder="Enter profile name..."
            value={profileName}
            onChange={(e) => setProfileName(e.target.value)}
            onKeyPress={(e) => e.key === "Enter" && handleCreateProfile()}
          />
        </div>
        <div className="form-actions">
          <button className="form-button primary" onClick={handleCreateProfile}>
            Create Profile
          </button>
        </div>
      </div>

      {/* Profiles List */}
      <div className="profiles-list">
        {profileList.length === 0 ? (
          <div className="profiles-list-empty">No profiles available</div>
        ) : (
          profileList.map((profile) => (
            <div
              key={profile.name}
              className={`profile-item ${profile.isDefault ? "default" : ""}`}
            >
              <div className="profile-info">
                <div className="profile-name">
                  {profile.name}
                  {profile.isDefault && (
                    <span className="profile-default-badge">DEFAULT</span>
                  )}
                  {profile.name === currentProfile && (
                    <span
                      style={{
                        display: "inline-block",
                        background: "var(--accent-color, #1f6fc5)",
                        color: "white",
                        fontSize: "11px",
                        padding: "2px 8px",
                        borderRadius: "3px",
                        fontWeight: "600",
                      }}
                    >
                      ACTIVE
                    </span>
                  )}
                </div>
                <div className="profile-description">
                  Toolbar: {profile.toolbar.buttons.length} buttons, Theme: {profile.theme.name}
                </div>
                <div className="profile-stats">
                  <div className="profile-stat">
                    <span className="profile-stat-label">Buttons:</span>
                    <span>{profile.toolbar.buttons.filter((b) => b !== "separator").length}</span>
                  </div>
                  <div className="profile-stat">
                    <span className="profile-stat-label">Separators:</span>
                    <span>{profile.toolbar.buttons.filter((b) => b === "separator").length}</span>
                  </div>
                  <div className="profile-stat">
                    <span className="profile-stat-label">Theme:</span>
                    <span>{profile.theme.preset}</span>
                  </div>
                </div>
              </div>
              <div className="profile-actions">
                {profile.name !== currentProfile && (
                  <button
                    className="profile-action-button"
                    onClick={() => handleSwitchProfile(profile.name)}
                  >
                    Load
                  </button>
                )}
                {!profile.isDefault && (
                  <>
                    <button
                      className="profile-action-button"
                      onClick={() => openRenameDialog(profile.name)}
                    >
                      Rename
                    </button>
                    <button
                      className="profile-action-button"
                      onClick={() => handleExportProfile(profile.name)}
                    >
                      Export
                    </button>
                    <button
                      className="profile-action-button danger"
                      onClick={() => openDeleteDialog(profile.name)}
                    >
                      Delete
                    </button>
                  </>
                )}
                {profile.isDefault && (
                  <button
                    className="profile-action-button"
                    onClick={() => handleExportProfile(profile.name)}
                  >
                    Export
                  </button>
                )}
              </div>
            </div>
          ))
        )}
      </div>

      {/* Import/Export Section */}
      <div style={{ marginBottom: "20px" }}>
        <h3 style={{ margin: "0 0 12px 0", fontSize: "14px", fontWeight: "600" }}>
          Import Profile
        </h3>
        <div className="file-input-group">
          <input
            type="file"
            id="profile-import"
            className="file-input"
            accept=".json"
            onChange={handleImportProfile}
          />
          <label htmlFor="profile-import" className="file-input-label">
            {fileInputLabel}
          </label>
          <button
            className="file-input-button"
            onClick={() => document.getElementById("profile-import")?.click()}
          >
            Browse
          </button>
        </div>
      </div>

      {/* Rename Dialog */}
      {dialog.type === "rename" && (
        <div className="profile-dialog">
          <div className="profile-dialog-content">
            <h3 className="dialog-title">Rename Profile</h3>
            <div className="dialog-body">
              <div className="form-group">
                <label className="form-label">New Name</label>
                <input
                  type="text"
                  className="form-input"
                  value={dialog.newName}
                  onChange={(e) =>
                    setDialog({ ...dialog, newName: e.target.value })
                  }
                  onKeyPress={(e) =>
                    e.key === "Enter" && handleRenameProfile()
                  }
                  autoFocus
                />
              </div>
            </div>
            <div className="dialog-footer">
              <button
                className="dialog-button"
                onClick={() =>
                  setDialog({ type: "none", profileName: "", newName: "" })
                }
              >
                Cancel
              </button>
              <button
                className="dialog-button primary"
                onClick={handleRenameProfile}
              >
                Rename
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Delete Confirmation Dialog */}
      {dialog.type === "delete" && (
        <div className="profile-dialog">
          <div className="profile-dialog-content">
            <h3 className="dialog-title">Delete Profile</h3>
            <div className="dialog-body">
              <p style={{ margin: "0 0 12px 0" }}>
                Are you sure you want to delete the profile "{dialog.profileName}"?
              </p>
              <p style={{ margin: "0", fontSize: "13px", color: "var(--text-secondary, #666666)" }}>
                This action cannot be undone.
              </p>
            </div>
            <div className="dialog-footer">
              <button
                className="dialog-button"
                onClick={() =>
                  setDialog({ type: "none", profileName: "", newName: "" })
                }
              >
                Cancel
              </button>
              <button
                className="dialog-button danger"
                onClick={handleDeleteProfile}
              >
                Delete
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default ProfilesManager;
