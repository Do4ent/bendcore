#pragma once

// Изделие уровня «редуктор» (критерий готовности v1.0, docs/ROADMAP_GENERAL_CAD.md): одноступенчатый
// цилиндрический редуктор РЦ-80, спроектированный средствами bendcore — детали документами детали,
// сборка документом сборки с сопряжениями и крепежом bendcore_fasteners, рабочие и сборочный
// чертежи документами чертежа, спецификация ГОСТ 2.106. Описание, допущения и ограничения —
// examples/gearbox/README.md. Все ошибки — kernel::Result с причиной.

#include <bendcore/assembly_v2/assembly_document.hpp>
#include <bendcore/drawing_v2/drawing_document.hpp>
#include <bendcore/kernel/result.hpp>
#include <bendcore/modeling/part_document.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bendcore::examples::gearbox {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;
using Status = kernel::Result<std::monostate>;

// Основные размеры редуктора (мм). Зацепление: m = 2, z1 = 20, z2 = 60, aw = m(z1 + z2)/2 = 80.
struct Layout {
    double module = 2.0;
    int z1 = 20;
    int z2 = 60;
    double aw() const { return module * (z1 + z2) / 2.0; }
    double d1() const { return module * z1; }
    double d2() const { return module * z2; }
    double axisHeight = 78.0;   // Za — оси валов над подошвой корпуса (без опорной плиты)
    double joint = 125.0;       // H — плоскость разъёма корпус/крышка
    double gasket = 1.0;        // толщина прокладки разъёма
    double boss = 64.0;         // торцы бобышек подшипников x = ±boss
    double spigot = 4.0;        // длина центрирующего пояска крышки подшипника
};

// Деталь собственной разработки: документ детали и свойства для BOM/основной надписи.
struct PartEntry {
    std::string key;          // «housing», «shaft_in», …
    std::string file;         // «housing.bcpart.json»
    std::string designation;  // обозначение ЕСКД
    std::string name;         // наименование
    std::string material;
    double density = 7850.0;  // кг/м³
    std::unique_ptr<modeling::PartDocument> doc;
};

// Детали собственной разработки (13 определений). Каждая — параметрический документ детали,
// перестроенный без ошибок. Ошибка — первая операция в состоянии Error и её сообщение.
Result<std::vector<PartEntry>> buildParts(const Layout& layout = {});
const PartEntry* findPart(const std::vector<PartEntry>& parts, const std::string& key);

// Стандартные изделия из каталогов библиотеки bendcore_fasteners, кроме крепежа: подшипники
// шариковые ГОСТ 8338 (кольца с дорожками качения и шарики, атрибуты «bearing.*») и шпонки
// призматические ГОСТ 23360 (исполнение 1, фаски s). Встраиваются в документ сборки, как крепёж.
struct StandardPart {
    std::string key;                    // «bearing205», «key10»
    std::string name;                   // полное обозначение для графы «Наименование» (графа «Обозначение» пустая)
    assembly_v2::PartData data;         // модель библиотеки
    assembly_v2::PartProperties properties;
    std::vector<std::pair<std::string, double>> dimensions;  // размеры модели (d, D, B … / b, h, l, s …)
    double dimension(const std::string& key) const;
};
Result<std::vector<StandardPart>> buildStandardParts(const Layout& layout = {});

// Сборка редуктора: определения, вхождения, сопряжения, крепёж. Ключи вхождений — для тестов.
struct Gearbox {
    std::shared_ptr<assembly_v2::AssemblyDocument> doc;
    std::map<std::string, assembly_v2::DefinitionId> defs;    // ключ детали → определение
    std::map<std::string, assembly_v2::OccurrenceId> occ;     // «shaft_in», «cap_in_l», …
    std::vector<assembly_v2::OccurrenceId> threadedScrews;    // винты/пробки в резьбовых отверстиях
    std::map<assembly_v2::DefinitionId, double> threadAllowance;  // предельный объём «резьбового» пересечения, мм³
    int expectedDof = 0;  // 2 вала + повороты осесимметричных деталей вокруг своих осей
};

// Источники деталей — файлы <key>.bcpart.json (относительно каталога сборки); геометрия берётся
// из переданных документов (resolver), поэтому сохранённая сборка открывается из файлов.
Result<Gearbox> buildAssembly(const std::vector<PartEntry>& parts, const std::vector<StandardPart>& standard,
                              const Layout& layout = {});

// Чертежи: рабочие чертежи деталей и сборочный чертёж, спецификация.
struct DrawingSet {
    struct PartDrawing {
        std::string key;   // ключ детали
        std::string file;  // «shaft_out.bcdrw.json»
        drawing_v2::document::DrawingDocument doc;
    };
    std::vector<PartDrawing> parts;
    drawing_v2::document::DrawingDocument assembly;
    drawing_v2::document::SpecificationDocument specification;
};

// Определения листов, видов, размеров (с допусками ISO 286), шероховатости, допусков формы и
// расположения, технических требований. Документы не обновлены — после построения нужен update.
Result<DrawingSet> defineDrawings(const std::vector<PartEntry>& parts, const Gearbox& gearbox, const Layout& layout = {});

// Обновление всех чертежей по текущим моделям (документы деталей и сборки в памяти).
struct UpdateSummary {
    std::vector<std::string> dangling;  // «<файл>: S1/D: причина»
    std::vector<std::string> stale;
    std::vector<std::string> warnings;
    std::size_t dimensions = 0;
    std::size_t balloons = 0;
};
Result<UpdateSummary> updateDrawings(DrawingSet& set, const std::vector<PartEntry>& parts, const Gearbox& gearbox,
                                     drawing_v2::views::ViewCache& cache);

// Запись комплекта в каталог: документы деталей, сборки, чертежей и спецификации, STEP сборки и
// деталей, многостраничный PDF комплекта и DXF каждого листа.
struct KitFiles {
    std::vector<std::string> partDocuments;
    std::string assemblyDocument;
    std::vector<std::string> drawingDocuments;
    std::string specificationDocument;
    std::string assemblyStep;
    std::vector<std::string> partSteps;
    std::string pdf;
    std::vector<std::string> dxf;
    int pdfPages = 0;
};
struct KitOptions {
    bool partSteps = true;   // STEP каждой детали собственной разработки
    bool dxf = true;
    bool pdf = true;
};
Result<KitFiles> writeKit(const std::string& directory, const std::vector<PartEntry>& parts,
                          const Gearbox& gearbox, const DrawingSet& set,
                          const KitOptions& options = {});

}  // namespace bendcore::examples::gearbox
