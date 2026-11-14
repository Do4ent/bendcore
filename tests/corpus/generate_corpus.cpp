// Генератор синтетического корпуса STEP с аналитически известными величинами.
//
// Использование: generate_corpus <каталог>
// Пишет <каталог>/<модель>.step, IGES-копии части моделей (<модель>.igs) и <каталог>/manifest.json.
//
//        generate_corpus --large <файл.step> <минимум МБ>
// Крупный STEP для нагрузочного теста (tests/regression/stress/test_stress_step.cpp): сетка
// чередующихся брусков 20×10×5 и цилиндров R5×12 (каждое тело — своя топология, без общих
// представлений), число тел подбирается так, чтобы файл был не меньше заданного размера.
// Рядом пишется <файл.step>.json с ожидаемыми величинами (по формулам). В git не хранится.
//
// Ожидаемые значения в манифесте (объём, площадь, габариты, число тел и граней, имена,
// цвета) вычисляются здесь по формулам из размеров модели, а не измерением OCCT:
// так регрессионный тест test_step_corpus сверяет импорт с независимым эталоном.
// Описание моделей и формул — tests/corpus/README.md.

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <DESTEP_Parameters.hxx>
#include <IGESCAFControl_Writer.hxx>
#include <IGESControl_Controller.hxx>
#include <Interface_Static.hxx>
#include <NCollection_IndexedMap.hxx>
#include <Quantity_Color.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Failure.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp.hxx>
#include <TopLoc_Location.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

using Vec3 = std::array<double, 3>;
using Rgb = std::array<double, 3>;

// Деталь сборки: прототип с именем и цветом и его вхождения (смещения).
struct Component {
  std::string name;
  TopoDS_Shape shape;
  Rgb color;
  std::vector<Vec3> placements;
};

// Модель корпуса: форма (или сборка) и ожидаемые значения, вычисленные по формулам.
struct Model {
  std::string name;
  std::string description;
  std::string formula;
  DESTEP_Parameters::WriteMode_StepSchema schema = DESTEP_Parameters::WriteMode_StepSchema_AP214IS;
  TopoDS_Shape shape;                 // для одиночной детали
  std::vector<Component> components;  // для сборки (shape пуст)
  int solids = 0;
  int faces = 0;
  double volume = 0.0;
  double area = 0.0;
  double volume_rel_tol = 1e-6;
  double area_rel_tol = 1e-6;
  Vec3 bbox_min{};
  Vec3 bbox_max{};
};

const char* schema_name(DESTEP_Parameters::WriteMode_StepSchema s) {
  switch (s) {
    case DESTEP_Parameters::WriteMode_StepSchema_AP203: return "AP203";
    case DESTEP_Parameters::WriteMode_StepSchema_AP242DIS: return "AP242";
    default: return "AP214";
  }
}

// Ключевое слово FILE_SCHEMA, которое стандарт задаёт для каждого протокола.
const char* schema_keyword(DESTEP_Parameters::WriteMode_StepSchema s) {
  switch (s) {
    case DESTEP_Parameters::WriteMode_StepSchema_AP203: return "CONFIG_CONTROL_DESIGN";
    case DESTEP_Parameters::WriteMode_StepSchema_AP242DIS: return "AP242_MANAGED_MODEL_BASED_3D_ENGINEERING";
    default: return "AUTOMOTIVE_DESIGN";
  }
}

void require_valid(const TopoDS_Shape& s, const std::string& what) {
  if (s.IsNull()) throw std::runtime_error(what + ": построение вернуло пустую форму");
  BRepCheck_Analyzer check(s);
  if (!check.IsValid()) throw std::runtime_error(what + ": BRepCheck_Analyzer: форма невалидна");
}

TopoDS_Shape box(double a, double b, double c) {
  return BRepPrimAPI_MakeBox(a, b, c).Shape();
}

TopoDS_Shape cylinder(double r, double h, const gp_Pnt& base = gp_Pnt(0, 0, 0)) {
  return BRepPrimAPI_MakeCylinder(gp_Ax2(base, gp_Dir(0, 0, 1)), r, h).Shape();
}

TopoDS_Shape cut(const TopoDS_Shape& a, const TopoDS_Shape& b, const std::string& what) {
  BRepAlgoAPI_Cut op(a, b);
  if (!op.IsDone()) throw std::runtime_error(what + ": булева разность не выполнена");
  return op.Shape();
}

// Скругление рёбер, отобранных предикатом, радиусом r.
TopoDS_Shape fillet_edges(const TopoDS_Shape& s, double r,
                          const std::function<bool(const TopoDS_Edge&)>& pick,
                          const std::string& what) {
  BRepFilletAPI_MakeFillet mk(s);
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges;
  TopExp::MapShapes(s, TopAbs_EDGE, edges);
  int added = 0;
  for (int i = 1; i <= edges.Extent(); ++i) {
    const TopoDS_Edge& e = TopoDS::Edge(edges(i));
    if (pick(e)) {
      mk.Add(r, e);
      ++added;
    }
  }
  if (added == 0) throw std::runtime_error(what + ": не выбрано ни одного ребра");
  mk.Build();
  if (!mk.IsDone()) throw std::runtime_error(what + ": скругление не построено");
  return mk.Shape();
}

bool edge_parallel_to_z(const TopoDS_Edge& e) {
  TopoDS_Vertex v1, v2;
  TopExp::Vertices(e, v1, v2);
  const gp_Pnt p1 = BRep_Tool::Pnt(v1), p2 = BRep_Tool::Pnt(v2);
  return std::abs(p1.X() - p2.X()) < 1e-9 && std::abs(p1.Y() - p2.Y()) < 1e-9;
}

// ---------------------------------------------------------------- модели и формулы

Model make_box() {
  const double a = 100, b = 60, c = 20;
  Model m;
  m.name = "box_100x60x20";
  m.description = "Прямоугольный параллелепипед a x b x c";
  m.formula = "V = a*b*c; S = 2(ab+bc+ca); граней 6";
  m.schema = DESTEP_Parameters::WriteMode_StepSchema_AP203;
  m.shape = box(a, b, c);
  m.solids = 1;
  m.faces = 6;
  m.volume = a * b * c;
  m.area = 2 * (a * b + b * c + c * a);
  m.bbox_min = {0, 0, 0};
  m.bbox_max = {a, b, c};
  return m;
}

Model make_cylinder() {
  const double r = 20, h = 50;
  Model m;
  m.name = "cylinder_r20_h50";
  m.description = "Круговой цилиндр радиуса r и высоты h";
  m.formula = "V = pi r^2 h; S = 2 pi r^2 + 2 pi r h; граней 3";
  m.shape = cylinder(r, h);
  m.solids = 1;
  m.faces = 3;
  m.volume = kPi * r * r * h;
  m.area = 2 * kPi * r * r + 2 * kPi * r * h;
  m.bbox_min = {-r, -r, 0};
  m.bbox_max = {r, r, h};
  return m;
}

Model make_tube() {
  const double R = 30, r = 10, h = 40;
  Model m;
  m.name = "tube_R30_r10_h40";
  m.description = "Цилиндр с соосным сквозным отверстием (булева разность)";
  m.formula = "V = pi (R^2 - r^2) h; S = 2 pi (R^2 - r^2) + 2 pi (R + r) h; граней 4";
  m.shape = cut(cylinder(R, h), cylinder(r, h + 2, gp_Pnt(0, 0, -1)), m.name);
  m.solids = 1;
  m.faces = 4;
  m.volume = kPi * (R * R - r * r) * h;
  m.area = 2 * kPi * (R * R - r * r) + 2 * kPi * (R + r) * h;
  m.bbox_min = {-R, -R, 0};
  m.bbox_max = {R, R, h};
  return m;
}

Model make_plate_with_hole() {
  const double a = 120, b = 80, c = 10, r = 10;
  Model m;
  m.name = "plate_120x80x10_hole_d20";
  m.description = "Пластина с центральным сквозным отверстием";
  m.formula = "V = abc - pi r^2 c; S = 2(ab+bc+ca) - 2 pi r^2 + 2 pi r c; граней 7";
  m.shape = cut(box(a, b, c), cylinder(r, c + 2, gp_Pnt(a / 2, b / 2, -1)), m.name);
  m.solids = 1;
  m.faces = 7;
  m.volume = a * b * c - kPi * r * r * c;
  m.area = 2 * (a * b + b * c + c * a) - 2 * kPi * r * r + 2 * kPi * r * c;
  m.bbox_min = {0, 0, 0};
  m.bbox_max = {a, b, c};
  return m;
}

Model make_torus() {
  const double R = 50, r = 10;
  Model m;
  m.name = "torus_R50_r10";
  m.description = "Тор: радиус осевой окружности R, радиус трубки r";
  m.formula = "V = 2 pi^2 R r^2; S = 4 pi^2 R r; граней 1";
  m.schema = DESTEP_Parameters::WriteMode_StepSchema_AP242DIS;
  m.shape = BRepPrimAPI_MakeTorus(R, r).Shape();
  m.solids = 1;
  m.faces = 1;
  m.volume = 2 * kPi * kPi * R * r * r;
  m.area = 4 * kPi * kPi * R * r;
  m.bbox_min = {-(R + r), -(R + r), -r};
  m.bbox_max = {R + r, R + r, r};
  return m;
}

Model make_sphere() {
  const double R = 25;
  Model m;
  m.name = "sphere_R25";
  m.description = "Шар радиуса R";
  m.formula = "V = 4/3 pi R^3; S = 4 pi R^2; граней 1";
  m.shape = BRepPrimAPI_MakeSphere(R).Shape();
  m.solids = 1;
  m.faces = 1;
  m.volume = 4.0 / 3.0 * kPi * R * R * R;
  m.area = 4 * kPi * R * R;
  m.bbox_min = {-R, -R, -R};
  m.bbox_max = {R, R, R};
  return m;
}

Model make_frustum() {
  const double R1 = 30, R2 = 10, h = 40;
  const double s = std::sqrt(h * h + (R1 - R2) * (R1 - R2));
  Model m;
  m.name = "frustum_R30_R10_h40";
  m.description = "Усечённый конус: радиусы оснований R1, R2, высота h";
  m.formula = "V = pi h/3 (R1^2 + R1 R2 + R2^2); S = pi (R1^2 + R2^2) + pi (R1 + R2) sqrt(h^2 + (R1-R2)^2); граней 3";
  m.shape = BRepPrimAPI_MakeCone(R1, R2, h).Shape();
  m.solids = 1;
  m.faces = 3;
  m.volume = kPi * h / 3.0 * (R1 * R1 + R1 * R2 + R2 * R2);
  m.area = kPi * (R1 * R1 + R2 * R2) + kPi * (R1 + R2) * s;
  m.bbox_min = {-R1, -R1, 0};
  m.bbox_max = {R1, R1, h};
  return m;
}

Model make_rounded_plate() {
  const double a = 100, b = 60, c = 10, r = 8;
  Model m;
  m.name = "plate_100x60x10_corner_fillets_r8";
  m.description = "Пластина со скруглёнными вертикальными рёбрами (BRepFilletAPI_MakeFillet)";
  m.formula = "V = c (ab - (4 - pi) r^2); S = 2 (ab - (4 - pi) r^2) + c (2(a+b) - 8r + 2 pi r); граней 10";
  m.shape = fillet_edges(box(a, b, c), r, edge_parallel_to_z, m.name);
  m.solids = 1;
  m.faces = 10;
  const double top = a * b - (4 - kPi) * r * r;
  m.volume = c * top;
  m.area = 2 * top + c * (2 * (a + b) - 8 * r + 2 * kPi * r);
  m.bbox_min = {0, 0, 0};
  m.bbox_max = {a, b, c};
  return m;
}

Model make_filleted_box() {
  const double a = 80, b = 60, c = 40, r = 5;
  const double ai = a - 2 * r, bi = b - 2 * r, ci = c - 2 * r;
  Model m;
  m.name = "box_80x60x40_all_fillets_r5";
  m.description = "Коробка со скруглением всех 12 рёбер радиусом r (сумма Минковского "
                  "внутренней коробки a'b'c' и шара r)";
  m.formula = "a'=a-2r...; V = a'b'c' + 2r(a'b'+b'c'+c'a') + pi r^2 (a'+b'+c') + 4/3 pi r^3; "
              "S = 2(a'b'+b'c'+c'a') + 2 pi r (a'+b'+c') + 4 pi r^2; граней 6+12+8=26";
  m.shape = fillet_edges(box(a, b, c), r, [](const TopoDS_Edge&) { return true; }, m.name);
  m.solids = 1;
  m.faces = 26;
  m.volume = ai * bi * ci + 2 * r * (ai * bi + bi * ci + ci * ai) + kPi * r * r * (ai + bi + ci) +
             4.0 / 3.0 * kPi * r * r * r;
  m.area = 2 * (ai * bi + bi * ci + ci * ai) + 2 * kPi * r * (ai + bi + ci) + 4 * kPi * r * r;
  // Угловые сопряжения OCCT строит приближёнными поверхностями: допуск шире.
  m.volume_rel_tol = 1e-4;
  m.area_rel_tol = 1e-4;
  m.bbox_min = {0, 0, 0};
  m.bbox_max = {a, b, c};
  return m;
}

Model make_l_bracket() {
  const double L = 60, H = 40, t = 8, w = 30;
  Model m;
  m.name = "l_bracket_60x40x8_w30";
  m.description = "Уголок: выдавливание L-профиля (полки L и H, толщина t) на ширину w";
  m.formula = "A = L t + (H - t) t; P = 2(L + H); V = A w; S = 2A + P w; граней 8";
  BRepBuilderAPI_MakePolygon poly;
  poly.Add(gp_Pnt(0, 0, 0));
  poly.Add(gp_Pnt(L, 0, 0));
  poly.Add(gp_Pnt(L, t, 0));
  poly.Add(gp_Pnt(t, t, 0));
  poly.Add(gp_Pnt(t, H, 0));
  poly.Add(gp_Pnt(0, H, 0));
  poly.Close();
  const TopoDS_Face face = BRepBuilderAPI_MakeFace(poly.Wire(), true).Face();
  m.shape = BRepPrimAPI_MakePrism(face, gp_Vec(0, 0, w)).Shape();
  m.solids = 1;
  m.faces = 8;
  const double A = L * t + (H - t) * t;
  const double P = 2 * (L + H);
  m.volume = A * w;
  m.area = 2 * A + P * w;
  m.bbox_min = {0, 0, 0};
  m.bbox_max = {L, H, w};
  return m;
}

Model make_multibody() {
  Model m;
  m.name = "multibody_two_boxes";
  m.description = "Одна деталь из двух несвязанных тел (compound): куб 10 и брус 20x10x5 со смещением";
  m.formula = "V = 10^3 + 20*10*5; S = 6*10^2 + 2(200+50+100); тел 2, граней 12";
  BRep_Builder bb;
  TopoDS_Compound comp;
  bb.MakeCompound(comp);
  bb.Add(comp, box(10, 10, 10));
  gp_Trsf tr;
  tr.SetTranslation(gp_Vec(30, 0, 0));
  bb.Add(comp, BRepBuilderAPI_Transform(box(20, 10, 5), tr, true).Shape());
  m.shape = comp;
  m.solids = 2;
  m.faces = 12;
  m.volume = 1000.0 + 20.0 * 10.0 * 5.0;
  m.area = 6 * 100.0 + 2 * (200.0 + 50.0 + 100.0);
  m.bbox_min = {0, 0, 0};
  m.bbox_max = {50, 10, 10};
  return m;
}

// Сборка: основание (красное), два вхождения штифта (синие), втулка (зелёная).
Model make_assembly(DESTEP_Parameters::WriteMode_StepSchema schema, const std::string& name) {
  const double a = 80, b = 40, c = 10;          // основание
  const double rp = 4, hp = 30;                 // штифт
  const double Rs = 6, rs = 3, hs = 15;         // втулка
  Model m;
  m.name = name;
  m.description = "Сборка XCAF: BASE_PLATE (1 вхождение), PIN (2 вхождения), SPACER (1 вхождение); "
                  "имена и цвета деталей";
  m.formula = "V = abc + 2 pi rp^2 hp + pi (Rs^2 - rs^2) hs; "
              "S = 2(ab+bc+ca) + 2(2 pi rp^2 + 2 pi rp hp) + 2 pi (Rs^2 - rs^2) + 2 pi (Rs + rs) hs; "
              "тел 4, граней 6 + 2*3 + 4";
  m.schema = schema;
  m.components.push_back({"BASE_PLATE", box(a, b, c), {1, 0, 0}, {{0, 0, 0}}});
  m.components.push_back({"PIN", cylinder(rp, hp), {0, 0, 1}, {{20, 20, c}, {60, 20, c}}});
  m.components.push_back({"SPACER", cut(cylinder(Rs, hs), cylinder(rs, hs + 2, gp_Pnt(0, 0, -1)), name),
                          {0, 1, 0}, {{40, 20, c}}});
  m.solids = 4;
  m.faces = 6 + 2 * 3 + 4;
  m.volume = a * b * c + 2 * kPi * rp * rp * hp + kPi * (Rs * Rs - rs * rs) * hs;
  m.area = 2 * (a * b + b * c + c * a) + 2 * (2 * kPi * rp * rp + 2 * kPi * rp * hp) +
           2 * kPi * (Rs * Rs - rs * rs) + 2 * kPi * (Rs + rs) * hs;
  m.bbox_min = {0, 0, 0};
  m.bbox_max = {a, b, c + hp};
  return m;
}

// ---------------------------------------------------------------- запись STEP

void set_name(const TDF_Label& l, const std::string& name) {
  TDataStd_Name::Set(l, TCollection_ExtendedString(name.c_str()));
}

// Документ XCAF модели: деталь или сборка с именами и цветами деталей.
Handle(TDocStd_Document) build_xcaf(const Model& m) {
  Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
  Handle(TDocStd_Document) doc;
  app->NewDocument("MDTV-XCAF", doc);
  Handle(XCAFDoc_ShapeTool) shapes = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
  Handle(XCAFDoc_ColorTool) colors = XCAFDoc_DocumentTool::ColorTool(doc->Main());

  if (m.components.empty()) {
    require_valid(m.shape, m.name);
    const TDF_Label part = shapes->AddShape(m.shape, false);
    set_name(part, m.name);
  } else {
    const TDF_Label asm_label = shapes->NewShape();
    set_name(asm_label, m.name);
    for (const Component& c : m.components) {
      require_valid(c.shape, m.name + "/" + c.name);
      const TDF_Label proto = shapes->AddShape(c.shape, false);
      set_name(proto, c.name);
      colors->SetColor(proto, Quantity_Color(c.color[0], c.color[1], c.color[2], Quantity_TOC_RGB),
                       XCAFDoc_ColorGen);
      int k = 0;
      for (const Vec3& p : c.placements) {
        gp_Trsf tr;
        tr.SetTranslation(gp_Vec(p[0], p[1], p[2]));
        const TDF_Label inst = shapes->AddComponent(asm_label, proto, TopLoc_Location(tr));
        set_name(inst, c.name + "_" + std::to_string(++k));
      }
    }
    shapes->UpdateAssemblies();
  }
  return doc;
}

void write_step(const Model& m, const std::filesystem::path& file) {
  Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
  Handle(TDocStd_Document) doc = build_xcaf(m);

  DESTEP_Parameters params;
  params.InitFromStatic();
  params.WriteSchema = m.schema;
  params.WriteUnit = UnitsMethods_LengthUnit_Millimeter;
  STEPCAFControl_Writer writer;
  writer.SetNameMode(true);
  writer.SetColorMode(true);
  if (!writer.Perform(doc, file.string().c_str(), params)) {
    app->Close(doc);
    throw std::runtime_error(m.name + ": STEPCAFControl_Writer::Perform не записал " + file.string());
  }
  app->Close(doc);
}

// IGES 5.3 той же модели (IGESCAFControl_Writer, единица мм). brep = true: тела как
// manifold solid B-rep object (тип 186); false: только обрезанные поверхности (тип 144), как
// пишет большинство CAD — читатель сшивает их в тело.
void write_iges(const Model& m, const std::filesystem::path& file, bool brep) {
  Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
  Handle(TDocStd_Document) doc = build_xcaf(m);
  IGESControl_Controller::Init();
  Interface_Static::SetCVal("write.iges.unit", "MM");
  Interface_Static::SetIVal("write.iges.brep.mode", brep ? 1 : 0);
  IGESCAFControl_Writer writer;
  writer.SetNameMode(true);
  writer.SetColorMode(true);
  const bool ok = writer.Transfer(doc) && writer.Write(file.string().c_str());
  app->Close(doc);
  if (!ok) throw std::runtime_error(m.name + ": IGESCAFControl_Writer не записал " + file.string());
}

nlohmann::json to_json(const Model& m) {
  nlohmann::json j;
  j["file"] = m.name + ".step";
  j["name"] = m.name;
  j["description"] = m.description;
  j["formula"] = m.formula;
  j["schema"] = schema_name(m.schema);
  j["schema_keyword"] = schema_keyword(m.schema);
  j["free_shapes"] = 1;
  j["top_name"] = m.name;
  j["solids"] = m.solids;
  j["faces"] = m.faces;
  j["volume"] = m.volume;
  j["volume_rel_tol"] = m.volume_rel_tol;
  j["area"] = m.area;
  j["area_rel_tol"] = m.area_rel_tol;
  j["bbox_min"] = m.bbox_min;
  j["bbox_max"] = m.bbox_max;
  j["bbox_abs_tol"] = 1e-4;
  if (!m.components.empty()) {
    nlohmann::json comps = nlohmann::json::array();
    for (const Component& c : m.components) {
      comps.push_back({{"name", c.name},
                       {"color", c.color},
                       {"instances", static_cast<int>(c.placements.size())}});
    }
    j["components"] = comps;
  }
  return j;
}


// ---- крупный STEP (--large)

constexpr double kLargeBox[3] = {20.0, 10.0, 5.0};
constexpr double kLargeCylR = 5.0, kLargeCylH = 12.0, kLargePitch = 30.0;
constexpr int kLargeRow = 100;  // тел в ряду по X; рядов по Y — 100, затем следующий слой по Z

// Тело i сетки: чётные — брусок, нечётные — цилиндр; угол/основание в узле сетки.
TopoDS_Shape large_item(int i) {
  const int ix = i % kLargeRow, iy = (i / kLargeRow) % kLargeRow, iz = i / (kLargeRow * kLargeRow);
  const gp_Pnt p(ix * kLargePitch, iy * kLargePitch, iz * kLargePitch);
  if (i % 2 == 0) return BRepPrimAPI_MakeBox(p, kLargeBox[0], kLargeBox[1], kLargeBox[2]).Shape();
  return BRepPrimAPI_MakeCylinder(gp_Ax2(p, gp::DZ()), kLargeCylR, kLargeCylH).Shape();
}

std::uintmax_t write_large(const std::filesystem::path& file, int count) {
  BRep_Builder bb;
  TopoDS_Compound c;
  bb.MakeCompound(c);
  for (int i = 0; i < count; ++i) bb.Add(c, large_item(i));
  STEPControl_Writer w;
  if (w.Transfer(c, STEPControl_AsIs) != IFSelect_RetDone)
    throw std::runtime_error("--large: STEPControl_Writer::Transfer не выполнен");
  if (w.Write(file.string().c_str()) != IFSelect_RetDone)
    throw std::runtime_error("--large: не записан " + file.string());
  return std::filesystem::file_size(file);
}

int generate_large(const std::filesystem::path& file, double min_mb) {
  if (!(min_mb > 0) || min_mb > 4096) throw std::runtime_error("--large: размер должен быть в (0, 4096] МБ");
  if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path());
  // Размер одного тела в файле — по пробной записи 400 тел; затем запас 3%.
  const int probe = 400;
  const double per_item = double(write_large(file, probe)) / probe;
  const double min_bytes = min_mb * 1024.0 * 1024.0;
  const int count = int(std::ceil(min_bytes / per_item * 1.03));
  const std::uintmax_t size = write_large(file, count);
  if (double(size) < min_bytes)
    throw std::runtime_error("--large: файл " + std::to_string(size) + " байт меньше заданного минимума");
  const int boxes = (count + 1) / 2, cylinders = count / 2;
  nlohmann::json j;
  j["generator"] = "tests/corpus/generate_corpus.cpp --large";
  j["file"] = file.filename().string();
  j["bytes"] = size;
  j["solids"] = count;
  j["boxes"] = boxes;
  j["cylinders"] = cylinders;
  j["faces"] = 6 * boxes + 3 * cylinders;
  j["volume"] = boxes * kLargeBox[0] * kLargeBox[1] * kLargeBox[2] + cylinders * kPi * kLargeCylR * kLargeCylR * kLargeCylH;
  j["volume_rel_tol"] = 1e-6;
  // Габариты: от минимума по узлам (0 или −R цилиндра) до максимума по последним узлам.
  double lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
  for (int i = 0; i < count; ++i) {
    const int ix = i % kLargeRow, iy = (i / kLargeRow) % kLargeRow, iz = i / (kLargeRow * kLargeRow);
    const double x = ix * kLargePitch, y = iy * kLargePitch, z = iz * kLargePitch;
    const double a[3] = {i % 2 ? x - kLargeCylR : x, i % 2 ? y - kLargeCylR : y, z};
    const double b[3] = {i % 2 ? x + kLargeCylR : x + kLargeBox[0], i % 2 ? y + kLargeCylR : y + kLargeBox[1],
                         z + (i % 2 ? kLargeCylH : kLargeBox[2])};
    for (int k = 0; k < 3; ++k) {
      lo[k] = std::min(lo[k], a[k]);
      hi[k] = std::max(hi[k], b[k]);
    }
  }
  j["bbox_min"] = {lo[0], lo[1], lo[2]};
  j["bbox_max"] = {hi[0], hi[1], hi[2]};
  std::ofstream mf(file.string() + ".json");
  mf << j.dump(2) << "\n";
  if (!mf) throw std::runtime_error("--large: не записан " + file.string() + ".json");
  std::cout << "записано: " << file.string() << " (" << size << " байт, тел: " << count << ")\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 4 && std::string(argv[1]) == "--large") {
    try {
      return generate_large(argv[2], std::strtod(argv[3], nullptr));
    } catch (const Standard_Failure& e) {
      std::cerr << "generate_corpus: ошибка OCCT: " << e.what() << "\n";
    } catch (const std::exception& e) {
      std::cerr << "generate_corpus: " << e.what() << "\n";
    }
    return 1;
  }
  if (argc != 2) {
    std::cerr << "Использование: " << argv[0] << " <каталог для STEP-файлов и manifest.json>\n"
              << "               " << argv[0] << " --large <файл.step> <минимум МБ>\n";
    return 2;
  }
  const std::filesystem::path out = argv[1];
  try {
    std::filesystem::create_directories(out);
    const std::vector<Model> models = {
        make_box(),
        make_cylinder(),
        make_tube(),
        make_plate_with_hole(),
        make_torus(),
        make_sphere(),
        make_frustum(),
        make_rounded_plate(),
        make_filleted_box(),
        make_l_bracket(),
        make_multibody(),
        make_assembly(DESTEP_Parameters::WriteMode_StepSchema_AP214IS, "assembly_bracket_pins_ap214"),
        make_assembly(DESTEP_Parameters::WriteMode_StepSchema_AP242DIS, "assembly_bracket_pins_ap242"),
    };
    nlohmann::json manifest;
    manifest["schema_version"] = 1;
    manifest["generator"] = "tests/corpus/generate_corpus.cpp";
    manifest["units"] = "mm";
    manifest["note"] = "Ожидаемые значения вычислены по формулам из размеров, не измерением OCCT";
    manifest["models"] = nlohmann::json::array();
    for (const Model& m : models) {
      write_step(m, out / (m.name + ".step"));
      manifest["models"].push_back(to_json(m));
      std::cout << "записано: " << (out / (m.name + ".step")).string() << "\n";
    }
    // IGES-копии части моделей: ожидаемые значения — у модели с тем же именем в "models".
    struct IgesCopy {
      std::string model;
      std::string file;
      bool brep;
    };
    const std::vector<IgesCopy> iges = {
        {"box_100x60x20", "box_100x60x20.igs", true},
        {"cylinder_r20_h50", "cylinder_r20_h50.igs", true},
        {"l_bracket_60x40x8_w30", "l_bracket_60x40x8_w30.igs", true},
        {"plate_120x80x10_hole_d20", "plate_120x80x10_hole_d20_faces.igs", false},
        {"assembly_bracket_pins_ap214", "assembly_bracket_pins.igs", true},
    };
    manifest["iges"] = nlohmann::json::array();
    for (const IgesCopy& c : iges) {
      const auto it = std::find_if(models.begin(), models.end(), [&](const Model& m) { return m.name == c.model; });
      if (it == models.end()) throw std::runtime_error("нет модели " + c.model + " для IGES");
      write_iges(*it, out / c.file, c.brep);
      manifest["iges"].push_back({{"file", c.file}, {"model", c.model}, {"mode", c.brep ? "brep" : "faces"}});
      std::cout << "записано: " << (out / c.file).string() << "\n";
    }
    std::ofstream mf(out / "manifest.json");
    mf << manifest.dump(2) << "\n";
    if (!mf) throw std::runtime_error("не записан manifest.json");
    std::cout << "манифест: " << (out / "manifest.json").string() << " (моделей: " << models.size() << ")\n";
  } catch (const Standard_Failure& e) {
    std::cerr << "generate_corpus: ошибка OCCT: " << e.what() << "\n";
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "generate_corpus: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
