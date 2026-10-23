/**
 * Template Library Marketplace UI Component
 * Q4 Task 2: JSON Template System
 * Поиск, фильтрация и установка шаблонов из маркетплейса
 */

import React, { useState, useEffect } from 'react';
import './TemplateLibraryMarketplace.css';

// ============ TYPES ============

interface TemplateMetadata {
  author: string;
  created_at: string;
  updated_at: string;
  category: string;
  popularity: number;
  tags: string[];
}

interface TemplateConstraints {
  outer_diameter: { min: number; max: number };
  wall_thickness: { min: number; max: number };
  bend_radius_ratio: { min: number; max: number };
  max_bend_angle: number;
  max_consecutive_bends: number;
  min_bend_spacing: number;
}

interface MarketplaceTemplate {
  template_data: {
    id: string;
    name: string;
    description: string;
    metadata: TemplateMetadata;
    constraints: TemplateConstraints;
  };
  download_count: number;
  average_rating: number;
  status: string;
  published_by: string;
  reviews: string[];
}

// ============ COMPONENT ============

export const TemplateLibraryMarketplace: React.FC = () => {
  // === STATE ===
  const [templates, setTemplates] = useState<MarketplaceTemplate[]>([]);
  const [filteredTemplates, setFilteredTemplates] = useState<MarketplaceTemplate[]>([]);
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedCategory, setSelectedCategory] = useState('all');
  const [selectedView, setSelectedView] = useState<'grid' | 'list'>('grid');
  const [sortBy, setSortBy] = useState<'downloads' | 'rating' | 'recent'>('downloads');
  const [installedTemplates, setInstalledTemplates] = useState<Set<string>>(new Set());
  const [selectedTemplate, setSelectedTemplate] = useState<MarketplaceTemplate | null>(null);
  const [installProgress, setInstallProgress] = useState<Map<string, number>>(new Map());

  // === CATEGORIES ===
  const categories = ['all', 'standard', 'reinforced', 'custom', 'large-assembly'];

  // === EFFECTS ===
  useEffect(() => {
    // Загружаем шаблоны с сервера
    fetch('/api/marketplace/templates')
      .then((res) => res.json())
      .then((data) => {
        setTemplates(data.templates || []);
      })
      .catch((err) => console.error('Failed to load templates:', err));
  }, []);

  useEffect(() => {
    // Фильтруем и сортируем шаблоны
    let filtered = templates.filter((tmpl) => {
      const matchesSearch =
        tmpl.template_data.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
        tmpl.template_data.description.toLowerCase().includes(searchQuery.toLowerCase());

      const matchesCategory =
        selectedCategory === 'all' || tmpl.template_data.metadata.category === selectedCategory;

      return matchesSearch && matchesCategory;
    });

    // Сортируем
    filtered.sort((a, b) => {
      switch (sortBy) {
        case 'downloads':
          return b.download_count - a.download_count;
        case 'rating':
          return b.average_rating - a.average_rating;
        case 'recent':
          return (
            new Date(b.template_data.metadata.updated_at).getTime() -
            new Date(a.template_data.metadata.updated_at).getTime()
          );
      }
    });

    setFilteredTemplates(filtered);
  }, [templates, searchQuery, selectedCategory, sortBy]);

  // === HANDLERS ===
  const handleInstall = async (template: MarketplaceTemplate) => {
    const templateId = template.template_data.id;

    if (installedTemplates.has(templateId)) {
      return;  // Уже установлен
    }

    // Имитируем установку с прогрессом
    setInstallProgress((prev) => new Map(prev).set(templateId, 0));

    try {
      const response = await fetch(`/api/marketplace/templates/${templateId}/install`, {
        method: 'POST',
      });

      if (response.ok) {
        setInstalledTemplates((prev) => new Set(prev).add(templateId));
        setInstallProgress((prev) => {
          const map = new Map(prev);
          map.delete(templateId);
          return map;
        });
      }
    } catch (err) {
      console.error('Install failed:', err);
      setInstallProgress((prev) => {
        const map = new Map(prev);
        map.delete(templateId);
        return map;
      });
    }
  };

  const handleRate = async (template: MarketplaceTemplate, stars: number) => {
    try {
      await fetch(`/api/marketplace/templates/${template.template_data.id}/rate`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ stars }),
      });

      // Обновляем локально
      const updated = templates.map((t) =>
        t.template_data.id === template.template_data.id
          ? { ...t, average_rating: stars }
          : t
      );
      setTemplates(updated);
    } catch (err) {
      console.error('Rating failed:', err);
    }
  };

  const renderStars = (rating: number) => {
    return (
      <div className="stars">
        {[1, 2, 3, 4, 5].map((star) => (
          <span key={star} className={star <= Math.round(rating) ? 'star filled' : 'star'}>
            ★
          </span>
        ))}
        <span className="rating-text">{rating.toFixed(1)}</span>
      </div>
    );
  };

  const renderTemplateCard = (template: MarketplaceTemplate) => {
    const templateId = template.template_data.id;
    const isInstalled = installedTemplates.has(templateId);
    const progress = installProgress.get(templateId);

    return (
      <div
        key={templateId}
        className={`template-card ${selectedView}`}
        onClick={() => setSelectedTemplate(template)}
      >
        {/* HEADER */}
        <div className="card-header">
          <div className="title-section">
            <h3>{template.template_data.name}</h3>
            <span className="category-badge">{template.template_data.metadata.category}</span>
          </div>
          {template.average_rating > 0 && renderStars(template.average_rating)}
        </div>

        {/* DESCRIPTION */}
        <p className="description">{template.template_data.description}</p>

        {/* METADATA */}
        <div className="metadata">
          <span className="author">By {template.published_by}</span>
          <span className="downloads">⬇ {template.download_count}</span>
          <span className="views">👁 {template.template_data.metadata.popularity}</span>
        </div>

        {/* TAGS */}
        {template.template_data.metadata.tags.length > 0 && (
          <div className="tags">
            {template.template_data.metadata.tags.slice(0, 3).map((tag) => (
              <span key={tag} className="tag">
                #{tag}
              </span>
            ))}
          </div>
        )}

        {/* ACTION BUTTON */}
        <div className="actions">
          {isInstalled ? (
            <button className="btn btn-installed" disabled>
              ✓ Installed
            </button>
          ) : progress !== undefined ? (
            <button className="btn btn-installing" disabled>
              Installing... {progress}%
            </button>
          ) : (
            <button className="btn btn-install" onClick={() => handleInstall(template)}>
              Install
            </button>
          )}
        </div>
      </div>
    );
  };

  return (
    <div className="template-library-marketplace dark-mode">
      {/* === HEADER === */}
      <header className="header">
        <h1>BendCore Template Marketplace</h1>
        <p>Discover and install professionally-designed templates</p>
      </header>

      {/* === SEARCH & FILTER BAR === */}
      <div className="search-filter-bar">
        {/* SEARCH */}
        <div className="search-box">
          <input
            type="text"
            placeholder="Search templates..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="search-input"
          />
          <span className="search-icon">🔍</span>
        </div>

        {/* CONTROLS */}
        <div className="controls">
          {/* VIEW TOGGLE */}
          <div className="view-toggle">
            <button
              className={`view-btn ${selectedView === 'grid' ? 'active' : ''}`}
              onClick={() => setSelectedView('grid')}
              title="Grid view"
            >
              ⊞
            </button>
            <button
              className={`view-btn ${selectedView === 'list' ? 'active' : ''}`}
              onClick={() => setSelectedView('list')}
              title="List view"
            >
              ≡
            </button>
          </div>

          {/* SORT DROPDOWN */}
          <select
            value={sortBy}
            onChange={(e) => setSortBy(e.target.value as any)}
            className="sort-select"
          >
            <option value="downloads">Most Downloaded</option>
            <option value="rating">Highest Rated</option>
            <option value="recent">Recently Updated</option>
          </select>
        </div>
      </div>

      {/* === MAIN CONTENT === */}
      <div className="marketplace-container">
        {/* === SIDEBAR: CATEGORIES === */}
        <aside className="sidebar">
          <h3>Categories</h3>
          <nav className="categories">
            {categories.map((category) => (
              <button
                key={category}
                className={`category-btn ${selectedCategory === category ? 'active' : ''}`}
                onClick={() => setSelectedCategory(category)}
              >
                {category.charAt(0).toUpperCase() + category.slice(1)}
              </button>
            ))}
          </nav>
        </aside>

        {/* === MAIN GRID/LIST === */}
        <main className="templates-container">
          {filteredTemplates.length > 0 ? (
            <div className={`templates-${selectedView}`}>
              {filteredTemplates.map((template) => renderTemplateCard(template))}
            </div>
          ) : (
            <div className="no-results">
              <p>No templates found. Try adjusting your filters.</p>
            </div>
          )}
        </main>
      </div>

      {/* === DETAIL MODAL === */}
      {selectedTemplate && (
        <div className="modal-overlay" onClick={() => setSelectedTemplate(null)}>
          <div className="modal-content" onClick={(e) => e.stopPropagation()}>
            <button className="close-btn" onClick={() => setSelectedTemplate(null)}>
              ✕
            </button>

            <h2>{selectedTemplate.template_data.name}</h2>
            <p className="detail-description">{selectedTemplate.template_data.description}</p>

            {/* CONSTRAINTS */}
            <section className="constraints">
              <h4>Constraints</h4>
              <div className="constraint-grid">
                <div className="constraint">
                  <label>Outer Diameter</label>
                  <p>
                    {selectedTemplate.template_data.constraints.outer_diameter.min} -{' '}
                    {selectedTemplate.template_data.constraints.outer_diameter.max} mm
                  </p>
                </div>
                <div className="constraint">
                  <label>Wall Thickness</label>
                  <p>
                    {selectedTemplate.template_data.constraints.wall_thickness.min} -{' '}
                    {selectedTemplate.template_data.constraints.wall_thickness.max} mm
                  </p>
                </div>
                <div className="constraint">
                  <label>Bend Radius Ratio</label>
                  <p>
                    {selectedTemplate.template_data.constraints.bend_radius_ratio.min} -{' '}
                    {selectedTemplate.template_data.constraints.bend_radius_ratio.max}
                  </p>
                </div>
              </div>
            </section>

            {/* REVIEWS */}
            {selectedTemplate.reviews.length > 0 && (
              <section className="reviews">
                <h4>Reviews</h4>
                {selectedTemplate.reviews.map((review, idx) => (
                  <p key={idx} className="review">
                    {review}
                  </p>
                ))}
              </section>
            )}

            {/* ACTION FOOTER */}
            <div className="modal-actions">
              {installedTemplates.has(selectedTemplate.template_data.id) ? (
                <button className="btn btn-installed" disabled>
                  ✓ Already Installed
                </button>
              ) : (
                <button
                  className="btn btn-install-large"
                  onClick={() => handleInstall(selectedTemplate)}
                >
                  Install Template
                </button>
              )}
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default TemplateLibraryMarketplace;
