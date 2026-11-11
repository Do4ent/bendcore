#pragma once

// Общие средства нагрузочных тестов (docs/qa/PERFORMANCE.md): замер времени с порогом,
// пиковая память процесса и отчёт с метками времени.
//
// Отчёт — строки JSON в файле BENDCORE_STRESS_REPORT (по умолчанию stress-report.jsonl в
// текущем каталоге ctest), одна строка на замер:
//   {"time":"2026-11-11T12:00:00Z","test":"part_300","step":"rebuild_after_sketch_edit",
//    "seconds":4.2,"limit_seconds":60,"peak_rss_mib":310}
// Порог умножается на BENDCORE_STRESS_TIME_SCALE (по умолчанию 1): медленные машины CI
// и санитайзеры получают запас, не меняя тест.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

namespace bendcore::qa::stress {

inline double timeScale() {
    const char* v = std::getenv("BENDCORE_STRESS_TIME_SCALE");
    const double s = v && *v ? std::strtod(v, nullptr) : 1.0;
    return s > 0 ? s : 1.0;
}

inline std::size_t readStatusKb(const char* key) {
    std::ifstream f("/proc/self/status");
    std::string line;
    const std::string k = std::string(key) + ":";
    while (std::getline(f, line)) {
        if (line.rfind(k, 0) == 0) {
            std::istringstream ss(line.substr(k.size()));
            std::size_t kb = 0;
            ss >> kb;
            return kb;
        }
    }
    return 0;
}

inline double peakRssMiB() { return double(readStatusKb("VmHWM")) / 1024.0; }
inline double rssMiB() { return double(readStatusKb("VmRSS")) / 1024.0; }

inline std::string utcNow() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

inline void report(const std::string& test, const std::string& step, double seconds, double limitSeconds,
                   const std::string& extra = {}) {
    const char* path = std::getenv("BENDCORE_STRESS_REPORT");
    std::ofstream f(path && *path ? path : "stress-report.jsonl", std::ios::app);
    char line[512];
    std::snprintf(line, sizeof(line),
                  "{\"time\":\"%s\",\"test\":\"%s\",\"step\":\"%s\",\"seconds\":%.3f,\"limit_seconds\":%.1f,"
                  "\"peak_rss_mib\":%.0f%s%s}",
                  utcNow().c_str(), test.c_str(), step.c_str(), seconds, limitSeconds, peakRssMiB(),
                  extra.empty() ? "" : ",", extra.c_str());
    f << line << "\n";
    std::printf("[stress] %s %s %-28s %9.3f s (limit %.1f s) peakRSS %.0f MiB%s%s\n", utcNow().c_str(),
                test.c_str(), step.c_str(), seconds, limitSeconds, peakRssMiB(), extra.empty() ? "" : " ",
                extra.c_str());
    std::fflush(stdout);
}

// Выполняет f, печатает и записывает время, проверяет порог (limitSeconds × timeScale()).
template <class F>
double timed(const std::string& test, const std::string& step, double limitSeconds, F&& f,
             const std::string& extra = {}) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double limit = limitSeconds * timeScale();
    report(test, step, s, limit, extra);
    EXPECT_LE(s, limit) << test << "/" << step << " exceeded its time limit";
    return s;
}

}  // namespace bendcore::qa::stress
