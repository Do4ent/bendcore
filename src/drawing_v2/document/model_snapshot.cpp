// Снимки модели для документа чертежа: деталь (итоговое тело документа детали с картой имён) и
// сборка (компаунд деталей развёртки с принадлежностью рёбер определениям), загрузка по пути.
// docs/drawing/DRAWING_DOCUMENT.md, раздел «Модель».

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/drawing_v2/drawing_document.hpp>
#include <bendcore/kernel/material.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>

#include <cstdio>
#include <map>
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

ModelProperties titleProperties(const ModelProperties& link, const std::optional<ModelProperties>& doc) {
    ModelProperties p = link;
    if (!doc) return p;
    auto take = [](std::string& to, const std::string& from) {
        if (!from.empty()) to = from;
    };
    take(p.designation, doc->designation);
    take(p.name, doc->name);
    take(p.material, doc->material);
    take(p.developer, doc->developer);
    take(p.organization, doc->organization);
    if (doc->density) p.density = doc->density;
    return p;
}

namespace {

// Свойства документа детали для основной надписи; nullopt — документ их не задаёт.
std::optional<ModelProperties> partDocumentProperties(const modeling::PartDocument& document) {
    const modeling::ConfigurationProperties e = document.effectiveProperties();
    ModelProperties p;
    p.designation = e.designation;
    p.name = e.name;
    p.density = e.density;
    p.developer = document.properties().developer;
    p.organization = document.properties().organization;
    // «custom» — плотность, заданная вручную на странице детали: наименования материала нет.
    if (!e.material.empty() && e.material != "custom") {
        p.material = e.material;
        for (const kernel::Material& m : kernel::builtInMaterials())
            if (m.id == e.material) p.material = m.name;
    }
    if (p.designation.empty() && p.name.empty() && p.material.empty() && !p.density && p.developer.empty() &&
        p.organization.empty())
        return std::nullopt;
    return p;
}

}  // namespace

Result<ModelSnapshot> snapshotPart(const modeling::PartDocument& document, const ModelProperties& properties) {
    modeling::NamedShape named = document.finalShape();
    if (named.isNull()) return invalid("drawing model: the part document has no body");
    ModelSnapshot s;
    s.kind = ModelKind::Part;
    // Зубчатые венцы: виды строятся по условной форме ГОСТ 2.402, масса — по форме с зубьями.
    const kernel::Shape real(named.shape());
    s.gears = modeling::gearsOf(named);
    if (!s.gears.empty()) {
        auto conv = annotations::conventionalGearModel(named, s.gears);
        if (!conv) return Error{conv.error().code, "drawing model: " + conv.error().message};
        named = std::move(conv).value();
    }
    s.shape = kernel::Shape(named.shape());
    s.named = std::make_shared<const modeling::NamedShape>(std::move(named));
    s.historyNames = true;
    s.properties = properties;
    s.documentProperties = partDocumentProperties(document);
    auto hash = views::shapeContentHash(s.shape);
    if (!hash) return hash.error();
    s.contentHash = hash.value();
    // Плотность документа детали, иначе — записанная в чертеже.
    const std::optional<double> density = titleProperties(properties, s.documentProperties).density;
    if (density) {
        if (!(*density > 0)) return invalid("drawing model: density must be positive");
        auto vp = real.volumeProperties();
        if (!vp) return vp.error();
        // Объём, мм³ → м³ (1e-9) · плотность, кг/м³.
        s.massKg = std::abs(vp.value().volume) * 1e-9 * *density;
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
    std::vector<assembly_v2::DefinitionId> childOwner;
    std::vector<std::vector<assembly_v2::DefinitionId>> childChain;
    // Детали с зубчатыми венцами — условной формой ГОСТ 2.402 (впадины заполнены до окружности
    // вершин), одной на определение.
    std::map<assembly_v2::DefinitionId, std::pair<TopoDS_Shape, std::vector<modeling::GearInfo>>> conventional;
    std::vector<modeling::GearInfo> allGears;
    std::vector<std::size_t> gearChild;
    std::vector<annotations::BearingInfo> bearings;
    for (assembly_v2::PlacedOccurrence& p : placed.value()) {
        if (p.shape.IsNull()) continue;
        const assembly_v2::Definition& def = document->definition(p.definition);
        const std::size_t childIndex = childOwner.size();
        if (def.elementMap && !def.shape.IsNull()) {
            auto it = conventional.find(p.definition);
            if (it == conventional.end()) {
                TopoDS_Shape conv;
                auto gears = modeling::gearsOf(*def.elementMap);
                if (!gears.empty()) {
                    auto c = annotations::conventionalGearShape(def.shape, gears);
                    if (!c) return Error{c.error().code, "drawing model: " + def.name + ": " + c.error().message};
                    conv = c.value().shape;
                }
                it = conventional.emplace(p.definition, std::make_pair(conv, std::move(gears))).first;
            }
            // Положение вхождения относительно формы определения: оно же переносит оси венцов.
            const TopLoc_Location place = p.shape.Location() * def.shape.Location().Inverted();
            if (!it->second.first.IsNull()) p.shape = it->second.first.Moved(place);
            for (modeling::GearInfo g : it->second.second) {
                g.gear.axis.Transform(place.Transformation());
                allGears.push_back(std::move(g));
                gearChild.push_back(childIndex);
            }
        }
        // Подшипник качения (ГОСТ 2.420): по телам качения в геометрии или по наименованию.
        if (auto b = annotations::recognizeBearing(p.shape, {def.properties.name, def.name})) {
            b->child = childIndex;
            bearings.push_back(*b);
        }
        builder.Add(compound, p.shape);
        childOwner.push_back(p.definition);
        std::vector<assembly_v2::DefinitionId> chain;
        for (assembly_v2::OccurrenceId id : p.path) {
            const assembly_v2::Occurrence* o = document->findOccurrence(id);
            if (!o) return invalid("drawing model: occurrence " + std::to_string(id) + " of a placed part is missing");
            chain.push_back(o->definition);
        }
        if (chain.empty() || chain.back() != p.definition) chain.push_back(p.definition);
        childChain.push_back(std::move(chain));
    }
    ModelSnapshot s;
    s.gears = std::move(allGears);
    s.gearChild = std::move(gearChild);
    s.bearings = std::move(bearings);
    s.childOwner = std::move(childOwner);
    s.childChain = std::move(childChain);
    s.kind = ModelKind::Assembly;
    s.shape = kernel::Shape(compound);
    s.assembly = document;
    auto topo = kernel::TopologyIndex::build(s.shape);
    if (!topo) return topo.error();
    // Принадлежность рёбер: развёртка даёт форму каждой детали в мировой СК, рёбра компаунда —
    // те же подформы (TShape + положение).
    s.edgeOwner.assign(topo.value().edges().size(), assembly_v2::DefinitionId(0));
    s.edgeChild.assign(topo.value().edges().size(), UINT32_MAX);
    s.faceChild.assign(topo.value().faces().size(), UINT32_MAX);
    std::uint32_t child = 0;
    for (const assembly_v2::PlacedOccurrence& p : placed.value()) {
        if (p.shape.IsNull()) continue;
        for (TopExp_Explorer ex(p.shape, TopAbs_EDGE); ex.More(); ex.Next()) {
            if (auto i = topo.value().edgeIndex(ex.Current())) {
                s.edgeOwner[*i] = p.definition;
                s.edgeChild[*i] = child;
            }
        }
        for (TopExp_Explorer ex(p.shape, TopAbs_FACE); ex.More(); ex.Next())
            if (auto i = topo.value().faceIndex(ex.Current())) s.faceChild[*i] = child;
        ++child;
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

std::optional<assembly_v2::DefinitionId> positionOwner(const ModelSnapshot& model, std::uint32_t edge,
                                                       const annotations::Specification& spec) {
    if (edge < model.edgeChild.size() && model.edgeChild[edge] < model.childChain.size()) {
        for (assembly_v2::DefinitionId d : model.childChain[model.edgeChild[edge]])
            if (spec.positionOf(d)) return d;
        return std::nullopt;
    }
    // Снимок без цепочек (собран вручную): владелец ребра — деталь.
    if (edge < model.edgeOwner.size() && spec.positionOf(model.edgeOwner[edge])) return model.edgeOwner[edge];
    return std::nullopt;
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
            // Групповой чертёж ГОСТ 2.113: все исполнения таблицы параметров.
            if (link.withConfigurations) return snapshotPartConfigurations(doc.value(), link.properties);
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
