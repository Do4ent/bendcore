// Детерминированный мутационный тест загрузчиков (docs/qa/QA.md, раздел 7).
//
// Для каждой цели: затравки (fuzz_targets.cpp) → BENDCORE_FUZZ_ITERATIONS мутаций с
// фиксированным зерном (BENDCORE_FUZZ_SEED, по умолчанию 20261111). Каждый вход выполняется в
// отдельном дочернем процессе (fork), поэтому падение не обрывает тест, а фиксируется:
//  * accepted / rejected (явная ошибка Result) — норма;
//  * exception — исключение вышло из загрузчика;
//  * crash — сигнал или ненормальный код (в том числе отчёт ASan/UBSan в конфигурации build-fuzz);
//  * hang — дольше BENDCORE_FUZZ_TIMEOUT_S секунд (по умолчанию 60), процесс убивается;
//  * memory — пиковая память дочернего процесса больше BENDCORE_FUZZ_RSS_LIMIT_MB (4096).
// Вход каждого дефекта сохраняется в BENDCORE_FUZZ_ARTIFACTS (<вид>-<цель>-<номер>.bin) и
// воспроизводится fuzz_replay <цель> <файл>.
//
// Дефекты, которые нельзя исправить в коде bendcore (в сторонней библиотеке), перечислены в
// known_defects.txt для своей конфигурации (native — gcc без санитайзеров, asan — build-fuzz).
// Тест требует точного совпадения: новый дефект — падение теста, исчезнувший (исправленный)
// дефект — тоже падение, чтобы список не устаревал. Затравки должны приниматься.

#include "fuzz_targets.hpp"

#include <gtest/gtest.h>

#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>

using namespace bendcore::qa::fuzz;

namespace {

#if defined(__SANITIZE_ADDRESS__)
constexpr const char* kConfig = "asan";
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
constexpr const char* kConfig = "asan";
#else
constexpr const char* kConfig = "native";
#endif
#else
constexpr const char* kConfig = "native";
#endif

constexpr int kAccepted = 10, kRejected = 11, kException = 12;

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
}

struct RunResult {
    std::string kind;  // accepted, rejected, exception, crash, hang
    std::string detail;
    double maxRssMiB = 0;
};

// Загрузчик в дочернем процессе; код выхода — исход, сообщение исключения — через канал.
RunResult runIsolated(const Target& t, const std::string& input, long timeoutS) {
    int fds[2];
    if (pipe(fds) != 0) return {"crash", "pipe() failed", 0};
    std::fflush(nullptr);
    const pid_t pid = fork();
    if (pid == 0) {
        close(fds[0]);
        int code = kRejected;
        try {
            code = t.run(input, nullptr) == Outcome::Accepted ? kAccepted : kRejected;
        } catch (const std::exception& e) {
            const std::string m = e.what();
            (void)!write(fds[1], m.data(), m.size());
            code = kException;
        } catch (...) {
            code = kException;
        }
        std::fflush(nullptr);
        _exit(code);
    }
    close(fds[1]);
    if (pid < 0) {
        close(fds[0]);
        return {"crash", "fork() failed", 0};
    }
    int status = 0;
    rusage ru{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutS);
    bool hung = false;
    while (wait4(pid, &status, WNOHANG, &ru) == 0) {
        if (std::chrono::steady_clock::now() > deadline) {
            kill(pid, SIGKILL);
            wait4(pid, &status, 0, &ru);
            hung = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::string message;
    char buf[512];
    for (ssize_t n; (n = read(fds[0], buf, sizeof(buf))) > 0;) message.append(buf, std::size_t(n));
    close(fds[0]);
    RunResult r;
    r.maxRssMiB = double(ru.ru_maxrss) / 1024.0;
    if (hung) {
        r.kind = "hang";
    } else if (WIFEXITED(status) && WEXITSTATUS(status) == kAccepted) {
        r.kind = "accepted";
    } else if (WIFEXITED(status) && WEXITSTATUS(status) == kRejected) {
        r.kind = "rejected";
    } else if (WIFEXITED(status) && WEXITSTATUS(status) == kException) {
        r.kind = "exception";
        r.detail = message;
    } else {
        r.kind = "crash";
        r.detail = WIFSIGNALED(status) ? "signal " + std::to_string(WTERMSIG(status))
                                       : "exit code " + std::to_string(WEXITSTATUS(status));
    }
    return r;
}

using Defect = std::tuple<std::string, long, std::string>;  // цель, номер мутации, вид

// known_defects.txt: «<конфигурация> <цель> <номер> <вид>  # причина».
std::set<Defect> knownDefects(const std::string& target) {
    std::set<Defect> out;
    std::ifstream f(std::string(BENDCORE_SOURCE_DIR) + "/tests/regression/fuzz/known_defects.txt");
    std::string line;
    while (std::getline(f, line)) {
        line = line.substr(0, line.find('#'));
        std::istringstream ss(line);
        std::string config, name, kind;
        long n = -1;
        if (!(ss >> config >> name >> n >> kind)) continue;
        if (config == kConfig && name == target) out.emplace(name, n, kind);
    }
    return out;
}

class FuzzLoaders : public ::testing::TestWithParam<const char*> {};

TEST_P(FuzzLoaders, MutatedInputsGiveExplicitErrorsOnly) {
    const Target* target = findTarget(GetParam());
    ASSERT_NE(target, nullptr);
    const long iterations = envLong("BENDCORE_FUZZ_ITERATIONS", 300);
    const std::uint64_t seed = std::uint64_t(envLong("BENDCORE_FUZZ_SEED", 20261111));
    const long timeoutS = envLong("BENDCORE_FUZZ_TIMEOUT_S", 60);
    const double rssLimit = double(envLong("BENDCORE_FUZZ_RSS_LIMIT_MB", 4096));

    const std::vector<std::string> base = seeds(target->name, BENDCORE_SOURCE_DIR);
    // Загрузчик никогда не выполняется в самом тесте: потоки OCCT (OSD_ThreadPool и др.),
    // запущенные в родителе, после fork остались бы с захваченными мьютексами в дочернем.
    for (std::size_t i = 0; i < base.size(); ++i) {
        const RunResult r = runIsolated(*target, base[i], timeoutS);
        EXPECT_EQ(r.kind, "accepted") << target->name << ": seed " << i << " must load " << r.detail;
    }

    // Зерно зависит от цели: наборы мутаций у целей разные, но воспроизводимые.
    Mutator mut(seed ^ std::hash<std::string>{}(target->name));
    long accepted = 0, rejected = 0;
    double maxRss = 0;
    long maxRssAt = -1;
    std::set<Defect> found;
    const auto t0 = std::chrono::steady_clock::now();
    for (long n = 0; n < iterations; ++n) {
        const std::string& origin = base[mut.below(base.size())];
        const std::string input = mut.mutate(origin, target->json);
        RunResult r = runIsolated(*target, input, timeoutS);
        if (r.maxRssMiB > maxRss) {
            maxRss = r.maxRssMiB;
            maxRssAt = n;
        }
        if (r.kind == "accepted" || r.kind == "rejected") {
            (r.kind == "accepted" ? accepted : rejected)++;
            if (r.maxRssMiB <= rssLimit) continue;
            r.kind = "memory";
            r.detail = std::to_string(long(r.maxRssMiB)) + " MiB";
        }
        found.emplace(target->name, n, r.kind);
        saveInput(r.kind + "-" + target->name + "-" + std::to_string(n) + ".bin", input);
        std::printf("[fuzz] %s %s #%ld: %s %s (%zu bytes, saved to %s)\n", kConfig, target->name, n, r.kind.c_str(),
                    r.detail.c_str(), input.size(), artifactsDir().c_str());
    }
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("[fuzz] %s %-13s seed=%llu iterations=%ld accepted=%ld rejected=%ld defects=%zu time=%.2fs "
                "max child RSS %.0f MiB (#%ld)\n",
                kConfig, target->name, static_cast<unsigned long long>(seed), iterations, accepted, rejected,
                found.size(), s, maxRss, maxRssAt);

    // Сравнение со списком известных дефектов — только при номинальных параметрах прогона.
    if (seed == 20261111 && iterations == 300) {
        const std::set<Defect> known = knownDefects(target->name);
        for (const Defect& d : found) {
            EXPECT_TRUE(known.count(d)) << "new defect: " << kConfig << " " << std::get<0>(d) << " " << std::get<1>(d)
                                        << " " << std::get<2>(d);
        }
        for (const Defect& d : known) {
            EXPECT_TRUE(found.count(d)) << "listed defect no longer reproduces (fixed? remove it from "
                                           "known_defects.txt): "
                                        << kConfig << " " << std::get<0>(d) << " " << std::get<1>(d) << " "
                                        << std::get<2>(d);
        }
    } else {
        EXPECT_TRUE(found.empty()) << found.size() << " defects with seed " << seed << ", " << iterations
                                   << " iterations";
    }
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
