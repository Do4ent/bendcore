// Обмен документа сборки: XCAF/STEP (bendcore_kernel_xcaf) и загрузка деталей из файлов
// (docs/assembly/ASSEMBLY_DOCUMENT.md, разделы 2 и 9).

#include <bendcore/assembly_v2/assembly_document.hpp>

#include <bendcore/kernel/brep_io.hpp>
#include <bendcore/kernel/step_io.hpp>
#include <bendcore/modeling/part_document.hpp>

#include <Standard_Failure.hxx>
#include <gp_Quaternion.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>

namespace bendcore::assembly_v2 {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;

TopLoc_Location toLocation(const Transform& t) {
    gp_Trsf trsf;
    const Quat q = t.rotation.normalized();
    trsf.SetTransformation(gp_Quaternion(q.x(), q.y(), q.z(), q.w()),
                           gp_Vec(t.position.x(), t.position.y(), t.position.z()));
    return TopLoc_Location(trsf);
}

Result<Transform> fromLocation(const TopLoc_Location& location) {
    const gp_Trsf trsf = location.Transformation();
    if (std::abs(trsf.ScaleFactor() - 1.0) > 1e-9 || trsf.IsNegative())
        return Error{ErrorCode::Unsupported, "placement with scale or mirror is not a rigid motion"};
    const gp_Quaternion q = trsf.GetRotation();
    const gp_XYZ p = trsf.TranslationPart();
    Transform t;
    t.rotation = Quat(q.W(), q.X(), q.Y(), q.Z()).normalized();
    t.position = Vec3(p.X(), p.Y(), p.Z());
    return t;
}

// ---------------------------------------------------------------- XCAF

Result<kernel::AssemblyDocument> AssemblyDocument::toXcaf() const {
    // Только достижимые от корня определения, в порядке обхода в глубину.
    std::map<DefinitionId, std::size_t> index;
    std::vector<DefinitionId> order;
    std::function<void(DefinitionId)> visit = [&](DefinitionId id) {
        if (index.count(id)) return;
        index[id] = order.size();
        order.push_back(id);
        for (const Occurrence& o : definitions_[id].occurrences) visit(o.definition);
    };
    visit(root());
    kernel::AssemblyDocument out;
    for (DefinitionId id : order) {
        const Definition& d = definitions_[id];
        kernel::AssemblyDefinition x;
        x.kind = d.kind == DefinitionKind::Part ? kernel::DefinitionKind::Part : kernel::DefinitionKind::Assembly;
        x.name = d.name;
        x.color = d.color;
        if (d.kind == DefinitionKind::Part) {
            x.shape = kernel::Shape(d.shape);
            x.faceColors = d.faceColors;
        }
        for (const Occurrence& o : d.occurrences) {
            kernel::AssemblyComponent c;
            c.name = o.name;
            c.definition = index.at(o.definition);
            c.location = toLocation(o.placement);
            c.color = o.color;
            x.components.push_back(std::move(c));
        }
        out.definitions.push_back(std::move(x));
    }
    out.roots = {0};
    return out;
}

Result<AssemblyDocument> AssemblyDocument::fromXcaf(const kernel::AssemblyDocument& x) {
    if (auto check = kernel::flattenAssembly(x); !check) return check.error();
    const bool singleAssemblyRoot =
        x.roots.size() == 1 && x.definitions[x.roots[0]].kind == kernel::DefinitionKind::Assembly;
    AssemblyDocument doc(singleAssemblyRoot ? x.definitions[x.roots[0]].name : std::string("Assembly"));
    if (singleAssemblyRoot) doc.definitions_[0].color = x.definitions[x.roots[0]].color;
    std::vector<DefinitionId> map(x.definitions.size(), 0);
    for (std::size_t i = 0; i < x.definitions.size(); ++i) {
        const kernel::AssemblyDefinition& d = x.definitions[i];
        if (singleAssemblyRoot && i == x.roots[0]) {
            map[i] = doc.root();
            continue;
        }
        if (d.kind == kernel::DefinitionKind::Part) {
            auto id = doc.addPart(d.name, PartData{d.shape.occt(), nullptr});
            if (!id) return Error{id.error().code, "part '" + d.name + "': " + id.error().message};
            map[i] = id.value();
            doc.definitions_[map[i]].faceColors = d.faceColors;
        } else {
            map[i] = doc.addSubassembly(d.name).value();
        }
        doc.definitions_[map[i]].color = d.color;
    }
    for (std::size_t i = 0; i < x.definitions.size(); ++i) {
        const kernel::AssemblyDefinition& d = x.definitions[i];
        if (d.kind != kernel::DefinitionKind::Assembly) continue;
        for (const kernel::AssemblyComponent& c : d.components) {
            auto t = fromLocation(c.location);
            if (!t) return Error{t.error().code, "component '" + c.name + "': " + t.error().message};
            auto id = doc.addOccurrence(map[c.definition], t.value(), c.name, map[i]);
            if (!id) return id.error();
            doc.findOccurrenceMutable(id.value())->color = c.color;
        }
    }
    if (!singleAssemblyRoot) {
        for (std::size_t r : x.roots) {
            auto id = doc.addOccurrence(map[r], Transform::identity(), x.definitions[r].name);
            if (!id) return id.error();
        }
    }
    return doc;
}

Result<std::string> AssemblyDocument::exportStep(const kernel::AssemblyWriteOptions& options) const {
    auto x = toXcaf();
    if (!x) return x.error();
    return kernel::writeStepAssembly(x.value(), options);
}

Result<AssemblyDocument> AssemblyDocument::importStep(std::string_view step) {
    auto x = kernel::readStepAssembly(step);
    if (!x) return x.error();
    return fromXcaf(x.value());
}

// ---------------------------------------------------------------- источники деталей

Result<PartData> partDataFromDocument(const modeling::PartDocument& document) {
    modeling::NamedShape named = document.finalShape();
    if (named.isNull()) return Error{ErrorCode::InvalidInput, "part document has no valid shape"};
    PartData data;
    data.shape = named.shape();
    data.elementMap = std::make_shared<const modeling::NamedShape>(std::move(named));
    return data;
}

PartResolver filePartResolver(std::shared_ptr<const modeling::FeatureRegistry> registry, std::string baseDirectory) {
    return [registry = std::move(registry), base = std::move(baseDirectory)](const std::string& source)
               -> Result<PartData> {
        std::filesystem::path path(source);
        if (path.is_relative() && !base.empty()) path = std::filesystem::path(base) / path;
        std::ifstream in(path, std::ios::binary);
        if (!in) return Error{ErrorCode::IoError, "cannot open '" + path.string() + "'"};
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (in.bad()) return Error{ErrorCode::IoError, "cannot read '" + path.string() + "'"};
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
        try {
            if (ext == ".json") {
                if (!registry) return Error{ErrorCode::InvalidInput, "no feature registry for part documents"};
                auto doc = modeling::PartDocument::fromJson(text, registry);
                if (!doc) return doc.error();
                return partDataFromDocument(doc.value());
            }
            if (ext == ".step" || ext == ".stp") {
                auto s = kernel::readStep(text);
                if (!s) return s.error();
                return PartData{s.value().occt(), nullptr};
            }
            if (ext == ".brep") {
                const bool isText = text.rfind("DBRep_DrawableShape", 0) == 0 || text.rfind("CASCADE Topology", 0) == 0;
                auto s = isText ? kernel::readBrepText(text)
                                : kernel::readBrepBinary(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
                if (!s) return s.error();
                return PartData{s.value().occt(), nullptr};
            }
        } catch (const Standard_Failure& e) {
            return Error{ErrorCode::OperationFailed, std::string("OCCT exception while loading part: ") + e.what()};
        }
        return Error{ErrorCode::Unsupported, "unsupported part file type '" + ext + "' (expected .json, .step, .stp, .brep)"};
    };
}

}  // namespace bendcore::assembly_v2
