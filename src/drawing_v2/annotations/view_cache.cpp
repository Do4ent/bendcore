// Фоновый расчёт видов: пул потоков, кэш LRU по хэшу формы и параметров вида, выбор
// алгоритма HLR. Замеры — docs/drawing/VIEWS.md.

#include <bendcore/drawing_v2/view_cache.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <TopExp.hxx>
#include <gp_Vec.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepTools.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <chrono>
#include <cmath>
#include <cstring>
#include <sstream>

namespace bendcore::drawing_v2::views {

namespace {

constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

void fnv(std::uint64_t& h, const void* data, std::size_t n) {
    const auto* p = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= kFnvPrime;
    }
}

void fnvDouble(std::uint64_t& h, double v) {
    // Округление до 1e-9 (координаты — после умножения на 1e-3, т. е. до 1e-6 мм): шум
    // последних битов и −0 не меняют хэш.
    long long q = std::llround(v * 1e9);
    fnv(h, &q, sizeof q);
}

double elapsedMs(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace

Result<std::uint64_t> shapeContentHash(const kernel::Shape& shape) {
    if (shape.isNull()) return Error{ErrorCode::InvalidInput, "shape hash: empty shape"};
    try {
        // Обход в порядке TopExp::MapShapes (как TopologyIndex): вершины — координаты, рёбра —
        // тип кривой, концы и точки на кривой, номера вершин, грани — тип поверхности, точки
        // и нормаль в узлах сетки UV, ориентация, номера рёбер контуров. Сетка (Poly) не
        // участвует, поэтому триангуляция PolyAlgo хэш не меняет; копия формы даёт тот же хэш.
        std::uint64_t h = kFnvOffset;
        auto pnt = [&](const gp_Pnt& p) {
            fnvDouble(h, p.X() * 1e-3);
            fnvDouble(h, p.Y() * 1e-3);
            fnvDouble(h, p.Z() * 1e-3);
        };
        kernel::ShapeIndexMap vertices, edges, faces;
        TopExp::MapShapes(shape.occt(), TopAbs_VERTEX, vertices);
        TopExp::MapShapes(shape.occt(), TopAbs_EDGE, edges);
        TopExp::MapShapes(shape.occt(), TopAbs_FACE, faces);
        const std::size_t counts[] = {std::size_t(vertices.Extent()), std::size_t(edges.Extent()),
                                      std::size_t(faces.Extent())};
        fnv(h, counts, sizeof counts);
        for (int i = 1; i <= vertices.Extent(); ++i) pnt(BRep_Tool::Pnt(TopoDS::Vertex(vertices(i))));
        for (int i = 1; i <= edges.Extent(); ++i) {
            const TopoDS_Edge& e = TopoDS::Edge(edges(i));
            TopoDS_Vertex a, b;
            TopExp::Vertices(e, a, b);
            const int ids[] = {a.IsNull() ? 0 : vertices.FindIndex(a), b.IsNull() ? 0 : vertices.FindIndex(b),
                               int(BRep_Tool::Degenerated(e))};
            fnv(h, ids, sizeof ids);
            if (BRep_Tool::Degenerated(e)) continue;
            BRepAdaptor_Curve c(e);
            const int type = int(c.GetType());
            fnv(h, &type, sizeof type);
            for (double t : {0.0, 0.31, 0.5, 0.77, 1.0}) {
                pnt(c.Value(c.FirstParameter() + t * (c.LastParameter() - c.FirstParameter())));
            }
        }
        for (int i = 1; i <= faces.Extent(); ++i) {
            const TopoDS_Face& f = TopoDS::Face(faces(i));
            BRepAdaptor_Surface s(f, true);
            const int head[] = {int(s.GetType()), int(f.Orientation())};
            fnv(h, head, sizeof head);
            double u0, u1, v0, v1;
            BRepTools::UVBounds(f, u0, u1, v0, v1);
            for (double a : {0.23, 0.5, 0.81}) {
                for (double b : {0.29, 0.5, 0.73}) {
                    gp_Pnt p;
                    gp_Vec du, dv;
                    s.D1(u0 + a * (u1 - u0), v0 + b * (v1 - v0), p, du, dv);
                    pnt(p);
                    const gp_Vec n = du.Crossed(dv);
                    if (n.Magnitude() > 1e-12) {
                        const gp_Vec un = n.Normalized();
                        fnvDouble(h, un.X());
                        fnvDouble(h, un.Y());
                        fnvDouble(h, un.Z());
                    }
                }
            }
            for (TopExp_Explorer w(f, TopAbs_EDGE); w.More(); w.Next()) {
                const int id = edges.FindIndex(w.Current());
                fnv(h, &id, sizeof id);
            }
        }
        return h;
    } catch (const Standard_Failure& f) {
        return Error{ErrorCode::OperationFailed, std::string("shape hash: ") + f.what()};
    }
}

std::uint64_t viewParametersHash(const ViewFrame& f, const HlrOptions& o) {
    std::uint64_t h = kFnvOffset;
    for (double v : {f.toward.x, f.toward.y, f.toward.z, f.right.x, f.right.y, f.right.z, o.linearDeflection,
                     o.angularDeflection, o.curveDeflection}) {
        fnvDouble(h, v);
    }
    const int flags[] = {int(o.method), o.hidden, o.smooth, o.hiddenSmooth};
    fnv(h, flags, sizeof flags);
    return h;
}

GeometryProfile profileGeometry(const kernel::Shape& shape, const ViewFrame& frame) {
    GeometryProfile p;
    if (shape.isNull()) return p;
    const Vec3 t = frame.toward;
    for (TopExp_Explorer ex(shape.occt(), TopAbs_FACE); ex.More(); ex.Next()) {
        ++p.faces;
        BRepAdaptor_Surface s(TopoDS::Face(ex.Current()), false);
        std::optional<gp_Dir> axis;
        switch (s.GetType()) {
            case GeomAbs_Plane: ++p.planar; break;
            case GeomAbs_Cylinder:
                ++p.quadric;
                axis = s.Cylinder().Axis().Direction();
                break;
            case GeomAbs_Cone:
                ++p.quadric;
                axis = s.Cone().Axis().Direction();
                break;
            case GeomAbs_Sphere:
            case GeomAbs_Torus: ++p.quadric; break;
            default: ++p.freeform; break;
        }
        if (axis) {
            const double c = std::abs(axis->X() * t.x + axis->Y() * t.y + axis->Z() * t.z);
            if (c > 1e-6 && c < 1.0 - 1e-6) ++p.obliqueCurved;
        }
    }
    return p;
}

AlgorithmChoice chooseHlrMethod(const kernel::Shape& shape, const ViewFrame& frame, const AlgorithmPolicy& policy) {
    const GeometryProfile p = profileGeometry(shape, frame);
    if (p.freeform > policy.freeformThreshold) {
        return {HlrMethod::Polygonal, std::to_string(p.freeform) + " free-form faces (> " +
                                          std::to_string(policy.freeformThreshold) + ")"};
    }
    return {HlrMethod::Exact, "analytic geometry: " + std::to_string(p.planar) + " planes, " +
                                  std::to_string(p.quadric) + " quadrics, " + std::to_string(p.obliqueCurved) +
                                  " seen obliquely"};
}

ViewCache::ViewCache(std::size_t capacity, unsigned threads) : capacity_(capacity == 0 ? 1 : capacity) {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    // WebAssembly без потоков (bendcore_drawing_wasm): рабочих потоков нет, request() считает
    // вид в вызывающем потоке; кэш и статистика работают так же.
    (void)threads;
#else
    if (threads == 0) threads = std::max(1u, std::thread::hardware_concurrency());
    for (unsigned i = 0; i < threads; ++i) workers_.emplace_back([this] { worker(); });
#endif
}

ViewCache::~ViewCache() {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) t.join();
}

void ViewCache::worker() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lk(mutex_);
            cv_.wait(lk, [this] { return stop_ || !queue_.empty(); });
            if (queue_.empty()) return;  // stop_ и очередь пуста
            task = std::move(queue_.front());
            queue_.pop_front();
        }
        task();
    }
}

std::uint64_t ViewCache::cachedShapeHash(const kernel::Shape& s, double& ms) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& [shape, h] : identityHashes_) {
            if (shape.IsEqual(s.occt())) return h;
        }
    }
    const auto t0 = std::chrono::steady_clock::now();
    const auto h = shapeContentHash(s);
    ms = elapsedMs(t0);
    const std::uint64_t value = h ? h.value() : 0;
    std::lock_guard<std::mutex> lk(mutex_);
    identityHashes_.emplace_back(s.occt(), value);
    if (identityHashes_.size() > 4 * capacity_) identityHashes_.erase(identityHashes_.begin());
    return value;
}

std::shared_future<ViewResultPtr> ViewCache::request(const ViewRequest& r) {
    HlrOptions opts = r.options;
    if (r.autoMethod && !r.shape.isNull()) opts.method = chooseHlrMethod(r.shape, r.frame).method;
    if (r.shape.isNull()) {
        std::promise<ViewResultPtr> p;
        p.set_value(std::make_shared<const Result<ProjectedView>>(
            Error{ErrorCode::InvalidInput, "view cache: empty shape"}));
        std::lock_guard<std::mutex> lk(mutex_);
        ++stats_.requests;
        return p.get_future().share();
    }
    double hashMs = 0.0;
    const Key key{cachedShapeHash(r.shape, hashMs), viewParametersHash(r.frame, opts)};
    auto lookup = [&]() -> std::optional<std::shared_future<ViewResultPtr>> {
        auto it = entries_.find(key);
        if (it == entries_.end()) return std::nullopt;
        lru_.splice(lru_.begin(), lru_, it->second.lru);
        if (it->second.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            ++stats_.hits;
        } else {
            ++stats_.joined;
        }
        return it->second.future;
    };
    {
        std::lock_guard<std::mutex> lk(mutex_);
        ++stats_.requests;
        stats_.hashMilliseconds += hashMs;
        if (auto f = lookup()) return *f;
    }
    // Копия формы для рабочего потока: HLR (PolyAlgo строит сетку в формах) не трогает
    // исходную форму, которой пользуется вызывающий поток.
    TopoDS_Shape copy;
    try {
        BRepBuilderAPI_Copy cp(r.shape.occt(), true, false);
        copy = cp.Shape();
    } catch (const Standard_Failure& f) {
        std::promise<ViewResultPtr> p;
        p.set_value(std::make_shared<const Result<ProjectedView>>(
            Error{ErrorCode::OperationFailed, std::string("view cache: copy failed: ") + f.what()}));
        return p.get_future().share();
    }
    auto promise = std::make_shared<std::promise<ViewResultPtr>>();
    std::shared_future<ViewResultPtr> future = promise->get_future().share();
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (auto f = lookup()) return *f;  // другой поток успел поставить ту же задачу
        lru_.push_front(key);
        entries_.emplace(key, Entry{future, lru_.begin()});
        ++stats_.computed;
        // Вытеснение готовых результатов с конца LRU.
        for (auto it = std::prev(lru_.end()); entries_.size() > capacity_;) {
            auto e = entries_.find(*it);
            const bool ready = e->second.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            auto prev = it == lru_.begin() ? lru_.end() : std::prev(it);
            if (ready) {
                entries_.erase(e);
                lru_.erase(it);
                ++stats_.evictions;
            }
            if (prev == lru_.end()) break;
            it = prev;
        }
        const ViewFrame frame = r.frame;
        auto task = [promise, copy, frame, opts] {
            try {
                auto res = projectShape(kernel::Shape(copy), frame, opts);
                promise->set_value(std::make_shared<const Result<ProjectedView>>(std::move(res)));
            } catch (const std::exception& e) {  // Standard_Failure — тоже std::exception
                promise->set_value(std::make_shared<const Result<ProjectedView>>(
                    Error{ErrorCode::OperationFailed, std::string("view cache: ") + e.what()}));
            }
        };
        if (!workers_.empty()) {
            queue_.emplace_back(std::move(task));
        } else {
            // Без рабочих потоков — расчёт сразу (под замком: других потоков нет).
            task();
        }
    }
    cv_.notify_one();
    return future;
}

ViewResultPtr ViewCache::get(const ViewRequest& r) { return request(r).get(); }

ViewCacheStats ViewCache::stats() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return stats_;
}

std::size_t ViewCache::size() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return entries_.size();
}

void ViewCache::clear() {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto it = lru_.begin(); it != lru_.end();) {
        auto e = entries_.find(*it);
        if (e->second.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            entries_.erase(e);
            it = lru_.erase(it);
        } else {
            ++it;
        }
    }
    identityHashes_.clear();
}

}  // namespace bendcore::drawing_v2::views
