#pragma once

// Фоновый расчёт видов (HLR) с кэшем по хэшу формы и параметров вида; выбор алгоритма HLR по
// типу геометрии (фаза 4, п. 2; риск «Производительность HLR» плана). Документация —
// docs/drawing/VIEWS.md, раздел «Фоновый расчёт и кэш».

#include <bendcore/drawing_v2/views.hpp>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace bendcore::drawing_v2::views {

// Хэш содержимого формы (FNV-1a 64): топология в порядке TopExp::MapShapes и геометрия в
// контрольных точках (вершины, точки на рёбрах, точки и нормали граней в узлах сетки UV,
// округление 1e-6 мм). Не зависит от адреса в памяти и от сетки, которую строит
// HLRBRep_PolyAlgo; копия формы (BRepBuilderAPI_Copy) даёт тот же хэш. Запись BRep
// (BRepTools::Write) для этого не годится: у копии и после построения сетки она другая.
Result<std::uint64_t> shapeContentHash(const kernel::Shape& shape);

// Хэш параметров вида (ориентация, параметры HLR), округлённых до 1e-9.
std::uint64_t viewParametersHash(const ViewFrame& frame, const HlrOptions& options);

// Сводка геометрии для выбора алгоритма.
struct GeometryProfile {
    std::size_t faces = 0;
    std::size_t planar = 0;
    std::size_t quadric = 0;      // цилиндр, конус, сфера, тор
    std::size_t freeform = 0;     // B-сплайн, Безье, прочие
    std::size_t obliqueCurved = 0;  // цилиндры/конусы, ось которых не ∥ и не ⟂ направлению взгляда
};

GeometryProfile profileGeometry(const kernel::Shape& shape, const ViewFrame& frame);

struct AlgorithmChoice {
    HlrMethod method = HlrMethod::Exact;
    std::string reason;
};

// Порог выбора (замеры bench_drawing_view_cache — docs/drawing/VIEWS.md).
struct AlgorithmPolicy {
    // Свободные поверхности (B-сплайн, Безье, прочие): точный HLR на них в 2,4–2,6 раза
    // медленнее триангуляционного (плита k = 1 после NurbsConvert, 54 грани: 146 против 56 мс
    // спереди, 161 против 68 мс в изометрии, сетка строится заново в каждом замере).
    std::size_t freeformThreshold = 20;
};

// Выбор алгоритма: Exact для аналитической геометрии (плоскости и квадрики — PolyAlgo с
// построением сетки медленнее во всех видах, в т. ч. в изометрии с сотнями эллипсов),
// Polygonal — при числе свободных поверхностей больше порога.
AlgorithmChoice chooseHlrMethod(const kernel::Shape& shape, const ViewFrame& frame,
                                const AlgorithmPolicy& policy = {});

struct ViewRequest {
    kernel::Shape shape;
    ViewFrame frame;
    HlrOptions options;
    bool autoMethod = false;  // выбрать алгоритм chooseHlrMethod (options.method игнорируется)
};

using ViewResultPtr = std::shared_ptr<const Result<ProjectedView>>;

struct ViewCacheStats {
    std::size_t requests = 0;
    std::size_t hits = 0;         // готовый результат из кэша
    std::size_t joined = 0;       // присоединение к расчёту, который уже идёт
    std::size_t computed = 0;     // запущенные расчёты HLR
    std::size_t evictions = 0;
    double hashMilliseconds = 0;  // суммарное время хэширования форм
};

// Пул потоков с кэшем LRU. Расчёт выполняется в рабочем потоке на копии формы
// (BRepBuilderAPI_Copy в потоке, вызвавшем request): исходная форма не меняется и может
// использоваться дальше. Индексы TopologyIndex копии совпадают с индексами исходной формы.
class ViewCache {
public:
    explicit ViewCache(std::size_t capacity = 64, unsigned threads = 0);
    ~ViewCache();
    ViewCache(const ViewCache&) = delete;
    ViewCache& operator=(const ViewCache&) = delete;

    // Асинхронный расчёт: готовый результат, присоединение к идущему или новая задача.
    std::shared_future<ViewResultPtr> request(const ViewRequest& r);
    // Синхронно (ожидание результата).
    ViewResultPtr get(const ViewRequest& r);

    ViewCacheStats stats() const;
    std::size_t size() const;
    void clear();
    unsigned threads() const { return unsigned(workers_.size()); }

private:
    struct Key {
        std::uint64_t shape = 0;
        std::uint64_t params = 0;
        bool operator==(const Key& o) const { return shape == o.shape && params == o.params; }
    };
    struct KeyHash {
        std::size_t operator()(const Key& k) const { return std::size_t(k.shape ^ (k.params * 0x9E3779B97F4A7C15ull)); }
    };
    struct Entry {
        std::shared_future<ViewResultPtr> future;
        std::list<Key>::iterator lru;
    };

    void worker();
    std::uint64_t cachedShapeHash(const kernel::Shape& s, double& ms);

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> queue_;
    std::vector<std::thread> workers_;
    bool stop_ = false;
    std::size_t capacity_;
    std::unordered_map<Key, Entry, KeyHash> entries_;
    std::list<Key> lru_;  // начало — недавно использованные
    // Хэши уже виденных форм по идентичности TShape (форма хранится, чтобы адрес не переиспользовался).
    std::vector<std::pair<TopoDS_Shape, std::uint64_t>> identityHashes_;
    ViewCacheStats stats_;
};

}  // namespace bendcore::drawing_v2::views
