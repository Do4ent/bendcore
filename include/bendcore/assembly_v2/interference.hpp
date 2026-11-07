#pragma once

// Проверка пересечений деталей сборки (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 6):
// предварительный отбор пар по габаритам (Bnd_Box, сортировка и проход по оси X), объём
// пересечения через kernel::booleanOp(Common), касание — нулевой объём при расстоянии
// BRepExtrema_DistShapeShape не больше contactTolerance.

#include <bendcore/assembly_v2/assembly_document.hpp>
#include <bendcore/kernel/boolean.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace bendcore::assembly_v2 {

enum class ContactKind { Interference, Touching };

const char* toString(ContactKind kind) noexcept;

struct InterferenceOptions {
    double volumeTolerance = 1e-6;   // мм³: объём не больше — не пересечение
    double contactTolerance = 1e-6;  // мм: расстояние не больше — касание
    bool reportTouching = true;
    // Проверять пары деталей внутри одного вхождения верхнего уровня (подсборки).
    bool includeSameTopLevel = true;
    kernel::BooleanOptions boolean = defaultBooleanOptions();

    static kernel::BooleanOptions defaultBooleanOptions() {
        kernel::BooleanOptions o;
        o.checkInputs = false;
        o.simplifyResult = false;
        return o;
    }
};

struct InterferencePair {
    std::size_t a = 0;  // индексы в InterferenceReport::parts
    std::size_t b = 0;
    ContactKind kind = ContactKind::Interference;
    double volume = 0.0;    // мм³
    double distance = 0.0;  // мм (0 при пересечении)
};

struct InterferenceReport {
    std::vector<PlacedOccurrence> parts;
    std::vector<InterferencePair> pairs;
    std::size_t totalPairs = 0;      // n(n−1)/2 (с учётом includeSameTopLevel)
    std::size_t candidatePairs = 0;  // прошли отбор по габаритам
    double totalVolume = 0.0;
};

// Ошибки: InvalidInput (параметры), ошибки развёртки документа и booleanOp.
kernel::Result<InterferenceReport> checkInterference(const AssemblyDocument& document,
                                                     const InterferenceOptions& options = {});

// То же для готового списка деталей в мировой СК.
kernel::Result<InterferenceReport> checkInterference(std::vector<PlacedOccurrence> parts,
                                                     const InterferenceOptions& options = {});

}  // namespace bendcore::assembly_v2
