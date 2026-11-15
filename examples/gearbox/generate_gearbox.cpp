// Генератор комплекта редуктора РЦ-80: строит детали, сборку, чертежи и спецификацию и
// записывает их в каталог (по умолчанию — текущий). Использование:
//   gearbox_generate <каталог> [--no-part-step]
// Код возврата 0 — комплект записан, сборка решена, все размеры и позиции привязаны к модели.

#include "gearbox.hpp"

#include <cstdio>
#include <cstring>
#include <string>

using namespace bendcore::examples::gearbox;

int main(int argc, char** argv) {
    std::string dir = ".";
    KitOptions options;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--no-part-step") == 0)
            options.partSteps = false;
        else
            dir = argv[i];
    }
    auto parts = buildParts();
    if (!parts.ok()) return std::fprintf(stderr, "parts: %s\n", parts.error().message.c_str()), 1;
    auto standard = buildStandardParts();
    if (!standard.ok()) return std::fprintf(stderr, "standard parts: %s\n", standard.error().message.c_str()), 1;
    auto gearbox = buildAssembly(parts.value(), standard.value());
    if (!gearbox.ok()) return std::fprintf(stderr, "assembly: %s\n", gearbox.error().message.c_str()), 1;
    auto set = defineDrawings(parts.value(), gearbox.value());
    if (!set.ok()) return std::fprintf(stderr, "drawings: %s\n", set.error().message.c_str()), 1;
    bendcore::drawing_v2::views::ViewCache cache(64, 2);
    auto upd = updateDrawings(set.value(), parts.value(), gearbox.value(), cache);
    if (!upd.ok()) return std::fprintf(stderr, "update: %s\n", upd.error().message.c_str()), 1;
    for (const auto& d : upd.value().dangling) std::fprintf(stderr, "dangling: %s\n", d.c_str());
    if (!upd.value().dangling.empty()) return 1;
    auto kit = writeKit(dir, parts.value(), standard.value(), gearbox.value(), set.value(), options);
    if (!kit.ok()) return std::fprintf(stderr, "kit: %s\n", kit.error().message.c_str()), 1;
    std::printf("parts: %zu documents, assembly: %zu occurrences, drawings: %zu, %zu dimensions, %zu positions\n",
                kit.value().partDocuments.size(), gearbox.value().doc->topLevel().size(), kit.value().drawingDocuments.size(),
                upd.value().dimensions, upd.value().balloons);
    std::printf("PDF: %s (%d pages), DXF: %zu, STEP: assembly + %zu parts\n", kit.value().pdf.c_str(), kit.value().pdfPages,
                kit.value().dxf.size(), kit.value().partSteps.size());
    return 0;
}
