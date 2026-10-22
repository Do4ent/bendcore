// Производство: генерация управляющего кода и планирование (заглушка, этап 5 фазы 24).
// Из спецификации (BOM) к операциям гибочной машины и выходному коду ЧПУ (G-код, абстрактный).
// Без системы ЧПУ: представление операций и шаблон выходного кода для интеграции с машиной.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "bendcore/blank.hpp"
#include "bendcore/tubegeom.hpp"

namespace bendcore {

// Операция гибки на машине: изгиб, угол, радиус, порядок.
struct BendOperation {
    int sequence = 0;           // номер операции (1, 2, 3, ...)
    double radius_mm = 0.0;     // радиус гиба, мм
    double angle_deg = 0.0;     // угол гиба, градусы
    double rotation_deg = 0.0;  // поворот вокруг оси трубы, градусы
    double feed_mm_sec = 0.0;   // скорость подачи, мм/сек (0 = по умолчанию)
    std::string tool_id;        // идентификатор оправки/инструмента
};

// Управляющая программа для одной трубы: последовательность гибок, время, путь.
struct ProductionJob {
    std::string tube_id;                  // идентификатор трубы из модели
    double blank_length_mm = 0.0;         // длина заготовки, мм (из BOM)
    double finished_length_mm = 0.0;      // длина готовой трубы, мм (=ось)
    std::vector<BendOperation> bends;     // операции гибки по порядку
    std::optional<std::string> tool_set;  // набор инструментов (оправки)
    double estimated_time_sec = 0.0;      // прогноз времени операции, сек
};

// Планирование партии: производственные работы, сгруппированные по инструментам.
struct ProductionBatch {
    std::string id;                    // идентификатор партии
    std::vector<ProductionJob> jobs;   // работы в партии
    double total_time_sec = 0.0;       // общее время, сек
    int total_tubes = 0;               // всего труб
};

// Генерировать управляющую программу из пути трубы.
// В заглушке: создать последовательность BendOperation из path (Straight пропускаются, Bend → BendOperation).
ProductionJob generate_job(const std::string& tube_id, const Path& path, double blank_length,
                           double default_feed_mm_sec = 50.0);

// Абстрактный G-код для одной работы (заглушка: текстовое представление операций).
std::string job_to_gcode(const ProductionJob& job, bool include_preamble = true);

// Планирование партии по идентификаторам инструментов.
ProductionBatch plan_batch(const std::vector<ProductionJob>& jobs, const std::string& batch_id);

}  // namespace bendcore
