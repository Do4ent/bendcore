// Повтор входов fuzz-проверки без libFuzzer (любая сборка, в том числе build-occt):
//
//   fuzz_replay <цель> <файл>...
//
// Для каждого файла печатает «accepted» или «rejected: <сообщение>». Код выхода 0, если ни один
// вход не вызвал исключения (падение процесса видно само по себе). Используется для разбора
// входов, сохранённых test_fuzz_loaders (BENDCORE_FUZZ_ARTIFACTS) или фаззером (crash-*).
#include "fuzz_targets.hpp"

#include <cstdio>
#include <exception>
#include <fstream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <target> <file>...\n", argv[0]);
        return 2;
    }
    const auto* t = bendcore::qa::fuzz::findTarget(argv[1]);
    if (!t) {
        std::fprintf(stderr, "fuzz_replay: unknown target %s\n", argv[1]);
        return 2;
    }
    int rc = 0;
    for (int i = 2; i < argc; ++i) {
        std::ifstream f(argv[i], std::ios::binary);
        if (!f) {
            std::fprintf(stderr, "fuzz_replay: cannot read %s\n", argv[i]);
            return 2;
        }
        std::ostringstream ss;
        ss << f.rdbuf();
        std::string why;
        try {
            const auto o = t->run(ss.str(), &why);
            std::printf("%s: %s%s\n", argv[i], o == bendcore::qa::fuzz::Outcome::Accepted ? "accepted" : "rejected: ",
                        why.c_str());
        } catch (const std::exception& e) {
            std::printf("%s: EXCEPTION %s\n", argv[i], e.what());
            rc = 1;
        }
    }
    return rc;
}
