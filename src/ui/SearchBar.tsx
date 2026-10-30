/**
 * BendCore Command Search Bar
 * Search-as-you-type command finder with Ctrl+K activation
 * Part of Phase 31 foundational work
 */

import React, { useState, useCallback, useEffect, useRef } from "react";
import "./SearchBar.css";

export interface CommandSearchResult {
  id: string;
  name: string;
  category: string;
  tab: string;
  shortcut?: string;
  keywords: string[];
  iconColor: string;
  description: string;
}

export interface SearchBarProps {
  onCommand?: (commandId: string, args?: Record<string, any>) => void;
}

const SearchBar: React.FC<SearchBarProps> = ({ onCommand }) => {
  const [isOpen, setIsOpen] = useState(false);
  const [query, setQuery] = useState("");
  const [results, setResults] = useState<CommandSearchResult[]>([]);
  const [selectedIndex, setSelectedIndex] = useState(0);
  const inputRef = useRef<HTMLInputElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);

  // Listen for Ctrl+K shortcut
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && e.key === "k") {
        e.preventDefault();
        setIsOpen(true);
        setTimeout(() => inputRef.current?.focus(), 0);
      }
    };

    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, []);

  // Search commands
  const handleSearch = useCallback(
    async (searchQuery: string) => {
      setQuery(searchQuery);
      setSelectedIndex(0);

      if (!searchQuery.trim()) {
        setResults([]);
        return;
      }

      // Call backend search
      if (window.electron?.ipc) {
        try {
          const result = await window.electron.ipc.invoke(
            "bendcore:command",
            {
              command: "search-commands",
              args: { query: searchQuery, maxResults: 10 },
            }
          );

          const res = result as { success?: boolean; data?: { results?: unknown[] } } | null;
          if (res?.success && Array.isArray(res?.data?.results)) {
            setResults(res.data.results as string[]);
          }
        } catch (error) {
          console.error("Search error:", error);
        }
      }
    },
    []
  );

  // Handle result selection
  const handleSelectResult = useCallback(
    (result: CommandSearchResult) => {
      if (onCommand) {
        onCommand(result.id);
      }
      setIsOpen(false);
      setQuery("");
      setResults([]);
    },
    [onCommand]
  );

  // Handle keyboard navigation
  const handleKeyDown = useCallback(
    (e: React.KeyboardEvent<HTMLInputElement>) => {
      if (!isOpen) return;

      switch (e.key) {
        case "ArrowDown":
          e.preventDefault();
          setSelectedIndex((prev) =>
            Math.min(prev + 1, results.length - 1)
          );
          break;

        case "ArrowUp":
          e.preventDefault();
          setSelectedIndex((prev) => Math.max(prev - 1, 0));
          break;

        case "Enter":
          e.preventDefault();
          if (results.length > 0) {
            handleSelectResult(results[selectedIndex]);
          }
          break;

        case "Escape":
          e.preventDefault();
          setIsOpen(false);
          setQuery("");
          setResults([]);
          break;
      }
    },
    [isOpen, results, selectedIndex, handleSelectResult]
  );

  // Handle click outside
  useEffect(() => {
    const handleClickOutside = (event: MouseEvent) => {
      if (
        containerRef.current &&
        !containerRef.current.contains(event.target as Node)
      ) {
        setIsOpen(false);
      }
    };

    document.addEventListener("mousedown", handleClickOutside);
    return () => document.removeEventListener("mousedown", handleClickOutside);
  }, []);

  return (
    <div className="search-bar-container" ref={containerRef}>
      <div className="search-bar-input-wrapper">
        <input
          ref={inputRef}
          type="text"
          className="search-bar-input"
          placeholder="Search commands... (Ctrl+K)"
          value={query}
          onChange={(e) => handleSearch(e.target.value)}
          onKeyDown={handleKeyDown}
          onFocus={() => setIsOpen(true)}
        />
        {query && (
          <button
            className="search-bar-clear"
            onClick={() => {
              setQuery("");
              setResults([]);
              inputRef.current?.focus();
            }}
            title="Clear search"
          >
            ✕
          </button>
        )}
        <div className="search-bar-hint">Ctrl+K</div>
      </div>

      {/* Results Dropdown */}
      {isOpen && (
        <div className="search-results-dropdown">
          {results.length === 0 ? (
            <div className="search-results-empty">
              {query ? "No commands found" : "Start typing to search..."}
            </div>
          ) : (
            <div className="search-results-list">
              {results.map((result, index) => (
                <SearchResultItem
                  key={result.id}
                  result={result}
                  selected={index === selectedIndex}
                  onSelect={() => handleSelectResult(result)}
                />
              ))}
            </div>
          )}
        </div>
      )}
    </div>
  );
};

interface SearchResultItemProps {
  result: CommandSearchResult;
  selected: boolean;
  onSelect: () => void;
}

const SearchResultItem: React.FC<SearchResultItemProps> = ({
  result,
  selected,
  onSelect,
}) => {
  return (
    <div
      className={`search-result-item ${selected ? "selected" : ""}`}
      onClick={onSelect}
      title={result.description}
    >
      <div className="result-icon" style={{ backgroundColor: result.iconColor }}>
        {getCommandIcon(result.category)}
      </div>

      <div className="result-content">
        <div className="result-name">{result.name}</div>
        <div className="result-meta">
          <span className="result-tab">{result.tab}</span>
          <span className="result-category">{result.category}</span>
        </div>
      </div>

      {result.shortcut && (
        <div className="result-shortcut">{result.shortcut}</div>
      )}
    </div>
  );
};

function getCommandIcon(category: string): string {
  const icons: Record<string, string> = {
    create: "➕",
    edit: "✎",
    delete: "✗",
    import: "📥",
    export: "📤",
    save: "💾",
    open: "📂",
    analyze: "🔍",
    output: "📄",
    settings: "⚙️",
    auxiliary: "📐",
  };
  return icons[category] || "•";
}

export default SearchBar;
