import React, { useState, useEffect } from 'react';
import { useIpc } from '../../hooks/useIpc';

interface CollaborationTabProps {
  tubeId?: string;
}

interface Comment {
  id: string;
  author: string;
  text: string;
  timestamp: number;
}

interface Version {
  id: string;
  timestamp: number;
  author: string;
  changes: string;
}

/**
 * Collaboration Tab Component
 * Part of Properties panel
 *
 * Features (Phase 26C):
 * - Display comments from team members
 * - Show version history
 * - List recent changes
 * - (Placeholder for add comment/version creation in Phase 26C)
 */
export const CollaborationTab: React.FC<CollaborationTabProps> = ({ tubeId }) => {
  const ipc = useIpc();
  const [comments, setComments] = useState<Comment[]>([]);
  const [versions, setVersions] = useState<Version[]>([]);
  const [changes, setChanges] = useState<string[]>([]);
  const [activeTab, setActiveTab] = useState<'comments' | 'versions' | 'changes'>(
    'comments'
  );
  const [isLoading, setIsLoading] = useState(false);

  // Load collaboration data
  useEffect(() => {
    if (!tubeId) return;

    setIsLoading(true);

    Promise.all([
      ipc.getComments(tubeId),
      ipc.getVersions(tubeId),
      ipc.getChanges(tubeId),
    ])
      .then(([cmts, vers, chgs]) => {
        setComments(Array.isArray(cmts) ? cmts : []);
        setVersions(Array.isArray(vers) ? vers : []);
        setChanges(Array.isArray(chgs) ? chgs : []);
      })
      .catch((error) => {
        console.error('Failed to load collaboration data:', error);
      })
      .finally(() => {
        setIsLoading(false);
      });
  }, [tubeId, ipc]);

  const formatTime = (timestamp: number): string => {
    try {
      const date = new Date(timestamp * 1000 || timestamp);
      return date.toLocaleString();
    } catch {
      return 'Unknown';
    }
  };

  return (
    <div className="collaboration-tab">
      <div className="collaboration-header">
        <h3>Collaboration</h3>
        {ipc.connected && <span className="status-badge">Connected</span>}
      </div>

      {/* Tab Navigation */}
      <div className="tab-navigation">
        <button
          className={`tab-button ${activeTab === 'comments' ? 'active' : ''}`}
          onClick={() => setActiveTab('comments')}
        >
          Comments ({comments.length})
        </button>
        <button
          className={`tab-button ${activeTab === 'versions' ? 'active' : ''}`}
          onClick={() => setActiveTab('versions')}
        >
          Versions ({versions.length})
        </button>
        <button
          className={`tab-button ${activeTab === 'changes' ? 'active' : ''}`}
          onClick={() => setActiveTab('changes')}
        >
          Changes ({changes.length})
        </button>
      </div>

      {/* Content Area */}
      <div className="tab-content">
        {isLoading ? (
          <div className="loading">Loading collaboration data...</div>
        ) : activeTab === 'comments' ? (
          <CommentsPanel comments={comments} formatTime={formatTime} />
        ) : activeTab === 'versions' ? (
          <VersionsPanel versions={versions} formatTime={formatTime} />
        ) : (
          <ChangesPanel changes={changes} />
        )}
      </div>

      {/* Phase 26C Notice */}
      <div className="phase-notice">
        <div className="notice-icon">ℹ</div>
        <div className="notice-text">
          <strong>Phase 26C:</strong> Add/edit functionality coming soon
        </div>
      </div>

      <style jsx>{`
        .collaboration-tab {
          display: flex;
          flex-direction: column;
          height: 100%;
          background: white;
        }

        .collaboration-header {
          display: flex;
          justify-content: space-between;
          align-items: center;
          padding: 12px 16px;
          border-bottom: 1px solid #e0e0e0;
        }

        .collaboration-header h3 {
          margin: 0;
          font-size: 16px;
          font-weight: 600;
        }

        .status-badge {
          font-size: 11px;
          padding: 2px 6px;
          background: #2196F3;
          color: white;
          border-radius: 3px;
          font-weight: 500;
        }

        .tab-navigation {
          display: flex;
          border-bottom: 1px solid #e0e0e0;
          padding: 0 8px;
        }

        .tab-button {
          flex: 1;
          padding: 10px 8px;
          background: none;
          border: none;
          border-bottom: 2px solid transparent;
          font-size: 12px;
          font-weight: 500;
          color: #666;
          cursor: pointer;
          transition: all 0.2s;
        }

        .tab-button:hover {
          color: #333;
        }

        .tab-button.active {
          color: #2196F3;
          border-bottom-color: #2196F3;
        }

        .tab-content {
          flex: 1;
          overflow-y: auto;
          padding: 8px 12px;
        }

        .loading {
          padding: 40px 20px;
          text-align: center;
          color: #999;
          font-size: 14px;
        }

        .phase-notice {
          display: flex;
          gap: 8px;
          padding: 10px 12px;
          background: #e3f2fd;
          border-top: 1px solid #bbdefb;
          border-bottom-left-radius: 4px;
          border-bottom-right-radius: 4px;
          font-size: 12px;
          color: #1565c0;
        }

        .notice-icon {
          font-weight: bold;
          min-width: 16px;
        }

        .notice-text {
          line-height: 1.4;
        }
      `}</style>
    </div>
  );
};

// ========== Comments Panel ==========

interface CommentsPanelProps {
  comments: Comment[];
  formatTime: (timestamp: number) => string;
}

const CommentsPanel: React.FC<CommentsPanelProps> = ({ comments, formatTime }) => {
  if (comments.length === 0) {
    return (
      <div className="empty-state">
        <div className="empty-icon">💬</div>
        <p>No comments yet</p>
      </div>
    );
  }

  return (
    <div className="comments-list">
      {comments.map((comment) => (
        <div key={comment.id} className="comment-item">
          <div className="comment-header">
            <span className="comment-author">{comment.author}</span>
            <span className="comment-time">{formatTime(comment.timestamp)}</span>
          </div>
          <div className="comment-text">{comment.text}</div>
        </div>
      ))}
    </div>
  );
};

// ========== Versions Panel ==========

interface VersionsPanelProps {
  versions: Version[];
  formatTime: (timestamp: number) => string;
}

const VersionsPanel: React.FC<VersionsPanelProps> = ({ versions, formatTime }) => {
  if (versions.length === 0) {
    return (
      <div className="empty-state">
        <div className="empty-icon">📦</div>
        <p>No versions available</p>
      </div>
    );
  }

  return (
    <div className="versions-list">
      {versions.map((version) => (
        <div key={version.id} className="version-item">
          <div className="version-header">
            <span className="version-id">{version.id}</span>
            <span className="version-author">by {version.author}</span>
          </div>
          <div className="version-date">{formatTime(version.timestamp)}</div>
          <div className="version-changes">{version.changes}</div>
        </div>
      ))}
    </div>
  );
};

// ========== Changes Panel ==========

interface ChangesPanelProps {
  changes: string[];
}

const ChangesPanel: React.FC<ChangesPanelProps> = ({ changes }) => {
  if (changes.length === 0) {
    return (
      <div className="empty-state">
        <div className="empty-icon">✓</div>
        <p>No recent changes</p>
      </div>
    );
  }

  return (
    <div className="changes-list">
      {changes.map((change, index) => (
        <div key={index} className="change-item">
          <span className="change-icon">•</span>
          <span className="change-text">{change}</span>
        </div>
      ))}
    </div>
  );
};

// ========== Styles for sub-components ==========

const subComponentStyles = `
  .empty-state {
    padding: 40px 20px;
    text-align: center;
    color: #999;
  }

  .empty-icon {
    font-size: 32px;
    margin-bottom: 10px;
  }

  .empty-state p {
    margin: 0;
    font-size: 14px;
  }

  .comments-list {
    display: flex;
    flex-direction: column;
    gap: 12px;
  }

  .comment-item {
    padding: 10px;
    border: 1px solid #e0e0e0;
    border-radius: 4px;
    background: #fafafa;
  }

  .comment-header {
    display: flex;
    justify-content: space-between;
    margin-bottom: 6px;
  }

  .comment-author {
    font-weight: 600;
    font-size: 13px;
    color: #333;
  }

  .comment-time {
    font-size: 11px;
    color: #999;
  }

  .comment-text {
    font-size: 13px;
    color: #666;
    line-height: 1.4;
  }

  .versions-list {
    display: flex;
    flex-direction: column;
    gap: 10px;
  }

  .version-item {
    padding: 10px;
    border: 1px solid #e0e0e0;
    border-radius: 4px;
    background: #fafafa;
  }

  .version-header {
    display: flex;
    justify-content: space-between;
    margin-bottom: 4px;
  }

  .version-id {
    font-family: monospace;
    font-size: 12px;
    color: #333;
    font-weight: 600;
  }

  .version-author {
    font-size: 12px;
    color: #999;
  }

  .version-date {
    font-size: 11px;
    color: #999;
    margin-bottom: 4px;
  }

  .version-changes {
    font-size: 12px;
    color: #666;
  }

  .changes-list {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }

  .change-item {
    display: flex;
    gap: 8px;
    padding: 6px 0;
    font-size: 13px;
    color: #666;
  }

  .change-icon {
    color: #4CAF50;
    font-weight: bold;
    min-width: 12px;
  }

  .change-text {
    line-height: 1.3;
  }
`;
