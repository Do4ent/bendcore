// Фаззер libFuzzer одной цели (BENDCORE_FUZZ_TARGET — имя из fuzz_targets.cpp), собирается в
// конфигурации -DBENDCORE_FUZZ=ON компилятором clang с -fsanitize=fuzzer,address,undefined.
//
//   build-fuzz/fuzz_step build-fuzz/fuzz-corpus/step -max_total_time=60
//
// Исключение, вышедшее из загрузчика, libFuzzer сообщает как падение (deadly signal): это дефект.
#include "fuzz_targets.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

const bendcore::qa::fuzz::Target& target() {
    static const bendcore::qa::fuzz::Target* t = bendcore::qa::fuzz::findTarget(BENDCORE_FUZZ_TARGET);
    if (!t) {
        std::fprintf(stderr, "fuzz: unknown target %s\n", BENDCORE_FUZZ_TARGET);
        std::abort();
    }
    return *t;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    target().run(std::string_view(reinterpret_cast<const char*>(data), size));
    return 0;
}
