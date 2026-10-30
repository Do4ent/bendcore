import React, { useState, useEffect } from 'react';

/**
 * RecentFilesManager Component - GAP 081
 * Recent files list and management
 */

interface RecentFile {
  id: string;
  name: string;
  path: string;
  modified: Date;
  pinned: boolean;
}

export interface RecentFilesManagerProps {
  isOpen?: boolean;
  onClose?: () => void;
  onSelect?: (path: string) => void;
  maxFiles?: number;
}

const RecentFilesManager: React.FC<RecentFilesManagerProps> = ({
  isOpen = true,
  onClose,
  onSelect,
  maxFiles = 50
}) => {
  const [files, setFiles] = useState<RecentFile[]>([]);
  const [sortBy, setSortBy] = useState<'date' | 'name' | 'pinned'>('date');

  useEffect(() => {
    const stored = localStorage.getItem('bendcore:recentFiles');
    if (stored) {
      setFiles(JSON.parse(stored));
    }
  }, []);

  const togglePin = (fileId: string) => {
    const updated = files.map(f =>
      f.id === fileId ? { ...f, pinned: !f.pinned } : f
    );
    setFiles(updated);
    localStorage.setItem('bendcore:recentFiles', JSON.stringify(updated));
  };

  const removeFile = (fileId: string) => {
    const updated = files.filter(f => f.id !== fileId);
    setFiles(updated);
    localStorage.setItem('bendcore:recentFiles', JSON.stringify(updated));
  };

  const clearHistory = () => {
    if (window.confirm('Clear all recent files?')) {
      setFiles([]);
      localStorage.removeItem('bendcore:recentFiles');
    }
  };

  const sortedFiles = [...files].sort((a, b) => {
    if (sortBy === 'pinned') {
      return (b.pinned ? 1 : 0) - (a.pinned ? 1 : 0);
    } else if (sortBy === 'date') {
      return new Date(b.modified).getTime() - new Date(a.modified).getTime();
    } else {
      return a.name.localeCompare(b.name);
    }
  });

  return (
    <div className="recent-files-manager">
      <div className="manager-header">
        <h3>Recent Files</h3>
        <div className="manager-controls">
          <select value={sortBy} onChange={(e) => setSortBy(e.target.value as any)}>
            <option value="date">Sort by Date</option>
            <option value="name">Sort by Name</option>
            <option value="pinned">Pinned First</option>
          </select>
          <button onClick={clearHistory} className="btn btn-small btn-danger">
            Clear History
          </button>
        </div>
      </div>

      <div className="files-list">
        {sortedFiles.length === 0 ? (
          <div className="empty-state">No recent files</div>
        ) : (
          sortedFiles.map(file => (
            <div key={file.id} className="file-entry">
              <button
                className="file-content"
                onClick={() => onSelect && onSelect(file.path)}
              >
                <span className="file-icon">📄</span>
                <div className="file-details">
                  <div className="file-name">{file.name}</div>
                  <div className="file-path">{file.path}</div>
                  <div className="file-modified">
                    Modified: {new Date(file.modified).toLocaleDateString()}
                  </div>
                </div>
              </button>
              <button
                className={`pin-btn ${file.pinned ? 'pinned' : ''}`}
                onClick={() => togglePin(file.id)}
                title={file.pinned ? 'Unpin' : 'Pin to top'}
              >
                📌
              </button>
              <button
                className="remove-btn"
                onClick={() => removeFile(file.id)}
                title="Remove from list"
              >
                ✕
              </button>
            </div>
          ))
        )}
      </div>

      {onClose && (
        <div className="manager-footer">
          <button className="btn btn-primary" onClick={onClose}>
            Close
          </button>
        </div>
      )}
    </div>
  );
};

export default RecentFilesManager;
