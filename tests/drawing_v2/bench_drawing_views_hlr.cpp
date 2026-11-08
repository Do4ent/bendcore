// Замер HLR на модели среднего размера: плита 200×150×30, сетка (8k)×(6k) сквозных отверстий,
// 4 ступенчатых отверстия Ø20/Ø12, скругления R5 четырёх вертикальных рёбер. Печатает число граней/рёбер, время HLR (мс) и число 2D-рёбер для точного и
// быстрого алгоритма на видах спереди, сверху, изометрии и разреза.
//
//   bench_drawing_views_hlr [повторов=3] [k=1]

#include <bendcore/drawing_v2/sections.hpp>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace bendcore::drawing_v2::views;

namespace {

TopoDS_Shape plate(int k) {
    TopoDS_Shape s = BRepPrimAPI_MakeBox(200, 150, 30).Shape();
    BRepFilletAPI_MakeFillet f(s);
    for (TopExp_Explorer ex(s, TopAbs_EDGE); ex.More(); ex.Next()) {
        TopoDS_Vertex a, b;
        TopExp::Vertices(TopoDS::Edge(ex.Current()), a, b);
        const gp_Pnt pa = BRep_Tool::Pnt(a), pb = BRep_Tool::Pnt(b);
        if (std::abs(pa.X() - pb.X()) < 1e-9 && std::abs(pa.Y() - pb.Y()) < 1e-9) f.Add(5.0, TopoDS::Edge(ex.Current()));
    }
    s = f.Shape();
    BRep_Builder bb;
    TopoDS_Compound tools;
    bb.MakeCompound(tools);
    const double px = 150.0 / (8 * k), py = 100.0 / (6 * k), r = std::min(px, py) * 0.3;
    for (int i = 0; i < 8 * k; ++i)
        for (int j = 0; j < 6 * k; ++j)
            bb.Add(tools, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(25 + px * (i + 0.5), 25 + py * (j + 0.5), -1),
                                                          gp_Dir(0, 0, 1)), r, 32).Shape());
    s = BRepAlgoAPI_Cut(s, tools).Shape();
    TopoDS_Compound steps;
    bb.MakeCompound(steps);
    for (auto [x, y] : {std::pair{12.0, 12.0}, {188.0, 12.0}, {12.0, 138.0}, {188.0, 138.0}}) {
        bb.Add(steps, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x, y, -1), gp_Dir(0, 0, 1)), 6, 32).Shape());
        bb.Add(steps, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x, y, 20), gp_Dir(0, 0, 1)), 10, 11).Shape());
    }
    s = BRepAlgoAPI_Cut(s, steps).Shape();
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    const int repeats = argc > 1 ? std::max(1, std::atoi(argv[1])) : 3;
    const int k = argc > 2 ? std::clamp(std::atoi(argv[2]), 1, 8) : 1;
    const bendcore::kernel::Shape model(plate(k));
    const auto c = model.counts();
    std::printf("model: %zu faces, %zu edges, %zu vertices\n", c.faces, c.edges, c.vertices);
    const ViewFrame front = defaultFrontFrame();
    struct Case {
        const char* name;
        ViewFrame frame;
    } cases[] = {{"front", front},
                 {"top", standardFrame(front, StandardView::Top)},
                 {"isometric", axonometricFrame(front, Axonometry::Isometric)}};
    for (HlrMethod m : {HlrMethod::Exact, HlrMethod::Polygonal}) {
        for (const Case& cs : cases) {
            double best = 1e300;
            std::size_t edges = 0, raw = 0;
            double hlr = 0;
            for (int r = 0; r < repeats; ++r) {
                HlrOptions o;
                o.method = m;
                const auto t0 = std::chrono::steady_clock::now();
                auto v = projectShape(model, cs.frame, o);
                const auto t1 = std::chrono::steady_clock::now();
                if (!v) {
                    std::printf("%s failed: %s\n", cs.name, v.error().message.c_str());
                    return 1;
                }
                best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count());
                edges = v.value().edges.size();
                raw = v.value().stats.rawPieces;
                hlr = v.value().stats.milliseconds;
            }
            std::printf("%-9s %-9s total %8.1f ms (OCCT %8.1f ms), pieces %zu -> 2D edges %zu\n",
                        m == HlrMethod::Exact ? "exact" : "polygonal", cs.name, best, hlr, raw, edges);
        }
        HlrOptions o;
        o.method = m;
        o.hidden = false;
        double best = 1e300;
        for (int r = 0; r < repeats; ++r) {
            const auto t0 = std::chrono::steady_clock::now();
            auto s = sectionShape(model, front, {{0, 75, 0}, {0, 1, 0}}, SectionKind::Cut, o);
            const auto t1 = std::chrono::steady_clock::now();
            if (!s) {
                std::printf("section failed: %s\n", s.error().message.c_str());
                return 1;
            }
            best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count());
        }
        std::printf("%-9s section   total %8.1f ms (cut + HLR)\n", m == HlrMethod::Exact ? "exact" : "polygonal", best);
    }
    return 0;
}
