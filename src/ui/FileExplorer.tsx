import React, { useState } from 'react';

/**
 * FileExplorer Component - GAP 082
 * Project file browser and organizer
 */

interface FileSystemItem {
  id: string;
  name: string;
  type: 'file' | 'folder';
  path: string;
  size?: number;
  modified?: Date;
  children?: FileSystemItem[];
}

export interface FileExplorerProps {
  isOpen?: boolean;
  onClose?: () => void;
  onSelect?: (path: string) => void;
}

const FileExplorer: React.FC<FileExplorerProps> = ({
  isOpen = true,
  onClose,
  onSelect
}) => {
  const [currentPath, setCurrentPath] = useState('/home/projects');
  const [expandedFolders, setExpandedFolders] = useState<Set<string>>(new Set(['/home/projects']));
  const [sortBy, setSortBy] = useState<'name' | 'date' | 'size'>('name');
  const [searchQuery, setSearchQuery] = useState('');

  // Mock file system
  const mockFileSystem: FileSystemItem = {
    id: 'root',
    name: 'projects',
    type: 'folder',
    path: '/home/projects',
    children: [
      {
        id: 'f1',
        name: 'TubeBending',
        type: 'folder',
        path: '/home/projects/TubeBending',
        children: [
          { id: 'f1f1', name: 'main.bnd', type: 'file', path: '/home/projects/TubeBending/main.bnd', size: 1024 },
          { id: 'f1f2', name: 'design.bnd', type: 'file', path: '/home/projects/TubeBending/design.bnd', size: 2048 }
        ]
      },
      {
        id: 'f2',
        name: 'Assembly',
        type: 'folder',
        path: '/home/projects/Assembly',
        children: [
          { id: 'f2f1', name: 'assembly.bnd', type: 'file', path: '/home/projects/Assembly/assembly.bnd', size: 5120 }
        ]
      },
      { id: 'f3', name: 'reference.pdf', type: 'file', path: '/home/projects/reference.pdf', size: 2048000 }
    ]
  };

  const toggleFolderExpand = (id: string) => {
    const newExpanded = new Set(expandedFolders);
    if (newExpanded.has(id)) {
      newExpanded.delete(id);
    } else {
      newExpanded.add(id);
    }
    setExpandedFolders(newExpanded);
  };

  const handleFileSelect = (path: string) => {
    setCurrentPath(path);
    if (onSelect) onSelect(path);
  };

  const renderFileTree = (items: FileSystemItem[], depth = 0): JSX.Element[] => {
    return items
      .filter(item => !searchQuery || item.name.toLowerCase().includes(searchQuery.toLowerCase()))
      .sort((a, b) => {
        if (sortBy === 'name') return a.name.localeCompare(b.name);
        if (sortBy === 'date') return (b.modified?.getTime() || 0) - (a.modified?.getTime() || 0);
        if (sortBy === 'size') return (b.size || 0) - (a.size || 0);
        return 0;
      })
      .map(item => (
        <div key={item.id} style={{ paddingLeft: `${depth * 16}px` }}>
          <div className="tree-item">
            {item.type === 'folder' && (
              <button
                className="expand-btn"
                onClick={() => toggleFolderExpand(item.id)}
              >
                {expandedFolders.has(item.id) ? '▼' : '▶'}
              </button>
            )}
            <span className="item-icon">{item.type === 'folder' ? '📁' : '📄'}</span>
            <button
              className={`item-name ${currentPath === item.path ? 'selected' : ''}`}
              onClick={() => handleFileSelect(item.path)}
            >
              {item.name}
            </button>
            {item.size && <span className="item-size">({(item.size / 1024).toFixed(1)} KB)</span>}
          </div>
          {item.type === 'folder' && expandedFolders.has(item.id) && item.children && (
            renderFileTree(item.children, depth + 1)
          )}
        </div>
      ));
  };

  return (
    <div className="file-explorer">
      <div className="explorer-header">
        <h3>File Explorer</h3>
        {onClose && <button className="close-btn" onClick={onClose}>×</button>}
      </div>

      <div className="explorer-toolbar">
        <input
          type="text"
          placeholder="Search files..."
          value={searchQuery}
          onChange={(e) => setSearchQuery(e.target.value)}
          className="search-input"
        />
        <select value={sortBy} onChange={(e) => setSortBy(e.target.value as any)} className="sort-select">
          <option value="name">Sort by Name</option>
          <option value="date">Sort by Date</option>
          <option value="size">Sort by Size</option>
        </select>
      </div>

      <div className="explorer-path">
        <span className="path-label">Current path:</span>
        <input
          type="text"
          value={currentPath}
          onChange={(e) => setCurrentPath(e.target.value)}
          className="path-input"
        />
      </div>

      <div className="file-tree">
        {renderFileTree([mockFileSystem])}
      </div>

      {onClose && (
        <div className="explorer-footer">
          <button className="btn btn-primary" onClick={onClose}>
            Close
          </button>
        </div>
      )}
    </div>
  );
};

export default FileExplorer;
