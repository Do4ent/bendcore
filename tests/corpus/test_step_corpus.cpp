// Регрессия импорта корпуса STEP (docs/ROADMAP_GENERAL_CAD.md, фаза 0, п. 6).
//
// Использование: test_step_corpus <каталог корпуса с manifest.json>
//
// Для каждой модели манифеста файл импортируется существующим читателем
// bendcore::step::STEPReader (src/step_reader.cpp) и сверяется с ожидаемыми значениями,
// вычисленными генератором по формулам (tests/corpus/generate_corpus.cpp):
//   - успех чтения, число свободных форм, имя верхней формы (свойство shape_<i>_name);
//   - число тел и граней (свойства solid_count / face_count);
//   - валидность B-Rep (BRepCheck_Analyzer) формы, восстановленной из get_shape_data();
//   - объём и площадь поверхности (BRepGProp) с относительным допуском из манифеста;
//   - точные габариты (BRepBndLib::AddOptimal) и габариты читателя (bbox_min/bbox_max);
//   - ключевое слово FILE_SCHEMA протокола в заголовке файла;
//   - для сборок: имена деталей, число вхождений и цвета через XCAF (STEPReader их
//     не возвращает, см. docs/qa/QA.md, «Известные ограничения читателя»).
// Все расхождения печатаются; код выхода 1, если есть хотя бы одно.

#include <bendcore/step_reader.hpp>

#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <NCollection_Sequence.hxx>
#include <Quantity_Color.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDF_Label.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_ColorType.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const std::string& model, const std::string& what) {
  ++g_checks;
  if (!ok) {
    ++g_failures;
    std::cout << "  FAIL [" << model << "] " << what << "\n";
  }
}

std::string fmt(double v) {
  std::ostringstream o;
  o.precision(12);
  o << v;
  return o.str();
}

void check_rel(double got, double want, double rel_tol, const std::string& model, const std::string& what) {
  const double err = std::abs(got - want) / std::max(std::abs(want), 1e-300);
  check(err <= rel_tol, model,
        what + ": получено " + fmt(got) + ", ожидалось " + fmt(want) + " (отн. ошибка " + fmt(err) +
            " > " + fmt(rel_tol) + ")");
}

std::optional<int> to_int(const std::string& s) {
  try {
    size_t pos = 0;
    const int v = std::stoi(s, &pos);
    if (pos != s.size()) return std::nullopt;
    return v;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<std::array<double, 3>> to_xyz(const std::string& s) {
  std::istringstream in(s);
  std::array<double, 3> v{};
  if (!(in >> v[0] >> v[1] >> v[2])) return std::nullopt;
  return v;
}

std::string label_name(const TDF_Label& l) {
  Handle(TDataStd_Name) n;
  if (!l.FindAttribute(TDataStd_Name::GetID(), n)) return "";
  return TCollection_AsciiString(n->Get()).ToCString();
}

// Цвет детали: на метке прототипа (общий или цвет поверхности), иначе на метке вхождения.
std::optional<Quantity_Color> part_color(const TDF_Label& proto, const TDF_Label& instance) {
  Quantity_Color c;
  for (const TDF_Label& l : {proto, instance}) {
    for (XCAFDoc_ColorType t : {XCAFDoc_ColorGen, XCAFDoc_ColorSurf}) {
      if (XCAFDoc_ColorTool::GetColor(l, t, c)) return c;
    }
  }
  return std::nullopt;
}

// Имена, число вхождений и цвета деталей сборки — читаются через XCAF напрямую.
void check_components(const std::filesystem::path& file, const nlohmann::json& model) {
  const std::string name = model.at("name");
  Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
  Handle(TDocStd_Document) doc;
  app->NewDocument("MDTV-XCAF", doc);
  STEPCAFControl_Reader reader;
  reader.SetNameMode(true);
  reader.SetColorMode(true);
  const bool read_ok = reader.ReadFile(file.string().c_str()) == IFSelect_RetDone && reader.Transfer(doc);
  check(read_ok, name, "XCAF: чтение сборки");
  if (!read_ok) {
    app->Close(doc);
    return;
  }
  Handle(XCAFDoc_ShapeTool) shapes = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
  NCollection_Sequence<TDF_Label> free;
  shapes->GetFreeShapes(free);
  check(free.Length() == 1, name, "XCAF: одна свободная форма (сборка), получено " + std::to_string(free.Length()));
  if (free.Length() != 1) {
    app->Close(doc);
    return;
  }
  const TDF_Label top = free.Value(1);
  check(XCAFDoc_ShapeTool::IsAssembly(top), name, "XCAF: верхняя метка — сборка");
  check(label_name(top) == model.at("top_name").get<std::string>(), name,
        "XCAF: имя сборки '" + label_name(top) + "'");

  struct Seen { int instances = 0; std::optional<Quantity_Color> color; };
  std::map<std::string, Seen> seen;
  NCollection_Sequence<TDF_Label> comps;
  XCAFDoc_ShapeTool::GetComponents(top, comps);
  for (int i = 1; i <= comps.Length(); ++i) {
    TDF_Label proto;
    if (!XCAFDoc_ShapeTool::GetReferredShape(comps.Value(i), proto)) {
      check(false, name, "XCAF: вхождение без прототипа");
      continue;
    }
    Seen& s = seen[label_name(proto)];
    ++s.instances;
    if (!s.color) s.color = part_color(proto, comps.Value(i));
  }

  const auto& want = model.at("components");
  check(seen.size() == want.size(), name,
        "XCAF: число разных деталей " + std::to_string(seen.size()) + ", ожидалось " + std::to_string(want.size()));
  for (const auto& wc : want) {
    const std::string cname = wc.at("name");
    auto it = seen.find(cname);
    check(it != seen.end(), name, "XCAF: нет детали с именем '" + cname + "'");
    if (it == seen.end()) continue;
    const int want_inst = wc.at("instances");
    check(it->second.instances == want_inst, name,
          "XCAF: '" + cname + "' вхождений " + std::to_string(it->second.instances) + ", ожидалось " +
              std::to_string(want_inst));
    check(it->second.color.has_value(), name, "XCAF: у '" + cname + "' нет цвета");
    if (!it->second.color) continue;
    double r = 0, g = 0, b = 0;
    it->second.color->Values(r, g, b, Quantity_TOC_RGB);
    const auto wrgb = wc.at("color");
    const double d = std::max({std::abs(r - wrgb[0].get<double>()), std::abs(g - wrgb[1].get<double>()),
                               std::abs(b - wrgb[2].get<double>())});
    check(d <= 1e-3, name,
          "XCAF: цвет '" + cname + "' = (" + fmt(r) + ", " + fmt(g) + ", " + fmt(b) + "), ожидался (" +
              fmt(wrgb[0]) + ", " + fmt(wrgb[1]) + ", " + fmt(wrgb[2]) + ")");
  }
  app->Close(doc);
}

void check_model(const std::filesystem::path& dir, const nlohmann::json& model) {
  const std::string name = model.at("name");
  const std::filesystem::path file = dir / model.at("file").get<std::string>();
  const int failures_before = g_failures;

  // Заголовок файла: протокол STEP.
  {
    std::ifstream in(file);
    std::string head((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    head = head.substr(0, std::min<size_t>(head.size(), 4096));
    const std::string kw = model.at("schema_keyword");
    check(head.find("FILE_SCHEMA") != std::string::npos && head.find(kw) != std::string::npos, name,
          "FILE_SCHEMA содержит " + kw);
  }

  bendcore::step::STEPReader reader;
  const bendcore::step::ImportResult res = reader.read(file.string());
  check(res.success, name, "STEPReader::read: " + res.error_message);
  if (!res.success) return;

  const int want_free = model.at("free_shapes");
  check(res.shape_count == want_free, name,
        "свободных форм " + std::to_string(res.shape_count) + ", ожидалось " + std::to_string(want_free));
  check(reader.get_property("format") == "STEP", name, "свойство format = STEP");
  check(reader.get_property("shape_0_name") == model.at("top_name").get<std::string>(), name,
        "имя верхней формы '" + reader.get_property("shape_0_name") + "', ожидалось '" +
            model.at("top_name").get<std::string>() + "'");

  const auto solids = to_int(reader.get_property("solid_count"));
  const auto faces = to_int(reader.get_property("face_count"));
  check(solids && *solids == model.at("solids").get<int>(), name,
        "тел " + reader.get_property("solid_count") + ", ожидалось " + std::to_string(model.at("solids").get<int>()));
  check(faces && *faces == model.at("faces").get<int>(), name,
        "граней " + reader.get_property("face_count") + ", ожидалось " + std::to_string(model.at("faces").get<int>()));

  // Геометрия: формы из B-Rep текста читателя.
  double volume = 0.0, area = 0.0;
  Bnd_Box exact;
  for (const std::string& id : res.shape_ids) {
    std::istringstream brep(reader.get_shape_data(id));
    TopoDS_Shape shape;
    BRep_Builder bb;
    BRepTools::Read(shape, brep, bb);
    check(!shape.IsNull(), name, id + ": B-Rep из get_shape_data не читается");
    if (shape.IsNull()) continue;
    check(BRepCheck_Analyzer(shape).IsValid(), name, id + ": BRepCheck_Analyzer: форма невалидна");
    GProp_GProps vp, sp;
    BRepGProp::VolumeProperties(shape, vp);
    BRepGProp::SurfaceProperties(shape, sp);
    volume += vp.Mass();
    area += sp.Mass();
    BRepBndLib::AddOptimal(shape, exact, false, false);
  }
  check_rel(volume, model.at("volume"), model.at("volume_rel_tol"), name, "объём");
  check_rel(area, model.at("area"), model.at("area_rel_tol"), name, "площадь поверхности");

  const auto bmin = model.at("bbox_min");
  const auto bmax = model.at("bbox_max");
  const double tol = model.at("bbox_abs_tol");
  check(!exact.IsVoid(), name, "габариты пусты");
  if (!exact.IsVoid()) {
    double x0, y0, z0, x1, y1, z1;
    exact.Get(x0, y0, z0, x1, y1, z1);
    const double got[6] = {x0, y0, z0, x1, y1, z1};
    const double want[6] = {bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2]};
    for (int k = 0; k < 6; ++k) {
      check(std::abs(got[k] - want[k]) <= tol, name,
            std::string("точные габариты, компонента ") + std::to_string(k) + ": " + fmt(got[k]) +
                ", ожидалось " + fmt(want[k]));
    }
  }

  // Габариты читателя (BRepBndLib::Add) могут быть шире точных, но обязаны их содержать;
  // расширение ограничено 5% диагонали (оценка по контрольным точкам поверхностей).
  const auto rmin = to_xyz(reader.get_property("bbox_min"));
  const auto rmax = to_xyz(reader.get_property("bbox_max"));
  check(rmin.has_value() && rmax.has_value(), name, "у читателя нет свойств bbox_min/bbox_max");
  if (rmin && rmax) {
    const double diag = std::sqrt(std::pow(bmax[0].get<double>() - bmin[0].get<double>(), 2) +
                                  std::pow(bmax[1].get<double>() - bmin[1].get<double>(), 2) +
                                  std::pow(bmax[2].get<double>() - bmin[2].get<double>(), 2));
    for (int k = 0; k < 3; ++k) {
      const double lo = bmin[k], hi = bmax[k];
      check((*rmin)[k] <= lo + tol && (*rmin)[k] >= lo - 0.05 * diag, name,
            "bbox_min читателя, компонента " + std::to_string(k) + ": " + fmt((*rmin)[k]) + " при точном " + fmt(lo));
      check((*rmax)[k] >= hi - tol && (*rmax)[k] <= hi + 0.05 * diag, name,
            "bbox_max читателя, компонента " + std::to_string(k) + ": " + fmt((*rmax)[k]) + " при точном " + fmt(hi));
    }
  }

  if (model.contains("components")) check_components(file, model);

  std::cout << (g_failures == failures_before ? "  ok   " : "  FAIL ") << name << "  (V=" << fmt(volume)
            << ", S=" << fmt(area) << ")\n";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "Использование: " << argv[0] << " <каталог корпуса с manifest.json>\n";
    return 2;
  }
  const std::filesystem::path dir = argv[1];
  std::ifstream mf(dir / "manifest.json");
  if (!mf) {
    std::cerr << "test_step_corpus: нет " << (dir / "manifest.json").string() << "\n";
    return 1;
  }
  nlohmann::json manifest;
  try {
    manifest = nlohmann::json::parse(mf);
  } catch (const std::exception& e) {
    std::cerr << "test_step_corpus: manifest.json не разобран: " << e.what() << "\n";
    return 1;
  }
  const auto& models = manifest.at("models");
  if (models.empty()) {
    std::cerr << "test_step_corpus: в манифесте нет моделей\n";
    return 1;
  }
  std::cout << "Корпус: " << dir.string() << ", моделей: " << models.size() << "\n";
  for (const auto& m : models) {
    try {
      check_model(dir, m);
    } catch (const std::exception& e) {
      check(false, m.value("name", std::string("?")), std::string("исключение: ") + e.what());
    }
  }
  std::cout << "Проверок: " << g_checks << ", провалено: " << g_failures << "\n";
  return g_failures == 0 ? 0 : 1;
}
