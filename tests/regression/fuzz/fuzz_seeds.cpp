// Записывает затравочный корпус фаззеров: <каталог>/<цель>/seed-<n> для каждой цели
// (fuzz_targets.cpp). Корпус в git не хранится — он строится из закоммиченных файлов и API.
//
//   fuzz_seeds <корень репозитория> <каталог>
#include "fuzz_targets.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s <source dir> <output dir>\n", argv[0]);
        return 2;
    }
    try {
        for (const auto& t : bendcore::qa::fuzz::targets()) {
            const auto dir = std::filesystem::path(argv[2]) / t.name;
            std::filesystem::create_directories(dir);
            const auto seeds = bendcore::qa::fuzz::seeds(t.name, argv[1]);
            for (std::size_t i = 0; i < seeds.size(); ++i) {
                std::ofstream f(dir / ("seed-" + std::to_string(i)), std::ios::binary);
                f.write(seeds[i].data(), std::streamsize(seeds[i].size()));
                if (!f) {
                    std::fprintf(stderr, "fuzz_seeds: cannot write %s\n", dir.c_str());
                    return 1;
                }
            }
            std::printf("%-13s %zu seeds -> %s\n", t.name, seeds.size(), dir.c_str());
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fuzz_seeds: %s\n", e.what());
        return 1;
    }
    return 0;
}
