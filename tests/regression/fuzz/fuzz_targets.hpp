#pragma once

// Fuzz-проверка загрузчиков файлов (docs/qa/QA.md, раздел 7): документ детали, сборки и
// чертежа (JSON), STEP, DXF, BRep (двоичный и текстовый).
//
// Критерий: загрузчик на любом входе либо принимает его, либо возвращает явную ошибку
// (Result с кодом). Падение, исключение наружу, зависание, неопределённое поведение (ASan/UBSan)
// — дефект. Один и тот же набор целей используется детерминированным мутационным тестом
// (test_fuzz_loaders, фиксированное зерно) и фаззерами libFuzzer (fuzz_<цель>).

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace bendcore::qa::fuzz {

enum class Outcome { Accepted, Rejected };

struct Target {
    const char* name;
    bool json;  // вход — JSON: мутатор добавляет замену значений полей и вложенность
    Outcome (*run)(std::string_view input);
};

// Все цели: part_json, assembly_json, drawing_json, step, dxf, brep_binary, brep_text.
const std::vector<Target>& targets();
const Target* findTarget(std::string_view name);

// Затравочные входы цели: закоммиченные файлы из sourceDir (корень репозитория) и документы,
// построенные здесь же через API модулей (toJson, writeStep, writeBrep*, writeDxf).
// Ошибка построения затравки — исключение std::runtime_error.
std::vector<std::string> seeds(std::string_view target, const std::string& sourceDir);

// Детерминированный мутатор: обрезка, перестановка и замена байтов, повтор и удаление блоков,
// «интересные» числа (огромные, отрицательные, денормализованные, NaN/inf как текст), глубокая
// вложенность; для JSON — замена значений случайных полей разбором и сериализацией.
class Mutator {
public:
    explicit Mutator(std::uint64_t seed) : rng_(seed) {}
    std::string mutate(const std::string& input, bool json);
    std::uint64_t below(std::uint64_t n) { return n == 0 ? 0 : rng_() % n; }

private:
    std::string mutateBytes(const std::string& input);
    std::string mutateJsonFields(const std::string& input);
    std::mt19937_64 rng_;
};

// Пиковая резидентная память процесса (VmHWM, байт; 0 — недоступно).
std::size_t peakRssBytes();

}  // namespace bendcore::qa::fuzz
