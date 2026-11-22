// Групповой рабочий чертёж детали ГОСТ 2.113-75 (способ с таблицей исполнений): снимки всех
// исполнений детали, значения размеров каждого исполнения, буквы различающихся размеров и
// таблица исполнений. docs/drawing/DRAWING_DOCUMENT.md, «Групповой чертёж».

#include <bendcore/drawing_v2/drawing_document.hpp>
#include <bendcore/kernel/material.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <algorithm>
#include <map>
#include <set>

namespace bendcore::drawing_v2::document {

namespace {

Error invalid(std::string message) { return Error{ErrorCode::InvalidInput, std::move(message)}; }

std::string massText(double kg) {
    // Как в основной надписи (ГОСТ 2.104): килограммы без единицы, десятичная запятая.
    const int decimals = kg < 1.0 ? 3 : (kg < 100.0 ? 2 : 1);
    return annotations::formatNumber(kg, decimals);
}

const kernel::Material* builtIn(const std::string& id) {
    for (const kernel::Material& m : kernel::builtInMaterials())
        if (m.id == id) return &m;
    return nullptr;
}

bool letterLess(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return a.size() < b.size();
    return a < b;
}

}  // namespace

Result<ModelSnapshot> snapshotPartConfigurations(const modeling::PartDocument& document,
                                                 const ModelProperties& properties) {
    const modeling::ConfigurationTable& table = document.configurationTable();
    if (table.rows.empty()) return snapshotPart(document, properties);
    // Копия документа: сохранение с кэшем результатов и загрузка (без пересчёта узлов), затем
    // перестроение в каждом исполнении. Исходный документ (активное исполнение, история) не меняется.
    modeling::SaveOptions so;
    so.includeCache = true;
    auto json = document.toJson(so);
    if (!json) return json.error();
    auto copyR = modeling::PartDocument::fromJson(json.value(), modeling::partRegistry());
    if (!copyR) return Error{copyR.error().code, "group drawing: cannot copy the part document: " + copyR.error().message};
    modeling::PartDocument copy = std::move(copyR).value();

    std::vector<std::pair<std::string, std::string>> features;
    for (const modeling::ConfigurationColumn& c : table.columns) {
        if (c.kind != modeling::ConfigurationColumnKind::Suppression) continue;
        auto f = copy.feature(c.target);
        if (!f) return f.error();
        features.emplace_back(c.target, f.value().label.empty() ? c.target : f.value().label);
    }
    std::vector<ConfigurationSnapshot> configs;
    for (std::size_t r = 0; r < table.rows.size(); ++r) {
        const modeling::Configuration& row = table.rows[r];
        const std::string prefix = "configuration '" + row.designation + "': ";
        if (auto st = copy.activateConfiguration(row.designation); !st) return Error{st.error().code, prefix + st.error().message};
        const modeling::RebuildReport rep = copy.rebuild();
        if (!rep.failed.empty()) return Error{ErrorCode::OperationFailed, prefix + "feature '" + rep.failed.front() + "' failed"};
        ModelProperties props = properties;
        const std::string materialId = copy.configurationMaterial();
        if (!materialId.empty()) {
            const kernel::Material* m = builtIn(materialId);
            if (!m) return invalid(prefix + "unknown material '" + materialId + "'");
            props.material = m->name;
            props.density = m->density;
        }
        auto snap = snapshotPart(copy, props);
        if (!snap) return Error{snap.error().code, prefix + snap.error().message};
        ConfigurationSnapshot cs;
        cs.designation = row.designation;
        cs.fullDesignation = modeling::fullDesignation(table, r);
        cs.name = row.name;
        cs.material = props.material;
        cs.massKg = snap.value().massKg;
        for (const auto& [id, label] : features) {
            auto f = copy.feature(id);
            if (!f) return f.error();
            cs.suppressed.push_back(f.value().suppressed);
        }
        cs.model = std::make_shared<const ModelSnapshot>(std::move(snap).value());
        configs.push_back(std::move(cs));
    }
    ModelSnapshot base = *configs.front().model;
    // Хэш снимка — по формам всех исполнений: изменение любого исполнения (ячейки таблицы
    // параметров) — изменение модели группового чертежа (checkModel, ревизия).
    std::uint64_t h = 1469598103934665603ull;
    for (const ConfigurationSnapshot& c : configs) {
        const std::uint64_t x = c.model->contentHash;
        const auto* b = reinterpret_cast<const unsigned char*>(&x);
        for (std::size_t i = 0; i < sizeof x; ++i) h = (h ^ b[i]) * 1099511628211ull;
    }
    base.contentHash = h;
    base.configurations = std::move(configs);
    base.configurationFeatures = std::move(features);
    return base;
}

Result<GroupDrawingResult> DrawingDocument::computeGroup(const ModelSnapshot& model, views::ViewCache& cache) const {
    const std::size_t n = model.configurations.size();
    GroupDrawingResult g;
    for (const ConfigurationSnapshot& c : model.configurations) g.designations.push_back(c.fullDesignation);
    g.baseDesignation = model.configurations.front().fullDesignation;

    // 1. Значения размеров в каждом исполнении: обновление копии документа (без группового
    // режима) по снимку исполнения — те же виды, привязки и правила, что у чертежа.
    // value[c][лист][размер], text[c][лист][размер].
    std::vector<std::vector<std::vector<std::optional<double>>>> value(n);
    std::vector<std::vector<std::vector<std::string>>> text(n);
    for (std::size_t c = 0; c < n; ++c) {
        if (!model.configurations[c].model) return invalid("group drawing: configuration without a snapshot");
        DrawingDocument copy = *this;
        copy.settings_.group.enabled = false;
        copy.group_.reset();
        copy.computed_.clear();
        auto r = copy.update(*model.configurations[c].model, cache, nullptr);
        if (!r)
            return Error{r.error().code, "group drawing, configuration '" + model.configurations[c].designation + "': " +
                                             r.error().message};
        value[c].resize(sheets_.size());
        text[c].resize(sheets_.size());
        for (std::size_t si = 0; si < sheets_.size(); ++si) {
            const auto& dims = copy.computed_[si].dimensions;
            for (std::size_t di = 0; di < sheets_[si].dimensions.size(); ++di) {
                const annotations::DimensionDef& def = sheets_[si].dimensions[di].def;
                if (di < dims.size() && dims[di].state == annotations::DimensionState::Ok) {
                    value[c][si].push_back(dims[di].value);
                    text[c][si].push_back(def.type == annotations::DimensionType::Angle
                                              ? annotations::formatAngle(dims[di].value)
                                              : annotations::formatNumber(dims[di].value, def.decimals));
                } else {
                    value[c][si].push_back(std::nullopt);
                    text[c][si].push_back("–");
                }
            }
        }
    }

    // 2. Размеры, различающиеся у исполнений (по выводимому числу; у базового исполнения размер
    // должен быть на чертеже), и их буквы: заданные вручную, остальные — по порядку.
    std::set<std::string> used;
    for (std::size_t si = 0; si < sheets_.size(); ++si) {
        for (std::size_t di = 0; di < sheets_[si].dimensions.size(); ++di) {
            if (!value[0][si][di]) continue;
            bool differs = false;
            for (std::size_t c = 1; c < n; ++c) differs = differs || text[c][si][di] != text[0][si][di];
            if (!differs) continue;
            const DimensionItem& item = sheets_[si].dimensions[di];
            GroupDimension gd;
            gd.sheet = sheets_[si].id;
            gd.id = item.id;
            gd.letter = item.groupLetter;
            if (!gd.letter.empty() && !used.insert(gd.letter).second)
                return invalid("group drawing: letter '" + gd.letter + "' of dimension '" + gd.sheet + "/" + gd.id +
                               "' is already used");
            for (std::size_t c = 0; c < n; ++c) {
                gd.values.push_back(value[c][si][di]);
                gd.texts.push_back(text[c][si][di]);
            }
            g.dimensions.push_back(std::move(gd));
        }
    }
    int next = 0;
    for (GroupDimension& gd : g.dimensions) {
        while (gd.letter.empty()) {
            std::string cand = annotations::groupDimensionLetter(next++);
            if (used.insert(cand).second) gd.letter = cand;
        }
    }

    // 3. Графы таблицы исполнений (ГОСТ 2.113, п. 2.6).
    auto header = [&](const std::string& key, const std::string& def) {
        auto it = settings_.group.headers.find(key);
        return GroupColumn{key, it != settings_.group.headers.end() && !it->second.empty() ? it->second : def, def};
    };
    std::vector<std::vector<std::string>> cells(n);
    auto addColumn = [&](GroupColumn col, const std::vector<std::string>& values) {
        g.columns.push_back(std::move(col));
        for (std::size_t c = 0; c < n; ++c) cells[c].push_back(values[c]);
    };
    addColumn(header("designation", "Обозначение"), g.designations);
    std::vector<const GroupDimension*> byLetter;
    for (const GroupDimension& gd : g.dimensions) byLetter.push_back(&gd);
    std::stable_sort(byLetter.begin(), byLetter.end(),
                     [](const GroupDimension* a, const GroupDimension* b) { return letterLess(a->letter, b->letter); });
    for (const GroupDimension* gd : byLetter) addColumn(header("dim:" + gd->sheet + "/" + gd->id, gd->letter), gd->texts);
    // Элементы, подавленные хотя бы в одном исполнении (но не во всех одинаково): «есть»/«нет».
    for (std::size_t f = 0; f < model.configurationFeatures.size(); ++f) {
        std::vector<std::string> v;
        bool differs = false;
        for (std::size_t c = 0; c < n; ++c) {
            const auto& s = model.configurations[c].suppressed;
            const bool sup = f < s.size() && s[f];
            v.push_back(sup ? "нет" : "есть");
            differs = differs || v.back() != v.front();
        }
        if (differs) addColumn(header("suppress:" + model.configurationFeatures[f].first, model.configurationFeatures[f].second), v);
    }
    if (settings_.group.mass &&
        std::all_of(model.configurations.begin(), model.configurations.end(), [](const ConfigurationSnapshot& c) { return c.massKg.has_value(); })) {
        std::vector<std::string> v;
        for (const ConfigurationSnapshot& c : model.configurations) v.push_back(massText(*c.massKg));
        addColumn(header("mass", "Масса, кг"), v);
    }
    bool materialDiffers = false;
    for (const ConfigurationSnapshot& c : model.configurations)
        materialDiffers = materialDiffers || c.material != model.configurations.front().material;
    // Материал, различающийся у исполнений, записывается в таблицу всегда.
    if (settings_.group.material || materialDiffers) {
        std::vector<std::string> v;
        for (const ConfigurationSnapshot& c : model.configurations) v.push_back(c.material.empty() ? "–" : c.material);
        addColumn(header("material", "Материал"), v);
    }
    for (const GroupColumn& col : g.columns) g.table.headers.push_back(col.header);
    g.table.rows = std::move(cells);
    return g;
}

std::string DrawingDocument::groupLetterOf(const std::string& sheet, const std::string& id) const {
    if (!group_) return {};
    for (const GroupDimension& gd : group_->dimensions)
        if (gd.sheet == sheet && gd.id == id) return gd.letter;
    return {};
}

}  // namespace bendcore::drawing_v2::document
