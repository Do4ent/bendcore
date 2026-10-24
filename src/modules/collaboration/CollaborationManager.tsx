import React, { useState } from 'react';
import './CollaborationManager.css';

export interface Collaborator {
  id: string;
  name: string;
  email: string;
  avatar?: string;
  role: 'owner' | 'editor' | 'viewer';
  status: 'online' | 'offline' | 'idle';
  lastSeen?: Date;
  color: string;
}

export interface ChangeLog {
  id: string;
  timestamp: Date;
  author: Collaborator;
  action: string;
  objectName: string;
  details: string;
}

interface CollaborationManagerProps {
  projectId: string;
  currentUser: Collaborator;
}

const formatTime = (date: Date): string => {
  const now = new Date();
  const diffMs = now.getTime() - date.getTime();
  const diffMins = Math.floor(diffMs / 60000);

  if (diffMins < 1) return 'just now';
  if (diffMins < 60) return `${diffMins}m ago`;
  const diffHours = Math.floor(diffMins / 60);
  if (diffHours < 24) return `${diffHours}h ago`;
  const diffDays = Math.floor(diffHours / 24);
  if (diffDays < 7) return `${diffDays}d ago`;
  return date.toLocaleDateString();
};

export const CollaborationManager: React.FC<CollaborationManagerProps> = ({
  projectId,
  currentUser,
}) => {
  const [collaborators, setCollaborators] = useState<Collaborator[]>([
    {
      id: 'user-1',
      name: 'John Doe',
      email: 'john@example.com',
      role: 'owner',
      status: 'online',
      color: '#FF6B6B',
    },
    {
      id: 'user-2',
      name: 'Jane Smith',
      email: 'jane@example.com',
      role: 'editor',
      status: 'online',
      color: '#4ECDC4',
    },
    {
      id: 'user-3',
      name: 'Bob Johnson',
      email: 'bob@example.com',
      role: 'viewer',
      status: 'offline',
      lastSeen: new Date(Date.now() - 30 * 60000),
      color: '#95E1D3',
    },
  ]);

  const [changeLogs, setChangeLogs] = useState<ChangeLog[]>([
    {
      id: 'log-1',
      timestamp: new Date(),
      author: collaborators[0],
      action: 'Modified',
      objectName: 'Tube_1',
      details: 'Changed OD from 25.4mm to 26mm',
    },
    {
      id: 'log-2',
      timestamp: new Date(Date.now() - 5 * 60000),
      author: collaborators[1],
      action: 'Created',
      objectName: 'Bend_3',
      details: 'Added 45° bend',
    },
  ]);

  const [activeTab, setActiveTab] = useState<'collaborators' | 'activity' | 'sharing'>(
    'collaborators'
  );

  const [showInviteForm, setShowInviteForm] = useState(false);
  const [inviteEmail, setInviteEmail] = useState('');
  const [inviteRole, setInviteRole] = useState<'editor' | 'viewer'>('editor');

  const handleInviteCollaborator = () => {
    const newCollaborator: Collaborator = {
      id: `user-${Date.now()}`,
      name: inviteEmail.split('@')[0],
      email: inviteEmail,
      role: inviteRole,
      status: 'offline',
      color: `hsl(${Math.random() * 360}, 70%, 50%)`,
    };

    setCollaborators([...collaborators, newCollaborator]);
    setInviteEmail('');
    setShowInviteForm(false);
  };

  const handleRemoveCollaborator = (userId: string) => {
    setCollaborators((prev) => prev.filter((c) => c.id !== userId));
  };

  const handleChangeRole = (userId: string, newRole: Collaborator['role']) => {
    setCollaborators((prev) =>
      prev.map((c) => (c.id === userId ? { ...c, role: newRole } : c))
    );
  };

  return (
    <div className="collaboration-manager">
      {/* Header */}
      <div className="collab-header">
        <h3>Collaboration</h3>
        <div className="online-count">
          <span className="status-dot online" />
          {collaborators.filter((c) => c.status === 'online').length} online
        </div>
      </div>

      {/* Tabs */}
      <div className="collab-tabs">
        {(['collaborators', 'activity', 'sharing'] as const).map((tab) => (
          <button
            key={tab}
            className={`tab-btn ${activeTab === tab ? 'active' : ''}`}
            onClick={() => setActiveTab(tab)}
          >
            {tab.charAt(0).toUpperCase() + tab.slice(1)}
          </button>
        ))}
      </div>

      {/* Content */}
      <div className="collab-content">
        {activeTab === 'collaborators' && (
          <div className="collaborators-tab">
            {collaborators.map((collab) => (
              <div key={collab.id} className="collaborator-card">
                <div className="collab-avatar" style={{ backgroundColor: collab.color }}>
                  {collab.name.charAt(0)}
                </div>
                <div className="collab-info">
                  <div className="collab-name">
                    {collab.name}
                    {collab.id === currentUser.id && (
                      <span className="you-badge">You</span>
                    )}
                  </div>
                  <div className="collab-status">
                    <span className={`status-dot ${collab.status}`} />
                    {collab.status === 'offline' && collab.lastSeen ? (
                      <span>Last seen {formatTime(collab.lastSeen)}</span>
                    ) : (
                      <span>{collab.status}</span>
                    )}
                  </div>
                </div>
                <div className="collab-role">
                  {collab.id === currentUser.id ? (
                    <span className="role-badge">{collab.role}</span>
                  ) : currentUser.role === 'owner' ? (
                    <select
                      value={collab.role}
                      onChange={(e) =>
                        handleChangeRole(
                          collab.id,
                          e.target.value as Collaborator['role']
                        )
                      }
                      className="role-select"
                    >
                      <option value="editor">Editor</option>
                      <option value="viewer">Viewer</option>
                    </select>
                  ) : (
                    <span className="role-badge">{collab.role}</span>
                  )}
                </div>
                {currentUser.role === 'owner' && collab.id !== currentUser.id && (
                  <button
                    className="remove-btn"
                    onClick={() => handleRemoveCollaborator(collab.id)}
                    title="Remove collaborator"
                  >
                    ×
                  </button>
                )}
              </div>
            ))}

            {currentUser.role === 'owner' && (
              <div className="invite-section">
                {!showInviteForm ? (
                  <button
                    className="invite-btn"
                    onClick={() => setShowInviteForm(true)}
                  >
                    + Invite Collaborator
                  </button>
                ) : (
                  <div className="invite-form">
                    <input
                      type="email"
                      placeholder="Email address"
                      value={inviteEmail}
                      onChange={(e) => setInviteEmail(e.target.value)}
                      className="invite-input"
                    />
                    <select
                      value={inviteRole}
                      onChange={(e) =>
                        setInviteRole(e.target.value as 'editor' | 'viewer')
                      }
                      className="role-select"
                    >
                      <option value="editor">Editor</option>
                      <option value="viewer">Viewer</option>
                    </select>
                    <div className="invite-actions">
                      <button
                        className="send-btn"
                        onClick={handleInviteCollaborator}
                        disabled={!inviteEmail}
                      >
                        Send Invite
                      </button>
                      <button
                        className="cancel-btn"
                        onClick={() => {
                          setShowInviteForm(false);
                          setInviteEmail('');
                        }}
                      >
                        Cancel
                      </button>
                    </div>
                  </div>
                )}
              </div>
            )}
          </div>
        )}

        {activeTab === 'activity' && (
          <div className="activity-tab">
            {changeLogs.length > 0 ? (
              changeLogs.map((log) => (
                <div key={log.id} className="activity-item">
                  <div className="activity-avatar" style={{ backgroundColor: log.author.color }}>
                    {log.author.name.charAt(0)}
                  </div>
                  <div className="activity-info">
                    <div className="activity-text">
                      <strong>{log.author.name}</strong>
                      <span className="activity-action"> {log.action} </span>
                      <strong>{log.objectName}</strong>
                    </div>
                    <div className="activity-details">{log.details}</div>
                    <div className="activity-time">{formatTime(log.timestamp)}</div>
                  </div>
                </div>
              ))
            ) : (
              <div className="empty-activity">No activity yet</div>
            )}
          </div>
        )}

        {activeTab === 'sharing' && (
          <div className="sharing-tab">
            <div className="sharing-section">
              <h4>Share Project</h4>
              <div className="share-options">
                <label>
                  <input type="radio" name="share" defaultChecked /> Specific people
                </label>
                <label>
                  <input type="radio" name="share" /> Anyone with link
                </label>
              </div>
              <div className="share-link">
                <input
                  type="text"
                  readOnly
                  value={`https://bendcore.app/projects/${projectId}/share`}
                  className="link-input"
                />
                <button className="copy-btn" onClick={() => {
                  const input = document.querySelector('.link-input') as HTMLInputElement;
                  if (input) {
                    navigator.clipboard.writeText(input.value);
                  }
                }}>
                  Copy Link
                </button>
              </div>
            </div>

            <div className="sharing-section">
              <h4>Version Control</h4>
              <div className="version-info">
                <p>Current version: v1.2.3</p>
                <p>Last saved: just now</p>
                <p>Auto-save: Enabled</p>
              </div>
              <button className="version-history-btn">View Version History</button>
            </div>

            <div className="sharing-section">
              <h4>Real-time Sync</h4>
              <div className="sync-status">
                <span className="status-dot online" />
                <span>Real-time sync: Active</span>
              </div>
              <p className="sync-info">
                Changes are automatically synchronized across all collaborators.
              </p>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
