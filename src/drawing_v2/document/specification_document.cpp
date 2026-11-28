// Спецификация ГОСТ 2.106 как отдельный документ и экспорт комплекта: листы чертежа и
// спецификации в один многостраничный PDF, каждый лист — в DXF и SVG.
// docs/drawing/DRAWING_DOCUMENT.md, разделы «Спецификация» и «Экспорт комплекта».

#include <bendcore/drawing_v2/drawing_document.hpp>
#include <bendcore/drawing_v2/export.hpp>

#include <filesystem>

namespace bendcore::drawing_v2::document {

namespace {

Error invalid(std::string message) { return Error{ErrorCode::InvalidInput, std::move(message)}; }

// Ключ составной части в настройках: обозначение, а если его нет — наименование.
std::string componentKey(const assembly_v2::Definition& d) {
    if (!d.properties.designation.empty()) return d.properties.designation;
    return d.properties.name.empty() ? d.name : d.properties.name;
}

// Совпадение состава: позиции, обозначения, наименования, количества.
bool sameContent(const annotations::Specification& a, const annotations::Specification& b) {
    if (a.designation != b.designation || a.name != b.name || a.sections.size() != b.sections.size()) return false;
    for (std::size_t i = 0; i < a.sections.size(); ++i) {
        const auto& x = a.sections[i];
        const auto& y = b.sections[i];
        if (x.section != y.section || x.items.size() != y.items.size()) return false;
        for (std::size_t k = 0; k < x.items.size(); ++k) {
            const auto& p = x.items[k];
            const auto& q = y.items[k];
            if (p.position != q.position || p.designation != q.designation || p.name != q.name || p.quantity != q.quantity ||
                p.format != q.format || p.note != q.note || p.zone != q.zone)
                return false;
        }
    }
    return true;
}

// Совпадение группового состава: исполнения, позиции, обозначения, количества по исполнениям.
bool sameGroup(const annotations::GroupSpecification& a, const annotations::GroupSpecification& b) {
    if (a.configurations != b.configurations || a.columnHeaders != b.columnHeaders || a.sections.size() != b.sections.size())
        return false;
    for (std::size_t i = 0; i < a.sections.size(); ++i) {
        const auto& x = a.sections[i];
        const auto& y = b.sections[i];
        if (x.section != y.section || x.items.size() != y.items.size()) return false;
        for (std::size_t k = 0; k < x.items.size(); ++k) {
            const auto& p = x.items[k];
            const auto& q = y.items[k];
            if (p.item.position != q.item.position || p.item.designation != q.item.designation || p.item.name != q.item.name ||
                p.quantities != q.quantities || p.item.format != q.item.format || p.item.note != q.item.note ||
                p.item.zone != q.item.zone)
                return false;
        }
    }
    return true;
}

}  // namespace

Result<bool> SpecificationDocument::update(const ModelSnapshot& model) {
    if (model.kind != ModelKind::Assembly || !model.assembly) return invalid("specification: the model is not an assembly");
    const assembly_v2::AssemblyDocument& doc = *model.assembly;
    annotations::SpecificationOptions o;
    for (const assembly_v2::Definition& d : doc.definitions()) {
        if (d.id == doc.root()) continue;
        const std::string key = componentKey(d);
        if (auto it = settings_.formats.find(key); it != settings_.formats.end()) o.formats[d.id] = it->second;
        if (auto it = settings_.notes.find(key); it != settings_.notes.end()) o.notes[d.id] = it->second;
        if (auto it = settings_.sections.find(key); it != settings_.sections.end()) o.sectionOverride[d.id] = it->second;
        if (d.kind == assembly_v2::DefinitionKind::Assembly && settings_.dissolved.count(key)) o.dissolve.insert(d.id);
    }
    o.documents = settings_.documents;
    o.addAssemblyDrawing = settings_.addAssemblyDrawing;
    o.assemblyDrawingFormat = settings_.assemblyDrawingFormat;
    o.reservePositions = settings_.reservePositions;
    if (settings_.group.enabled) return updateGroup(model, o);
    group_.reset();
    auto spec = annotations::buildSpecification(doc, o);
    if (!spec) return spec.error();
    if (!title_.designation.empty()) spec.value().designation = title_.designation;
    if (!title_.name.empty()) spec.value().name = title_.name;
    const std::optional<annotations::Specification> before = spec_;
    spec_ = std::move(spec).value();
    applyZones();
    const bool changed = !before || !sameContent(*before, *spec_);
    const std::string hash = hashHex(model.contentHash);
    if (hash != model_.resultHash) {
        ++model_.revision;
        model_.resultHash = hash;
    }
    return changed;
}

Result<bool> SpecificationDocument::updateGroup(const ModelSnapshot& model, const annotations::SpecificationOptions& base) {
    const assembly_v2::AssemblyDocument& doc = *model.assembly;
    auto bom = assembly_v2::configurationBom(doc, loadOptions_);
    if (!bom) return Error{bom.error().code, "group specification: " + bom.error().message};
    // Настройки по ключу составной части: строки configurationBom ссылаются на определения
    // копии документа (исполнения компонентов), поэтому ключ берётся из самой строки.
    annotations::GroupSpecOptions o;
    o.base = base;
    o.base.formats.clear();
    o.base.notes.clear();
    o.base.sectionOverride.clear();
    o.base.dissolve.clear();
    for (const assembly_v2::ConfigurationBomRow& r : bom.value().rows) {
        const std::string key = !r.designation.empty() ? r.designation : r.name;
        if (auto it = settings_.formats.find(key); it != settings_.formats.end()) o.base.formats[r.definition] = it->second;
        if (auto it = settings_.notes.find(key); it != settings_.notes.end()) o.base.notes[r.definition] = it->second;
        if (auto it = settings_.sections.find(key); it != settings_.sections.end()) o.base.sectionOverride[r.definition] = it->second;
        if (r.kind == assembly_v2::DefinitionKind::Assembly && settings_.dissolved.count(key)) o.base.dissolve.insert(r.definition);
    }
    o.layout = settings_.group.layout;
    o.columnHeaders = settings_.group.columnHeaders;
    const auto& root = doc.definition(doc.root());
    // Обозначение группового документа — базовое исполнение (первая строка таблицы исполнений).
    std::string designation = !title_.designation.empty() ? title_.designation : bom.value().configurations.front().fullDesignation;
    if (designation.empty()) designation = root.properties.designation;
    std::string name = !title_.name.empty() ? title_.name : (root.properties.name.empty() ? root.name : root.properties.name);
    auto group = annotations::buildGroupSpecification(bom.value(), designation, name, o);
    if (!group) return group.error();
    // Спецификация активного исполнения с номерами позиций групповой — для позиций сборочного
    // чертежа. Определения строк — из копии документа: сопоставляются с составом документа по
    // обозначению и наименованию.
    const std::string active = doc.configurationTable().active;
    std::size_t activeIndex = 0;
    for (std::size_t i = 0; i < bom.value().configurations.size(); ++i)
        if (bom.value().configurations[i].designation == active) activeIndex = i;
    annotations::Specification spec = group.value().forConfiguration(activeIndex);
    auto own = assembly_v2::structuredBom(doc);
    if (!own) return own.error();
    for (auto& sec : spec.sections)
        for (auto& item : sec.items) {
            if (!item.definition) continue;
            for (const assembly_v2::BomRow& r : own.value().rows)
                if (r.level == 0 && r.designation == item.designation && r.name == item.name) item.definition = r.definition;
        }
    const std::optional<annotations::Specification> before = spec_;
    const std::optional<annotations::GroupSpecification> beforeGroup = group_;
    spec_ = std::move(spec);
    group_ = std::move(group).value();
    applyZones();
    const bool changed = !before || !sameContent(*before, *spec_) || !beforeGroup || !sameGroup(*beforeGroup, *group_);
    const std::string hash = hashHex(model.contentHash);
    if (hash != model_.resultHash) {
        ++model_.revision;
        model_.resultHash = hash;
    }
    return changed;
}

bool SpecificationDocument::applyZones() {
    bool changed = false;
    std::map<int, std::string> byPosition;
    if (spec_) {
        for (auto& sec : spec_->sections)
            for (auto& item : sec.items) {
                std::string zone;
                if (item.definition)
                    if (auto it = zones_.find(*item.definition); it != zones_.end()) zone = it->second;
                changed = changed || item.zone != zone;
                item.zone = zone;
                if (item.position > 0) byPosition[item.position] = zone;
            }
    }
    if (group_) {
        for (auto& sec : group_->sections)
            for (auto& gi : sec.items) {
                const auto it = byPosition.find(gi.item.position);
                const std::string zone = gi.item.position > 0 && it != byPosition.end() ? it->second : std::string();
                changed = changed || gi.item.zone != zone;
                gi.item.zone = zone;
            }
    }
    return changed;
}

bool SpecificationDocument::setPositionZones(std::map<std::uint32_t, std::string> zones) {
    zones_ = std::move(zones);
    return applyZones();
}

Result<annotations::SpecDocument> SpecificationDocument::render(const TrueTypeFont* font) const {
    if (!spec_) return invalid("specification render: the document is not updated");
    annotations::SpecSheetOptions so;
    so.textFont = settings_.textFont;
    so.font = settings_.textFont == TextFont::Gost2304 ? static_cast<const TextMetrics*>(&gostStrokeFont()) : font;
    so.title.litera = title_.litera;
    so.title.organization = title_.organization;
    so.title.developer = title_.developer;
    so.title.checker = title_.checker;
    so.title.techControl = title_.techControl;
    so.title.normControl = title_.normControl;
    so.title.approver = title_.approver;
    if (settings_.group.enabled && group_) return annotations::renderGroupSpecification(*group_, so);
    return annotations::renderSpecification(*spec_, so);
}

Result<ExportKitResult> exportKit(const DrawingDocument& drawing, const SpecificationDocument* specification,
                                  const ExportKitOptions& options) {
    if (options.directory.empty()) return invalid("export kit: no output directory");
    const bool trueType = drawing.settings().textFont == TextFont::TrueType ||
                          (specification && specification->settings().textFont == TextFont::TrueType);
    if (!options.font && trueType) return invalid("export kit: a TrueType font is required for the texts (text font 'truetype')");
    std::error_code ec;
    std::filesystem::create_directories(options.directory, ec);
    if (ec) return Error{ErrorCode::IoError, "export kit: cannot create '" + options.directory + "': " + ec.message()};
    auto sheets = drawing.render(options.font);
    if (!sheets) return sheets.error();
    std::optional<annotations::SpecDocument> spec;
    if (specification) {
        auto r = specification->render(options.font);
        if (!r) return r.error();
        spec = std::move(r).value();
    }
    ExportKitResult out;
    out.drawingPages = int(sheets.value().size());
    out.specificationPages = spec ? int(spec->sheets.size()) : 0;
    const std::filesystem::path dir(options.directory);
    auto save = [&](const std::string& name, const std::string& format, const std::string& sheet,
                    const std::string& bytes) -> Status {
        const std::string path = (dir / name).string();
        auto n = saveToFile(path, bytes);
        if (!n) return n.error();
        out.files.push_back({path, format, sheet, n.value()});
        return okStatus();
    };
    std::vector<std::pair<std::string, const Drawing*>> all;
    for (std::size_t i = 0; i < sheets.value().size(); ++i) all.emplace_back(drawing.computedSheets()[i].id, &sheets.value()[i]);
    if (spec)
        for (std::size_t i = 0; i < spec->sheets.size(); ++i) all.emplace_back("SP" + std::to_string(i + 1), &spec->sheets[i]);
    if (options.pdf) {
        std::vector<const Drawing*> pages;
        for (const auto& [id, d] : all) pages.push_back(d);
        PdfOptions po;
        po.font = options.font;
        const auto& cs = drawing.computedSheets();
        po.title = cs.empty() ? options.baseName : (cs.front().title.designation + " " + cs.front().title.name);
        auto pdf = writePdfPages(pages, po);
        if (!pdf) return pdf.error();
        if (auto st = save(options.baseName + ".pdf", "pdf", "", pdf.value()); !st) return st.error();
    }
    for (const auto& [id, d] : all) {
        if (options.dxf) {
            auto dxf = writeDxf(*d);
            if (!dxf) return Error{dxf.error().code, "export kit, sheet '" + id + "': " + dxf.error().message};
            if (auto st = save(options.baseName + "_" + id + ".dxf", "dxf", id, dxf.value()); !st) return st.error();
        }
        if (options.svg) {
            SvgOptions so;
            so.font = options.font;
            so.title = id;
            auto svg = writeSvg(*d, so);
            if (!svg) return Error{svg.error().code, "export kit, sheet '" + id + "': " + svg.error().message};
            if (auto st = save(options.baseName + "_" + id + ".svg", "svg", id, svg.value()); !st) return st.error();
        }
    }
    return out;
}

}  // namespace bendcore::drawing_v2::document
