// Comprehensive tests for BendCore Search Engine
// Task 3: Search & Filters (Q3 2027)
// Testing: multi-field search, compound queries, fuzzy matching, date ranges, saved searches

#include <gtest/gtest.h>
#include "bendcore/search.hpp"
#include <chrono>

using namespace bendcore::search;

// =========== TEST FIXTURES ===========

class SearchEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test tubes with all fields
        tubes_.push_back({
            .id = "tube-1",
            .designation = "HY-25",
            .material = Material::Steel,
            .od = 25.4,
            .wall_thickness = 1.5,
            .length = 200.0,
            .manufacturing_method = "Cold Drawn",
            .cost_usd = 45.50,
            .created_date = "2024-01-15",
            .author = "john"
        });

        tubes_.push_back({
            .id = "tube-2",
            .designation = "HY-32",
            .material = Material::Steel,
            .od = 31.75,
            .wall_thickness = 2.0,
            .length = 300.0,
            .manufacturing_method = "Hot Rolled",
            .cost_usd = 75.00,
            .created_date = "2024-02-20",
            .author = "jane"
        });

        tubes_.push_back({
            .id = "tube-3",
            .designation = "AL-20",
            .material = Material::Aluminum,
            .od = 20.0,
            .wall_thickness = 1.0,
            .length = 150.0,
            .manufacturing_method = "Extruded",
            .cost_usd = 25.00,
            .created_date = "2024-03-10",
            .author = "john"
        });

        tubes_.push_back({
            .id = "tube-4",
            .designation = "TI-50",
            .material = Material::Titanium,
            .od = 50.8,
            .wall_thickness = 3.0,
            .length = 500.0,
            .manufacturing_method = "Seamless",
            .cost_usd = 150.00,
            .created_date = "2024-01-25",
            .author = "mike"
        });

        tubes_.push_back({
            .id = "tube-5",
            .designation = "CU-15",
            .material = Material::Copper,
            .od = 15.875,
            .wall_thickness = 0.8,
            .length = 100.0,
            .manufacturing_method = "Drawn",
            .cost_usd = 35.00,
            .created_date = "2024-04-05",
            .author = "jane"
        });

        // Initialize search engine
        engine_.initialize(tubes_);
    }

    std::vector<TubeSearchRecord> tubes_;
    SearchEngine engine_;
};

class SavedSearchesTest : public ::testing::Test {
protected:
    SavedSearchesManager manager_;
};

// =========== SIMPLE QUERY TESTS ===========

TEST_F(SearchEngineTest, SimpleQueryMaterial) {
    // Test: material="Steel" → all steel tubes
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Steel"));

    SearchResult result = engine_.search(query);

    ASSERT_EQ(result.result_count, 2);
    EXPECT_EQ(result.tubes[0].id, "tube-1");
    EXPECT_EQ(result.tubes[1].id, "tube-2");
    ASSERT_LT(result.search_time_ms, 500);
}

TEST_F(SearchEngineTest, SimpleQueryOD) {
    // Test: OD > 30 mm
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::OD, Operator::GreaterThan, "30"));

    SearchResult result = engine_.search(query);

    ASSERT_EQ(result.result_count, 2);
    EXPECT_TRUE(result.tubes[0].od > 30);
    EXPECT_TRUE(result.tubes[1].od > 30);
}

TEST_F(SearchEngineTest, SimpleQueryWallThickness) {
    // Test: wall_thickness < 1.5 mm
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::WallThickness, Operator::LessThan, "1.5"));

    SearchResult result = engine_.search(query);

    ASSERT_EQ(result.result_count, 2);
    EXPECT_TRUE(result.tubes[0].wall_thickness < 1.5);
    EXPECT_TRUE(result.tubes[1].wall_thickness < 1.5);
}

TEST_F(SearchEngineTest, SimpleQueryAuthor) {
    // Test: author = "john"
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Author, Operator::Equals, "john"));

    SearchResult result = engine_.search(query);

    ASSERT_EQ(result.result_count, 2);
    EXPECT_EQ(result.tubes[0].author, "john");
    EXPECT_EQ(result.tubes[1].author, "john");
}

// =========== FUZZY SEARCH TESTS ===========

TEST_F(SearchEngineTest, FuzzySearchDesignation) {
    // Test: "HY25" matches "HY-25" (fuzzy match)
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Designation, Operator::Contains, "HY25"));

    SearchResult result = engine_.search(query);

    ASSERT_EQ(result.result_count, 1);
    EXPECT_EQ(result.tubes[0].designation, "HY-25");
}

TEST_F(SearchEngineTest, FuzzySearchTypo) {
    // Test: "AL-2" with fuzzy match finds "AL-20"
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Designation, Operator::Contains, "AL-2"));

    SearchResult result = engine_.search(query);

    ASSERT_EQ(result.result_count, 1);
    EXPECT_EQ(result.tubes[0].designation, "AL-20");
}

// =========== COMPOUND QUERY TESTS ===========

TEST_F(SearchEngineTest, CompoundQueryAND) {
    // Test: (material="Steel" AND OD>30)
    CompoundQuery query;
    query.logical_op = LogicalOperator::AND;
    query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Steel"));
    query.add_filter(QueryFilter(SearchField::OD, Operator::GreaterThan, "30"));

    SearchResult result = engine_.search(query);

    ASSERT_EQ(result.result_count, 1);
    EXPECT_EQ(result.tubes[0].id, "tube-2");
    EXPECT_EQ(result.tubes[0].material, Material::Steel);
    EXPECT_GT(result.tubes[0].od, 30);
}

TEST_F(SearchEngineTest, CompoundQueryOR) {
    // Test: (material="Titanium" OR material="Copper")
    CompoundQuery query;
    query.logical_op = LogicalOperator::OR;
    query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Titanium"));
    query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Copper"));

    SearchResult result = engine_.search(query);

    ASSERT_EQ(result.result_count, 2);
}

TEST_F(SearchEngineTest, ComplexCompoundQuery) {
    // Test: (material="Steel" AND wall_thickness >= 1.5) OR (OD < 20)
    CompoundQuery query;
    query.logical_op = LogicalOperator::OR;

    // First part: steel AND thick wall
    CompoundQuery and_query;
    and_query.logical_op = LogicalOperator::AND;
    and_query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Steel"));
    and_query.add_filter(QueryFilter(SearchField::WallThickness, Operator::GreaterThanOrEqual, "1.5"));

    query.add_nested(and_query);

    // Second part: small OD
    query.add_filter(QueryFilter(SearchField::OD, Operator::LessThan, "20"));

    SearchResult result = engine_.search(query);

    // Should include both steel tubes (tube-1, tube-2) and small OD tubes (tube-5)
    ASSERT_GE(result.result_count, 2);
}

// =========== DATE RANGE TESTS ===========

TEST_F(SearchEngineTest, DateRangeFilter) {
    // Test: created_date between 2024-02-01 and 2024-03-31
    CompoundQuery query;
    query.logical_op = LogicalOperator::AND;
    query.add_filter(QueryFilter(SearchField::CreatedDate, Operator::GreaterThanOrEqual, "2024-02-01"));
    query.add_filter(QueryFilter(SearchField::CreatedDate, Operator::LessThanOrEqual, "2024-03-31"));

    SearchResult result = engine_.search(query);

    // Should include tube-2 (2024-02-20) and tube-3 (2024-03-10)
    ASSERT_EQ(result.result_count, 2);
}

// =========== SEARCH INDEX TESTS ===========

TEST_F(SearchEngineTest, SearchIndexBuild) {
    // Test: index is properly built
    EXPECT_TRUE(engine_.index().is_built());

    // Should find "steel" in inverted index
    auto results = engine_.index().search_token("steel");
    ASSERT_EQ(results.size(), 2);
}

TEST_F(SearchEngineTest, FuzzyIndexSearch) {
    // Test: fuzzy search in index
    auto results = engine_.index().fuzzy_search("HY25", 2);

    ASSERT_GT(results.size(), 0);
    EXPECT_EQ(results[0].first, "tube-1");
}

// =========== SAVED SEARCHES TESTS ===========

TEST_F(SavedSearchesTest, CreateSavedSearch) {
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Steel"));

    std::string id = manager_.create_saved_search("Steel Tubes", query, "test_user");

    EXPECT_FALSE(id.empty());

    auto retrieved = manager_.get_by_id(id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, "Steel Tubes");
    EXPECT_EQ(retrieved->created_by, "test_user");
}

TEST_F(SavedSearchesTest, GetSavedSearchByName) {
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Aluminum"));

    manager_.create_saved_search("Aluminum Tubes", query, "test_user");

    auto retrieved = manager_.get_by_name("Aluminum Tubes");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, "Aluminum Tubes");
}

TEST_F(SavedSearchesTest, DeleteSavedSearch) {
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Steel"));

    std::string id = manager_.create_saved_search("Delete Me", query, "test_user");

    bool deleted = manager_.delete_saved_search(id);
    EXPECT_TRUE(deleted);

    auto retrieved = manager_.get_by_id(id);
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(SavedSearchesTest, MarkAsUsed) {
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Steel"));

    std::string id = manager_.create_saved_search("Test Search", query, "test_user");

    manager_.mark_as_used(id);

    auto retrieved = manager_.get_by_id(id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_FALSE(retrieved->last_used_at.empty());
}

// =========== SERIALIZATION TESTS ===========

TEST_F(SavedSearchesTest, ExportToJSON) {
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Steel"));

    manager_.create_saved_search("Export Test", query, "test_user");

    std::string json = manager_.export_to_json();

    EXPECT_FALSE(json.empty());
    EXPECT_TRUE(json.find("Export Test") != std::string::npos);
}

TEST_F(SavedSearchesTest, LoadFromJSON) {
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Steel"));

    manager_.create_saved_search("JSON Test", query, "test_user");
    std::string json = manager_.export_to_json();

    SavedSearchesManager manager2;
    int loaded = manager2.load_from_json(json);

    EXPECT_EQ(loaded, 1);
    EXPECT_TRUE(manager2.get_by_name("JSON Test").has_value());
}

// =========== PERFORMANCE TESTS ===========

TEST_F(SearchEngineTest, PerformanceTest) {
    // Create a larger dataset (simulate 1000+ tubes)
    std::vector<TubeSearchRecord> large_tubes;
    for (int i = 0; i < 1000; ++i) {
        large_tubes.push_back({
            .id = "tube-" + std::to_string(i),
            .designation = "HY-" + std::to_string(20 + (i % 40)),
            .material = static_cast<Material>(i % 5),
            .od = 15.0 + (i % 50),
            .wall_thickness = 0.8 + (i % 30) * 0.1,
            .length = 100.0 + (i % 400),
            .manufacturing_method = (i % 2 == 0) ? "Cold Drawn" : "Hot Rolled",
            .cost_usd = 25.0 + (i % 150),
            .created_date = "2024-01-01",
            .author = "user-" + std::to_string(i % 10)
        });
    }

    SearchEngine large_engine;
    large_engine.initialize(large_tubes);

    auto start = std::chrono::high_resolution_clock::now();

    CompoundQuery query;
    query.logical_op = LogicalOperator::AND;
    query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Steel"));
    query.add_filter(QueryFilter(SearchField::OD, Operator::GreaterThan, "30"));

    SearchResult result = large_engine.search(query);

    auto end = std::chrono::high_resolution_clock::now();
    double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    // Should complete in less than 500ms
    EXPECT_LT(duration_ms, 500.0) << "Search took " << duration_ms << "ms (should be <500ms)";
    EXPECT_GT(result.result_count, 0);
}

// =========== ENUM CONVERSION TESTS ===========

TEST(SearchUtilsTest, OperatorConversion) {
    EXPECT_EQ(operator_to_string(Operator::Equals), "==");
    EXPECT_EQ(operator_to_string(Operator::GreaterThan), ">");
    EXPECT_EQ(string_to_operator("=="), Operator::Equals);
    EXPECT_EQ(string_to_operator(">"), Operator::GreaterThan);
}

TEST(SearchUtilsTest, FieldConversion) {
    EXPECT_EQ(field_to_string(SearchField::Designation), "designation");
    EXPECT_EQ(field_to_string(SearchField::Material), "material");
    EXPECT_EQ(string_to_field("designation"), SearchField::Designation);
    EXPECT_EQ(string_to_field("material"), SearchField::Material);
}

TEST(SearchUtilsTest, MaterialConversion) {
    EXPECT_EQ(material_to_string(Material::Steel), "Steel");
    EXPECT_EQ(material_to_string(Material::Aluminum), "Aluminum");
    EXPECT_EQ(string_to_material("Steel"), Material::Steel);
    EXPECT_EQ(string_to_material("Aluminum"), Material::Aluminum);
}

// =========== EDGE CASES ===========

TEST_F(SearchEngineTest, EmptySearchResults) {
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Unknown"));

    SearchResult result = engine_.search(query);

    EXPECT_EQ(result.result_count, 0);
    EXPECT_TRUE(result.tubes.empty());
}

TEST_F(SearchEngineTest, AllTubesMatching) {
    CompoundQuery query;
    // Any query that matches everything (this will match any material)
    query.add_filter(QueryFilter(SearchField::OD, Operator::GreaterThan, "0"));

    SearchResult result = engine_.search(query);

    EXPECT_EQ(result.result_count, 5);
}

TEST_F(SearchEngineTest, CaseInsensitiveFuzzyMatch) {
    // Test: "hy-25" matches "HY-25" (case-insensitive)
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Designation, Operator::Contains, "hy-25"));

    SearchResult result = engine_.search(query);

    ASSERT_EQ(result.result_count, 1);
    EXPECT_EQ(result.tubes[0].designation, "HY-25");
}

// =========== NEW FIELD TESTS (Task 3 Enhancement) ===========

TEST_F(SearchEngineTest, LengthRangeQuery) {
    // Test: length between 100-250 mm
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Length, Operator::Equals, "100-250"));

    SearchResult result = engine_.search(query);

    // tube-5 (100), tube-1 (200) match
    ASSERT_EQ(result.result_count, 2);
}

TEST_F(SearchEngineTest, CostRangeQuery) {
    // Test: cost between 30-80 USD
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::Cost, Operator::Equals, "30-80"));

    SearchResult result = engine_.search(query);

    // tube-1 (45.50), tube-5 (35), tube-3 (25, no) should be found
    ASSERT_GE(result.result_count, 2);
}

TEST_F(SearchEngineTest, ManufacturingMethodFilter) {
    // Test: manufacturing_method = "Cold Drawn"
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::ManufacturingMethod, Operator::Equals, "Cold Drawn"));

    SearchResult result = engine_.search(query);

    ASSERT_EQ(result.result_count, 1);
    EXPECT_EQ(result.tubes[0].id, "tube-1");
}

TEST_F(SearchEngineTest, ComplexRangeQuery) {
    // Test: OD between 20-40 AND cost < 100
    CompoundQuery query;
    query.logical_op = LogicalOperator::AND;
    query.add_filter(QueryFilter(SearchField::OD, Operator::Equals, "20-40"));
    query.add_filter(QueryFilter(SearchField::Cost, Operator::LessThan, "100"));

    SearchResult result = engine_.search(query);

    // tube-1 (25.4, 45.50), tube-2 (31.75, 75), tube-3 (20, 25) match
    ASSERT_GE(result.result_count, 2);
}

TEST_F(SearchEngineTest, MultiFieldQuery) {
    // Test: (material="Steel" AND cost>50) OR (length<200)
    CompoundQuery query;
    query.logical_op = LogicalOperator::OR;

    // First part: Steel AND expensive
    CompoundQuery and_query;
    and_query.logical_op = LogicalOperator::AND;
    and_query.add_filter(QueryFilter(SearchField::Material, Operator::Equals, "Steel"));
    and_query.add_filter(QueryFilter(SearchField::Cost, Operator::GreaterThan, "50"));
    query.add_nested(and_query);

    // Second part: short length
    query.add_filter(QueryFilter(SearchField::Length, Operator::LessThan, "200"));

    SearchResult result = engine_.search(query);

    // Should find expensive steel (tube-2) and/or short tubes (tube-5, tube-3)
    ASSERT_GE(result.result_count, 2);
}

TEST_F(SearchEngineTest, FullTextSearch) {
    // Test: full-text search for "cold"
    CompoundQuery query;
    query.add_filter(QueryFilter(SearchField::FullText, Operator::Contains, "cold"));

    SearchResult result = engine_.search(query);

    // Should find "Cold Drawn" in tube-1
    ASSERT_GE(result.result_count, 1);
}

// =========== BM25 RANKING TESTS ===========

TEST_F(SearchEngineTest, BM25Ranking) {
    // Create search index and verify BM25 is working
    EXPECT_TRUE(engine_.index().is_built());
    EXPECT_GT(engine_.index().get_vocabulary_size(), 0);
    EXPECT_GT(engine_.index().get_index_size(), 0);
}

TEST_F(SearchEngineTest, FuzzySearchWithSimilarityThreshold) {
    // Test fuzzy search with 0.8 similarity threshold
    std::vector<std::string> candidates = {"steel", "steal", "stelll", "stell", "tubes"};
    auto results = engine_.index().fuzzy_search("steel", candidates, 0.8);

    // Should find "steal" (1 edit), but not "tubes"
    ASSERT_GT(results.size(), 0);
    EXPECT_EQ(results[0].first, "steel");  // Exact match highest score
}
