// Детерминированный мутационный тест загрузчиков (docs/qa/QA.md, раздел 7).
//
// Для каждой цели: затравки (fuzz_targets.cpp) → BENDCORE_FUZZ_ITERATIONS мутаций с
// фиксированным зерном (BENDCORE_FUZZ_SEED, по умолчанию 20261111). Каждый вход:
//  * загрузчик возвращает результат (принят или явная ошибка) — исключение наружу — дефект;
//  * не дольше BENDCORE_FUZZ_TIMEOUT_S секунд (по умолчанию 30): сторож пишет вход в
//    <BENDCORE_FUZZ_ARTIFACTS>/hang-<цель>-<n>.bin и аварийно завершает процесс;
//  * под ASan/UBSan (конфигурация build-fuzz) — без ошибок санитайзеров.
// Затравки должны приниматься — иначе тест проверял бы только ветку ошибок.

#include "fuzz_targets.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

using namespace bendcore::qa::fuzz;

namespace {

long envLong(const char* name, long fallback) {
    const char* v = std::getenv(name);
    return v && *v ? std::strtol(v, nullptr, 10) : fallback;
}

std::string artifactsDir() {
    const char* v = std::getenv("BENDCORE_FUZZ_ARTIFACTS");
    return v && *v ? v : (std::filesystem::temp_directory_path() / "bendcore-fuzz").string();
}

void saveInput(const std::string& name, const std::string& data) {
    std::filesystem::create_directories(artifactsDir());
    const std::string path = artifactsDir() + "/" + name;
    std::ofstream(path, std::ios::binary).write(data.data(), std::streamsize(data.size()));
    std::fprintf(stderr, "fuzz: input saved to %s (%zu bytes)\n", path.c_str(), data.size());
}

// Сторож зависаний: поток проверяет время текущего входа и при превышении сохраняет вход
// и завершает процесс (std::abort) — зависание нельзя прервать изнутри загрузчика.
class Watchdog {
public:
    explicit Watchdog(std::chrono::seconds limit) : limit_(limit), thread_([this] { loop(); }) {}
    ~Watchdog() {
        {
            std::lock_guard<std::mutex> lock(m_);
            stop_ = true;
        }
        cv_.notify_all();
        thread_.join();
    }
    void begin(const std::string& label, const std::string* input) {
        std::lock_guard<std::mutex> lock(m_);
        label_ = label;
        input_ = input;
        start_ = std::chrono::steady_clock::now();
        busy_ = true;
    }
    void end() {
        std::lock_guard<std::mutex> lock(m_);
        busy_ = false;
    }

private:
    void loop() {
        std::unique_lock<std::mutex> lock(m_);
        while (!stop_) {
            cv_.wait_for(lock, std::chrono::milliseconds(200));
            if (busy_ && std::chrono::steady_clock::now() - start_ > limit_) {
                std::fprintf(stderr, "fuzz: HANG > %llds on %s\n", static_cast<long long>(limit_.count()),
                             label_.c_str());
                saveInput("hang-" + label_ + ".bin", *input_);
                std::abort();
            }
        }
    }
    std::chrono::seconds limit_;
    std::mutex m_;
    std::condition_variable cv_;
    bool stop_ = false, busy_ = false;
    std::string label_;
    const std::string* input_ = nullptr;
    std::chrono::steady_clock::time_point start_;
    std::thread thread_;
};

class FuzzLoaders : public ::testing::TestWithParam<const char*> {};

TEST_P(FuzzLoaders, MutatedInputsGiveExplicitErrorsOnly) {
    const Target* target = findTarget(GetParam());
    ASSERT_NE(target, nullptr);
    const long iterations = envLong("BENDCORE_FUZZ_ITERATIONS", 300);
    const std::uint64_t seed = std::uint64_t(envLong("BENDCORE_FUZZ_SEED", 20261111));
    Watchdog dog(std::chrono::seconds(envLong("BENDCORE_FUZZ_TIMEOUT_S", 30)));

    const std::vector<std::string> base = seeds(target->name, BENDCORE_SOURCE_DIR);
    for (std::size_t i = 0; i < base.size(); ++i) {
        dog.begin(std::string(target->name) + "-seed" + std::to_string(i), &base[i]);
        const Outcome o = target->run(base[i]);
        dog.end();
        EXPECT_EQ(o, Outcome::Accepted) << target->name << ": seed " << i << " must load";
    }

    // Зерно зависит от цели: наборы мутаций у целей разные, но воспроизводимые.
    Mutator mut(seed ^ std::hash<std::string>{}(target->name));
    long accepted = 0, rejected = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (long n = 0; n < iterations; ++n) {
        const std::string& origin = base[mut.below(base.size())];
        const std::string input = mut.mutate(origin, target->json);
        const std::string label = std::string(target->name) + "-" + std::to_string(n);
        dog.begin(label, &input);
        try {
            (target->run(input) == Outcome::Accepted ? accepted : rejected)++;
        } catch (const std::exception& e) {
            saveInput("exception-" + label + ".bin", input);
            ADD_FAILURE() << label << ": exception escaped the loader: " << e.what();
        } catch (...) {
            saveInput("exception-" + label + ".bin", input);
            ADD_FAILURE() << label << ": non-standard exception escaped the loader";
        }
        dog.end();
    }
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("[fuzz] %-13s seed=%llu iterations=%ld accepted=%ld rejected=%ld time=%.2fs peakRSS=%.0fMiB\n",
                target->name, static_cast<unsigned long long>(seed), iterations, accepted, rejected, s,
                double(peakRssBytes()) / (1024.0 * 1024.0));
    // Мутации должны давать оба исхода: только отказы значат, что мутатор рушит каждый вход
    // ещё до разбора полей, только приёмы — что загрузчик ничего не проверяет.
    if (iterations >= 100) {
        EXPECT_GT(rejected, 0) << target->name;
        EXPECT_GT(accepted, 0) << target->name;
    }
}

INSTANTIATE_TEST_SUITE_P(Targets, FuzzLoaders,
                         ::testing::Values("part_json", "assembly_json", "drawing_json", "step", "dxf",
                                           "brep_binary", "brep_text"),
                         [](const ::testing::TestParamInfo<const char*>& p) { return std::string(p.param); });

}  // namespace
