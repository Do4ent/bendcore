#include "bendcore/cam.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <sstream>

namespace bendcore {

namespace {

// Оценить время операции гибки по углу (простая модель).
double estimate_bend_time(double angle_deg, double feed_mm_sec) {
    if (feed_mm_sec <= 0.0) feed_mm_sec = 50.0;   // по умолчанию 50 мм/сек
    // Время ~ |угол| / (скорость угловая в градусах/сек). Примерно 20 градусов/сек.
    double angular_speed = 20.0;   // градусы/сек (машинозависимо)
    return std::abs(angle_deg) / angular_speed;
}

}  // namespace

ProductionJob generate_job(const std::string& tube_id, const Path& path, double blank_length,
                           double default_feed_mm_sec) {
    ProductionJob job;
    job.tube_id = tube_id;
    job.blank_length_mm = blank_length;
    job.finished_length_mm = axis_length(path);

    int seq = 0;
    double total_time = 0.0;

    for (const auto& item : path) {
        if (const auto* bend = std::get_if<Bend>(&item)) {
            ++seq;
            BendOperation op;
            op.sequence = seq;
            op.radius_mm = bend->R;
            op.angle_deg = bend->B;
            op.rotation_deg = bend->A;
            op.feed_mm_sec = default_feed_mm_sec;
            op.tool_id = "mandrel";   // заглушка: стандартная оправка

            double time_sec = estimate_bend_time(bend->B, default_feed_mm_sec);
            total_time += time_sec;

            job.bends.push_back(op);
        }
    }

    // Добавить время подачи (заготовка движется между гибками).
    double feed_time = job.finished_length_mm / default_feed_mm_sec;
    total_time += feed_time;

    job.estimated_time_sec = total_time;
    job.tool_set = "standard";   // заглушка: стандартный набор

    return job;
}

std::string job_to_gcode(const ProductionJob& job, bool include_preamble) {
    std::ostringstream out;

    if (include_preamble) {
        out << "; Управляющая программа для трубы " << job.tube_id << "\n";
        out << "; Заготовка: " << job.blank_length_mm << " мм, готовая длина: " << job.finished_length_mm
            << " мм\n";
        out << "; Время: " << job.estimated_time_sec << " сек\n";
        out << "G21 G90 G54\n";   // мм, абсолютные координаты, нулевая точка
        out << "T0 M6\n";         // инструмент 0
        out << "S1000 M3\n";      // 1000 об/мин, вращение
    }

    for (const auto& op : job.bends) {
        char buf[256];
        std::snprintf(buf, sizeof buf, "; Операция %d: R=%.1f мм, угол=%.1f°, поворот=%.1f°\n",
                      op.sequence, op.radius_mm, op.angle_deg, op.rotation_deg);
        out << buf;

        // Абстрактное представление: G65 с параметрами (заглушка для интеграции).
        std::snprintf(buf, sizeof buf, "G65 P1234 R%.1f A%.1f B%.1f F%.0f\n", op.radius_mm,
                      op.angle_deg, op.rotation_deg, op.feed_mm_sec);
        out << buf;
    }

    if (include_preamble) {
        out << "M30\n";   // конец программы
    }

    return out.str();
}

ProductionBatch plan_batch(const std::vector<ProductionJob>& jobs, const std::string& batch_id) {
    ProductionBatch batch;
    batch.id = batch_id;
    batch.jobs = jobs;
    batch.total_tubes = static_cast<int>(jobs.size());

    double total_time = 0.0;
    for (const auto& job : jobs) {
        total_time += job.estimated_time_sec;
    }
    batch.total_time_sec = total_time;

    return batch;
}

}  // namespace bendcore
