/**
 * BendCore Enterprise Profile Manager Component
 * Organization-level profile library with version control
 *
 * Features:
 * - Share profiles across projects/team members
 * - Version control for profiles
 * - Profile activation/deactivation
 * - Backup and recovery
 * - Import/export in bulk
 * - Role-based access (Admin, Engineer, View-only)
 */

import React, { useState, useCallback } from "react";
import "./EnterpriseProfileManager.css";

/**
 * User roles
 */
export enum UserRole {
  ADMIN = "admin",           // Full access
  ENGINEER = "engineer",     // Can create/edit
  VIEW_ONLY = "view_only",  // Read-only
}

/**
 * Profile version entry
 */
interface ProfileVersion {
  version: string;
  createdAt: Date;
  createdBy: string;
  changes: string;
  isActive: boolean;
}

/**
 * Organization profile with version history
 */
interface OrgProfile {
  id: string;
  name: string;
  description: string;
  category: string;
  owner: string;
  createdAt: Date;
  updatedAt: Date;
  versions: ProfileVersion[];
  isArchived: boolean;
  accessLevel: UserRole;
  tags: string[];
  profileData: any; // Bending profile data
}

/**
 * Profile statistics
 */
interface ProfileStats {
  totalProfiles: number;
  activeProfiles: number;
  archivedProfiles: number;
  totalVersions: number;
  mostRecentUpdate: Date | null;
}

interface EnterpriseProfileManagerProps {
  userRole: UserRole;
  currentUserId: string;
  onProfileSelect?: (profile: OrgProfile) => void;
  onProfileCreate?: (profile: OrgProfile) => void;
}

/**
 * Profile card component
 */
interface ProfileCardProps {
  profile: OrgProfile;
  userRole: UserRole;
  onSelect: (profile: OrgProfile) => void;
  onActivate: (profileId: string) => void;
  onArchive: (profileId: string) => void;
  onExport: (profileId: string) => void;
  onDelete: (profileId: string) => void;
}

const ProfileCard: React.FC<ProfileCardProps> = ({
  profile,
  userRole,
  onSelect,
  onActivate,
  onArchive,
  onExport,
  onDelete,
}) => {
  const [showVersions, setShowVersions] = useState(false);
  const activeVersion = profile.versions.find((v) => v.isActive);
  const canEdit = userRole === UserRole.ADMIN || userRole === UserRole.ENGINEER;

  return (
    <div className={`profile-card ${profile.isArchived ? "archived" : ""}`}>
      <div className="profile-card-header">
        <div className="profile-title-section">
          <h4 className="profile-name">{profile.name}</h4>
          {profile.isArchived && <span className="archived-badge">Archived</span>}
          {activeVersion && (
            <span className="active-badge">v{activeVersion.version}</span>
          )}
        </div>
        <div className="profile-meta">
          <span className="profile-category">{profile.category}</span>
          <span className="profile-owner">by {profile.owner}</span>
        </div>
      </div>

      <div className="profile-card-body">
        <p className="profile-description">{profile.description}</p>
        <div className="profile-tags">
          {profile.tags.map((tag) => (
            <span key={tag} className="tag">
              {tag}
            </span>
          ))}
        </div>
      </div>

      <div className="profile-card-footer">
        <div className="profile-stats">
          <span>{profile.versions.length} versions</span>
          <span>Updated {new Date(profile.updatedAt).toLocaleDateString()}</span>
        </div>

        <div className="profile-actions">
          <button
            className="btn-small btn-primary"
            onClick={() => onSelect(profile)}
            title="Select this profile"
          >
            Select
          </button>

          {canEdit && !profile.isArchived && (
            <button
              className="btn-small btn-success"
              onClick={() => onActivate(profile.id)}
              title="Activate as default"
            >
              Activate
            </button>
          )}

          <button
            className="btn-small btn-export"
            onClick={() => onExport(profile.id)}
            title="Export profile"
          >
            Export
          </button>

          {canEdit && (
            <button
              className="btn-small btn-warning"
              onClick={() => onArchive(profile.id)}
              title={profile.isArchived ? "Restore" : "Archive"}
            >
              {profile.isArchived ? "Restore" : "Archive"}
            </button>
          )}

          {userRole === UserRole.ADMIN && (
            <button
              className="btn-small btn-danger"
              onClick={() => onDelete(profile.id)}
              title="Delete permanently"
            >
              Delete
            </button>
          )}

          <button
            className="btn-small btn-secondary"
            onClick={() => setShowVersions(!showVersions)}
            title="View version history"
          >
            {showVersions ? "Hide" : "Show"} History
          </button>
        </div>
      </div>

      {showVersions && (
        <div className="profile-versions">
          <h5>Version History</h5>
          <div className="versions-list">
            {profile.versions.map((version) => (
              <div key={version.version} className="version-item">
                <div className="version-header">
                  <span className="version-num">v{version.version}</span>
                  {version.isActive && <span className="active-indicator">Active</span>}
                </div>
                <div className="version-meta">
                  <span>{new Date(version.createdAt).toLocaleString()}</span>
                  <span>by {version.createdBy}</span>
                </div>
                <div className="version-changes">{version.changes}</div>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};

/**
 * Profile creation/edit dialog
 */
interface ProfileEditorProps {
  profile?: OrgProfile;
  onSave: (profile: OrgProfile) => void;
  onCancel: () => void;
}

const ProfileEditor: React.FC<ProfileEditorProps> = ({
  profile,
  onSave,
  onCancel,
}) => {
  const [formData, setFormData] = useState<Partial<OrgProfile>>({
    name: profile?.name || "",
    description: profile?.description || "",
    category: profile?.category || "Standard",
    tags: profile?.tags || [],
  });
  const [tagInput, setTagInput] = useState("");

  const handleAddTag = () => {
    if (tagInput.trim() && !formData.tags?.includes(tagInput.trim())) {
      setFormData({
        ...formData,
        tags: [...(formData.tags || []), tagInput.trim()],
      });
      setTagInput("");
    }
  };

  const handleRemoveTag = (tag: string) => {
    setFormData({
      ...formData,
      tags: (formData.tags || []).filter((t) => t !== tag),
    });
  };

  const handleSave = () => {
    if (!formData.name?.trim()) {
      alert("Profile name is required");
      return;
    }
    onSave({
      ...profile,
      ...formData,
      id: profile?.id || `profile-${Date.now()}`,
      owner: profile?.owner || "current-user",
      createdAt: profile?.createdAt || new Date(),
      updatedAt: new Date(),
      versions: profile?.versions || [],
      isArchived: profile?.isArchived || false,
      accessLevel: profile?.accessLevel || UserRole.ENGINEER,
      profileData: profile?.profileData || {},
    } as OrgProfile);
  };

  return (
    <div className="profile-editor-modal">
      <div className="profile-editor">
        <h3>{profile ? "Edit Profile" : "Create New Profile"}</h3>

        <div className="form-group">
          <label>Profile Name</label>
          <input
            type="text"
            value={formData.name || ""}
            onChange={(e) => setFormData({ ...formData, name: e.target.value })}
            placeholder="e.g., Standard Aluminum 5083"
          />
        </div>

        <div className="form-group">
          <label>Description</label>
          <textarea
            value={formData.description || ""}
            onChange={(e) =>
              setFormData({ ...formData, description: e.target.value })
            }
            placeholder="Describe this profile and its use cases"
            rows={3}
          />
        </div>

        <div className="form-group">
          <label>Category</label>
          <select
            value={formData.category || "Standard"}
            onChange={(e) => setFormData({ ...formData, category: e.target.value })}
          >
            <option>Standard</option>
            <option>Custom</option>
            <option>Material-Specific</option>
            <option>Application-Specific</option>
          </select>
        </div>

        <div className="form-group">
          <label>Tags</label>
          <div className="tag-input-group">
            <input
              type="text"
              value={tagInput}
              onChange={(e) => setTagInput(e.target.value)}
              onKeyPress={(e) => e.key === "Enter" && handleAddTag()}
              placeholder="Add tag and press Enter"
            />
            <button type="button" onClick={handleAddTag}>
              Add Tag
            </button>
          </div>
          <div className="tags-display">
            {(formData.tags || []).map((tag) => (
              <div key={tag} className="tag-chip">
                {tag}
                <button
                  type="button"
                  onClick={() => handleRemoveTag(tag)}
                  className="remove-tag"
                >
                  ×
                </button>
              </div>
            ))}
          </div>
        </div>

        <div className="editor-actions">
          <button className="btn-save" onClick={handleSave}>
            Save Profile
          </button>
          <button className="btn-cancel" onClick={onCancel}>
            Cancel
          </button>
        </div>
      </div>
    </div>
  );
};

/**
 * Main Enterprise Profile Manager component
 */
export const EnterpriseProfileManager: React.FC<
  EnterpriseProfileManagerProps
> = ({ userRole, currentUserId, onProfileSelect, onProfileCreate }) => {
  const [profiles, setProfiles] = useState<OrgProfile[]>([]);
  const [filteredProfiles, setFilteredProfiles] = useState<OrgProfile[]>([]);
  const [searchTerm, setSearchTerm] = useState("");
  const [selectedCategory, setSelectedCategory] = useState<string | null>(null);
  const [showEditor, setShowEditor] = useState(false);
  const [editingProfile, setEditingProfile] = useState<OrgProfile | undefined>();
  const [viewArchived, setViewArchived] = useState(false);

  // Calculate statistics
  const stats: ProfileStats = {
    totalProfiles: profiles.length,
    activeProfiles: profiles.filter((p) => !p.isArchived).length,
    archivedProfiles: profiles.filter((p) => p.isArchived).length,
    totalVersions: profiles.reduce((sum, p) => sum + p.versions.length, 0),
    mostRecentUpdate: profiles.length
      ? new Date(Math.max(...profiles.map((p) => p.updatedAt.getTime())))
      : null,
  };

  // Filter profiles
  const filterProfiles = useCallback(() => {
    let filtered = profiles.filter((p) => p.isArchived === viewArchived);

    if (searchTerm) {
      const term = searchTerm.toLowerCase();
      filtered = filtered.filter(
        (p) =>
          p.name.toLowerCase().includes(term) ||
          p.description.toLowerCase().includes(term) ||
          p.tags.some((t) => t.toLowerCase().includes(term))
      );
    }

    if (selectedCategory) {
      filtered = filtered.filter((p) => p.category === selectedCategory);
    }

    setFilteredProfiles(filtered);
  }, [profiles, searchTerm, selectedCategory, viewArchived]);

  React.useEffect(() => {
    filterProfiles();
  }, [filterProfiles]);

  // Load mock data
  React.useEffect(() => {
    const mockProfiles: OrgProfile[] = [
      {
        id: "prof-1",
        name: "Standard Steel 3/8\"",
        description: "Common steel tubing for structural applications",
        category: "Standard",
        owner: "John Doe",
        createdAt: new Date("2024-10-01"),
        updatedAt: new Date("2024-10-15"),
        versions: [
          {
            version: "1.0",
            createdAt: new Date("2024-10-01"),
            createdBy: "John Doe",
            changes: "Initial version",
            isActive: true,
          },
          {
            version: "1.1",
            createdAt: new Date("2024-10-15"),
            createdBy: "Jane Smith",
            changes: "Updated bend radius",
            isActive: false,
          },
        ],
        isArchived: false,
        accessLevel: UserRole.ENGINEER,
        tags: ["structural", "steel", "common"],
        profileData: {},
      },
      {
        id: "prof-2",
        name: "Aluminum 6061-T6 1/2\"",
        description: "Lightweight aluminum for aerospace applications",
        category: "Material-Specific",
        owner: "Jane Smith",
        createdAt: new Date("2024-09-20"),
        updatedAt: new Date("2024-10-10"),
        versions: [
          {
            version: "1.0",
            createdAt: new Date("2024-09-20"),
            createdBy: "Jane Smith",
            changes: "Initial version",
            isActive: true,
          },
        ],
        isArchived: false,
        accessLevel: UserRole.ENGINEER,
        tags: ["aerospace", "aluminum", "lightweight"],
        profileData: {},
      },
    ];
    setProfiles(mockProfiles);
  }, []);

  const handleCreateProfile = (profile: OrgProfile) => {
    const newProfiles = editingProfile
      ? profiles.map((p) => (p.id === profile.id ? profile : p))
      : [...profiles, profile];
    setProfiles(newProfiles);
    onProfileCreate?.(profile);
    setShowEditor(false);
    setEditingProfile(undefined);
  };

  const handleSelectProfile = (profile: OrgProfile) => {
    onProfileSelect?.(profile);
  };

  const handleActivateProfile = (profileId: string) => {
    const updated = profiles.map((p) => ({
      ...p,
      versions: p.versions.map((v) => ({
        ...v,
        isActive: p.id === profileId && v.isActive,
      })),
    }));
    setProfiles(updated);
  };

  const handleArchiveProfile = (profileId: string) => {
    const updated = profiles.map((p) =>
      p.id === profileId ? { ...p, isArchived: !p.isArchived } : p
    );
    setProfiles(updated);
  };

  const handleExportProfile = (profileId: string) => {
    const profile = profiles.find((p) => p.id === profileId);
    if (profile) {
      const json = JSON.stringify(profile, null, 2);
      const blob = new Blob([json], { type: "application/json" });
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = `${profile.name.replace(/\s+/g, "-")}.json`;
      a.click();
    }
  };

  const handleDeleteProfile = (profileId: string) => {
    if (confirm("Permanently delete this profile?")) {
      setProfiles(profiles.filter((p) => p.id !== profileId));
    }
  };

  const categories = Array.from(
    new Set(profiles.map((p) => p.category))
  ).sort();

  const canCreate = userRole === UserRole.ADMIN || userRole === UserRole.ENGINEER;

  return (
    <div className="enterprise-profile-manager">
      <div className="epm-header">
        <h2>Organization Profile Library</h2>
        <div className="epm-stats">
          <div className="stat">
            <span className="stat-value">{stats.activeProfiles}</span>
            <span className="stat-label">Active</span>
          </div>
          <div className="stat">
            <span className="stat-value">{stats.totalVersions}</span>
            <span className="stat-label">Versions</span>
          </div>
          <div className="stat">
            <span className="stat-value">{stats.archivedProfiles}</span>
            <span className="stat-label">Archived</span>
          </div>
        </div>
      </div>

      <div className="epm-toolbar">
        <div className="search-box">
          <input
            type="text"
            placeholder="Search profiles..."
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
          />
        </div>

        <div className="category-filter">
          <button
            className={!selectedCategory ? "active" : ""}
            onClick={() => setSelectedCategory(null)}
          >
            All Categories
          </button>
          {categories.map((cat) => (
            <button
              key={cat}
              className={selectedCategory === cat ? "active" : ""}
              onClick={() => setSelectedCategory(cat)}
            >
              {cat}
            </button>
          ))}
        </div>

        <div className="toolbar-actions">
          <button
            className={`btn-archived-toggle ${viewArchived ? "active" : ""}`}
            onClick={() => setViewArchived(!viewArchived)}
          >
            {viewArchived ? "Show Active" : "Show Archived"}
          </button>

          {canCreate && (
            <button
              className="btn-create-profile"
              onClick={() => {
                setEditingProfile(undefined);
                setShowEditor(true);
              }}
            >
              + New Profile
            </button>
          )}
        </div>
      </div>

      <div className="epm-content">
        {filteredProfiles.length === 0 ? (
          <div className="epm-empty">
            <p>
              {searchTerm || selectedCategory
                ? "No profiles match your search"
                : "No profiles yet"}
            </p>
            {canCreate && (
              <button
                className="btn-create-profile-large"
                onClick={() => {
                  setEditingProfile(undefined);
                  setShowEditor(true);
                }}
              >
                Create First Profile
              </button>
            )}
          </div>
        ) : (
          <div className="profiles-grid">
            {filteredProfiles.map((profile) => (
              <ProfileCard
                key={profile.id}
                profile={profile}
                userRole={userRole}
                onSelect={handleSelectProfile}
                onActivate={handleActivateProfile}
                onArchive={handleArchiveProfile}
                onExport={handleExportProfile}
                onDelete={handleDeleteProfile}
              />
            ))}
          </div>
        )}
      </div>

      {showEditor && (
        <ProfileEditor
          profile={editingProfile}
          onSave={handleCreateProfile}
          onCancel={() => {
            setShowEditor(false);
            setEditingProfile(undefined);
          }}
        />
      )}
    </div>
  );
};

export default EnterpriseProfileManager;
