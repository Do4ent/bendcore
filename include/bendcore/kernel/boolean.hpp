#pragma once

// Устойчивые булевы операции на OCCT: проверка входа, лестница нечёткого допуска,
// проверка результата после каждой попытки. Подробности: docs/kernel/GEOMETRY_HEALING.md.

#include <bendcore/kernel/result.hpp>

#include <TopoDS_Shape.hxx>

#include <optional>
#include <string>
#include <vector>

namespace bendcore::kernel {

enum class BooleanType { Fuse, Cut, Common };

const char* toString(BooleanType type) noexcept;

struct BooleanOptions {
    // Лестница FuzzyValue: если fuzzyLadder пуст, строится 0, fuzzyStart, fuzzyStart*fuzzyFactor, ...
    // до maxFuzzyValue включительно. Если задан явно — используется как есть (по возрастанию,
    // значения больше maxFuzzyValue отбрасываются).
    std::vector<double> fuzzyLadder;
    double fuzzyStart = 1e-7;
    double fuzzyFactor = 10.0;
    double maxFuzzyValue = 1e-4;

    bool runParallel = true;
    bool checkInputs = true;       // BRepAlgoAPI_Check для каждого аргумента и инструмента
    bool simplifyResult = true;    // BRepAlgoAPI_BuilderAlgo::SimplifyResult после успеха
    bool checkVolumeBounds = true; // объём результата в пределах, допустимых для операции

    // Ожидаемое число тел в результате (например, 1 для Fuse соприкасающихся деталей).
    std::optional<int> expectedSolidCount;
    // Тела тоньше этого (оценка 2·V/A) считаются щелями-артефактами; 0 — проверка выключена.
    double minSolidThickness = 0.0;
};

struct BooleanResult {
    TopoDS_Shape shape;
    double fuzzyValue = 0.0;   // допуск удачной попытки
    int attempts = 0;          // число выполненных попыток, включая удачную
    bool simplifyApplied = false;  // применён SimplifyResult
    double volume = 0.0;       // объём результата, мм³
    std::vector<std::string> warnings;  // предупреждения OCCT и причины отказа прежних попыток
};

// Ошибки: InvalidInput — пустая форма или вход не прошёл BRepAlgoAPI_Check;
// OperationFailed — ни одна попытка лестницы не дала приемлемого результата
// (сообщение содержит причины по каждой попытке) или исключение OCCT.
Result<BooleanResult> booleanOp(BooleanType type, const TopoDS_Shape& a, const TopoDS_Shape& b,
                                const BooleanOptions& options = {});

// Вариант со списками: аргументы (objects) и инструменты (tools) через
// BRepAlgoAPI_BooleanOperation::SetArguments / SetTools.
Result<BooleanResult> booleanOp(BooleanType type, const std::vector<TopoDS_Shape>& arguments,
                                const std::vector<TopoDS_Shape>& tools,
                                const BooleanOptions& options = {});

// Лестница допусков, которую использует booleanOp для данных опций.
std::vector<double> fuzzyLadder(const BooleanOptions& options);

}  // namespace bendcore::kernel
