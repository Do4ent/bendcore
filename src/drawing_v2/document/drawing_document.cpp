// Документ чертежа: проверка определений, обновление по модели (виды через кэш, компоновка,
// масштабы ГОСТ 2.302, переразрешение ссылок размеров и знаков, позиции по спецификации,
// основная надпись) и вывод листов. docs/drawing/DRAWING_DOCUMENT.md.

#include <bendcore/drawing_v2/drawing_document.hpp>

#include <algorithm>
#include <cmath>
#include <future>
#include <set>

namespace bendcore::drawing_v2::document {

using namespace views;
using annotations::Anchor;
using annotations::DimensionResult;
using annotations::DimensionState;
using annotations::ModelBinding;
using annotations::ModelRef;

namespace {

Error invalid(std::string message) { return Error{ErrorCode::InvalidInput, std::move(message)}; }

// Высота надписи над видом (renderViewLabel: отступ 5 + буквы 7) — запас при компоновке.
constexpr double kLabelRoom = 14.0;

struct Rect2 {
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    double w() const { return x1 - x0; }
    double h() const { return y1 - y0; }
    bool valid() const { return w() > 1.0 && h() > 1.0; }
};

std::uint64_t fnv(std::uint64_t h, const void* data, std::size_t n) {
    const auto* p = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}
std::uint64_t fnvDouble(std::uint64_t h, double v) {
    const long long q = std::llround(v * 1e9);
    return fnv(h, &q, sizeof q);
}

bool isGroupView(const ViewDef& v) {
    return v.kind == ViewKind::Base || v.kind == ViewKind::Projected || (v.kind == ViewKind::Section && v.side);
}

int rank(modeling::ResolveMethod m) {
    switch (m) {
        case modeling::ResolveMethod::Exact: return 0;
        case modeling::ResolveMethod::History: return 1;
        case modeling::ResolveMethod::Signature: return 2;
        case modeling::ResolveMethod::Broken: return 3;
    }
    return 3;
}

// Ряд масштабов ГОСТ 2.302 по убыванию, не больше maxScale.
std::vector<ScaleRatio> scalesUpTo(const ScaleRatio& maxScale) {
    std::vector<ScaleRatio> out;
    for (const ScaleRatio& s : gostScales())
        if (s.value() <= maxScale.value() * (1 + 1e-12)) out.push_back(s);
    return out;
}

std::string formatMass(double kg) {
    // ГОСТ 2.104: масса в килограммах без единицы; ЕСКД — десятичная запятая.
    const int decimals = kg < 1.0 ? 3 : (kg < 100.0 ? 2 : 1);
    return annotations::formatNumber(kg, decimals);
}

Box2 inflate(Box2 b, double top) {
    if (b.empty) return b;
    b.maxY += top;
    return b;
}

// Положение вида: точка привязки так, чтобы центр габарита (с местом под надпись сверху) был в c.
Point2 anchorForCenter(const ProjectedView& v, double scale, Point2 c, double labelRoom) {
    const double cx = 0.5 * (v.bounds.minX + v.bounds.maxX);
    const double cy = 0.5 * (v.bounds.minY + v.bounds.maxY);
    return {c.x - scale * cx, c.y - labelRoom / 2.0 - scale * cy};
}

// Точка и направление 2D-линии вида с параметром t.
struct LinePick {
    Point2 a, b;
    double length = 0;
};
std::optional<LinePick> straightPiece(const Curve2& c) {
    if (const auto* l = std::get_if<LineSeg>(&c)) {
        return LinePick{l->a, l->b, std::hypot(l->b.x - l->a.x, l->b.y - l->a.y)};
    }
    if (const auto* p = std::get_if<Polyline>(&c)) {
        if (p->points.size() < 2) return std::nullopt;
        LinePick best;
        for (std::size_t i = 0; i + 1 < p->points.size(); ++i) {
            const double len = std::hypot(p->points[i + 1].x - p->points[i].x, p->points[i + 1].y - p->points[i].y);
            if (len > best.length) best = {p->points[i], p->points[i + 1], len};
        }
        if (best.length > 0) return best;
    }
    return std::nullopt;
}

// Точка на кривой для выноски позиции и её «вес» (длина).
std::optional<std::pair<Point2, double>> pickPoint(const Curve2& c) {
    if (auto s = straightPiece(c)) return std::make_pair(Point2{0.5 * (s->a.x + s->b.x), 0.5 * (s->a.y + s->b.y)}, s->length);
    constexpr double kDeg = 3.14159265358979323846 / 180.0;
    if (const auto* a = std::get_if<Arc>(&c)) {
        double sweep = a->endDeg - a->startDeg;
        while (sweep <= 0) sweep += 360.0;
        const double mid = (a->startDeg + sweep / 2.0) * kDeg;
        return std::make_pair(Point2{a->center.x + a->radius * std::cos(mid), a->center.y + a->radius * std::sin(mid)},
                              a->radius * sweep * kDeg);
    }
    if (const auto* ci = std::get_if<Circle>(&c)) {
        const double ang = 45.0 * kDeg;
        return std::make_pair(Point2{ci->center.x + ci->radius * std::cos(ang), ci->center.y + ci->radius * std::sin(ang)},
                              2 * 3.14159265358979323846 * ci->radius);
    }
    return std::nullopt;
}

struct SheetRegions {
    Rect2 frame;
    Rect2 title;
    Rect2 group;
    Rect2 secondary;
};

// Области листа: рамка ГОСТ 2.104 (поля 20/5), основная надпись 185×55 в правом нижнем углу,
// технические требования над ней, знак «остальное» в правом верхнем углу.
SheetRegions computeRegions(const Sheet& sheet, const SheetDef& def, const DrawingSettings& st,
                            bool hasSecondary, const annotations::AnnotationStyle& style) {
    SheetRegions r;
    r.frame = {20.0, 5.0, sheet.widthMm - 5.0, sheet.heightMm - 5.0};
    r.title = {r.frame.x1 - 185.0, r.frame.y0, r.frame.x1, r.frame.y0 + 55.0};
    double trTop = r.title.y1;
    if (!def.technicalRequirements.empty()) {
        std::size_t lines = 0;
        const bool numbered = def.technicalRequirements.size() > 1;
        for (const std::string& item : def.technicalRequirements) {
            auto w = annotations::wrapText(numbered ? "1. " + item : item, 185.0, 5.0, style);
            lines += w ? w.value().size() : 1;
        }
        trTop += 10.0 + double(lines) * 1.7 * 5.0 + 5.0;
    }
    if (def.partsList) trTop += 8.0 * 9.0;  // перечень ISO 7573: заголовок + строки (оценка)
    const double m = st.margin;
    const double topReserve = def.roughnessRemainder ? 25.0 : 0.0;
    if (r.title.x0 - r.frame.x0 >= 150.0) {
        r.group = {r.frame.x0 + m, r.frame.y0 + m, r.title.x0 - m / 2.0, r.frame.y1 - m};
        r.secondary = {r.title.x0 + m / 2.0, trTop + m, r.frame.x1 - m, r.frame.y1 - m - topReserve};
        if (!hasSecondary) {
            // Без видов вне связи вся высота над основной надписью доступна группе справа.
            r.group.y1 = r.frame.y1 - m - topReserve;
        }
    } else {
        const Rect2 b{r.frame.x0 + m, trTop + m, r.frame.x1 - m, r.frame.y1 - m - topReserve};
        r.group = b;
        if (hasSecondary) {
            r.group.x1 = b.x0 + 0.6 * b.w();
            r.secondary = {r.group.x1 + m / 2.0, b.y0, b.x1, b.y1};
        }
    }
    return r;
}

struct PlacedGroup {
    ScaleRatio scale;
    std::map<std::string, Point2> anchors;
};

}  // namespace

const char* toString(ViewKind kind) noexcept {
    switch (kind) {
        case ViewKind::Base: return "base";
        case ViewKind::Projected: return "projected";
        case ViewKind::Axonometric: return "axonometric";
        case ViewKind::Section: return "section";
        case ViewKind::Detail: return "detail";
    }
    return "base";
}

SheetDef* DrawingDocument::findSheet(const std::string& id) {
    for (SheetDef& s : sheets_)
        if (s.id == id) return &s;
    return nullptr;
}

Status DrawingDocument::validate() const {
    if (sheets_.empty()) return invalid("drawing: no sheets");
    if (!(settings_.lineThickness >= 0.5 && settings_.lineThickness <= 1.4))
        return invalid("drawing: line thickness s must be 0.5..1.4 mm (GOST 2.303)");
    std::set<std::string> sheetIds;
    for (const SheetDef& s : sheets_) {
        if (s.id.empty() || !sheetIds.insert(s.id).second) return invalid("drawing: empty or duplicate sheet id '" + s.id + "'");
        std::map<std::string, const ViewDef*> seen;
        int bases = 0;
        for (const ViewDef& v : s.views) {
            const std::string where = "sheet '" + s.id + "', view '" + v.id + "'";
            if (v.id.empty() || seen.count(v.id)) return invalid("drawing: empty or duplicate view id in " + where);
            auto needParent = [&](bool required) -> Status {
                if (v.parent.empty()) {
                    if (required) return invalid("drawing: " + where + " needs a parent view");
                    return okStatus();
                }
                auto it = seen.find(v.parent);
                if (it == seen.end()) return invalid("drawing: " + where + ": parent '" + v.parent + "' is not declared before it");
                if (it->second->kind == ViewKind::Axonometric || it->second->kind == ViewKind::Detail)
                    return invalid("drawing: " + where + ": parent must be an orthogonal view or a section");
                return okStatus();
            };
            switch (v.kind) {
                case ViewKind::Base:
                    ++bases;
                    if (!v.parent.empty() || v.side) return invalid("drawing: " + where + ": the base view has no parent");
                    break;
                case ViewKind::Projected:
                    if (!v.side) return invalid("drawing: " + where + ": a projected view needs a side");
                    if (auto st = needParent(true); !st) return st;
                    break;
                case ViewKind::Axonometric:
                    if (auto st = needParent(true); !st) return st;
                    break;
                case ViewKind::Section: {
                    if (auto st = needParent(bool(v.side)); !st) return st;
                    if (v.markView.empty() || !seen.count(v.markView))
                        return invalid("drawing: " + where + ": markView must name a view declared before the section");
                    const ViewKind mk = seen[v.markView]->kind;
                    if (mk == ViewKind::Axonometric || mk == ViewKind::Detail)
                        return invalid("drawing: " + where + ": the cutting plane is marked on an orthogonal view");
                    break;
                }
                case ViewKind::Detail:
                    if (auto st = needParent(true); !st) return st;
                    if (!(v.detailRadius > 0)) return invalid("drawing: " + where + ": detail radius must be positive");
                    break;
            }
            if (v.side && seen.count(v.parent) && !isGroupView(*seen[v.parent]))
                return invalid("drawing: " + where + ": a view in projection relation needs a parent in the group");
            seen[v.id] = &v;
        }
        if (bases != 1) return invalid("drawing: sheet '" + s.id + "' must have exactly one base view");
        std::set<std::string> annIds;
        for (const DimensionItem& d : s.dimensions) {
            if (d.id.empty() || !annIds.insert(d.id).second) return invalid("drawing: empty or duplicate annotation id '" + d.id + "'");
            if (!seen.count(d.view)) return invalid("drawing: dimension '" + d.id + "' refers to unknown view '" + d.view + "'");
        }
        for (const RoughnessItem& r : s.roughness) {
            if (r.id.empty() || !annIds.insert(r.id).second) return invalid("drawing: empty or duplicate annotation id '" + r.id + "'");
            if (!seen.count(r.view)) return invalid("drawing: roughness '" + r.id + "' refers to unknown view '" + r.view + "'");
            if (r.edge.kind != modeling::ElementKind::Edge) return invalid("drawing: roughness '" + r.id + "' must reference an edge");
        }
    }
    return okStatus();
}

StalenessReport DrawingDocument::checkModel(const ModelSnapshot& model) const {
    StalenessReport r;
    r.storedHash = model_.resultHash;
    r.currentHash = hashHex(model.contentHash);
    r.modelChanged = r.storedHash != r.currentHash;
    if (r.modelChanged) {
        for (const SheetDef& s : sheets_) {
            for (const ViewDef& v : s.views) r.staleViews.push_back(s.id + "/" + v.id);
            for (const DimensionItem& d : s.dimensions) r.dimensions.push_back(s.id + "/" + d.id);
        }
    }
    return r;
}

Result<UpdateReport> DrawingDocument::update(const ModelLoader& loader, ViewCache& cache,
                                             const annotations::Specification* spec) {
    if (!loader) return invalid("drawing update: no model loader");
    auto snap = loader(model_);
    if (!snap) return snap.error();
    if (snap.value().kind != model_.kind)
        return invalid(std::string("drawing update: the link expects a ") + toString(model_.kind) + " model");
    return update(snap.value(), cache, spec);
}

Result<UpdateReport> DrawingDocument::update(const ModelSnapshot& model, ViewCache& cache,
                                             const annotations::Specification* specIn) {
    if (auto st = validate(); !st) return st.error();
    if (model.shape.isNull() || !model.named) return invalid("drawing update: empty model snapshot");
    if (model.kind != model_.kind)
        return invalid(std::string("drawing update: the document references a ") + toString(model_.kind) +
                       " but the snapshot is a " + toString(model.kind));

    UpdateReport report;
    report.previousHash = model_.resultHash;
    report.currentHash = hashHex(model.contentHash);
    report.modelChanged = report.previousHash != report.currentHash;

    annotations::AnnotationStyle style;
    style.font = font_;

    // Привязка размеров: деталь — имена из истории операций, сборка — только сигнатуры.
    auto bindingR = model.historyNames ? ModelBinding::fromNamedShape(*model.named) : ModelBinding::fromShape(model.shape);
    if (!bindingR) return bindingR.error();
    const ModelBinding& binding = bindingR.value();

    // Спецификация для позиций сборочного чертежа.
    std::optional<annotations::Specification> ownSpec;
    const annotations::Specification* spec = specIn;
    if (model.kind == ModelKind::Assembly && !spec) {
        auto built = annotations::buildSpecification(*model.assembly);
        if (!built) return built.error();
        ownSpec = std::move(built).value();
        spec = &*ownSpec;
    }

    // Буквы обозначений разрезов и выносных элементов по всему чертежу (ГОСТ 2.316).
    std::map<std::pair<std::size_t, std::string>, std::string> letters;
    {
        std::set<std::string> used;
        for (const SheetDef& s : sheets_)
            for (const ViewDef& v : s.views)
                if ((v.kind == ViewKind::Section || v.kind == ViewKind::Detail) && !v.letter.empty()) used.insert(v.letter);
        int next = 0;
        for (std::size_t si = 0; si < sheets_.size(); ++si) {
            for (const ViewDef& v : sheets_[si].views) {
                if (v.kind != ViewKind::Section && v.kind != ViewKind::Detail) continue;
                std::string l = v.letter;
                while (l.empty()) {
                    std::string cand = designationLetter(next++);
                    if (!used.count(cand)) l = cand;
                    if (next > 100) return invalid("drawing update: too many designations");
                }
                used.insert(l);
                letters[{si, v.id}] = l;
            }
        }
    }

    const ViewCacheStats statsBefore = cache.stats();
    std::size_t sectionsComputed = 0, sectionsCached = 0;

    std::vector<ComputedSheet> out;
    out.reserve(sheets_.size());
    const int sheetCount = int(sheets_.size());
    for (std::size_t si = 0; si < sheets_.size(); ++si) {
        SheetDef& sd = sheets_[si];
        auto sheetR = gostSheet(sd.format.base, sd.format.multiplier, sd.format.orientation);
        if (!sheetR) return sheetR.error();
        ComputedSheet cs;
        cs.id = sd.id;
        cs.sheet = sheetR.value();
        cs.index = int(si) + 1;
        cs.count = sheetCount;

        // 1. Ориентации видов.
        std::map<std::string, ViewFrame> frames;
        std::map<std::string, std::size_t> indexOf;
        for (std::size_t vi = 0; vi < sd.views.size(); ++vi) {
            const ViewDef& v = sd.views[vi];
            indexOf[v.id] = vi;
            ViewFrame f;
            const std::string where = "sheet '" + sd.id + "', view '" + v.id + "'";
            switch (v.kind) {
                case ViewKind::Base: {
                    auto fr = makeFrame(v.toward, v.right);
                    if (!fr) return Error{fr.error().code, where + ": " + fr.error().message};
                    f = fr.value();
                    break;
                }
                case ViewKind::Projected: f = projectedFrame(frames.at(v.parent), *v.side, settings_.projection); break;
                case ViewKind::Axonometric: f = axonometricFrame(frames.at(v.parent), v.axonometry); break;
                case ViewKind::Section: {
                    if (v.side) {
                        f = projectedFrame(frames.at(v.parent), *v.side, settings_.projection);
                    } else {
                        auto fr = makeFrame(v.toward, v.right);
                        if (!fr) return Error{fr.error().code, where + ": " + fr.error().message};
                        f = fr.value();
                    }
                    break;
                }
                case ViewKind::Detail: f = frames.at(v.parent); break;
            }
            frames[v.id] = f;
        }

        // 2. Запросы HLR (параллельно, через кэш) и разрезы (кэш документа).
        std::map<std::string, std::shared_future<ViewResultPtr>> pending;
        std::map<std::string, std::shared_ptr<const SectionView>> sections;
        std::map<std::string, bool> cachedFlag;
        for (const ViewDef& v : sd.views) {
            HlrOptions ho;
            ho.method = v.method;
            ho.hidden = v.hidden;
            if (v.kind == ViewKind::Base || v.kind == ViewKind::Projected || v.kind == ViewKind::Axonometric) {
                const ViewCacheStats b = cache.stats();
                pending[v.id] = cache.request(ViewRequest{model.shape, frames[v.id], ho, v.autoMethod});
                const ViewCacheStats a = cache.stats();
                cachedFlag[v.id] = a.computed == b.computed;
            } else if (v.kind == ViewKind::Section) {
                const ViewFrame& f = frames[v.id];
                const CuttingPlane plane{v.planePoint, -1.0 * f.toward};
                std::uint64_t key = 1469598103934665603ull;
                key = fnv(key, &model.contentHash, sizeof model.contentHash);
                const std::uint64_t ph = viewParametersHash(f, ho);
                key = fnv(key, &ph, sizeof ph);
                for (double c : {plane.point.x, plane.point.y, plane.point.z}) key = fnvDouble(key, c);
                const int kind = int(v.sectionKind);
                key = fnv(key, &kind, sizeof kind);
                auto it = sectionCache_.find(key);
                if (it != sectionCache_.end()) {
                    sections[v.id] = it->second;
                    cachedFlag[v.id] = true;
                    ++sectionsCached;
                } else {
                    auto sec = sectionShape(model.shape, f, plane, v.sectionKind, ho);
                    if (!sec) return Error{sec.error().code, "sheet '" + sd.id + "', section '" + v.id + "': " + sec.error().message};
                    auto ptr = std::make_shared<const SectionView>(std::move(sec).value());
                    if (sectionCache_.size() >= 64) sectionCache_.erase(sectionCache_.begin());
                    sectionCache_[key] = ptr;
                    sections[v.id] = ptr;
                    cachedFlag[v.id] = false;
                    ++sectionsComputed;
                }
            }
        }
        std::map<std::string, ProjectedView> projected;
        for (const ViewDef& v : sd.views) {
            if (pending.count(v.id)) {
                ViewResultPtr res = pending[v.id].get();
                if (!res || !res->ok())
                    return Error{res ? res->error().code : ErrorCode::OperationFailed,
                                 "sheet '" + sd.id + "', view '" + v.id + "': " + (res ? res->error().message : "no result")};
                projected[v.id] = res->value();
            } else if (sections.count(v.id)) {
                projected[v.id] = sections[v.id]->view;
            } else if (v.kind == ViewKind::Detail) {
                auto dv = detailView(projected.at(v.parent), v.detailCenter, v.detailRadius);
                if (!dv) return Error{dv.error().code, "sheet '" + sd.id + "', detail '" + v.id + "': " + dv.error().message};
                projected[v.id] = std::move(dv).value();
                cachedFlag[v.id] = cachedFlag[v.parent];
            }
            if (projected[v.id].bounds.empty)
                return invalid("sheet '" + sd.id + "', view '" + v.id + "': the view has no geometry");
        }

        // 3. Компоновка.
        bool hasSecondary = false;
        for (const ViewDef& v : sd.views) hasSecondary = hasSecondary || !isGroupView(v);
        SheetRegions reg = computeRegions(cs.sheet, sd, settings_, hasSecondary, style);
        if (!reg.group.valid()) return invalid("sheet '" + sd.id + "': no room for views on the sheet");

        std::vector<const ViewDef*> group;
        for (const ViewDef& v : sd.views)
            if (isGroupView(v)) group.push_back(&v);
        PlacedGroup pg;
        {
            std::vector<ScaleRatio> candidates = sd.scale ? std::vector<ScaleRatio>{*sd.scale} : scalesUpTo(settings_.maxScale);
            bool placedOk = false;
            for (const ScaleRatio& s : candidates) {
                std::map<std::string, SheetView> placed;
                Box2 all;
                for (const ViewDef* v : group) {
                    SheetView sv{projected[v->id], s.value(), {0, 0}};
                    if (v->kind != ViewKind::Base) sv.anchor = alignedAnchor(placed.at(v->parent), projected[v->id], s.value(), *v->side, settings_.viewGap);
                    Box2 b = sv.sheetBounds();
                    if (v->kind == ViewKind::Section) b = inflate(b, kLabelRoom);
                    all.add({b.minX, b.minY});
                    all.add({b.maxX, b.maxY});
                    placed.emplace(v->id, std::move(sv));
                }
                const bool fits = all.width() <= reg.group.w() && all.height() <= reg.group.h();
                if (!fits && !sd.scale) continue;
                if (!fits) report.warnings.push_back("sheet '" + sd.id + "': views at " + s.label() + " exceed the view area");
                Point2 c{0.5 * (reg.group.x0 + reg.group.x1), 0.5 * (reg.group.y0 + reg.group.y1)};
                const ViewDef* base = group.front();
                if (base->position) c = *base->position;
                const double dx = c.x - 0.5 * (all.minX + all.maxX), dy = c.y - 0.5 * (all.minY + all.maxY);
                pg.scale = s;
                for (auto& [id, sv] : placed) pg.anchors[id] = {sv.anchor.x + dx, sv.anchor.y + dy};
                placedOk = true;
                break;
            }
            if (!placedOk) return invalid("sheet '" + sd.id + "': the views do not fit the sheet at any GOST 2.302 scale");
        }
        cs.scale = pg.scale;

        // Виды вне проекционной связи — столбцом в своей области.
        std::vector<const ViewDef*> secondary;
        for (const ViewDef& v : sd.views)
            if (!isGroupView(v)) secondary.push_back(&v);
        std::map<std::string, std::pair<double, ScaleRatio>> secScale;  // масштаб изображения и ряд ГОСТ
        std::map<std::string, Point2> secAnchor;
        if (!secondary.empty()) {
            Rect2 area = reg.secondary.valid() ? reg.secondary : reg.group;
            const double slotH = area.h() / double(secondary.size());
            for (std::size_t k = 0; k < secondary.size(); ++k) {
                const ViewDef& v = *secondary[k];
                const ProjectedView& pv = projected[v.id];
                const double factor =
                    v.kind == ViewKind::Axonometric ? axonometricFactor(v.axonometry, settings_.reducedAxonometry) : 1.0;
                const double room = v.kind == ViewKind::Axonometric ? 0.0 : kLabelRoom;
                ScaleRatio want = pg.scale;
                if (v.scale) {
                    want = *v.scale;
                } else if (v.kind == ViewKind::Detail) {
                    // Ближайший масштаб ряда не меньше удвоенного масштаба листа.
                    for (auto it = gostScales().rbegin(); it != gostScales().rend(); ++it) {
                        if (it->value() >= 2.0 * pg.scale.value() - 1e-12) {
                            want = *it;
                            break;
                        }
                    }
                }
                const Rect2 slot{area.x0, area.y1 - slotH * double(k + 1), area.x1, area.y1 - slotH * double(k)};
                auto fitsAt = [&](const ScaleRatio& s) {
                    const double sc = s.value() * factor;
                    return pv.bounds.width() * sc <= slot.w() && pv.bounds.height() * sc + room <= slot.h();
                };
                ScaleRatio chosen = want;
                if (!fitsAt(want)) {
                    bool found = false;
                    for (const ScaleRatio& s : gostScales()) {
                        if (s.value() > want.value() + 1e-12) continue;
                        if (fitsAt(s)) {
                            chosen = s;
                            found = true;
                            break;
                        }
                    }
                    if (!found) return invalid("sheet '" + sd.id + "': view '" + v.id + "' does not fit its area");
                    if (v.scale) report.warnings.push_back("sheet '" + sd.id + "': view '" + v.id + "' reduced to " + chosen.label());
                }
                Point2 c{0.5 * (slot.x0 + slot.x1), 0.5 * (slot.y0 + slot.y1)};
                if (v.position) c = *v.position;
                secScale[v.id] = {chosen.value() * factor, chosen};
                secAnchor[v.id] = anchorForCenter(pv, chosen.value() * factor, c, room);
            }
        }

        // 4. Вычисленные виды: масштаб, привязка, надписи, осевые.
        for (const ViewDef& v : sd.views) {
            ComputedView cv;
            cv.def = v;
            cv.id = v.id;
            cv.kind = v.kind;
            cv.fromCache = cachedFlag[v.id];
            if (isGroupView(v)) {
                cv.sheetView = SheetView{projected[v.id], pg.scale.value(), pg.anchors.at(v.id)};
                cv.scale = pg.scale;
            } else {
                cv.sheetView = SheetView{projected[v.id], secScale[v.id].first, secAnchor[v.id]};
                cv.scale = secScale[v.id].second;
            }
            if (sections.count(v.id)) cv.section = sections[v.id];
            if (v.kind == ViewKind::Section || v.kind == ViewKind::Detail) {
                cv.letter = letters.at({si, v.id});
                // ГОСТ 2.305: «А–А»; масштаб — в скобках, если отличается от масштаба листа;
                // выносной элемент — буква и масштаб всегда.
                if (v.kind == ViewKind::Section) {
                    cv.label = cv.letter + "–" + cv.letter;
                    if (std::abs(cv.scale.value() - pg.scale.value()) > 1e-12) cv.label += " (" + cv.scale.label() + ")";
                } else {
                    cv.label = cv.letter + " (" + cv.scale.label() + ")";
                }
            }
            if (v.centerLines && v.kind != ViewKind::Axonometric && v.kind != ViewKind::Detail) {
                const kernel::Shape& src = v.kind == ViewKind::Section ? cv.section->remaining : model.shape;
                auto cg = findCenterGeometry(src, frames[v.id]);
                if (!cg) return Error{cg.error().code, "sheet '" + sd.id + "', view '" + v.id + "': " + cg.error().message};
                cv.centers = std::move(cg).value();
            }
            cs.views.push_back(std::move(cv));
        }
        // Секущая плоскость должна быть видна на виде обозначения следом.
        for (const ComputedView& cv : cs.views) {
            if (cv.kind != ViewKind::Section) continue;
            const ComputedView& mark = cs.views[indexOf.at(cv.def.markView)];
            const CuttingPlane plane{cv.def.planePoint, -1.0 * cv.sheetView.view.frame.toward};
            auto tr = cuttingTrace(mark.sheetView.view.frame, mark.sheetView.view.bounds, plane);
            if (!tr) return invalid("sheet '" + sd.id + "', section '" + cv.id + "': the plane cannot be marked on '" +
                                    mark.id + "': " + tr.error().message);
        }

        // 5. Размеры: перепривязка ссылок к новой форме и пересчёт по виду.
        cs.dimensions.resize(sd.dimensions.size());
        for (std::size_t di = 0; di < sd.dimensions.size(); ++di) {
            DimensionItem& item = sd.dimensions[di];
            const ComputedView& cv = cs.views[indexOf.at(item.view)];
            DimensionReport dr;
            dr.sheet = sd.id;
            dr.id = item.id;
            dr.oldValue = item.lastValue;
            annotations::DimensionDef def = item.def;
            std::string broken;
            modeling::ResolveMethod worst = modeling::ResolveMethod::Exact;
            auto rebindAll = [&](std::vector<Anchor>& anchors) {
                for (Anchor& a : anchors) {
                    const std::string label = a.ref.persistent.name.empty() ? "unnamed element" : "'" + a.ref.persistent.name + "'";
                    auto rb = annotations::rebindRef(a.ref, binding);
                    if (rank(rb.method) > rank(worst)) worst = rb.method;
                    if (!rb.ok && broken.empty()) broken = "reference " + label + " is broken: " + rb.detail;
                }
            };
            rebindAll(def.anchors);
            rebindAll(def.offsetFrom);
            dr.method = worst;
            DimensionResult res;
            if (!broken.empty()) {
                res.state = DimensionState::Dangling;
                res.reason = broken;
            } else {
                auto ev = annotations::evaluateDimension(def, binding, cv.sheetView, style);
                if (!ev) return Error{ev.error().code, "sheet '" + sd.id + "', dimension '" + item.id + "': " + ev.error().message};
                res = std::move(ev).value();
                // Ссылки «следуют» за элементами: сохраняются новые имена и сигнатуры.
                item.def.anchors = def.anchors;
                item.def.offsetFrom = def.offsetFrom;
            }
            dr.state = res.state;
            dr.reason = res.reason;
            if (res.state == DimensionState::Ok) {
                dr.newValue = res.value;
                dr.changed = item.lastValue && std::abs(*item.lastValue - res.value) > 1e-9;
                if (dr.changed) {
                    report.stale.push_back(sd.id + "/" + item.id + ": " + annotations::formatNumber(*item.lastValue, 4) +
                                           " → " + annotations::formatNumber(res.value, 4));
                }
                item.lastValue = res.value;
            } else {
                report.dangling.push_back(sd.id + "/" + item.id + ": " + res.reason);
            }
            report.dimensions.push_back(dr);
            cs.dimensions[di] = std::move(res);
        }

        // 6. Знаки шероховатости на изображениях рёбер.
        for (RoughnessItem& ri : sd.roughness) {
            const ComputedView& cv = cs.views[indexOf.at(ri.view)];
            ModelRef ref;
            ref.kind = annotations::RefKind::Edge;
            ref.persistent = ri.edge;
            auto rb = annotations::rebindRef(ref, binding);
            std::optional<LinePick> best;
            if (rb.ok) {
                for (const ViewEdge& e : cv.sheetView.view.edges) {
                    if (e.visibility != Visibility::Visible || !e.edgeIndex || *e.edgeIndex != ref.index) continue;
                    if (auto p = straightPiece(e.curve); p && (!best || p->length > best->length)) best = p;
                }
            }
            if (!rb.ok || !best) {
                report.dangling.push_back(sd.id + "/" + ri.id + ": " +
                                          (rb.ok ? "edge is not shown as a straight visible line in view '" + ri.view + "'"
                                                 : "reference is broken: " + rb.detail));
                cs.roughness.emplace_back(std::nullopt);
                continue;
            }
            ri.edge = ref.persistent;
            Point2 a = cv.sheetView.toSheet(best->a), b = cv.sheetView.toSheet(best->b);
            // Знак — с левой стороны направления линии; сторона выбирается наружу от центра вида.
            const Box2 vb = cv.sheetView.sheetBounds();
            const Point2 c{0.5 * (vb.minX + vb.maxX), 0.5 * (vb.minY + vb.maxY)};
            const Point2 mid{0.5 * (a.x + b.x), 0.5 * (a.y + b.y)};
            const double leftX = -(b.y - a.y), leftY = b.x - a.x;
            if (leftX * (mid.x - c.x) + leftY * (mid.y - c.y) < 0) std::swap(a, b);
            const double t = std::clamp(ri.at, 0.0, 1.0);
            const Point2 tip{a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)};
            const double ang = std::atan2(b.y - a.y, b.x - a.x) * 180.0 / 3.14159265358979323846;
            // Знак, параметр которого читался бы «вверх ногами» (линия, обращённая наружу вниз или
            // влево вниз), ставится на полке линии-выноски со стрелкой на линии поверхности
            // (ГОСТ 2.309, ГОСТ 2.316).
            const bool readable = ang > -90.0 + 1e-9 && ang <= 90.0 + 1e-9;
            if (readable) {
                auto sign = annotations::roughnessSign(tip, ang, ri.spec, style);
                if (!sign) return Error{sign.error().code, "sheet '" + sd.id + "', roughness '" + ri.id + "': " + sign.error().message};
                cs.roughness.emplace_back(std::move(sign).value());
            } else {
                const double len = std::hypot(leftX, leftY);
                const double nx = leftX / len * (leftX * (mid.x - c.x) + leftY * (mid.y - c.y) < 0 ? -1.0 : 1.0);
                const double ny = leftY / len * (leftX * (mid.x - c.x) + leftY * (mid.y - c.y) < 0 ? -1.0 : 1.0);
                // Выноска наружу на 10 мм и вбок под 45°, полка вправо длиной 2,5·H знака.
                const double out = 10.0;
                const Point2 knee{tip.x + out * nx + out * 0.7, tip.y + out * ny};
                const double shelf = 2.5 * ri.spec.heightFactor * style.textHeight;
                auto lead = annotations::frameLeader({knee, tip}, style);
                if (!lead) return lead.error();
                annotations::SymbolGeometry g = std::move(lead).value();
                g.lines.push_back({knee, {knee.x + shelf, knee.y}});
                auto sign = annotations::roughnessSign({knee.x + 0.3 * shelf, knee.y}, 0.0, ri.spec, style);
                if (!sign) return Error{sign.error().code, "sheet '" + sd.id + "', roughness '" + ri.id + "': " + sign.error().message};
                g.append(sign.value());
                cs.roughness.emplace_back(std::move(g));
            }
        }

        // 7. Позиции по спецификации (ГОСТ 2.109): выноски на видимые линии составных частей.
        for (const ComputedView& cv : cs.views) {
            if (!cv.def.autoBalloons) continue;
            if (model.kind != ModelKind::Assembly)
                return invalid("sheet '" + sd.id + "', view '" + cv.id + "': positions need an assembly model");
            BalloonReport br;
            br.sheet = sd.id;
            br.view = cv.id;
            std::vector<annotations::Balloon> balloons;
            for (const annotations::SpecSectionData& sec : spec->sections) {
                for (const annotations::SpecItem& it : sec.items) {
                    if (!it.definition || it.position <= 0) continue;
                    std::optional<std::pair<Point2, double>> best;
                    for (const ViewEdge& e : cv.sheetView.view.edges) {
                        if (e.visibility != Visibility::Visible || !e.edgeIndex || *e.edgeIndex >= model.edgeOwner.size()) continue;
                        if (model.edgeOwner[*e.edgeIndex] != *it.definition) continue;
                        auto p = pickPoint(e.curve);
                        if (p && (!best || p->second > best->second)) best = p;
                    }
                    if (!best) {
                        br.withoutBalloon.push_back(*it.definition);
                        continue;
                    }
                    annotations::Balloon b;
                    b.definition = *it.definition;
                    b.target = cv.sheetView.toSheet(best->first);
                    balloons.push_back(b);
                }
            }
            if (balloons.empty()) {
                report.warnings.push_back("sheet '" + sd.id + "', view '" + cv.id + "': no assembly part is visible for positions");
            } else {
                // Полки в колонну справа от вида (слева, если справа нет места), сверху вниз в
                // порядке точек выносок, шаг не менее 10 мм.
                const Box2 vb = cv.sheetView.sheetBounds();
                std::sort(balloons.begin(), balloons.end(),
                          [](const annotations::Balloon& a, const annotations::Balloon& b) { return a.target.y > b.target.y; });
                const bool right = vb.maxX + 40.0 <= reg.frame.x1 - 5.0;
                const double x = right ? vb.maxX + 15.0 : vb.minX - 15.0;
                const double step = 10.0;
                double prev = 1e300;
                for (auto& b : balloons) {
                    double y = std::min(b.target.y + 8.0, prev - step);
                    b.at = {x, y};
                    prev = y;
                }
                const double lowest = balloons.back().at.y;
                const double floorY = reg.frame.y0 + 10.0;
                if (lowest < floorY)
                    for (auto& b : balloons) b.at.y += floorY - lowest;
                annotations::alignBalloonsInColumn(balloons, x);
                auto sync = annotations::synchronizeBalloons(balloons, *spec);
                if (!sync) return sync.error();
                for (const auto& b : balloons)
                    if (auto pos = spec->positionOf(b.definition)) br.placed.emplace_back(*pos, b.definition);
                for (auto& b : balloons) cs.balloons.push_back(b);
            }
            report.balloons.push_back(std::move(br));
        }
        if (sd.partsList) {
            if (model.kind != ModelKind::Assembly) return invalid("sheet '" + sd.id + "': a parts list needs an assembly model");
            cs.specification = *spec;
        }

        // 8. Основная надпись.
        TitleBlockData& td = cs.title;
        const ModelProperties& mp = model.kind == ModelKind::Part ? model_.properties : model.properties;
        td.designation = !sd.title.designation.empty() ? sd.title.designation
                                                       : (mp.designation + (model.kind == ModelKind::Assembly && !mp.designation.empty() ? " СБ" : ""));
        td.name = !sd.title.name.empty() ? sd.title.name : mp.name;
        td.material = !sd.title.material.empty() ? sd.title.material : (model.kind == ModelKind::Part ? mp.material : "");
        td.mass = !sd.title.mass.empty() ? sd.title.mass : (model.massKg ? formatMass(*model.massKg) : "");
        td.litera = sd.title.litera;
        td.organization = sd.title.organization;
        td.developer = sd.title.developer;
        td.checker = sd.title.checker;
        td.techControl = sd.title.techControl;
        td.normControl = sd.title.normControl;
        td.approver = sd.title.approver;
        td.scale = cs.scale.label();
        td.sheet = cs.index;
        td.sheets = cs.count;
        out.push_back(std::move(cs));
    }

    const ViewCacheStats statsAfter = cache.stats();
    report.viewsComputed = (statsAfter.computed - statsBefore.computed) + sectionsComputed;
    report.viewsFromCache = (statsAfter.hits - statsBefore.hits) + (statsAfter.joined - statsBefore.joined) + sectionsCached;
    if (report.modelChanged) ++model_.revision;
    model_.resultHash = report.currentHash;
    report.revision = model_.revision;
    computed_ = std::move(out);
    return report;
}

Result<std::vector<Drawing>> DrawingDocument::render(const TrueTypeFont* font) const {
    if (computed_.empty() || computed_.size() != sheets_.size()) return invalid("drawing render: the document is not updated");
    annotations::AnnotationStyle style;
    style.font = font;
    std::vector<Drawing> out;
    for (std::size_t si = 0; si < computed_.size(); ++si) {
        const ComputedSheet& cs = computed_[si];
        const SheetDef& sd = sheets_[si];
        auto dr = Drawing::create(cs.sheet, settings_.lineThickness);
        if (!dr) return dr.error();
        Drawing d = std::move(dr).value();
        FrameOptions fo;
        fo.form = si == 0 ? TitleBlockForm::Form1 : TitleBlockForm::Form2a;
        fo.font = font;
        auto lay = addFrameAndTitleBlock(d, cs.title, fo);
        if (!lay) return Error{lay.error().code, "sheet '" + cs.id + "': " + lay.error().message};
        if (auto st = ensureViewLayers(d); !st) return st.error();
        std::map<std::string, const ComputedView*> byId;
        for (const ComputedView& cv : cs.views) byId[cv.id] = &cv;
        for (const ComputedView& cv : cs.views) {
            RenderOptions ro;
            ro.hidden = cv.def.hidden;
            auto fail = [&](const Error& e) { return Error{e.code, "sheet '" + cs.id + "', view '" + cv.id + "': " + e.message}; };
            if (auto st = renderView(d, cv.sheetView, ro); !st) return fail(st.error());
            if (cv.section) {
                if (auto h = renderSectionHatch(d, cv.sheetView, *cv.section, 0.0, ro); !h) return fail(h.error());
            }
            if (!cv.centers.marks.empty() || !cv.centers.axes.empty()) {
                if (auto st = renderCenterLines(d, cv.sheetView, cv.centers, ro); !st) return fail(st.error());
            }
            if (!cv.label.empty()) {
                if (auto st = renderViewLabel(d, cv.sheetView, cv.label, ro); !st) return fail(st.error());
            }
            if (cv.kind == ViewKind::Section) {
                const CuttingPlane plane{cv.def.planePoint, -1.0 * cv.sheetView.view.frame.toward};
                auto mk = renderCuttingPlaneMark(d, byId.at(cv.def.markView)->sheetView, plane, cv.letter, 12.0, ro);
                if (!mk) return fail(mk.error());
            }
            if (cv.kind == ViewKind::Detail) {
                const ComputedView& parent = *byId.at(cv.def.parent);
                if (auto st = renderDetailMark(d, parent.sheetView, cv.def.detailCenter, cv.def.detailRadius, cv.letter, ro); !st)
                    return fail(st.error());
                if (auto st = renderDetailBoundary(d, cv.sheetView, cv.def.detailCenter, cv.def.detailRadius, ro); !st)
                    return fail(st.error());
            }
        }
        for (std::size_t di = 0; di < cs.dimensions.size(); ++di) {
            if (cs.dimensions[di].state != DimensionState::Ok) continue;
            if (auto st = annotations::renderDimension(d, cs.dimensions[di], style); !st)
                return Error{st.error().code, "sheet '" + cs.id + "', dimension '" + sd.dimensions[di].id + "': " + st.error().message};
        }
        for (const auto& g : cs.roughness) {
            if (!g) continue;
            if (auto st = annotations::renderSymbol(d, *g, style); !st) return st.error();
        }
        if (sd.roughnessRemainder) {
            auto g = annotations::roughnessRemainder(lay.value().frame, *sd.roughnessRemainder, style);
            if (!g) return g.error();
            if (auto st = annotations::renderSymbol(d, g.value(), style); !st) return st.error();
        }
        for (const annotations::Balloon& b : cs.balloons) {
            auto g = annotations::balloonGeometry(b, {}, style);
            if (!g) return g.error();
            if (auto st = annotations::renderSymbol(d, g.value(), style); !st) return st.error();
        }
        TitleBlockLayout above = lay.value();
        if (sd.partsList && cs.specification) {
            annotations::PartsListOptions po;
            po.font = font;
            auto table = annotations::addPartsList(d, lay.value(), *cs.specification, po);
            if (!table) return Error{table.error().code, "sheet '" + cs.id + "', parts list: " + table.error().message};
            above.titleBlock.y1 = table.value().y1;
        }
        if (!sd.technicalRequirements.empty()) {
            auto tr = annotations::addTechnicalRequirements(d, above, sd.technicalRequirements, {}, style);
            if (!tr) return Error{tr.error().code, "sheet '" + cs.id + "', technical requirements: " + tr.error().message};
        }
        out.push_back(std::move(d));
    }
    return out;
}

}  // namespace bendcore::drawing_v2::document
