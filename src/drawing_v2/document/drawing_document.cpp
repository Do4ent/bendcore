// Документ чертежа: проверка определений, обновление по модели (виды через кэш, компоновка,
// масштабы ГОСТ 2.302, переразрешение ссылок размеров и знаков, позиции по спецификации,
// основная надпись) и вывод листов. docs/drawing/DRAWING_DOCUMENT.md.

#include <bendcore/drawing_v2/drawing_document.hpp>

#include <algorithm>
#include <cctype>
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
constexpr double kLabelRoom = kViewLabelRoom;

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

// Деталь сборки (дочерняя форма), которой принадлежит ребро вида: по ребру или грани модели; у
// контура фигуры сечения без ссылок — по области сечения, на контуре которой лежит ребро.
std::optional<std::size_t> edgeChild(const ModelSnapshot& model, const ViewEdge& e, const SectionView* section) {
    if (e.edgeIndex && *e.edgeIndex < model.edgeChild.size() && model.edgeChild[*e.edgeIndex] != UINT32_MAX)
        return model.edgeChild[*e.edgeIndex];
    if (e.faceIndex && *e.faceIndex < model.faceChild.size() && model.faceChild[*e.faceIndex] != UINT32_MAX)
        return model.faceChild[*e.faceIndex];
    if (section && section->regionOwner.size() == section->regions.size() && e.cls == EdgeClass::SectionCut && !model.childOwner.empty())
        if (auto o = annotations::sectionEdgeOwner(*section, e, 0.05); o && *o >= 0) return std::size_t(*o);
    return std::nullopt;
}

// Определение с позицией для детали child: первое определение цепочки вхождений в спецификации.
std::optional<assembly_v2::DefinitionId> childPosition(const ModelSnapshot& model, std::size_t child,
                                                       const annotations::Specification& spec) {
    if (child < model.childChain.size()) {
        for (assembly_v2::DefinitionId d : model.childChain[child])
            if (spec.positionOf(d)) return d;
        return std::nullopt;
    }
    if (child < model.childOwner.size() && spec.positionOf(model.childOwner[child])) return model.childOwner[child];
    return std::nullopt;
}

// Составные части, видимые на виде, и точки выносок на их изображении (мм листа): середины самых
// длинных видимых прямых участков линий (до 6 на деталь), точки внутри фигур сечения деталей,
// точки внутри контура условного изображения подшипника (ГОСТ 2.420).
std::map<assembly_v2::DefinitionId, std::vector<Point2>> shownParts(const ModelSnapshot& model,
                                                                     const annotations::Specification& spec,
                                                                     const ComputedView& cv) {
    std::map<assembly_v2::DefinitionId, std::vector<Point2>> out;
    std::map<assembly_v2::DefinitionId, std::vector<std::pair<double, Point2>>> lines;
    for (const ViewEdge& e : cv.sheetView.view.edges) {
        if (e.visibility != Visibility::Visible) continue;
        const auto c = edgeChild(model, e, nullptr);
        if (!c) continue;
        const auto owner = childPosition(model, *c, spec);
        if (!owner) continue;
        if (auto p = pickPoint(e.curve)) lines[*owner].push_back({p->second, cv.sheetView.toSheet(p->first)});
    }
    if (cv.section && cv.section->regionOwner.size() == cv.section->regions.size() && !model.childOwner.empty()) {
        for (std::size_t i = 0; i < cv.section->regions.size(); ++i) {
            if (cv.section->regionOwner[i] < 0) continue;
            const auto owner = childPosition(model, std::size_t(cv.section->regionOwner[i]), spec);
            if (!owner) continue;
            if (auto p = annotations::regionInteriorPoint(cv.section->regions[i])) out[*owner].push_back(cv.sheetView.toSheet(*p));
        }
    }
    for (const annotations::BearingImage& b : cv.bearings) {
        const auto owner = childPosition(model, b.info.child, spec);
        if (!owner) continue;
        const Point2 u = b.axis, w{-b.axis.y, b.axis.x};
        for (double side : {1.0, -1.0}) {
            // Между диагоналями, ближе к внутреннему кольцу: s = B/2, t = d/2 + (D − d)/8.
            const double t = side * (b.info.d / 2.0 + (b.info.D - b.info.d) / 8.0);
            out[*owner].push_back(cv.sheetView.toSheet({b.origin.x + u.x * b.info.B / 2.0 + w.x * t,
                                                         b.origin.y + u.y * b.info.B / 2.0 + w.y * t}));
        }
    }
    for (auto& [owner, pts] : lines) {
        std::stable_sort(pts.begin(), pts.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        for (std::size_t k = 0; k < pts.size() && k < 6; ++k) out[owner].push_back(pts[k].second);
    }
    return out;
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
        // Те же строки, что выведет addTechnicalRequirements (параметры по умолчанию).
        const annotations::TechnicalRequirementsOptions to;
        auto w = annotations::technicalRequirementsLines(def.technicalRequirements, to, style);
        const std::size_t lines = w ? w.value().size() : def.technicalRequirements.size();
        trTop += to.gapAboveTitle + double(lines) * to.lineSpacingFactor * to.textHeight + to.textHeight;
    }
    if (def.partsList) trTop += 8.0 * 9.0;  // перечень ISO 7573: заголовок + строки (оценка)
    const double m = st.margin;
    double topReserve = def.roughnessRemainder ? 25.0 : 0.0;
    // Таблица параметров венца ГОСТ 2.403 в правом верхнем углу: строки по 8 мм (оценка — до 14).
    if (def.gearTable) topReserve = std::max(topReserve, 8.0 * 14.0 + 5.0);
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

Status DrawingDocument::setAutoBalloon(const std::string& sheet, const std::string& view, assembly_v2::DefinitionId definition,
                                      std::optional<Point2> at) {
    SheetDef* sd = findSheet(sheet);
    if (!sd) return invalid("move position shelf: no sheet '" + sheet + "'");
    auto v = std::find_if(sd->views.begin(), sd->views.end(), [&](const ViewDef& x) { return x.id == view; });
    if (v == sd->views.end()) return invalid("move position shelf: no view '" + view + "' on sheet '" + sheet + "'");
    if (at && (!std::isfinite(at->x) || !std::isfinite(at->y))) return invalid("move position shelf: the point is not finite");
    if (!at) {
        v->balloonShelves.erase(definition);
        return okStatus();
    }
    // Позиция могла быть перенесена на этот вид с вида с autoBalloons (скрытая часть на разрезе).
    if (!v->autoBalloons && std::none_of(sd->views.begin(), sd->views.end(), [](const ViewDef& x) { return x.autoBalloons; }))
        return invalid("move position shelf: view '" + view + "' has no positions by the specification");
    v->balloonShelves[definition] = *at;
    return okStatus();
}

Result<std::size_t> DrawingDocument::arrangeBalloons(const std::string& sheet, const std::string& view) {
    SheetDef* sd = findSheet(sheet);
    if (!sd) return invalid("arrange positions: no sheet '" + sheet + "'");
    if (!view.empty() && std::none_of(sd->views.begin(), sd->views.end(), [&](const ViewDef& v) { return v.id == view; }))
        return invalid("arrange positions: no view '" + view + "' on sheet '" + sheet + "'");
    std::size_t n = 0;
    for (BalloonItem& b : sd->balloons) {
        if (!view.empty() && b.view != view) continue;
        b.arranged = true;
        b.at.reset();
        ++n;
    }
    // Полки автоматических позиций, перенесённые мышью, возвращаются на линию полок.
    for (ViewDef& v : sd->views)
        if (view.empty() || v.id == view) {
            n += v.balloonShelves.size();
            v.balloonShelves.clear();
        }
    return n;
}

Result<annotations::ArrangeResult> DrawingDocument::arrangeDimensions(const ModelSnapshot& model, const std::string& sheet,
                                                                      const std::string& view,
                                                                      const annotations::ArrangeOptions& options) {
    if (!computed()) return invalid("arrange dimensions: the drawing is not updated");
    SheetDef* sd = findSheet(sheet);
    const ComputedSheet* cs = nullptr;
    for (const ComputedSheet& c : computed_)
        if (c.id == sheet) cs = &c;
    if (!sd || !cs) return invalid("arrange dimensions: no sheet '" + sheet + "'");
    const ComputedView* cv = nullptr;
    for (const ComputedView& v : cs->views)
        if (v.id == view) cv = &v;
    if (!cv) return invalid("arrange dimensions: no view '" + view + "' on sheet '" + sheet + "'");
    if (model.shape.isNull() || !model.named) return invalid("arrange dimensions: empty model snapshot");
    auto bindingR = model.historyNames ? ModelBinding::fromNamedShape(*model.named) : ModelBinding::fromShape(model.shape);
    if (!bindingR) return bindingR.error();
    annotations::AnnotationStyle style;
    style.font = textMetrics(font_);
    std::vector<std::size_t> items;
    std::vector<annotations::DimensionDef> defs;
    for (std::size_t i = 0; i < sd->dimensions.size(); ++i) {
        if (sd->dimensions[i].view != view) continue;
        items.push_back(i);
        defs.push_back(sd->dimensions[i].def);
    }
    auto r = annotations::arrangeDimensions(defs, bindingR.value(), cv->sheetView, style, options);
    if (!r) return r.error();
    for (std::size_t k = 0; k < items.size(); ++k) sd->dimensions[items[k]].def = r.value().defs[k];
    return r;
}

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
            if (!v.localSections.empty()) {
                if (v.kind != ViewKind::Base && v.kind != ViewKind::Projected)
                    return invalid("drawing: " + where + ": local sections are made on the base or a projected view");
                for (const LocalSection& c : v.localSections) {
                    if (c.boundary.points.size() < 3) return invalid("drawing: " + where + ": a local section boundary needs at least 3 points");
                    for (const Point2& p : c.boundary.points)
                        if (!std::isfinite(p.x) || !std::isfinite(p.y)) return invalid("drawing: " + where + ": non-finite local section point");
                    if (!(c.depth > 0) || !std::isfinite(c.depth)) return invalid("drawing: " + where + ": local section depth must be positive");
                }
            }
            if (!v.stepPoints.empty()) {
                if (v.kind != ViewKind::Section) return invalid("drawing: " + where + ": step points belong to a section");
                for (const Vec3& p : v.stepPoints)
                    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
                        return invalid("drawing: " + where + ": non-finite step point");
            }
            if (v.side && seen.count(v.parent) && !isGroupView(*seen[v.parent]))
                return invalid("drawing: " + where + ": a view in projection relation needs a parent in the group");
            seen[v.id] = &v;
        }
        // Лист без видов допустим (только рамка и основная надпись — новый чертёж до первого вида).
        if (bases != 1 && !(bases == 0 && s.views.empty()))
            return invalid("drawing: sheet '" + s.id + "' must have exactly one base view");
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
        for (const ToleranceItem& t : s.tolerances) {
            if (t.id.empty() || !annIds.insert(t.id).second) return invalid("drawing: empty or duplicate annotation id '" + t.id + "'");
            if (!seen.count(t.view)) return invalid("drawing: tolerance '" + t.id + "' refers to unknown view '" + t.view + "'");
            if (t.edge.kind != modeling::ElementKind::Edge) return invalid("drawing: tolerance '" + t.id + "' must reference an edge");
            if (t.frame.tolerance.empty()) return invalid("drawing: tolerance '" + t.id + "' has no value");
            // Базы допуска проверяет featureControlFrame при обновлении (ISO 1101, табл. 2).
        }
        for (const DatumItem& d : s.datums) {
            if (d.id.empty() || !annIds.insert(d.id).second) return invalid("drawing: empty or duplicate annotation id '" + d.id + "'");
            if (!seen.count(d.view)) return invalid("drawing: datum '" + d.id + "' refers to unknown view '" + d.view + "'");
            if (d.edge.kind != modeling::ElementKind::Edge) return invalid("drawing: datum '" + d.id + "' must reference an edge");
            if (d.letter.empty()) return invalid("drawing: datum '" + d.id + "' has no letter");
        }
        for (const ThreadNoteItem& t : s.threadNotes) {
            if (t.id.empty() || !annIds.insert(t.id).second) return invalid("drawing: empty or duplicate annotation id '" + t.id + "'");
            if (!seen.count(t.view)) return invalid("drawing: thread '" + t.id + "' refers to unknown view '" + t.view + "'");
            if (t.face.kind != modeling::ElementKind::Face) return invalid("drawing: thread '" + t.id + "' must reference a face");
            if (!std::isfinite(t.def.offset) || !std::isfinite(t.def.leaderAngleDeg) || !std::isfinite(t.def.textShift))
                return invalid("drawing: thread '" + t.id + "' has a non-finite placement");
            for (char c : t.def.toleranceField) {
                if (!std::isalnum(static_cast<unsigned char>(c)))
                    return invalid("drawing: thread '" + t.id + "': tolerance field '" + t.def.toleranceField + "' must be letters and digits (6H, 6g)");
            }
        }
        for (const BalloonItem& b : s.balloons) {
            if (b.id.empty() || !annIds.insert(b.id).second) return invalid("drawing: empty or duplicate annotation id '" + b.id + "'");
            if (!seen.count(b.view)) return invalid("drawing: position '" + b.id + "' refers to unknown view '" + b.view + "'");
            if (b.edge.kind != modeling::ElementKind::Edge) return invalid("drawing: position '" + b.id + "' must reference an edge");
            if (b.at && (!std::isfinite(b.at->x) || !std::isfinite(b.at->y)))
                return invalid("drawing: position '" + b.id + "' has a non-finite shelf point");
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
    style.font = textMetrics(font_);

    // Привязка размеров: деталь — имена из истории операций, сборка — только сигнатуры.
    auto bindingR = model.historyNames ? ModelBinding::fromNamedShape(*model.named) : ModelBinding::fromShape(model.shape);
    if (!bindingR) return bindingR.error();
    const ModelBinding& binding = bindingR.value();

    // Условная резьба модели (атрибуты «thread.*» граней, docs/modeling/FEATURES.md).
    const std::vector<modeling::ThreadInfo> threadInfos = modeling::threadsOf(*model.named);

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
            if ((v.kind == ViewKind::Base || v.kind == ViewKind::Projected) && !v.localSections.empty()) {
                // Вид с местными разрезами (ГОСТ 2.305, разд. 6) — кэш документа.
                const ViewFrame& f = frames[v.id];
                const SectionParts parts = sectionParts(model, -1.0 * f.toward);
                std::uint64_t key = 1469598103934665603ull;
                key = fnv(key, &model.contentHash, sizeof model.contentHash);
                const std::uint64_t ph = viewParametersHash(f, ho);
                key = fnv(key, &ph, sizeof ph);
                const int tag = 0x4c;
                key = fnv(key, &tag, sizeof tag);
                for (const LocalSection& c : v.localSections) {
                    key = fnvDouble(key, c.depth);
                    const int sp = c.boundary.spline ? 1 : 0;
                    key = fnv(key, &sp, sizeof sp);
                    for (const Point2& p : c.boundary.points) key = fnvDouble(fnvDouble(key, p.x), p.y);
                    const std::size_t count = c.boundary.points.size();
                    key = fnv(key, &count, sizeof count);
                }
                for (bool k : parts.keepWhole) key = fnv(key, &k, sizeof k);
                auto it = sectionCache_.find(key);
                if (it != sectionCache_.end()) {
                    sections[v.id] = it->second;
                    cachedFlag[v.id] = true;
                    ++sectionsCached;
                } else {
                    auto sec = localSectionShape(model.shape, f, v.localSections, ho, parts);
                    if (!sec) return Error{sec.error().code, "sheet '" + sd.id + "', view '" + v.id + "': " + sec.error().message};
                    auto ptr = std::make_shared<const SectionView>(std::move(sec).value());
                    if (sectionCache_.size() >= 64) sectionCache_.erase(sectionCache_.begin());
                    sectionCache_[key] = ptr;
                    sections[v.id] = ptr;
                    cachedFlag[v.id] = false;
                    ++sectionsComputed;
                }
            } else if (v.kind == ViewKind::Base || v.kind == ViewKind::Projected || v.kind == ViewKind::Axonometric) {
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
                const SectionParts parts = sectionParts(model, plane.sight);
                for (bool k : parts.keepWhole) key = fnv(key, &k, sizeof k);
                // Ступенчатый разрез: уступы — вдоль следа на виде обозначения.
                Vec3 stepAxis{0, 0, 0};
                if (!v.stepPoints.empty()) {
                    stepAxis = cross(plane.sight, frames.at(v.markView).toward);
                    if (!(length(stepAxis) > 1e-9))
                        return invalid("sheet '" + sd.id + "', section '" + v.id + "': the planes are parallel to the view '" + v.markView + "'");
                    for (const Vec3& p : v.stepPoints)
                        for (double c : {p.x, p.y, p.z}) key = fnvDouble(key, c);
                }
                auto it = sectionCache_.find(key);
                if (it != sectionCache_.end()) {
                    sections[v.id] = it->second;
                    cachedFlag[v.id] = true;
                    ++sectionsCached;
                } else {
                    std::vector<Vec3> pts{v.planePoint};
                    pts.insert(pts.end(), v.stepPoints.begin(), v.stepPoints.end());
                    auto sec = v.stepPoints.empty() ? sectionShape(model.shape, f, plane, v.sectionKind, ho, parts)
                                                    : steppedSectionShape(model.shape, f, pts, stepAxis, v.sectionKind, ho, parts);
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
        if (group.empty()) {
            // Лист без видов: масштаб графы 6 — заданный или 1:1.
            pg.scale = sd.scale ? *sd.scale : ScaleRatio{1, 1};
        } else {
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

        // Виды вне проекционной связи. Без положения — столбцом в своей области (ячейка на вид);
        // масштаб без явного значения уменьшается до помещающегося в ячейку. С явным положением
        // (ViewDef::position) вид не ограничивается ячейкой: центр — заданная точка, масштаб —
        // заданный (или масштаб листа). Явный масштаб не уменьшается и без положения; выход за
        // ячейку, перекрытие видов, основной надписи и рамки — предупреждения (проверка ниже).
        std::vector<const ViewDef*> secondary;
        for (const ViewDef& v : sd.views)
            if (!isGroupView(v)) secondary.push_back(&v);
        std::map<std::string, std::pair<double, ScaleRatio>> secScale;  // масштаб изображения и ряд ГОСТ
        std::map<std::string, Point2> secAnchor;
        if (!secondary.empty()) {
            Rect2 area = reg.secondary.valid() ? reg.secondary : reg.group;
            std::size_t cells = 0;
            for (const ViewDef* v : secondary) cells += v->position ? 0 : 1;
            const double slotH = area.h() / double(std::max<std::size_t>(cells, 1));
            std::size_t k = 0;  // номер ячейки
            for (const ViewDef* vp : secondary) {
                const ViewDef& v = *vp;
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
                ScaleRatio chosen = want;
                Point2 c;
                if (v.position) {
                    c = *v.position;
                } else {
                    const Rect2 slot{area.x0, area.y1 - slotH * double(k + 1), area.x1, area.y1 - slotH * double(k)};
                    ++k;
                    auto fitsAt = [&](const ScaleRatio& s) {
                        const double sc = s.value() * factor;
                        return pv.bounds.width() * sc <= slot.w() && pv.bounds.height() * sc + room <= slot.h();
                    };
                    if (!fitsAt(want)) {
                        if (v.scale) {
                            report.warnings.push_back("sheet '" + sd.id + "': view '" + v.id + "' at " + want.label() +
                                                      " does not fit its cell; set its position on the sheet");
                        } else {
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
                        }
                    }
                    c = {0.5 * (slot.x0 + slot.x1), 0.5 * (slot.y0 + slot.y1)};
                }
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
            // Условная резьба модели (ГОСТ 2.311): ортогональные виды и разрезы; аксонометрия
            // (ГОСТ 2.317) — эллипс наружного диаметра; выносной элемент — изображение резьбы
            // вида-родителя, отсечённое окружностью элемента.
            // Зубчатые венцы (ГОСТ 2.402): виды сбоку и с торца и разрезы через ось; в разрезе
            // полоса зубьев не штрихуется.
            // Деталь сборки, которой принадлежит ребро вида: по ребру или грани модели, у контура
            // фигуры сечения без ссылки — по области сечения (до изменений условностями).
            const std::shared_ptr<const SectionView> originalSection = cv.section;
            const annotations::EdgeChildOf childOf = [&model, originalSection](const ViewEdge& e) -> std::optional<std::size_t> {
                return edgeChild(model, e, originalSection.get());
            };
            if (v.gears && !model.gears.empty() && v.kind != ViewKind::Axonometric && v.kind != ViewKind::Detail) {
                std::optional<CuttingPlane> plane;
                if (v.kind == ViewKind::Section && v.stepPoints.empty())
                    plane = CuttingPlane{v.planePoint, -1.0 * cv.sheetView.view.frame.toward};
                auto gi = annotations::gearImages(model.gears, cv.sheetView.view.frame, plane, {}, model.gearChild);
                if (!gi) return Error{gi.error().code, "sheet '" + sd.id + "', view '" + v.id + "': " + gi.error().message};
                cv.gears = std::move(gi).value();
                if (plane && model.kind == ModelKind::Assembly) {
                    // Колесо, которое в разрезе не рассекается (вал-шестерня в продольном разрезе),
                    // изображается как на виде.
                    const SectionParts parts = sectionParts(model, plane->sight);
                    for (annotations::GearImage& g : cv.gears)
                        if (g.child && *g.child < parts.keepWhole.size() && parts.keepWhole[*g.child]) g.section = false;
                }
                if (cv.section && std::any_of(cv.gears.begin(), cv.gears.end(), [](const auto& g) { return g.section; }))
                    cv.section = std::make_shared<const SectionView>(annotations::withUncutTeeth(*cv.section, cv.gears));
                // Зацепление колёс сборки (ГОСТ 2.402): зуб шестерни — перед зубом колеса.
                if (model.kind == ModelKind::Assembly) {
                    cv.gearMeshes = annotations::gearMeshes(cv.gears);
                    if (!cv.gearMeshes.empty()) annotations::applyGearMesh(cv.sheetView.view.edges, cv.gears, cv.gearMeshes, childOf);
                }
            }
            // Подшипники качения в разрезе через ось — условно по ГОСТ 2.420 (не штрихуются).
            if (v.bearings && !model.bearings.empty() && v.kind == ViewKind::Section && v.stepPoints.empty() && cv.section) {
                const CuttingPlane plane{v.planePoint, -1.0 * cv.sheetView.view.frame.toward};
                cv.bearings = annotations::bearingImages(model.bearings, cv.sheetView.view.frame, plane);
                if (!cv.bearings.empty()) {
                    cv.section = std::make_shared<const SectionView>(annotations::withConventionalBearings(*cv.section, cv.bearings, childOf));
                    std::vector<std::size_t> bc;
                    for (const auto& b : cv.bearings) bc.push_back(b.info.child);
                    std::erase_if(cv.sheetView.view.edges, [&](const ViewEdge& e) {
                        const auto c = childOf(e);
                        return c && std::find(bc.begin(), bc.end(), *c) != bc.end();
                    });
                }
            }
            if (v.threads && !threadInfos.empty()) {
                auto ti = annotations::threadImages(threadInfos, binding.topology(), cv.sheetView.view);
                if (!ti) return Error{ti.error().code, "sheet '" + sd.id + "', view '" + v.id + "': " + ti.error().message};
                cv.threads = std::move(ti).value();
                if (v.kind == ViewKind::Detail)
                    for (annotations::ThreadImage& t : cv.threads) t.clip = annotations::ThreadClip{v.detailCenter, v.detailRadius};
            }
            cs.views.push_back(std::move(cv));
        }
        // Проверка компоновки (предупреждения, не ошибки): изображения (с надписью «А–А» над
        // разрезом и выносным элементом) не перекрывают друг друга, основную надпись и не выходят
        // за рамку. Касание и перекрытие меньше 0,5 мм не считаются.
        {
            std::vector<Rect2> boxes;
            for (const ComputedView& cv : cs.views) {
                Box2 b = cv.sheetView.sheetBounds();
                if (cv.kind == ViewKind::Section || cv.kind == ViewKind::Detail) b = inflate(b, kLabelRoom);
                boxes.push_back({b.minX, b.minY, b.maxX, b.maxY});
            }
            auto overlap = [](const Rect2& a, const Rect2& b) {
                return std::min(a.x1, b.x1) - std::max(a.x0, b.x0) > 0.5 && std::min(a.y1, b.y1) - std::max(a.y0, b.y0) > 0.5;
            };
            for (std::size_t i = 0; i < boxes.size(); ++i) {
                for (std::size_t j = i + 1; j < boxes.size(); ++j)
                    if (overlap(boxes[i], boxes[j]))
                        report.warnings.push_back("sheet '" + sd.id + "': views '" + cs.views[i].id + "' and '" +
                                                  cs.views[j].id + "' overlap");
                if (overlap(boxes[i], reg.title))
                    report.warnings.push_back("sheet '" + sd.id + "': view '" + cs.views[i].id + "' overlaps the title block");
                const Rect2& f = reg.frame;
                if (boxes[i].x0 < f.x0 - 1e-9 || boxes[i].y0 < f.y0 - 1e-9 || boxes[i].x1 > f.x1 + 1e-9 || boxes[i].y1 > f.y1 + 1e-9)
                    report.warnings.push_back("sheet '" + sd.id + "': view '" + cs.views[i].id + "' goes beyond the drawing frame");
            }
        }
        // Секущая плоскость должна быть видна на виде обозначения следом.
        for (const ComputedView& cv : cs.views) {
            if (cv.kind != ViewKind::Section) continue;
            const ComputedView& mark = cs.views[indexOf.at(cv.def.markView)];
            std::vector<Vec3> pts{cv.def.planePoint};
            pts.insert(pts.end(), cv.def.stepPoints.begin(), cv.def.stepPoints.end());
            for (const Vec3& p : pts) {
                const CuttingPlane plane{p, -1.0 * cv.sheetView.view.frame.toward};
                auto tr = cuttingTrace(mark.sheetView.view.frame, mark.sheetView.view.bounds, plane);
                if (!tr) return invalid("sheet '" + sd.id + "', section '" + cv.id + "': the plane cannot be marked on '" +
                                        mark.id + "': " + tr.error().message);
            }
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
                // Точки контура сечения следуют за линиями сечения: подсказка — найденная точка.
                if (res.state == DimensionState::Ok) {
                    for (auto* list : {&item.def.anchors, &item.def.offsetFrom})
                        for (Anchor& a : *list)
                            if (a.kind == annotations::AnchorKind::Section)
                                if (auto p = annotations::anchorPoint(a, binding, cv.sheetView.view)) a.at = p.value();
                }
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

        // Самый длинный видимый прямолинейный участок изображения ребра edge на виде, концы
        // упорядочены так, что левая сторона направления a→b — снаружи вида (от центра габарита).
        struct EdgePlacement {
            Point2 a, b, tip, outward;  // outward — единичная нормаль наружу
            double angleDeg = 0.0;
        };
        auto placeOnEdge = [&](const ComputedView& cv, modeling::TopoReference& edge, double at,
                               std::string& reason) -> std::optional<EdgePlacement> {
            ModelRef ref;
            ref.kind = annotations::RefKind::Edge;
            ref.persistent = edge;
            auto rb = annotations::rebindRef(ref, binding);
            if (!rb.ok) {
                reason = "reference is broken: " + rb.detail;
                return std::nullopt;
            }
            std::optional<LinePick> best;
            for (const ViewEdge& e : cv.sheetView.view.edges) {
                if (e.visibility != Visibility::Visible || !e.edgeIndex || *e.edgeIndex != ref.index) continue;
                if (auto p = straightPiece(e.curve); p && (!best || p->length > best->length)) best = p;
            }
            if (!best) {
                reason = "edge is not shown as a straight visible line in view '" + cv.id + "'";
                return std::nullopt;
            }
            edge = ref.persistent;
            EdgePlacement pl;
            pl.a = cv.sheetView.toSheet(best->a);
            pl.b = cv.sheetView.toSheet(best->b);
            const Box2 vb = cv.sheetView.sheetBounds();
            const Point2 c{0.5 * (vb.minX + vb.maxX), 0.5 * (vb.minY + vb.maxY)};
            const Point2 mid{0.5 * (pl.a.x + pl.b.x), 0.5 * (pl.a.y + pl.b.y)};
            if (-(pl.b.y - pl.a.y) * (mid.x - c.x) + (pl.b.x - pl.a.x) * (mid.y - c.y) < 0) std::swap(pl.a, pl.b);
            const double t = std::clamp(at, 0.0, 1.0);
            pl.tip = {pl.a.x + t * (pl.b.x - pl.a.x), pl.a.y + t * (pl.b.y - pl.a.y)};
            const double len = std::hypot(pl.b.x - pl.a.x, pl.b.y - pl.a.y);
            pl.outward = {-(pl.b.y - pl.a.y) / len, (pl.b.x - pl.a.x) / len};
            pl.angleDeg = std::atan2(pl.b.y - pl.a.y, pl.b.x - pl.a.x) * 180.0 / 3.14159265358979323846;
            return pl;
        };

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

        // 6а. Допуски формы и расположения (ГОСТ 2.308): рамка снаружи вида на расстоянии 12 мм
        // от линии, соединительная линия от ближней стороны рамки к линии со стрелкой.
        for (ToleranceItem& ti : sd.tolerances) {
            const ComputedView& cv = cs.views[indexOf.at(ti.view)];
            std::string reason;
            auto pl = placeOnEdge(cv, ti.edge, ti.at, reason);
            if (!pl) {
                report.dangling.push_back(sd.id + "/" + ti.id + ": " + reason);
                cs.tolerances.emplace_back(std::nullopt);
                continue;
            }
            auto fail = [&](const Error& e) { return Error{e.code, "sheet '" + sd.id + "', tolerance '" + ti.id + "': " + e.message}; };
            // Пробная рамка в начале координат — для габарита.
            auto probe = annotations::featureControlFrame({0, 0}, ti.frame, style);
            if (!probe) return fail(probe.error());
            const double fw = probe.value().box.width(), fh = probe.value().box.height();
            const double out = 12.0;
            const Point2 knee{pl->tip.x + out * pl->outward.x, pl->tip.y + out * pl->outward.y};
            // Рамка — горизонтально; конец соединительной линии — середина ближней к линии стороны.
            Point2 origin, attach;
            if (std::abs(pl->outward.y) >= std::abs(pl->outward.x)) {
                origin = {knee.x - 0.5 * fw, pl->outward.y > 0 ? knee.y : knee.y - fh};
                attach = {knee.x, pl->outward.y > 0 ? origin.y : origin.y + fh};
            } else {
                origin = {pl->outward.x > 0 ? knee.x : knee.x - fw, knee.y - 0.5 * fh};
                attach = {pl->outward.x > 0 ? origin.x : origin.x + fw, knee.y};
            }
            auto frame = annotations::featureControlFrame(origin, ti.frame, style);
            if (!frame) return fail(frame.error());
            auto lead = annotations::frameLeader({attach, pl->tip}, style);
            if (!lead) return fail(lead.error());
            annotations::SymbolGeometry g = std::move(frame).value().geometry;
            g.append(lead.value());
            cs.tolerances.emplace_back(std::move(g));
        }
        // 6б. Обозначения баз (ГОСТ 2.308): треугольник на линии снаружи вида.
        for (DatumItem& di : sd.datums) {
            const ComputedView& cv = cs.views[indexOf.at(di.view)];
            std::string reason;
            auto pl = placeOnEdge(cv, di.edge, di.at, reason);
            if (!pl) {
                report.dangling.push_back(sd.id + "/" + di.id + ": " + reason);
                cs.datums.emplace_back(std::nullopt);
                continue;
            }
            auto g = annotations::datumSymbol(pl->tip, pl->angleDeg, di.letter, 5.0, style);
            if (!g) return Error{g.error().code, "sheet '" + sd.id + "', datum '" + di.id + "': " + g.error().message};
            cs.datums.emplace_back(std::move(g).value());
        }

        // 7. Позиции по спецификации (ГОСТ 2.109). Сначала ручные: точка выноски на видимой линии
        // выбранного ребра, номер — позиция детали, которой принадлежит ребро.
        std::set<assembly_v2::DefinitionId> manualDefinitions;
        std::vector<std::pair<std::size_t, std::string>> arrangedManual;  // ручные позиции для разнесения → вид
        // Определение с позицией для каждого ребра (0 — нет): деталь или подсборка-сборочная единица.
        std::vector<assembly_v2::DefinitionId> edgePosition;
        if (model.kind == ModelKind::Assembly && spec) {
            edgePosition.assign(model.edgeOwner.size(), assembly_v2::DefinitionId(0));
            for (std::uint32_t e = 0; e < edgePosition.size(); ++e)
                if (auto o = positionOwner(model, e, *spec)) edgePosition[e] = *o;
        }
        if (!sd.balloons.empty() && model.kind != ModelKind::Assembly)
            return invalid("sheet '" + sd.id + "': positions need an assembly model");
        for (BalloonItem& bi : sd.balloons) {
            const ComputedView& cv = cs.views[indexOf.at(bi.view)];
            ModelRef ref;
            ref.kind = annotations::RefKind::Edge;
            ref.persistent = bi.edge;
            auto rb = annotations::rebindRef(ref, binding);
            std::optional<std::pair<Point2, double>> best;
            if (rb.ok) {
                for (const ViewEdge& e : cv.sheetView.view.edges) {
                    if (e.visibility != Visibility::Visible || !e.edgeIndex || *e.edgeIndex != ref.index) continue;
                    auto p = pickPoint(e.curve);
                    if (p && (!best || p->second > best->second)) best = p;
                }
            }
            std::optional<int> position;
            std::optional<assembly_v2::DefinitionId> owner;
            if (rb.ok) {
                owner = positionOwner(model, ref.index, *spec);
                if (owner) position = spec->positionOf(*owner);
            }
            if (!rb.ok || !best || !position) {
                report.dangling.push_back(sd.id + "/" + bi.id + ": " +
                                          (!rb.ok   ? "reference is broken: " + rb.detail
                                           : !best ? "edge is not shown as a visible line in view '" + bi.view + "'"
                                                   : "the part of the edge has no position in the specification"));
                cs.manualBalloons.emplace_back(std::nullopt);
                continue;
            }
            bi.edge = ref.persistent;
            annotations::Balloon b;
            b.definition = *owner;
            b.target = cv.sheetView.toSheet(best->first);
            b.at = bi.at && !bi.arranged ? *bi.at : Point2{b.target.x + 12.0, b.target.y + 8.0};
            b.text = std::to_string(*position);
            manualDefinitions.insert(*owner);
            if (bi.arranged) arrangedManual.push_back({cs.manualBalloons.size(), bi.view});
            cs.manualBalloons.emplace_back(std::move(b));
        }
        // Таблица параметров зубчатого венца (ГОСТ 2.403) по венцу модели.
        cs.gearTable.clear();
        if (sd.gearTable) {
            if (sd.gearTable->gear >= model.gears.size())
                return invalid("sheet '" + sd.id + "': gear table refers to gear #" + std::to_string(sd.gearTable->gear + 1) +
                               ", the model has " + std::to_string(model.gears.size()));
            auto rows = annotations::gearTableRows(model.gears[sd.gearTable->gear].gear, *sd.gearTable);
            if (!rows) return Error{rows.error().code, "sheet '" + sd.id + "', gear table: " + rows.error().message};
            cs.gearTable = std::move(rows).value();
        }
        // Размеры резьбы: грань по ссылке → резьба на виде → размер с обозначением (ГОСТ 2.311, п. 5).
        cs.threadNotes.clear();
        for (ThreadNoteItem& tn : sd.threadNotes) {
            const ComputedView& cv = cs.views[indexOf.at(tn.view)];
            ModelRef ref;
            ref.kind = annotations::RefKind::Face;
            ref.persistent = tn.face;
            auto rb = annotations::rebindRef(ref, binding);
            const annotations::ThreadImage* image = nullptr;
            if (rb.ok) {
                for (const annotations::ThreadImage& t : cv.threads) {
                    if (std::find(t.faces.begin(), t.faces.end(), ref.index) != t.faces.end()) image = &t;
                }
            }
            DimensionResult res;
            if (!image || image->aspect == annotations::ThreadAspect::Oblique) {
                res.state = DimensionState::Dangling;
                const bool isThread = rb.ok && std::any_of(threadInfos.begin(), threadInfos.end(),
                                                           [&](const modeling::ThreadInfo& t) { return t.face == ref.index; });
                res.reason = !rb.ok      ? "reference is broken: " + rb.detail
                             : !isThread ? "the face carries no thread"
                             : image     ? "thread dimensions are not placed on an axonometric view '" + tn.view + "'"
                                         : "the thread is not shown in view '" + tn.view + "'";
                report.dangling.push_back(sd.id + "/" + tn.id + ": " + res.reason);
            } else {
                auto r = annotations::threadDimension(*image, cv.sheetView, tn.def, style);
                if (!r) return Error{r.error().code, "sheet '" + sd.id + "', thread '" + tn.id + "': " + r.error().message};
                res = std::move(r).value();
                tn.face = ref.persistent;
            }
            cs.threadNotes.push_back(std::move(res));
        }

        // Затем автоматические (ГОСТ 2.109): составная часть без ручной позиции получает одну позицию
        // на листе — на первом виде с autoBalloons, где она видна (видимыми линиями, фигурой сечения,
        // условным изображением подшипника); скрытая на них — на ближайшем к первому такому виду
        // разрезе листа, где видна. Полки каждого вида разносятся на одну линию (arrangeBalloons).
        {
            std::vector<std::size_t> autoViews;
            for (std::size_t vi = 0; vi < cs.views.size(); ++vi)
                if (cs.views[vi].def.autoBalloons) autoViews.push_back(vi);
            if (!autoViews.empty() && model.kind != ModelKind::Assembly)
                return invalid("sheet '" + sd.id + "', view '" + cs.views[autoViews[0]].id + "': positions need an assembly model");
            std::vector<std::map<assembly_v2::DefinitionId, std::vector<Point2>>> shown(cs.views.size());
            if (model.kind == ModelKind::Assembly && spec)
                for (std::size_t vi = 0; vi < cs.views.size(); ++vi) shown[vi] = shownParts(model, *spec, cs.views[vi]);
            // Группы позиций по видам: кандидаты автоматических и ручных «разнесённых» позиций.
            struct Group {
                std::vector<annotations::BalloonCandidate> cands;
                std::vector<std::optional<std::size_t>> manual;  // индекс в cs.manualBalloons или автоматическая
                BalloonReport report;
                bool reported = false;
            };
            std::vector<Group> groups(cs.views.size());
            for (std::size_t vi = 0; vi < cs.views.size(); ++vi) {
                groups[vi].report.sheet = sd.id;
                groups[vi].report.view = cs.views[vi].id;
                groups[vi].reported = cs.views[vi].def.autoBalloons;
            }
            std::set<assembly_v2::DefinitionId> assigned;
            std::vector<assembly_v2::DefinitionId> pending;
            if (!autoViews.empty()) {
                for (const annotations::SpecSectionData& sec : spec->sections)
                    for (const annotations::SpecItem& it : sec.items) {
                        if (!it.definition || it.position <= 0 || manualDefinitions.count(*it.definition)) continue;
                        bool placed = false;
                        for (std::size_t vi : autoViews) {
                            auto f = shown[vi].find(*it.definition);
                            if (f == shown[vi].end()) continue;
                            groups[vi].cands.push_back({*it.definition, it.position, f->second});
                            groups[vi].manual.push_back(std::nullopt);
                            placed = true;
                            break;
                        }
                        if (!placed) pending.push_back(*it.definition);
                    }
            }
            // Скрытые — через ближайший разрез листа, где составная часть видна.
            Group* first = autoViews.empty() ? nullptr : &groups[autoViews[0]];
            for (assembly_v2::DefinitionId def : pending) {
                const Box2 fb = cs.views[autoViews[0]].sheetView.sheetBounds();
                const Point2 fc{0.5 * (fb.minX + fb.maxX), 0.5 * (fb.minY + fb.maxY)};
                std::optional<std::size_t> bestView;
                double bestDist = 1e300;
                for (std::size_t vi = 0; vi < cs.views.size(); ++vi) {
                    const ComputedView& v = cs.views[vi];
                    const bool isSection = v.kind == ViewKind::Section || !v.def.localSections.empty();
                    if (!isSection || !shown[vi].count(def)) continue;
                    const Box2 b = v.sheetView.sheetBounds();
                    const double d = std::hypot(0.5 * (b.minX + b.maxX) - fc.x, 0.5 * (b.minY + b.maxY) - fc.y);
                    if (d < bestDist) {
                        bestDist = d;
                        bestView = vi;
                    }
                }
                if (!bestView) {
                    first->report.withoutBalloon.push_back(def);
                    continue;
                }
                groups[*bestView].cands.push_back({def, *spec->positionOf(def), shown[*bestView].at(def)});
                groups[*bestView].manual.push_back(std::nullopt);
                if (&groups[*bestView] != first) first->report.movedTo.emplace_back(def, cs.views[*bestView].id);
                groups[*bestView].reported = true;
            }
            for (const auto& [mi, viewId] : arrangedManual) {
                const std::size_t vi = indexOf.at(viewId);
                const annotations::Balloon& b = *cs.manualBalloons[mi];
                groups[vi].cands.push_back({b.definition, std::stoi(b.text), {b.target}});
                groups[vi].manual.push_back(mi);
            }
            // Препятствия для полок: размеры и размеры резьбы листа, ручные позиции с полкой в
            // заданной точке, автоматические позиции с перенесённой полкой и полки видов,
            // разнесённых раньше.
            annotations::BalloonObstacles obstacles;
            for (const DimensionResult& r : cs.dimensions)
                if (r.state == DimensionState::Ok) annotations::addDimensionObstacles(obstacles, r.geometry, style);
            for (const DimensionResult& r : cs.threadNotes)
                if (r.state == DimensionState::Ok) annotations::addDimensionObstacles(obstacles, r.geometry, style);
            auto addBalloonObstacle = [&](const annotations::Balloon& b) {
                obstacles.boxes.push_back(annotations::balloonShelfBox(b, {}, style));
                obstacles.lines.push_back({b.target, b.at});
            };
            {
                std::set<std::size_t> arrangedIdx;
                for (const auto& am : arrangedManual) arrangedIdx.insert(am.first);
                for (std::size_t i = 0; i < cs.manualBalloons.size(); ++i)
                    if (cs.manualBalloons[i] && !arrangedIdx.count(i)) addBalloonObstacle(*cs.manualBalloons[i]);
            }
            // Автоматические позиции с полкой, перенесённой мышью (ViewDef::balloonShelves): полка —
            // в сохранённой точке, точка выноски — ближайшая к ней из точек изображения части.
            for (std::size_t vi = 0; vi < cs.views.size(); ++vi) {
                Group& g = groups[vi];
                const std::map<assembly_v2::DefinitionId, Point2>& moved = cs.views[vi].def.balloonShelves;
                if (moved.empty()) continue;
                Group rest;
                for (std::size_t k = 0; k < g.cands.size(); ++k) {
                    auto f = moved.find(g.cands[k].definition);
                    if (g.manual[k] || f == moved.end()) {
                        rest.cands.push_back(g.cands[k]);
                        rest.manual.push_back(g.manual[k]);
                        continue;
                    }
                    annotations::Balloon b;
                    b.definition = g.cands[k].definition;
                    b.at = f->second;
                    b.target = g.cands[k].targets.front();
                    for (const Point2& t : g.cands[k].targets)
                        if (std::hypot(t.x - b.at.x, t.y - b.at.y) < std::hypot(b.target.x - b.at.x, b.target.y - b.at.y)) b.target = t;
                    b.text = std::to_string(g.cands[k].number);
                    g.report.placed.emplace_back(g.cands[k].number, b.definition);
                    g.reported = true;
                    addBalloonObstacle(b);
                    cs.balloons.push_back(std::move(b));
                    cs.balloonViews.push_back(cs.views[vi].id);
                    cs.balloonMoved.push_back(true);
                }
                g.cands = std::move(rest.cands);
                g.manual = std::move(rest.manual);
            }
            for (std::size_t vi = 0; vi < cs.views.size(); ++vi) {
                Group& g = groups[vi];
                const ComputedView& cv = cs.views[vi];
                if (g.cands.empty()) {
                    // Silent when every part already has a manual position on this sheet.
                    if (cv.def.autoBalloons && (manualDefinitions.empty() || !g.report.withoutBalloon.empty()) &&
                        g.report.movedTo.empty() && g.report.placed.empty())
                        report.warnings.push_back("sheet '" + sd.id + "', view '" + cv.id + "': no assembly part is visible for positions");
                    if (g.reported) report.balloons.push_back(std::move(g.report));
                    continue;
                }
                annotations::BalloonArrangeOptions ao;
                ao.line = cv.def.balloonLine;
                const Box2 vb = cv.sheetView.sheetBounds();
                ao.view = {vb.minX, vb.minY, vb.maxX, vb.maxY};
                ao.area = {reg.frame.x0 + 5.0, reg.frame.y0 + 5.0, reg.frame.x1 - 5.0, reg.frame.y1 - 5.0};
                ao.style = style;
                ao.obstacles = obstacles;
                auto arranged = annotations::arrangeBalloons(g.cands, ao);
                if (!arranged) {
                    report.warnings.push_back("sheet '" + sd.id + "', view '" + cv.id + "': " + arranged.error().message);
                    if (g.reported) report.balloons.push_back(std::move(g.report));
                    continue;
                }
                const annotations::BalloonArrangement& ar = arranged.value();
                g.report.line = ar.line;
                g.report.ordered = ar.ordered;
                g.report.crossings = ar.crossings;
                g.report.shelfHits = ar.shelfHits;
                if (ar.crossings > 0)
                    report.warnings.push_back("sheet '" + sd.id + "', view '" + cv.id + "': " + std::to_string(ar.crossings) +
                                              " crossing leader line(s) remain");
                if (ar.shelfHits > 0)
                    report.warnings.push_back("sheet '" + sd.id + "', view '" + cv.id + "': " + std::to_string(ar.shelfHits) +
                                              " position shelf(s) overlap dimensions");
                for (std::size_t k = 0; k < ar.balloons.size(); ++k) {
                    addBalloonObstacle(ar.balloons[k]);
                    if (g.manual[k]) {
                        cs.manualBalloons[*g.manual[k]]->at = ar.balloons[k].at;
                        continue;
                    }
                    g.report.placed.emplace_back(g.cands[k].number, ar.balloons[k].definition);
                    cs.balloons.push_back(ar.balloons[k]);
                    cs.balloonViews.push_back(cv.id);
                    cs.balloonMoved.push_back(false);
                }
                g.reported = g.reported || !g.report.placed.empty();
                if (g.reported) report.balloons.push_back(std::move(g.report));
            }
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

const TextMetrics* DrawingDocument::textMetrics(const TrueTypeFont* trueType) const {
    if (settings_.textFont == TextFont::Gost2304) return &gostStrokeFont();
    return trueType;
}

Result<std::vector<Drawing>> DrawingDocument::render(const TrueTypeFont* font) const {
    if (computed_.empty() || computed_.size() != sheets_.size()) return invalid("drawing render: the document is not updated");
    const TextMetrics* metrics = textMetrics(font);
    annotations::AnnotationStyle style;
    style.font = metrics;
    std::vector<Drawing> out;
    for (std::size_t si = 0; si < computed_.size(); ++si) {
        const ComputedSheet& cs = computed_[si];
        const SheetDef& sd = sheets_[si];
        auto dr = Drawing::create(cs.sheet, settings_.lineThickness);
        if (!dr) return dr.error();
        Drawing d = std::move(dr).value();
        d.setTextFont(settings_.textFont);
        FrameOptions fo;
        fo.form = si == 0 ? TitleBlockForm::Form1 : TitleBlockForm::Form2a;
        fo.font = metrics;
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
                if (!cv.section->breakLines.empty())
                    if (auto st = renderBreakLines(d, cv.sheetView, *cv.section, ro); !st) return fail(st.error());
            }
            if (!cv.centers.marks.empty() || !cv.centers.axes.empty()) {
                if (auto st = renderCenterLines(d, cv.sheetView, cv.centers, ro); !st) return fail(st.error());
            }
            if (!cv.threads.empty()) {
                if (auto st = annotations::renderThreads(d, cv.sheetView, cv.threads, ro); !st) return fail(st.error());
            }
            if (!cv.gears.empty()) {
                if (auto st = annotations::renderGears(d, cv.sheetView, cv.gears, ro); !st) return fail(st.error());
            }
            if (!cv.bearings.empty()) {
                if (auto st = annotations::renderBearings(d, cv.sheetView, cv.bearings, ro, cv.def.bearingSymbol); !st)
                    return fail(st.error());
            }
            if (!cv.label.empty()) {
                if (auto st = renderViewLabel(d, cv.sheetView, cv.label, ro); !st) return fail(st.error());
            }
            if (cv.kind == ViewKind::Section && cv.def.stepPoints.empty()) {
                const CuttingPlane plane{cv.def.planePoint, -1.0 * cv.sheetView.view.frame.toward};
                auto mk = renderCuttingPlaneMark(d, byId.at(cv.def.markView)->sheetView, plane, cv.letter, 12.0, ro);
                if (!mk) return fail(mk.error());
            } else if (cv.kind == ViewKind::Section) {
                std::vector<CuttingPlane> planes{{cv.def.planePoint, -1.0 * cv.sheetView.view.frame.toward}};
                for (const Vec3& p : cv.def.stepPoints) planes.push_back({p, -1.0 * cv.sheetView.view.frame.toward});
                auto mk = renderSteppedPlaneMark(d, byId.at(cv.def.markView)->sheetView, planes, cv.letter, 12.0, ro);
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
        for (std::size_t ti = 0; ti < cs.threadNotes.size(); ++ti) {
            if (cs.threadNotes[ti].state != DimensionState::Ok) continue;
            if (auto st = annotations::renderDimension(d, cs.threadNotes[ti], style); !st)
                return Error{st.error().code, "sheet '" + cs.id + "', thread '" + sd.threadNotes[ti].id + "': " + st.error().message};
        }
        for (const auto* list : {&cs.roughness, &cs.tolerances, &cs.datums}) {
            for (const auto& g : *list) {
                if (!g) continue;
                if (auto st = annotations::renderSymbol(d, *g, style); !st) return st.error();
            }
        }
        Rect cornerFrame = lay.value().frame;
        if (!cs.gearTable.empty()) {
            auto table = annotations::addGearTable(d, lay.value().frame, cs.gearTable, metrics);
            if (!table) return Error{table.error().code, "sheet '" + cs.id + "', gear table: " + table.error().message};
            cornerFrame.x1 = table.value().box.x0;  // знак «остальное» — левее таблицы
        }
        if (sd.roughnessRemainder) {
            auto g = annotations::roughnessRemainder(cornerFrame, *sd.roughnessRemainder, style);
            if (!g) return g.error();
            if (auto st = annotations::renderSymbol(d, g.value(), style); !st) return st.error();
        }
        for (const annotations::Balloon& b : cs.balloons) {
            auto g = annotations::balloonGeometry(b, {}, style);
            if (!g) return g.error();
            if (auto st = annotations::renderSymbol(d, g.value(), style); !st) return st.error();
        }
        for (std::size_t i = 0; i < cs.manualBalloons.size(); ++i) {
            if (!cs.manualBalloons[i]) continue;
            auto g = annotations::balloonGeometry(*cs.manualBalloons[i], {}, style);
            if (!g) return Error{g.error().code, "sheet '" + cs.id + "', position '" + sd.balloons[i].id + "': " + g.error().message};
            if (auto st = annotations::renderSymbol(d, g.value(), style); !st) return st.error();
        }
        TitleBlockLayout above = lay.value();
        if (sd.partsList && cs.specification) {
            annotations::PartsListOptions po;
            po.font = metrics;
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
