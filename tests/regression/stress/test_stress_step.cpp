// Нагрузка: крупный STEP (ROADMAP_GENERAL_CAD.md, фаза 5, п. 1) — импорт, тесселяция, память.
//
// Файл строит генератор корпуса (generate_corpus --large <файл> <МБ>, ctest stress_step_generate)
// в каталоге сборки; в git он не хранится. Ожидаемые значения — из <файл>.json того же
// генератора, вычисленные по формулам (число тел и граней, объём, габариты).
//
//   test_stress_step <файл.step>

#include "stress_util.hpp"

#include <bendcore/kernel/step_io.hpp>
#include <bendcore/kernel/tessellation.hpp>

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

using bendcore::qa::stress::peakRssMiB;
using bendcore::qa::stress::rssMiB;
using bendcore::qa::stress::timed;

namespace {

std::string g_file;

std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

TEST(StressStep, LargeFileImportTessellationMemory) {
    ASSERT_FALSE(g_file.empty()) << "usage: test_stress_step <file.step>";
    ASSERT_TRUE(std::filesystem::exists(g_file + ".json")) << "run generate_corpus --large first";
    const nlohmann::json expect = nlohmann::json::parse(slurp(g_file + ".json"));
    const double mib = double(std::filesystem::file_size(g_file)) / (1024.0 * 1024.0);
    ASSERT_GE(mib, 50.0) << "phase 5 load test needs a STEP file of at least 50 MB";
    const std::string sizeTag = "\"file_mib\":" + std::to_string(mib);

    std::string text;
    timed("step_large", "read_file", 30, [&] { text = slurp(g_file); }, sizeTag);
    const double rssBefore = rssMiB();

    bendcore::kernel::Shape shape;
    timed("step_large", "import_readStep", 600, [&] {
        auto r = bendcore::kernel::readStep(text);
        ASSERT_TRUE(r.ok()) << r.error().message;
        shape = std::move(r.value());
    }, sizeTag);
    text.clear();
    text.shrink_to_fit();
    const auto c = shape.counts();
    EXPECT_EQ(c.solids, expect["solids"].get<std::size_t>());
    EXPECT_EQ(c.faces, expect["faces"].get<std::size_t>());

    GProp_GProps props;
    BRepGProp::VolumeProperties(shape.occt(), props);
    const double volume = expect["volume"].get<double>();
    EXPECT_NEAR(props.Mass(), volume, volume * expect["volume_rel_tol"].get<double>());
    Bnd_Box box;
    BRepBndLib::AddOptimal(shape.occt(), box, false, false);
    double b[6];
    box.Get(b[0], b[1], b[2], b[3], b[4], b[5]);
    for (int k = 0; k < 3; ++k) {
        EXPECT_NEAR(b[k], expect["bbox_min"][k].get<double>(), 1e-4);
        EXPECT_NEAR(b[k + 3], expect["bbox_max"][k].get<double>(), 1e-4);
    }

    bendcore::kernel::Mesh mesh;
    bendcore::kernel::TessellationParams tp;
    tp.linearDeflection = 0.1;
    tp.angularDeflection = 0.5;
    tp.inParallel = true;
    timed("step_large", "tessellate", 600, [&] {
        auto m = bendcore::kernel::tessellate(shape, tp);
        ASSERT_TRUE(m.ok()) << m.error().message;
        mesh = std::move(m.value());
    });
    EXPECT_TRUE(mesh.missingFaces.empty());
    EXPECT_EQ(mesh.faceSpans.size(), c.faces);
    const std::size_t triangles = mesh.indices.size() / 3;
    // Нижняя граница по топологии: брусок — 12 треугольников, у цилиндра торцы и боковая грань
    // при прогибе 0,1 мм на R5 дают не меньше 2·(n−2) + 2n при n ≥ 16 точках окружности.
    const std::size_t boxes = expect["boxes"].get<std::size_t>(), cyl = expect["cylinders"].get<std::size_t>();
    EXPECT_GE(triangles, 12 * boxes + (2 * 14 + 2 * 16) * cyl);

    // Память: прирост резидентной памяти от импорта и тесселяции и пик процесса.
    const double peak = peakRssMiB();
    const double grown = rssMiB() - rssBefore;
    char extra[160];
    std::snprintf(extra, sizeof(extra), "\"file_mib\":%.1f,\"rss_growth_mib\":%.0f,\"triangles\":%zu", mib, grown,
                  triangles);
    bendcore::qa::stress::report("step_large", "memory", 0.0, 0.0, extra);
    // Порог памяти: 40 байт файла на байт... — с запасом: пик не больше 60× размера файла и 8 ГиБ.
    EXPECT_LT(peak, std::min(60.0 * mib, 8192.0)) << "peak RSS " << peak << " MiB for a " << mib << " MiB file";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (argc > 1) g_file = argv[1];
    return RUN_ALL_TESTS();
}
