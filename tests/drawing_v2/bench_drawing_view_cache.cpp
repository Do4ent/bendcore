// Замер фонового расчёта видов и кэша (docs/drawing/VIEWS.md): плита 200×150×30 с сеткой
// (8k)×(6k) отверстий (та же модель, что в bench_drawing_views_hlr). Печатает:
//  - последовательный расчёт 4 видов (спереди, сверху, слева, изометрия) в вызывающем потоке;
//  - тот же набор через ViewCache с N потоками (время до готовности всех видов);
//  - повторный запрос (попадание в кэш) и время хэширования формы;
//  - изометрия: точный, триангуляционный и автоматически выбранный алгоритм.
//
//   bench_drawing_view_cache [k=2] [потоков=4]

#include <bendcore/drawing_v2/view_cache.hpp>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_NurbsConvert.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace bendcore::drawing_v2::views;

namespace {

TopoDS_Shape plate(int k) {
    TopoDS_Shape s = BRepPrimAPI_MakeBox(200, 150, 30).Shape();
    BRep_Builder bb;
    TopoDS_Compound tools;
    bb.MakeCompound(tools);
    const double px = 150.0 / (8 * k), py = 100.0 / (6 * k), r = std::min(px, py) * 0.3;
    for (int i = 0; i < 8 * k; ++i)
        for (int j = 0; j < 6 * k; ++j)
            bb.Add(tools, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(25 + px * (i + 0.5), 25 + py * (j + 0.5), -1),
                                                          gp_Dir(0, 0, 1)), r, 32).Shape());
    return BRepAlgoAPI_Cut(s, tools).Shape();
}

double ms(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace

int main(int argc, char** argv) {
    const int k = argc > 1 ? std::clamp(std::atoi(argv[1]), 1, 6) : 2;
    const unsigned threads = argc > 2 ? unsigned(std::clamp(std::atoi(argv[2]), 1, 64)) : 4u;
    const bendcore::kernel::Shape model(plate(k));
    const auto c = model.counts();
    std::printf("model k=%d: %zu faces, %zu edges\n", k, c.faces, c.edges);
    const ViewFrame front = defaultFrontFrame();
    const std::vector<ViewFrame> frames = {front, standardFrame(front, StandardView::Top),
                                           standardFrame(front, StandardView::Left),
                                           axonometricFrame(front, Axonometry::Isometric)};

    auto t0 = std::chrono::steady_clock::now();
    for (const auto& f : frames) {
        if (!projectShape(model, f)) return 1;
    }
    const double sequential = ms(t0);
    std::printf("sequential, 4 views (exact):          %8.1f ms\n", sequential);

    ViewCache cache(64, threads);
    t0 = std::chrono::steady_clock::now();
    std::vector<std::shared_future<ViewResultPtr>> fut;
    for (const auto& f : frames) fut.push_back(cache.request({model, f, {}, false}));
    const double submit = ms(t0);
    for (auto& f : fut) {
        if (!f.get()->ok()) return 1;
    }
    const double background = ms(t0);
    std::printf("ViewCache %u threads, 4 views:         %8.1f ms (request() returned after %.1f ms)\n", threads,
                background, submit);
    std::printf("speed-up:                              %8.2f x\n", sequential / background);

    t0 = std::chrono::steady_clock::now();
    for (const auto& f : frames) cache.get({model, f, {}, false});
    std::printf("repeat 4 requests (cache hits):        %8.3f ms\n", ms(t0));
    t0 = std::chrono::steady_clock::now();
    const auto h = shapeContentHash(model);
    std::printf("shape content hash (topology+geometry):%8.1f ms (%016llx)\n", ms(t0),
                static_cast<unsigned long long>(h.ok() ? h.value() : 0));
    const auto st = cache.stats();
    std::printf("stats: requests %zu, computed %zu, hits %zu, joined %zu, hashing %.1f ms\n", st.requests,
                st.computed, st.hits, st.joined, st.hashMilliseconds);

    // Каждый замер — на свежей копии формы (без сетки от предыдущего PolyAlgo), лучшее из 3.
    const ViewFrame iso = frames[3];
    for (const char* name : {"exact", "polygonal", "auto"}) {
        double best = 1e300;
        std::string choice;
        for (int rep = 0; rep < 3; ++rep) {
            const bendcore::kernel::Shape fresh(BRepBuilderAPI_Copy(model.occt(), true, false).Shape());
            t0 = std::chrono::steady_clock::now();
            HlrOptions o;
            if (std::string(name) == "polygonal") o.method = HlrMethod::Polygonal;
            if (std::string(name) == "auto") {
                const auto a = chooseHlrMethod(fresh, iso);
                o.method = a.method;
                choice = std::string(" -> ") + (a.method == HlrMethod::Exact ? "exact" : "polygonal") + " (" +
                         a.reason + ")";
            }
            if (!projectShape(fresh, iso, o)) return 1;
            best = std::min(best, ms(t0));
        }
        std::printf("isometric %-9s                     %8.1f ms%s\n", name, best, choice.c_str());
    }
    // Та же плита, преобразованная в B-сплайны (BRepBuilderAPI_NurbsConvert): свободные поверхности.
    const bendcore::kernel::Shape nurbs(BRepBuilderAPI_NurbsConvert(model.occt()).Shape());
    for (const auto& [vname, vf] : {std::pair<const char*, ViewFrame>{"front", front}, {"isometric", iso}}) {
        for (HlrMethod m : {HlrMethod::Exact, HlrMethod::Polygonal}) {
            double best = 1e300;
            for (int rep = 0; rep < 3; ++rep) {
                const bendcore::kernel::Shape fresh(BRepBuilderAPI_Copy(nurbs.occt(), true, false).Shape());
                t0 = std::chrono::steady_clock::now();
                HlrOptions o;
                o.method = m;
                if (!projectShape(fresh, vf, o)) return 1;
                best = std::min(best, ms(t0));
            }
            std::printf("B-spline %-9s %-9s                %8.1f ms\n", vname,
                        m == HlrMethod::Exact ? "exact" : "polygonal", best);
        }
    }
    const auto an = chooseHlrMethod(nurbs, iso);
    std::printf("B-spline auto: %s (%s)\n", an.method == HlrMethod::Exact ? "exact" : "polygonal", an.reason.c_str());
    return 0;
}
