# Import System API Reference

## C++ Classes

### STEPReader
```cpp
class bendcore::step::STEPReader {
public:
  ImportResult read(const std::string& file_path);
  static bool is_supported_format(const std::string& file_path);
};
```

### GeometryAnalyzer
```cpp
class bendcore::geometry::GeometryAnalyzer {
public:
  ExtractedParams analyze(const std::string& geometry_data);
  double fit_cylinder(const std::vector<std::pair<double, double>>& points);
};
```

### ParametricDimensionBuilder
```cpp
class bendcore::parametric::ParametricDimensionBuilder {
public:
  DimensionSet build(
    const ExtractedParams& params,
    const std::string& source_file,
    const std::string& author
  );
};
```

### GeometryCache
```cpp
class bendcore::cache::GeometryCache {
public:
  CacheKey add(const std::string& file, const std::string& shape, const ExtractedParams& params);
  std::optional<CacheEntry> get(const CacheKey& key);
  void invalidate(const CacheKey& key);
};
```

### ImportWorkflow
```cpp
class bendcore::import::ImportWorkflow {
public:
  ImportResult import(const ImportConfig& config);
  static std::string detect_format(const std::string& file_path);
};
```

## REST API Endpoints

### POST /api/bendcore/import-file
Import CAD file with multipart upload

**Request:**
- Content-Type: multipart/form-data
- Parameters: file, author, useCache, autoCreateFormulas

**Response:**
```json
{
  "success": true,
  "formulas": [...],
  "warnings": [],
  "execution_time_ms": 150
}
```

### POST /api/bendcore/detect-format
Detect file format

**Request:**
```json
{"extension": ".step"}
```

**Response:**
```json
{"format": "STEP", "supported": true}
```

### GET /api/bendcore/import-history
Get recent imports

**Response:**
```json
{"imports": [{"filename": "...", "timestamp": "...", "author": "..."}]}
```

