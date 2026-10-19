// Проверка: труба из пути как тело (Open CASCADE). Ось и объём сверяются с ядром bendcore.
// Путь: прямой 100 мм → гиб R=50 на 90° → прямой 100 мм. Профиль: круг ∅20.
// Ожидаемый объём: π·r²·L (протяжка без скручивания), L — длина оси из ядра bendcore.
#include "bendcore/tubegeom.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>

#include <algorithm>
#include <cmath>
#include <cstdio>

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static bool close_rel(double a, double b, double rel) {
    return std::fabs(a - b) <= rel * std::max(1.0, std::fabs(b));
}

int main() {
    const double pi = std::acos(-1.0);
    const double r = 10.0;     // профиль ∅20
    const double R = 50.0;     // радиус гиба
    const double S = 100.0;    // прямые участки

    // 1. Ось из ядра bendcore (эталон длины).
    const bendcore::Path path{bendcore::Straight{S}, bendcore::Bend{R, 90.0}, bendcore::Straight{S}};
    const double L_core = bendcore::axis_length(path);
    const double L_expected = 2.0 * S + R * pi / 2.0;
    check(close_rel(L_core, L_expected, 1e-9), "ядро: длина оси = 2S + R·π/2");

    // 2. Та же ось в OCCT: прямая по X, дуга в плоскости XY, прямая по Y.
    //    Центр дуги (S, R), дуга от (S, 0) до (S+R, R).
    const gp_Pnt p0(0.0, 0.0, 0.0);
    const gp_Pnt p1(S, 0.0, 0.0);
    const gp_Pnt p2(S + R, R, 0.0);
    const gp_Pnt p3(S + R, R + S, 0.0);

    BRepBuilderAPI_MakeEdge e1(p0, p1);
    gp_Circ arc(gp_Ax2(gp_Pnt(S, R, 0.0), gp_Dir(0.0, 0.0, 1.0)), R);
    BRepBuilderAPI_MakeEdge e2(arc, p1, p2);
    BRepBuilderAPI_MakeEdge e3(p2, p3);
    BRepBuilderAPI_MakeWire spineMaker;
    spineMaker.Add(e1.Edge());
    spineMaker.Add(e2.Edge());
    spineMaker.Add(e3.Edge());
    const TopoDS_Wire spine = spineMaker.Wire();

    GProp_GProps axisProps;
    BRepGProp::LinearProperties(spine, axisProps);
    const double L_occt = axisProps.Mass();
    check(close_rel(L_occt, L_core, 1e-6), "OCCT: длина оси совпадает с ядром");

    // 3. Профиль: круг r, нормаль по касательной в начале пути (+X).
    gp_Circ circ(gp_Ax2(p0, gp_Dir(1.0, 0.0, 0.0)), r);
    BRepBuilderAPI_MakeEdge profileEdge(circ);
    BRepBuilderAPI_MakeWire profileMaker;
    profileMaker.Add(profileEdge.Edge());
    const TopoDS_Wire profile = profileMaker.Wire();

    // 4. Протяжка профиля вдоль оси.
    BRepOffsetAPI_MakePipe pipe(spine, profile);
    pipe.Build();
    check(pipe.IsDone(), "OCCT: MakePipe выполнен");
    if (pipe.IsDone()) {
        // MakePipe даёт только боковую поверхность: добавляем торцы и сшиваем в замкнутую оболочку.
        const TopoDS_Shape lateral = pipe.Shape();
        const TopoDS_Wire capStart = profile;
        const gp_Circ capEndCirc(gp_Ax2(p3, gp_Dir(0.0, 1.0, 0.0)), r);
        BRepBuilderAPI_MakeEdge capEdge(capEndCirc);
        BRepBuilderAPI_MakeWire capEndMaker;
        capEndMaker.Add(capEdge.Edge());
        const TopoDS_Wire capEnd = capEndMaker.Wire();

        BRepBuilderAPI_MakeFace faceStart(capStart);
        BRepBuilderAPI_MakeFace faceEnd(capEnd);

        BRepBuilderAPI_Sewing sewing(1e-4);
        sewing.Add(lateral);
        sewing.Add(faceStart.Face());
        sewing.Add(faceEnd.Face());
        sewing.Perform();
        const TopoDS_Shape sewn = sewing.SewedShape();

        TopExp_Explorer shellEx(sewn, TopAbs_SHELL);
        check(shellEx.More(), "OCCT: сшивка дала оболочку");
        if (shellEx.More()) {
            const TopoDS_Shell shell = TopoDS::Shell(shellEx.Current());
            BRepBuilderAPI_MakeSolid makeSolid(shell);
            const TopoDS_Solid solid = makeSolid.Solid();
            BRepCheck_Analyzer analyzer(solid);
            check(analyzer.IsValid(), "OCCT: тело трубы валидно");

            GProp_GProps volProps;
            BRepGProp::VolumeProperties(solid, volProps);
            const double V_occt = volProps.Mass();
            const double V_expected = pi * r * r * L_core;
            std::printf("объём OCCT = %.4f мм³, ожидается π·r²·L = %.4f мм³\n", V_occt, V_expected);
            check(close_rel(V_occt, V_expected, 1e-4), "OCCT: объём трубы = π·r²·L");
        }
    }

    if (failures == 0) std::printf("OK tube_occt\n");
    return failures == 0 ? 0 : 1;
}
