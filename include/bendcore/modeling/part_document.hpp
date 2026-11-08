#pragma once

// Документ детали: упорядоченное дерево операций с параметрами-выражениями, ссылками на
// топологию по именам (naming.hpp), состояниями, перестроением от изменённой операции,
// откатом, подавлением, переносом и сохранением в JSON (docs/modeling/PART_DOCUMENT.md).

#include <bendcore/modeling/features.hpp>
#include <bendcore/modeling/naming.hpp>
#include <bendcore/modeling/part_sketch.hpp>

#include <BRepTools_History.hxx>
#include <Standard_Handle.hxx>
#include <TopoDS_Shape.hxx>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace bendcore::modeling {

using Status = Result<std::monostate>;

enum class FeatureState {
    Ok,          // built, references resolved exactly or through history
    Warning,     // built, but with a remark (signature fallback, cut removed nothing, ...)
    Error,       // not built; the next features receive the last valid shape
    Suppressed,  // skipped by the user; the shape passes through
    RolledBack,  // after the rollback marker; not evaluated
};

const char* toString(FeatureState state) noexcept;

struct ParameterSpec {
    std::string name;
    std::string defaultExpression;
};

struct ReferenceSlot {
    std::string name;
    ElementKind kind = ElementKind::Edge;
    bool multiple = false;  // false: exactly one reference (zero allowed when optional)
    bool optional = false;  // true: the slot may stay empty (the feature decides by its options)
};

// Link to an earlier node of the tree: a sketch, a datum, the source feature of a pattern.
struct LinkSlot {
    std::string name;
    std::vector<std::string> types;  // accepted feature types; empty = any type
    bool optional = false;
};

struct OptionSpec {
    std::string name;
    std::vector<std::string> allowed;  // first value is the default
};

// What a feature returns: the new shape and how its elements relate to the input shape
// and to the feature's own elements (roles).
// A null shape means that the feature does not change the body (sketch, datum): it must then
// return a sketch or datum result, and the body passes through.
struct FeatureOutput {
    TopoDS_Shape shape;
    opencascade::handle<BRepTools_History> history;
    std::vector<ElementRole> roles;
    std::vector<std::string> warnings;
    // Further named inputs of the history besides the input body (the sketch profile faces,
    // the tool of a pattern source): their element names become history sources.
    std::vector<std::shared_ptr<const NamedShape>> namingInputs;
    // Tool body applied to the input (patterns and mirrors of this feature reuse it) and
    // whether it added or removed material.
    TopoDS_Shape tool;
    std::optional<FeatureMode> toolMode;
    std::shared_ptr<const SketchResult> sketch;
    std::shared_ptr<const DatumResult> datum;
};

// Result of an earlier node seen through a link.
struct LinkedFeature {
    std::string id;
    std::string type;
    std::shared_ptr<const NamedShape> output;      // body after the node (null for sketch/datum)
    TopoDS_Shape tool;
    std::optional<FeatureMode> toolMode;
    std::shared_ptr<const NamedShape> toolNamed;   // tool with element names "<id>:Tool.F<k>"
    std::shared_ptr<const SketchResult> sketch;
    std::shared_ptr<const DatumResult> datum;
};

// Everything a feature sees while executing.
class FeatureContext {
public:
    FeatureContext(std::string featureId, const NamedShape& input, std::map<std::string, double> parameters,
                   std::map<std::string, std::vector<TopoDS_Shape>> references,
                   std::map<std::string, std::string> options,
                   std::map<std::string, const LinkedFeature*> links = {}, const SketchData* sketch = nullptr,
                   std::vector<double> sketchValues = {})
        : featureId_(std::move(featureId)), input_(input), parameters_(std::move(parameters)),
          references_(std::move(references)), options_(std::move(options)), links_(std::move(links)),
          sketch_(sketch), sketchValues_(std::move(sketchValues)) {}

    const std::string& featureId() const noexcept { return featureId_; }
    // Shape produced by the last valid feature before this one (may be null).
    const NamedShape& input() const noexcept { return input_; }
    // Evaluated parameter in document units (mm, rad). Throws std::out_of_range for a name
    // that is not among the feature's parameterSpecs().
    double parameter(const std::string& name) const { return parameters_.at(name); }
    // Resolved elements of a reference slot (a split reference contributes all its parts).
    const std::vector<TopoDS_Shape>& references(const std::string& slot) const { return references_.at(slot); }
    const std::string& option(const std::string& name) const { return options_.at(name); }
    // Linked node of a link slot; nullptr for an empty optional slot.
    const LinkedFeature* link(const std::string& slot) const {
        auto it = links_.find(slot);
        return it == links_.end() ? nullptr : it->second;
    }
    // Sketch data of a sketch node (nullptr for other features) and the values of
    // sketchExpressions(*sketch()) in the same order.
    const SketchData* sketch() const noexcept { return sketch_; }
    const std::vector<double>& sketchValues() const noexcept { return sketchValues_; }

private:
    std::string featureId_;
    const NamedShape& input_;
    std::map<std::string, double> parameters_;
    std::map<std::string, std::vector<TopoDS_Shape>> references_;
    std::map<std::string, std::string> options_;
    std::map<std::string, const LinkedFeature*> links_;
    const SketchData* sketch_ = nullptr;
    std::vector<double> sketchValues_;
};

// A feature type: stateless description plus the geometric operation. The document stores
// the per-instance data (parameters, references, options).
class Feature {
public:
    virtual ~Feature() = default;
    virtual std::string type() const = 0;
    virtual std::vector<ParameterSpec> parameterSpecs() const = 0;
    virtual std::vector<ReferenceSlot> referenceSlots() const = 0;
    virtual std::vector<OptionSpec> optionSpecs() const = 0;
    // Links to earlier nodes; features without links keep the empty list.
    virtual std::vector<LinkSlot> linkSlots() const { return {}; }
    // true: the node carries SketchData (sketch features).
    virtual bool hasSketchData() const { return false; }
    virtual Result<FeatureOutput> execute(const FeatureContext& context) const = 0;
};

class FeatureRegistry {
public:
    using Factory = std::function<std::unique_ptr<Feature>()>;
    Status add(const std::string& type, Factory factory);
    bool contains(const std::string& type) const { return factories_.count(type) != 0; }
    std::unique_ptr<Feature> create(const std::string& type) const;
    std::vector<std::string> types() const;

private:
    std::map<std::string, Factory> factories_;
};

// Data needed to add a feature.
struct FeatureSpec {
    std::string type;
    std::string id;     // empty: generated as <type><NNN>
    std::string label;  // empty: equal to the id
    std::map<std::string, std::string> parameters;  // expressions; missing ones take defaults
    std::map<std::string, std::vector<TopoReference>> references;
    std::map<std::string, std::string> options;     // missing ones take defaults
    bool suppressed = false;
    std::map<std::string, std::string> links;       // slot -> id of an earlier feature
    std::optional<SketchData> sketch;               // sketch features only
};

struct ReferenceReport {
    std::string slot;
    TopoReference reference;  // as stored before this rebuild
    Resolution resolution;
};

// Read-only view of a feature node.
struct FeatureInfo {
    std::string id;
    std::string type;
    std::string label;
    bool suppressed = false;
    FeatureState state = FeatureState::RolledBack;
    std::vector<std::string> messages;
    std::map<std::string, std::string> parameters;  // expressions
    std::map<std::string, double> values;           // evaluated at the last rebuild
    std::map<std::string, std::vector<TopoReference>> references;
    std::map<std::string, std::string> options;
    std::vector<ReferenceReport> referenceReports;
    std::shared_ptr<const NamedShape> output;       // null unless Ok/Warning; null for sketch/datum
    std::map<std::string, std::string> links;
    std::optional<SketchData> sketch;
    std::shared_ptr<const SketchResult> sketchResult;
    std::shared_ptr<const DatumResult> datum;
    TopoDS_Shape tool;
};

struct RebuildReport {
    std::vector<std::string> recomputed;  // executed in this rebuild
    std::vector<std::string> reused;      // inputs unchanged, previous result kept
    std::vector<std::string> fromCache;   // taken from the BRep cache of a loaded file
    std::vector<std::string> failed;      // state Error
};

// Entry of the undo/redo history.
struct TransactionInfo {
    std::string name;
    std::size_t edits = 0;  // number of editing calls grouped in the transaction
};

struct SaveOptions {
    bool includeCache = false;  // BRep + element map of each built feature, keyed by input hash
};

class PartDocument {
public:
    explicit PartDocument(std::shared_ptr<const FeatureRegistry> registry);
    PartDocument(PartDocument&&) noexcept;
    PartDocument& operator=(PartDocument&&) noexcept;
    ~PartDocument();

    const FeatureRegistry& registry() const { return *registry_; }

    // ---- variables (expressions over other variables; bendcore::formula syntax)
    Status setVariable(const std::string& name, const std::string& expression);
    Status removeVariable(const std::string& name);
    std::vector<std::pair<std::string, std::string>> variables() const;
    // Value at the last evaluation; errors carry the cause (cycle, unknown name, syntax).
    Result<double> variableValue(const std::string& name) const;

    // ---- tree editing; every edit takes effect at the next rebuild()
    // Inserts at `position` (default: at the rollback marker if set, else at the end).
    Result<std::string> addFeature(FeatureSpec spec, std::optional<std::size_t> position = std::nullopt);
    Status removeFeature(const std::string& id);
    Status moveFeature(const std::string& id, std::size_t newIndex);
    Status setSuppressed(const std::string& id, bool suppressed);
    Status setParameter(const std::string& id, const std::string& name, const std::string& expression);
    Status setReferences(const std::string& id, const std::string& slot, std::vector<TopoReference> refs);
    Status setOption(const std::string& id, const std::string& name, const std::string& value);
    Status setLink(const std::string& id, const std::string& slot, const std::string& target);
    Status setSketch(const std::string& id, SketchData sketch);
    Status setLabel(const std::string& id, const std::string& label);
    // Features at index >= position are rolled back; nullopt removes the marker.
    Status setRollback(std::optional<std::size_t> position);
    std::optional<std::size_t> rollback() const noexcept { return rollback_; }

    // ---- undo / redo. Every successful editing call (variables, features, rollback) is
    // recorded; calls between beginTransaction and commitTransaction form one entry.
    // undo/redo restore the definitions and rebuild; results of restored states are reused
    // from the per-feature result pool, so volumes and names come back exactly.
    Status beginTransaction(const std::string& name);
    Status commitTransaction();
    // Restores the state at beginTransaction (without rebuilding) and drops the transaction.
    Status abortTransaction();
    bool inTransaction() const noexcept;
    bool canUndo() const noexcept;
    bool canRedo() const noexcept;
    Result<RebuildReport> undo();
    Result<RebuildReport> redo();
    std::vector<TransactionInfo> undoHistory() const;  // oldest first
    std::vector<TransactionInfo> redoHistory() const;  // next redo last
    void clearHistory();

    // ---- evaluation
    RebuildReport rebuild();

    // ---- queries (state as of the last rebuild)
    std::size_t size() const noexcept;
    std::vector<std::string> featureIds() const;
    std::optional<std::size_t> indexOf(const std::string& id) const;
    Result<FeatureInfo> feature(const std::string& id) const;
    // Shape seen by feature `id` (last valid result before it); empty NamedShape if none.
    Result<NamedShape> inputOf(const std::string& id) const;
    // Last valid shape of the active part of the tree.
    NamedShape finalShape() const;

    // ---- persistence (format "bendcore-part", version kFormatVersion)
    static constexpr int kFormatVersion = 3;
    Result<std::string> toJson(const SaveOptions& options = {}) const;
    // Loads, migrates older versions and rebuilds. Errors: ParseError (not JSON, missing
    // fields), Unsupported (newer version, unknown feature type).
    static Result<PartDocument> fromJson(std::string_view json, std::shared_ptr<const FeatureRegistry> registry,
                                         RebuildReport* report = nullptr);

    struct Node;
    struct State;
    struct Transactions;

private:
    Status checkExpression(const std::string& expression) const;
    Node* node(const std::string& id);
    const Node* node(const std::string& id) const;
    State captureState() const;
    void restoreState(State state);
    // Records an undo entry with `before` (state before a successful edit).
    void recordEdit(const std::string& name, State before);

    std::shared_ptr<const FeatureRegistry> registry_;
    std::vector<std::unique_ptr<Node>> nodes_;
    std::vector<std::pair<std::string, std::string>> variables_;
    std::map<std::string, Result<double>> variableValues_;
    std::optional<std::size_t> rollback_;
    std::unique_ptr<Transactions> transactions_;
};

}  // namespace bendcore::modeling
