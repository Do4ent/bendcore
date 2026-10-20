#include "bendcore/tube_body.hpp"

#include "bendcore/axis.hpp"
#include "bendcore/ports.hpp"

#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_Shape.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Handle.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace bendcore::geom {
namespace {

gp_Pnt to_pnt(const Vec3& v) { return gp_Pnt(v[0], v[1], v[2]); }
gp_Dir to_dir(const Vec3& v) { return gp_Dir(v[0], v[1], v[2]); }

TopoDS_Edge make_edge(const AxisPiece& p) {
    switch (p.kind) {
    case AxisPiece::Kind::Line:
        return BRepBuilderAPI_MakeEdge(to_pnt(p.p0), to_pnt(p.p1)).Edge();
    case AxisPiece::Kind::Arc: {
        GC_MakeArcOfCircle arc(to_pnt(p.p0), to_pnt(p.mid), to_pnt(p.p1));
        if (!arc.IsDone()) throw std::invalid_argument("не строится дуга оси");
        Handle(Geom_TrimmedCurve) curve = arc.Value();
        return BRepBuilderAPI_MakeEdge(curve).Edge();
    }
    case AxisPiece::Kind::Spiral: {
        // Отсчёты центральной линии аппроксимируются B-сплайном (степень до 8, C2, допуск 1e-4 мм).
        TColgp_Array1OfPnt points(1, static_cast<Standard_Integer>(p.pts.size()));
        for (std::size_t k = 0; k < p.pts.size(); ++k) {
            points.SetValue(static_cast<Standard_Integer>(k) + 1, to_pnt(p.pts[k]));
        }
        GeomAPI_PointsToBSpline fit(points, 3, 8, GeomAbs_C2, 1e-4);
        if (!fit.IsDone()) throw std::invalid_argument("не строится сплайн спирали");
        return BRepBuilderAPI_MakeEdge(fit.Curve()).Edge();
    }
    }
    throw std::invalid_argument("неизвестный кусок оси");
}

// Сплошное тело радиуса radius по пути (с торцами).
TopoDS_Solid build_solid(const Path& path, double radius) {
    // 256 отсчётов на виток: ошибка длины спирали ~ 1e-5 относительно точной.
    const auto pieces = axis_pieces(path, {0.0, 0.0, 0.0}, 256);
    if (pieces.empty()) throw std::invalid_argument("путь без участков");

    BRepBuilderAPI_MakeWire spineMaker;
    for (const auto& p : pieces) spineMaker.Add(make_edge(p));
    const TopoDS_Wire spine = spineMaker.Wire();

    const Vec3 t0 = pieces.front().t0;
    const Vec3 t1 = pieces.back().t1;
    const gp_Circ startCirc(gp_Ax2(to_pnt(pieces.front().p0), to_dir(t0)), radius);
    const gp_Circ endCirc(gp_Ax2(to_pnt(pieces.back().p1), to_dir(t1)), radius);

    BRepBuilderAPI_MakeWire profileMaker;
    profileMaker.Add(BRepBuilderAPI_MakeEdge(startCirc).Edge());
    BRepOffsetAPI_MakePipe pipe(spine, profileMaker.Wire());
    pipe.Build();
    if (!pipe.IsDone()) throw std::invalid_argument("протяжка профиля не выполнена");

    // MakePipe даёт только боковую поверхность: добавляем торцы и сшиваем в solid.
    BRepBuilderAPI_MakeWire endWire;
    endWire.Add(BRepBuilderAPI_MakeEdge(endCirc).Edge());
    BRepBuilderAPI_MakeFace faceStart(profileMaker.Wire());
    BRepBuilderAPI_MakeFace faceEnd(endWire.Wire());

    BRepBuilderAPI_Sewing sewing(1e-3);
    sewing.Add(pipe.Shape());
    sewing.Add(faceStart.Face());
    sewing.Add(faceEnd.Face());
    sewing.Perform();

    TopExp_Explorer shellEx(sewing.SewedShape(), TopAbs_SHELL);
    if (!shellEx.More()) throw std::invalid_argument("сшивка не дала оболочку");
    BRepBuilderAPI_MakeSolid makeSolid(TopoDS::Shell(shellEx.Current()));
    return makeSolid.Solid();
}

// Тело трубы: сплошное (wall = 0) или наружное минус внутреннее (0 < wall < r).
TopoDS_Shape build_body(const Path& path, double od_mm, double wall_mm) {
    if (!(od_mm > 0.0)) throw std::invalid_argument("∅ трубы должен быть больше нуля");
    if (wall_mm < 0.0) throw std::invalid_argument("толщина стенки не может быть отрицательной");
    const double r = od_mm / 2.0;
    const TopoDS_Solid outer = build_solid(path, r);
    if (wall_mm == 0.0) return outer;
    if (wall_mm >= r) throw std::invalid_argument("стенка не меньше радиуса трубы");

    const TopoDS_Solid inner = build_solid(path, r - wall_mm);
    BRepAlgoAPI_Cut cut(outer, inner);
    if (!cut.IsDone()) throw std::invalid_argument("вычитание внутреннего тела не выполнено");
    TopExp_Explorer solidEx(cut.Shape(), TopAbs_SOLID);
    if (!solidEx.More()) throw std::invalid_argument("вычитание не дало тело");
    return solidEx.Current();
}

}  // namespace

TubeSolidResult make_tube(const Path& path, double od_mm, double wall_mm) {
    TubeSolidResult res;
    res.axis_length = axis_length(path);
    const TopoDS_Shape body = build_body(path, od_mm, wall_mm);
    BRepCheck_Analyzer analyzer(body);
    res.valid = analyzer.IsValid();
    GProp_GProps props;
    BRepGProp::VolumeProperties(body, props);
    res.volume = props.Mass();
    return res;
}

TubeSolidResult make_solid_tube(const Path& path, double od_mm) {
    return make_tube(path, od_mm, 0.0);
}

// Порты по кромкам торцов готового тела. taken — имена, которые уже заняты (авто-имена их обходят).
// circles (если задан) получает все окружности торцов до объединения: наружные и внутренние кольца.
static std::vector<Port> ports_of_body(const TopoDS_Shape& body, const std::vector<std::string>& taken,
                                       std::vector<Port>* circles = nullptr) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(body, props);
    const gp_Pnt massCentre = props.CentreOfMass();

    // Кромки концов: окружности, у которых одна из граней — плоская (торец). Стыки участков — окружности
    // между двумя цилиндрическими гранями, их пропускаем. У трубы со стенкой торец — кольцо (две окружности).
    TopTools_IndexedDataMapOfShapeListOfShape edgeFaces;
    TopExp::MapShapesAndAncestors(body, TopAbs_EDGE, TopAbs_FACE, edgeFaces);

    std::vector<Port> ports;
    std::vector<std::string> names = taken;
    for (int i = 1; i <= edgeFaces.Extent(); ++i) {
        const TopoDS_Edge edge = TopoDS::Edge(edgeFaces.FindKey(i));
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() != GeomAbs_Circle) continue;

        bool atEnd = false;
        for (const TopoDS_Shape& face : edgeFaces.FindFromIndex(i)) {
            BRepAdaptor_Surface surface(TopoDS::Face(face));
            atEnd = atEnd || surface.GetType() == GeomAbs_Plane;
        }
        if (!atEnd) continue;

        const gp_Circ circ = curve.Circle();
        gp_Dir axis = circ.Axis().Direction();
        const gp_Pnt centre = circ.Location();
        // наружу — от центра масс тела к торцу
        if (axis.X() * (centre.X() - massCentre.X()) + axis.Y() * (centre.Y() - massCentre.Y()) +
                axis.Z() * (centre.Z() - massCentre.Z()) < 0) {
            axis.Reverse();
        }
        Port port;
        port.pos = {centre.X(), centre.Y(), centre.Z()};
        port.direction = {axis.X(), axis.Y(), axis.Z()};
        port.od = 2.0 * circ.Radius();
        if (circles) circles->push_back(port);

        // Окружность торца делится швом на две дуги с одним центром (один порт), а внутренняя окружность
        // кольца — с тем же центром и направлением, но меньшим ∅: для порта берётся наружная.
        auto same = [&](const Port& q) {
            return norm(sub(q.pos, port.pos)) < 1e-4 && dot(q.direction, port.direction) > 0.9999;
        };
        auto found = std::find_if(ports.begin(), ports.end(), same);
        if (found != ports.end()) {
            found->od = std::max(found->od, port.od);
            continue;
        }
        port.name = auto_name("tube", names);
        names.push_back(port.name);
        ports.push_back(port);
    }
    return ports;
}

std::vector<Port> end_ports(const Path& path, double od_mm, double wall_mm) {
    return ports_of_body(build_body(path, od_mm, wall_mm), {});
}

Port face_port(const Path& path, double od_mm, const Vec3& pick, const std::string& name, double wall_mm) {
    const TopoDS_Shape body = build_body(path, od_mm, wall_mm);
    const gp_Pnt target = to_pnt(pick);

    // Ближайшая грань тела к точке pick: расстояние до каждой грани, минимум.
    double best = std::numeric_limits<double>::infinity();
    TopoDS_Face bestFace;
    double bestU = 0.0;
    double bestV = 0.0;
    gp_Pnt bestPoint;
    bool found = false;
    TopExp_Explorer faceEx(body, TopAbs_FACE);
    for (; faceEx.More(); faceEx.Next()) {
        const TopoDS_Face face = TopoDS::Face(faceEx.Current());
        BRepExtrema_DistShapeShape dist(BRepBuilderAPI_MakeVertex(target).Vertex(), face);
        if (!dist.IsDone() || dist.NbSolution() == 0) continue;
        if (dist.Value() < best) {
            best = dist.Value();
            bestFace = face;
            bestPoint = dist.PointOnShape2(1);
            dist.ParOnFaceS2(1, bestU, bestV);
            found = true;
        }
    }
    if (!found) throw std::invalid_argument("грань тела не найдена");

    // Нормаль наружу в найденной точке: BRepGProp_Face учитывает ориентацию грани в теле.
    BRepGProp_Face gface(bestFace);
    gp_Pnt surfacePoint;
    gp_Vec normal;
    gface.Normal(bestU, bestV, surfacePoint, normal);
    if (normal.Magnitude() < 1e-12) throw std::invalid_argument("нормаль грани вырождена");
    normal.Normalize();

    Port port;
    port.name = name;
    port.pos = {bestPoint.X(), bestPoint.Y(), bestPoint.Z()};
    port.direction = {normal.X(), normal.Y(), normal.Z()};
    port.od = od_mm;
    return port;
}

TubeModel complete_model(const TubeModel& model) {
    if (!model.path) return model;
    TubeModel out = model;
    std::vector<std::string> taken;
    for (const Port& p : model.ports) taken.push_back(p.name);
    const std::vector<Port> ends = ports_of_body(build_body(*model.path, model.od, model.wall), taken);
    out.ports.insert(out.ports.end(), ends.begin(), ends.end());
    return out;
}

TubeModel load_step(const std::string& file, const std::string& id) {
    STEPControl_Reader reader;
    if (reader.ReadFile(file.c_str()) != IFSelect_RetDone) {
        throw std::invalid_argument("STEP не читается: " + file);
    }
    if (reader.TransferRoots() <= 0) throw std::invalid_argument("в STEP нет переносимых тел");
    const TopoDS_Shape shape = reader.OneShape();
    TopExp_Explorer solidEx(shape, TopAbs_SOLID);
    if (!solidEx.More()) throw std::invalid_argument("в STEP нет твёрдого тела");

    TubeModel m;
    m.id = id;
    m.source = "step";
    std::vector<Port> circles;
    m.ports = ports_of_body(solidEx.Current(), {}, &circles);
    if (m.ports.empty()) throw std::invalid_argument("у тела нет торцов с окружностями: порты не найдены");
    for (const Port& p : m.ports) m.od = std::max(m.od, p.od);

    // Стенка: у каждого торца внутренняя окружность того же центра и направления; стенка = (∅нар − ∅вн) / 2.
    // Если торцы дают разные стенки или внутренних окружностей нет у части торцов, стенка не определена (0).
    double wall = -1.0;
    bool consistent = true;
    for (const Port& p : m.ports) {
        double inner = 0.0;
        for (const Port& c : circles) {
            const bool sameEnd = norm(sub(c.pos, p.pos)) < 1e-4 && dot(c.direction, p.direction) > 0.9999;
            if (sameEnd && c.od < p.od - 1e-6) inner = std::max(inner, c.od);
        }
        const double w = inner > 0.0 ? (p.od - inner) / 2.0 : 0.0;
        if (wall < 0.0) {
            wall = w;
        } else if (std::abs(w - wall) > 1e-6) {
            consistent = false;
        }
    }
    m.wall = (consistent && wall > 0.0) ? wall : 0.0;
    return m;
}

void export_step(const Path& path, double od_mm, double wall_mm, const std::string& file) {
    const TopoDS_Shape body = build_body(path, od_mm, wall_mm);
    STEPControl_Writer writer;
    if (writer.Transfer(body, STEPControl_AsIs) != IFSelect_RetDone) {
        throw std::invalid_argument("STEP: передача тела не выполнена");
    }
    if (writer.Write(file.c_str()) != IFSelect_RetDone) {
        throw std::invalid_argument("STEP: запись не выполнена: " + file);
    }
}

}  // namespace bendcore::geom
