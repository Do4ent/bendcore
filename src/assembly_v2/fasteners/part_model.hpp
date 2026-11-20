#pragma once

// Общие средства моделей стандартных изделий (подшипники, шпонки): ошибки построения,
// грани с геометрией для определения ролей, карта элементов с ролями и атрибутами.

#include <bendcore/kernel/result.hpp>
#include <bendcore/modeling/naming.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace bendcore::assembly_v2::fasteners::detail {

struct PartFailure {
    kernel::ErrorCode code;
    std::string message;
};

[[noreturn]] inline void partFail(kernel::ErrorCode code, std::string message) {
    throw PartFailure{code, std::move(message)};
}

inline std::string partNum(double v) {
    std::ostringstream os;
    os << std::setprecision(10) << v;
    return os.str();
}

// Число для обозначения ГОСТ: без лишних нулей, разделитель — запятая.
inline std::string gostNumber(double v) {
    std::ostringstream os;
    os << std::setprecision(6) << v;
    std::string s = os.str();
    for (char& c : s)
        if (c == '.') c = ',';
    return s;
}

struct PartFace {
    TopoDS_Face face;
    GeomAbs_SurfaceType type = GeomAbs_OtherSurface;
    gp_Pnt centroid;
    gp_Dir normal;           // плоскость: наружная нормаль
    double radius = 0.0;     // цилиндр, сфера; тор — малый радиус
    double major = 0.0;      // тор — большой радиус
    int body = 0;            // номер тела (TopExp_Explorer по телам, с 0)
};

// Грани по телам формы (центр тяжести грани тела вращения лежит на оси, поэтому принадлежность
// кольцу определяется телом, а не положением грани).
inline std::vector<PartFace> partFaces(const TopoDS_Shape& shape) {
    std::vector<std::pair<TopoDS_Face, int>> list;
    int body = 0;
    for (TopExp_Explorer s(shape, TopAbs_SOLID); s.More(); s.Next(), ++body)
        for (TopExp_Explorer f(s.Current(), TopAbs_FACE); f.More(); f.Next()) list.emplace_back(TopoDS::Face(f.Current()), body);
    std::vector<PartFace> out;
    for (const auto& [face, b] : list) {
        PartFace g;
        g.face = face;
        g.body = b;
        BRepAdaptor_Surface s(g.face, true);
        g.type = s.GetType();
        GProp_GProps props;
        BRepGProp::SurfaceProperties(g.face, props);
        g.centroid = props.CentreOfMass();
        if (g.type == GeomAbs_Plane) {
            gp_Dir n = s.Plane().Axis().Direction();
            if (!s.Plane().Position().Direct()) n.Reverse();
            if (g.face.Orientation() == TopAbs_REVERSED) n.Reverse();
            g.normal = n;
        } else if (g.type == GeomAbs_Cylinder) {
            g.radius = s.Cylinder().Radius();
        } else if (g.type == GeomAbs_Sphere) {
            g.radius = s.Sphere().Radius();
            g.centroid = s.Sphere().Location();
        } else if (g.type == GeomAbs_Torus) {
            g.radius = s.Torus().MinorRadius();
            g.major = s.Torus().MajorRadius();
        }
        out.push_back(g);
    }
    return out;
}

inline double shapeVolume(const TopoDS_Shape& s) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(s, props);
    return props.Mass();
}

inline void requireValid(const TopoDS_Shape& s, const std::string& what) {
    BRepCheck_Analyzer check(s);
    if (!check.IsValid()) partFail(kernel::ErrorCode::OperationFailed, what + " body is not valid (BRepCheck)");
}

// Роль грани: имя и ключ сортировки для нумерованных ролей (пустое имя — грань без роли).
struct FaceRole {
    std::string role;
    bool numbered = false;  // имя получает номер 1…n по возрастанию key
    double key = 0.0;
};

// Карта элементов: classify назначает роли (уникальные или нумерованные), attributes ставятся
// на все грани. Роль, которую получили две грани, или отсутствие обязательной — ошибка.
inline std::shared_ptr<const modeling::NamedShape> namePartFaces(const TopoDS_Shape& shape, const std::string& feature,
                                                                  const std::function<FaceRole(const PartFace&)>& classify,
                                                                  const std::vector<std::string>& required,
                                                                  const modeling::ElementAttributes& attributes) {
    std::vector<modeling::ElementRole> roles;
    std::map<std::string, std::vector<std::pair<double, TopoDS_Face>>> numbered;
    std::map<std::string, int> seen;
    modeling::NamingRequest req;
    for (const PartFace& g : partFaces(shape)) {
        req.attributes.emplace_back(g.face, attributes);
        const FaceRole r = classify(g);
        if (r.role.empty()) continue;
        if (r.numbered) {
            numbered[r.role].emplace_back(r.key, g.face);
            continue;
        }
        if (++seen[r.role] > 1) partFail(kernel::ErrorCode::OperationFailed, feature + " face role '" + r.role + "' is not unique");
        roles.push_back({g.face, r.role, attributes});
    }
    for (auto& [role, list] : numbered) {
        std::stable_sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (std::size_t i = 0; i < list.size(); ++i) {
            const std::string name = role + std::to_string(i + 1);
            roles.push_back({list[i].second, name, attributes});
            seen[name] = 1;
        }
    }
    for (const std::string& r : required)
        if (seen.count(r) == 0) partFail(kernel::ErrorCode::OperationFailed, feature + " face role '" + r + "' not found");
    req.featureId = feature;
    req.result = shape;
    req.roles = std::move(roles);
    auto named = modeling::buildNamedShape(req);
    if (!named) partFail(named.error().code, feature + " naming: " + named.error().message);
    return std::make_shared<const modeling::NamedShape>(std::move(named).value());
}

inline std::optional<double> findDimension(const std::vector<std::pair<std::string, double>>& dims, const std::string& key) {
    for (const auto& [k, v] : dims)
        if (k == key) return v;
    return std::nullopt;
}

}  // namespace bendcore::assembly_v2::fasteners::detail
