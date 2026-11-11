// Снимки модели для документа чертежа: деталь (итоговое тело документа детали с картой имён) и
// сборка (компаунд деталей развёртки с принадлежностью рёбер определениям), загрузка по пути.
// docs/drawing/DRAWING_DOCUMENT.md, раздел «Модель».

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/drawing_v2/drawing_document.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace bendcore::drawing_v2::document {

namespace {

Error invalid(std::string message) { return Error{ErrorCode::InvalidInput, std::move(message)}; }

Result<std::string> readText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return Error{ErrorCode::IoError, "cannot open '" + path + "'"};
    std::ostringstream s;
    s << in.rdbuf();
    if (!in.good() && !in.eof()) return Error{ErrorCode::IoError, "cannot read '" + path + "'"};
    return s.str();
}

}  // namespace

const char* toString(ModelKind kind) noexcept { return kind == ModelKind::Part ? "part" : "assembly"; }

std::string hashHex(std::uint64_t hash) {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(hash));
    return buf;
}

Result<ModelSnapshot> snapshotPart(const modeling::PartDocument& document, const ModelProperties& properties) {
    modeling::NamedShape named = document.finalShape();
    if (named.isNull()) return invalid("drawing model: the part document has no body");
    ModelSnapshot s;
    s.kind = ModelKind::Part;
    s.shape = kernel::Shape(named.shape());
    s.named = std::make_shared<const modeling::NamedShape>(std::move(named));
    s.historyNames = true;
    s.properties = properties;
    auto hash = views::shapeContentHash(s.shape);
    if (!hash) return hash.error();
    s.contentHash = hash.value();
    if (properties.density) {
        if (!(*properties.density > 0)) return invalid("drawing model: density must be positive");
        auto vp = s.shape.volumeProperties();
        if (!vp) return vp.error();
        // Объём, мм³ → м³ (1e-9) · плотность, кг/м³.
        s.massKg = std::abs(vp.value().volume) * 1e-9 * *properties.density;
    }
    return s;
}

Result<ModelSnapshot> snapshotAssembly(std::shared_ptr<const assembly_v2::AssemblyDocument> document) {
    if (!document) return invalid("drawing model: null assembly document");
    auto placed = document->placedParts();
    if (!placed) return placed.error();
    if (placed.value().empty()) return invalid("drawing model: the assembly has no parts");
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const assembly_v2::PlacedOccurrence& p : placed.value()) {
        if (!p.shape.IsNull()) builder.Add(compound, p.shape);
    }
    ModelSnapshot s;
    s.kind = ModelKind::Assembly;
    s.shape = kernel::Shape(compound);
    s.assembly = document;
    auto topo = kernel::TopologyIndex::build(s.shape);
    if (!topo) return topo.error();
    // Принадлежность рёбер: развёртка даёт форму каждой детали в мировой СК, рёбра компаунда —
    // те же подформы (TShape + положение).
    s.edgeOwner.assign(topo.value().edges().size(), assembly_v2::DefinitionId(0));
    for (const assembly_v2::PlacedOccurrence& p : placed.value()) {
        if (p.shape.IsNull()) continue;
        for (TopExp_Explorer ex(p.shape, TopAbs_EDGE); ex.More(); ex.Next()) {
            if (auto i = topo.value().edgeIndex(ex.Current())) s.edgeOwner[*i] = p.definition;
        }
    }
    modeling::NamingRequest req;
    req.featureId = "Model";
    req.result = compound;
    auto named = modeling::buildNamedShape(req);
    if (!named) return named.error();
    s.named = std::make_shared<const modeling::NamedShape>(std::move(named).value());
    s.historyNames = false;
    const assembly_v2::Definition& root = document->definition(document->root());
    s.properties.designation = root.properties.designation;
    s.properties.name = root.properties.name.empty() ? root.name : root.properties.name;
    s.properties.material = root.properties.material;
    auto bom = assembly_v2::flatBom(*document);
    if (!bom) return bom.error();
    s.massKg = bom.value().totalMass;
    auto hash = views::shapeContentHash(s.shape);
    if (!hash) return hash.error();
    s.contentHash = hash.value();
    return s;
}

ModelLoader fileModelLoader(std::string baseDirectory) {
    return [base = std::move(baseDirectory)](const ModelLink& link) -> Result<ModelSnapshot> {
        if (link.path.empty()) return invalid("drawing model: the model path is empty");
        std::filesystem::path p(link.path);
        if (p.is_relative() && !base.empty()) p = std::filesystem::path(base) / p;
        if (link.kind == ModelKind::Part) {
            auto text = readText(p.string());
            if (!text) return text.error();
            auto doc = modeling::PartDocument::fromJson(text.value(), modeling::partRegistry());
            if (!doc) return doc.error();
            return snapshotPart(doc.value(), link.properties);
        }
        auto loaded = assembly_v2::AssemblyDocument::loadFile(p.string(), modeling::partRegistry());
        if (!loaded) return loaded.error();
        auto doc = std::make_shared<const assembly_v2::AssemblyDocument>(std::move(loaded.value().document));
        return snapshotAssembly(std::move(doc));
    };
}

Result<modeling::TopoReference> elementReference(const ModelSnapshot& model, modeling::ElementKind kind,
                                                 std::uint32_t index) {
    if (!model.named) return invalid("element reference: the model snapshot has no element map");
    if (index >= model.named->count(kind)) {
        return invalid(std::string("element reference: ") + modeling::toString(kind) + " index " +
                       std::to_string(index) + " out of range");
    }
    auto r = modeling::makeReference(*model.named, model.named->element(kind, index));
    if (!r) return r.error();
    if (!model.historyNames) r.value().name.clear();
    return r;
}

}  // namespace bendcore::drawing_v2::document
