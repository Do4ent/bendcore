// Сквозной импорт на Open CASCADE: тело трубы -> STEP-файл -> ImportWorkflow
// (STEPReader -> GeometryAnalyzer с бэкендом OCCT -> параметрические размеры).
// Труба ∅20×2: прямой 100, гиб R40 на 90°, прямой 80.
#include "bendcore/import_workflow.hpp"
#include "bendcore/occt/geometry_backend.hpp"
#include "bendcore/step_reader.hpp"
#include "bendcore/tube_body.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace bendcore;

static int failures = 0;

static void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what.c_str());
        ++failures;
    }
}

static bool near(double a, double b, double tol) { return std::abs(a - b) <= tol; }

static double variable(const import::ImportResult& r, const std::string& name, bool& found) {
    for (const auto& f : r.formulas) {
        auto it = f.variables_map.find(name);
        if (it != f.variables_map.end()) {
            found = true;
            return it->second;
        }
    }
    found = false;
    return 0.0;
}

int main() {
    occt::install_geometry_backend();

    const auto file = (std::filesystem::temp_directory_path() / "bendcore_import_workflow_occt.step").string();
    const Path path{Straight{100}, Bend{40, 90}, Straight{80}};
    geom::export_step(path, 20.0, 2.0, file);

    // STEPReader: одно тело, B-rep текст непустой.
    step::STEPReader reader;
    const step::ImportResult read = reader.read(file);
    check(read.success, "STEPReader::read: " + read.error_message);
    check(read.shape_count == 1, "одно тело в STEP");
    check(reader.get_property("format") == "STEP", "свойство format");
    check(!read.shape_ids.empty() && reader.get_shape_data(read.shape_ids[0]).find("CASCADE Topology") !=
                                         std::string::npos,
          "данные тела в формате BRepTools");

    // ImportWorkflow: распознанные размеры трубы.
    import::ImportWorkflow workflow;
    import::ImportConfig config;
    config.file_path = file;
    config.author = "test";
    config.use_cache = true;
    const import::ImportResult res = workflow.import(config);
    for (const auto& w : res.warnings) std::printf("warning: %s\n", w.c_str());
    check(res.success, "ImportWorkflow::import");

    bool found = false;
    const double od = variable(res, "$OD", found);
    check(found && near(od, 20.0, 1e-3), "OD = 20");
    const double wall = variable(res, "$Wall", found);
    check(found && near(wall, 2.0, 1e-3), "стенка = 2");
    const double length = variable(res, "$Length", found);
    const double expected_length = 100.0 + 80.0 + 40.0 * std::acos(-1.0) / 2.0;
    check(found && near(length, expected_length, 0.5), "длина по оси");
    std::printf("OD %.3f, стенка %.3f, длина %.3f (ожидается %.3f), размеров %zu\n", od, wall, length,
                expected_length, res.formulas.size());

    // Повторный импорт того же файла — из кэша, те же размеры.
    const import::ImportResult cached = workflow.import(config);
    check(cached.success, "повторный импорт (кэш)");
    check(cached.log.find("Cache hit") != std::string::npos, "попадание в кэш");
    check(near(variable(cached, "$OD", found), od, 1e-9) && found, "OD из кэша");

    std::filesystem::remove(file);
    if (failures) {
        std::printf("FAILED: %d\n", failures);
        return 1;
    }
    std::printf("OK\n");
    return 0;
}
