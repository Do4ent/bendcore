# BendCore JSON Template System - Quick Start Guide

**Q4 Task 2 Implementation**  
**Быстрый старт для системы JSON шаблонов BendCore**

---

## 🚀 Quick Start (5 minutes)

### 1. Load Templates from Files

```cpp
#include "bendcore/json_template_loader.hpp"
using namespace bendcore;

// Load single template
auto template_opt = JSONTemplateLoader::load_from_file(
  "/path/to/std-90-deg-v2.json"
);

if (template_opt.has_value()) {
  JSONTemplate tmpl = template_opt.value();
  std::cout << "Loaded: " << tmpl.name << std::endl;
}

// Load all templates from directory
std::vector<JSONTemplate> all_templates = 
  JSONTemplateLoader::load_all_from_directory(
    "/home/claude/bendcore/data/templates"
  );

std::cout << "Loaded " << all_templates.size() << " templates" << std::endl;
```

### 2. Validate Templates

```cpp
// Validate a template
JSONTemplate tmpl = /* ... */;
ValidationResult result = JSONTemplateLoader::validate_template(tmpl);

if (result.is_valid) {
  std::cout << "✅ Template is valid" << std::endl;
} else {
  std::cout << "❌ Errors found:" << std::endl;
  for (const auto& error : result.errors) {
    std::cout << "  - " << error << std::endl;
  }
}

if (!result.warnings.empty()) {
  std::cout << "⚠️  Warnings:" << std::endl;
  for (const auto& warning : result.warnings) {
    std::cout << "  - " << warning << std::endl;
  }
}
```

### 3. Use Template Repository

```cpp
TemplateRepository repo;

// Load templates
repo.load_directory("/home/claude/bendcore/data/templates");

// Get template by ID
auto tmpl = repo.get_template("std-90-deg-v2");

// Get templates by category
auto standard_templates = repo.get_templates_by_category("standard");

// Search by tags
auto results = repo.search_by_tags({"90-degree", "standard"});

std::cout << "Found " << results.size() << " templates" << std::endl;
```

### 4. Publish to Marketplace

```cpp
#include "bendcore/template_marketplace.hpp"

TemplateMarketplace marketplace;

// Publish a template
JSONTemplate new_tmpl = /* ... */;
bool published = marketplace.publish_template(
  new_tmpl,
  "my-username"  // publisher ID
);

if (published) {
  std::cout << "✅ Template published!" << std::endl;
  // Need approval from moderator
}
```

### 5. Search Marketplace

```cpp
// Search by keyword
auto results = marketplace.search_templates("90 degree", 10);

std::cout << "Search results:" << std::endl;
for (const auto& tmpl : results) {
  std::cout << "- " << tmpl.template_data.name 
            << " (⭐ " << tmpl.average_rating << ", ⬇ " 
            << tmpl.download_count << ")" << std::endl;
}

// Get featured templates
auto featured = marketplace.get_featured_templates(5);

// Get trending templates
auto trending = marketplace.get_trending_templates(10);
```

### 6. Rate and Install

```cpp
// Rate a template
bool rated = marketplace.rate_template(
  "std-90-deg-v2",
  "user-id",
  5,                    // 1-5 stars
  "Excellent template!" // optional review
);

// Install template
bool installed = marketplace.install_template("std-90-deg-v2");

if (installed) {
  std::cout << "✅ Template installed to local library" << std::endl;
}

// Check if installed
if (marketplace.is_installed("std-90-deg-v2")) {
  std::cout << "Template is installed ✓" << std::endl;
}
```

### 7. Version Management

```cpp
#include "bendcore/template_versioning.hpp"

TemplateVersionManager version_mgr;

// Create new version
JSONTemplate v1 = /* ... */;
version_mgr.create_new_version(
  "my-template",
  v1,
  "Initial release",  // release notes
  "author-id"
);

// Get latest version
auto latest = version_mgr.get_latest_version("my-template");
if (latest.has_value()) {
  std::cout << "Latest version: " << latest.value().version << std::endl;
}

// Get version history
auto history = version_mgr.get_version_history("my-template");
for (const auto& [version, notes] : history) {
  std::cout << version << ": " << notes << std::endl;
}

// Deprecate old version
version_mgr.deprecate_version(
  "my-template",
  "1.0",
  "Use version 2.0 instead"
);

// Check deprecation warning
auto warning = version_mgr.get_deprecation_warning("my-template", "1.0");
if (warning.has_value()) {
  std::cout << "⚠️  " << warning.value() << std::endl;
}
```

---

## 📋 JSON Template Example

```json
{
  "version": "1.0",
  "id": "std-90-deg-v2",
  "name": "Standard 90° Bend v2",
  "description": "Improved standard tube bending template for 90 degree bends",
  
  "metadata": {
    "author": "engineering-team",
    "created_at": "2026-11-01T00:00:00Z",
    "updated_at": "2026-12-01T00:00:00Z",
    "category": "standard",
    "popularity": 92,
    "tags": ["90-degree", "standard", "popular"]
  },
  
  "constraints": {
    "outer_diameter": {"min": 6, "max": 100},
    "wall_thickness": {"min": 0.5, "max": 5},
    "bend_radius_ratio": {"min": 1.5, "max": 3.0},
    "max_bend_angle": 180,
    "max_consecutive_bends": 2,
    "min_bend_spacing": 10
  },
  
  "manufacturing": {
    "materials": ["Steel", "Aluminum"],
    "method": "3D CNC bending",
    "setup_time_min": 15,
    "cost_per_unit": 50,
    "min_batch_size": 10
  },
  
  "compatible_cad_formats": ["STEP", "IGES", "DXF"],
  "warranty_info": "12 months standard",
  "notes": [
    "Optimal for standard bends",
    "Fast turnaround time"
  ]
}
```

---

## 🎨 React Components

### Template Library Marketplace

```typescript
import { TemplateLibraryMarketplace } from './TemplateLibraryMarketplace';

function App() {
  return (
    <div>
      <TemplateLibraryMarketplace />
    </div>
  );
}
```

**Features**:
- Search templates
- Filter by category
- Sort by downloads, rating, or date
- Toggle grid/list view
- View template details
- Install templates
- Rate templates

### Template Creator

```typescript
import { TemplateCreator } from './TemplateCreator';

function App() {
  return (
    <div>
      <TemplateCreator />
    </div>
  );
}
```

**Features**:
- Step-by-step wizard
- Form validation
- JSON preview
- Save as draft
- Publish to marketplace

---

## 🌐 API Endpoints

### Get Templates

```bash
# Get all templates
curl http://localhost:3000/marketplace/templates

# Get with filters
curl 'http://localhost:3000/marketplace/templates?category=standard&sort=downloads'

# Get template details
curl http://localhost:3000/marketplace/templates/std-90-deg-v2
```

### Publish Template

```bash
curl -X POST http://localhost:3000/marketplace/templates \
  -H "Content-Type: application/json" \
  -d @template.json
```

### Rate Template

```bash
curl -X POST http://localhost:3000/marketplace/templates/std-90-deg-v2/rate \
  -H "Content-Type: application/json" \
  -d '{
    "stars": 5,
    "review": "Excellent template!"
  }'
```

### Install Template

```bash
curl -X POST http://localhost:3000/marketplace/templates/std-90-deg-v2/install
```

### Get Versions

```bash
curl http://localhost:3000/marketplace/templates/std-90-deg-v2/versions
```

---

## 📦 Files Location

### Headers
```
/home/claude/bendcore/include/bendcore/
├── json_template_format.hpp
├── json_template_loader.hpp
├── template_marketplace.hpp
└── template_versioning.hpp
```

### Implementations
```
/home/claude/bendcore/src/bendcore/
├── json_template_loader.cpp
├── template_marketplace.cpp
└── template_versioning.cpp
```

### Templates
```
/home/claude/bendcore/data/templates/
├── std-90-deg-v2.json
├── reinforced-45-deg-v1.json
└── schema.json
```

### React Components
```
/home/claude/bendcore/src/ui/
├── TemplateLibraryMarketplace.tsx
├── TemplateLibraryMarketplace.css
├── TemplateCreator.tsx
└── TemplateCreator.css
```

### Tests
```
/home/claude/bendcore/test/
└── test_json_template_system.cpp
```

---

## 🔍 Common Tasks

### Load and Display All Templates

```cpp
TemplateRepository repo;
repo.load_directory("/home/claude/bendcore/data/templates");

for (const auto& tmpl : repo.get_all_templates()) {
  std::cout << "Template: " << tmpl.name << std::endl;
  std::cout << "  Category: " << tmpl.metadata.category << std::endl;
  std::cout << "  Author: " << tmpl.metadata.author << std::endl;
  std::cout << "  OD Range: " << tmpl.constraints.outer_diameter.min 
            << "-" << tmpl.constraints.outer_diameter.max << " mm" << std::endl;
  std::cout << std::endl;
}
```

### Create Custom Template

```cpp
JSONTemplate custom;
custom.version = "1.0";
custom.id = "my-custom-template";
custom.name = "My Custom Bending Template";
custom.description = "A custom template for special applications";

custom.metadata.author = "my-team";
custom.metadata.category = "custom";
custom.metadata.created_at = "2026-10-23T12:00:00Z";
custom.metadata.updated_at = "2026-10-23T12:00:00Z";

custom.constraints.outer_diameter = {10, 80};
custom.constraints.wall_thickness = {1, 4};
custom.constraints.bend_radius_ratio = {2.0, 3.5};
custom.constraints.max_bend_angle = 180;
custom.constraints.max_consecutive_bends = 3;
custom.constraints.min_bend_spacing = 15;

custom.manufacturing.materials = {"Steel", "Aluminum", "Copper"};
custom.manufacturing.method = "CNC 5-axis bending";
custom.manufacturing.setup_time_min = 20;
custom.manufacturing.cost_per_unit = 75;
custom.manufacturing.min_batch_size = 5;

custom.compatible_cad_formats = {"STEP", "IGES", "DXF"};
custom.warranty_info = "12 months";
custom.notes = {"Custom template", "Special purpose"};

// Validate before publishing
ValidationResult result = JSONTemplateLoader::validate_template(custom);
if (result.is_valid) {
  // Publish to marketplace
  marketplace.publish_template(custom, "my-user-id");
}
```

### Migrate Template to New Version

```cpp
VersionManager mgr;

// Get old template
auto old_tmpl = repo.get_template("my-template");

// Migrate to new version
if (mgr.can_auto_migrate("1.0", "2.0")) {
  auto migrated = mgr.migrate_to_version(
    old_tmpl.value(),
    "1.0",
    "2.0"
  );
  
  if (migrated.has_value()) {
    std::cout << "✅ Migration successful!" << std::endl;
  }
}
```

---

## 📚 Documentation Links

- **Complete Implementation**: `Q4_TASK_2_IMPLEMENTATION_SUMMARY.md`
- **Unit Tests**: `test/test_json_template_system.cpp`
- **API Routes**: `src/api/template_marketplace_routes.ts`
- **React Components**: `src/ui/*.tsx`

---

## 💡 Tips & Best Practices

1. **Always validate** templates before publishing
2. **Use categories** to organize templates by type
3. **Add meaningful tags** for better searchability
4. **Keep notes** for usage hints and warnings
5. **Set reasonable costs** for manufacturing parameters
6. **Track versions** as you improve templates
7. **Deprecate old versions** with clear migration guides
8. **Test migrations** before deploying to production

---

## 🆘 Troubleshooting

### Template Not Loading?
- Check file path is correct
- Verify JSON is valid
- Check file permissions
- See validation errors

### Validation Failing?
- Check against schema in `data/templates/schema.json`
- Verify all required fields present
- Check numeric ranges
- Look at error messages

### Migration Issues?
- Check version compatibility
- Look for breaking changes
- Check migration guide
- Verify backward compatibility

---

## 📞 Support

For more information:
1. Read inline comments in header files
2. Check unit tests for usage examples
3. Review example JSON files
4. Check API endpoint implementations

**Статус**: ✅ ГОТОВО К ИСПОЛЬЗОВАНИЮ (READY TO USE)
