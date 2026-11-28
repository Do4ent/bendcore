#pragma once

// Общие средства сквозных тестов документа чертежа: эталонные детали фазы 1
// (docs/modeling/REFERENCE_PARTS.md, №1–3) как документы детали, поиск элементов модели по
// геометрии (независимо от карты имён), разбор выходных файлов.

#include "test_support.hpp"

#include <bendcore/drawing_v2/drawing_document.hpp>
#include <bendcore/modeling/part_document.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ddtest {

using namespace bendcore::drawing_v2;
using namespace bendcore::drawing_v2::document;
using bendcore::drawing_v2::views::Vec3;

// Эталонные детали (те же построения, что в tests/modeling/test_modeling_document_reference_parts.cpp):
// 1 — кронштейн (A, B, T, Wd, r, hx, d), 2 — вал со шпоночным пазом (L, L1, R1, R2, lk, b, t,
// kx, c), 3 — фланец с отверстиями по окружности (D, d0, T, Dh, h2, BC, dh, N, c).
std::unique_ptr<bendcore::modeling::PartDocument> makeBracket();
std::unique_ptr<bendcore::modeling::PartDocument> makeShaft();
std::unique_ptr<bendcore::modeling::PartDocument> makeFlange();

// Детали тестовой сборки: вал Ø2R длиной L вдоль Z от z = 0 (эскиз-круг + выдавливание) и втулка
// фланцевая: диск D×T с отверстием d0, ступица Dh×h2, N отверстий dh на окружности BC.
std::unique_ptr<bendcore::modeling::PartDocument> makeRod(double R, double L);
std::unique_ptr<bendcore::modeling::PartDocument> makeBushing();

void setVariable(bendcore::modeling::PartDocument& doc, const std::string& name, double value);

// Индексы элементов (TopologyIndex формы снимка) по геометрии; nullopt — не найден или не один.
std::optional<std::uint32_t> vertexAt(const ModelSnapshot& m, Vec3 p, double tol = 1e-6);
std::optional<std::uint32_t> lineEdge(const ModelSnapshot& m, Vec3 a, Vec3 b, double tol = 1e-6);
std::optional<std::uint32_t> circleEdge(const ModelSnapshot& m, Vec3 c, double r, double tol = 1e-6);
std::optional<std::uint32_t> cylinderFace(const ModelSnapshot& m, double r, Vec3 axisPoint, Vec3 axisDir,
                                          double tol = 1e-6);

// Точка привязки размера по элементу снимка (ссылка — имя + сигнатура).
annotations::Anchor anchorOn(const ModelSnapshot& m, bendcore::modeling::ElementKind kind, std::optional<std::uint32_t> index,
                             annotations::AnchorKind ak);

DimensionItem dimension(const std::string& id, const std::string& view, annotations::DimensionType type,
                        std::vector<annotations::Anchor> anchors, double offset = 10.0);

// Тексты и отрезки DXF (после декодирования \U+XXXX).
struct DxfContent {
    std::vector<std::string> texts;
    struct Line {
        double x0, y0, x1, y1;
        std::string layer;
    };
    std::vector<Line> lines;
    std::size_t entities = 0;
    double limMaxX = 0, limMaxY = 0;
};
DxfContent readDxf(const std::string& path);
bool hasText(const DxfContent& d, const std::string& text);
// Есть ли отрезок размерного слоя DIM длиной length (±tol), горизонтальный или вертикальный.
bool hasDimLine(const DxfContent& d, double length, bool horizontal, double tol = 1e-4);

// Результат размера по идентификатору в вычисленном листе.
const annotations::DimensionResult* dimensionResult(const DrawingDocument& doc, const std::string& sheet,
                                                    const std::string& id);

// Проверка файла ezdxf (если задан BENDCORE_PYTHON_EZDXF): AC1015, audit без ошибок, $INSUNITS = 4,
// присутствие декодированного текста. Возвращает пустую строку при успехе или пропуске.
std::string ezdxfCheck(const std::string& path, const std::string& expectText);

}  // namespace ddtest
