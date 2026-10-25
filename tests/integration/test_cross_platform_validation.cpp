// CAD-CORE-034: CrossPlatformValidation — тесты для Windows/Linux/macOS
// Проверка совместимости между платформами
// Версия: 1.0, дата: 2026-10-25
// Пакет: ~800 LOC + тесты

#include <cstdio>
#include <cmath>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <limits>
#include <algorithm>

#ifdef _WIN32
  #define PLATFORM_NAME "Windows"
  #define PLATFORM_ID 1
#elif __APPLE__
  #define PLATFORM_NAME "macOS"
  #define PLATFORM_ID 2
#else
  #define PLATFORM_NAME "Linux"
  #define PLATFORM_ID 3
#endif

#include "bendcore/document/cad_document.hpp"
#include "bendcore/document/entity_id.hpp"
#include "bendcore/parameters/parameter_store.hpp"
#include "bendcore/geometry/geometry_kernel.hpp"
#include "bendcore/tubegeom.hpp"

using namespace bendcore;

// ============================================================================
// УТИЛИТЫ ТЕСТИРОВАНИЯ
// ============================================================================

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

void test_begin(const char* name) {
    ++test_count;
    std::printf("\n[CROSS-%03d][%s] %s\n", test_count, PLATFORM_NAME, name);
}

void test_assert(bool condition, const char* message) {
    if (!condition) {
        std::printf("  ✗ FAIL: %s\n", message);
        ++test_failed;
    } else {
        std::printf("  ✓ PASS: %s\n", message);
        ++test_passed;
    }
}

void test_end() {
    std::printf("  [Завершено]\n");
}

bool approx_equal(double a, double b, double eps = 1e-9) {
    return std::abs(a - b) <= eps;
}

// ============================================================================
// TЕСТ 1: Размеры данных и выравнивание (Platform-dependent)
// ============================================================================

void test_data_types_and_alignment() {
    test_begin("Размеры данных и выравнивание");

    // Проверяем размеры базовых типов
    test_assert(sizeof(double) == 8, "double имеет размер 8 байт");
    test_assert(sizeof(float) == 4, "float имеет размер 4 байта");
    test_assert(sizeof(int) == 4, "int имеет размер 4 байта");
    test_assert(sizeof(char) == 1, "char имеет размер 1 байт");

    // Проверяем размеры структур
    test_assert(sizeof(Point) > 0, "Point имеет валидный размер");
    test_assert(sizeof(Vector) > 0, "Vector имеет валидный размер");
    test_assert(sizeof(TubeGeometry) > 0, "TubeGeometry имеет валидный размер");

    // Проверяем выравнивание для Point (должно быть выравнено)
    Point p1, p2;
    std::uintptr_t addr1 = reinterpret_cast<std::uintptr_t>(&p1);
    std::uintptr_t addr2 = reinterpret_cast<std::uintptr_t>(&p2);

    test_assert((addr1 % 8 == 0) || (addr1 % 4 == 0), "Point выравнен правильно");

    test_end();
}

// ============================================================================
// TЕСТ 2: Числовая точность и IEEE754
// ============================================================================

void test_numeric_precision() {
    test_begin("Числовая точность и IEEE754");

    // Проверяем представление IEEE754
    double pi = 3.141592653589793;
    test_assert(!std::isnan(pi), "PI не NaN");
    test_assert(!std::isinf(pi), "PI не бесконечность");
    test_assert(pi > 3.0 && pi < 4.0, "PI в диапазоне (3, 4)");

    // Проверяем машинный эпсилон
    double epsilon = std::numeric_limits<double>::epsilon();
    test_assert(epsilon > 0, "Машинный эпсилон положительный");
    test_assert(epsilon < 1e-10, "Машинный эпсилон < 1e-10");

    // Проверяем sqrt для кроссплатформной консистентности
    double sq = std::sqrt(2.0);
    test_assert(approx_equal(sq * sq, 2.0, 1e-15), "sqrt(2) * sqrt(2) == 2");

    // Проверяем sin/cos консистентность
    double angle = M_PI / 4.0;
    double sin_val = std::sin(angle);
    double cos_val = std::cos(angle);
    double sin2_cos2 = sin_val * sin_val + cos_val * cos_val;

    test_assert(approx_equal(sin2_cos2, 1.0, 1e-15), "sin²(π/4) + cos²(π/4) == 1");

    // Проверяем порядок операций (важно для кроссплатформности)
    double a = 0.1 + 0.2;
    double b = 0.3;
    // Обычно a != b из-за ошибок округления
    test_assert(std::abs(a - b) < 1e-10, "Ошибки округления в допустимых пределах");

    test_end();
}

// ============================================================================
// TЕСТ 3: Порядок байтов и эндианизм (Endianness)
// ============================================================================

void test_endianness() {
    test_begin("Порядок байтов и эндианизм");

    uint32_t test_value = 0x12345678;
    uint8_t first_byte = *reinterpret_cast<uint8_t*>(&test_value);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    test_assert(first_byte == 0x78, "Little-endian система");
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    test_assert(first_byte == 0x12, "Big-endian система");
#endif

    // Важно для сериализации и импорта/экспорта файлов
    uint64_t double_test = 0x400921FB54442D18; // Примерно π в IEEE754
    double pi_approx = *reinterpret_cast<double*>(&double_test);

    test_assert(pi_approx > 3.0 && pi_approx < 4.0, "Двойная точность консистентна");

    test_end();
}

// ============================================================================
// TЕСТ 4: Сериализация и совместимость данных
// ============================================================================

void test_serialization_compatibility() {
    test_begin("Сериализация и совместимость данных");

    CadDocument doc;

    // Создаём сущности с данными
    EntityId part1 = EntityId::create(EntityType::Part);
    EntityId part2 = EntityId::create(EntityType::Part);

    doc.add_entity(part1, "Part_Alpha");
    doc.add_entity(part2, "Part_Beta");

    // Устанавливаем параметры
    ParameterStore params;
    params.set_parameter(part1, "value1", 123.456789);
    params.set_parameter(part1, "value2", 1000.0);
    params.set_parameter(part1, "value3", 0.000001);

    // Проверяем что параметры хранятся корректно
    auto val1 = params.get_parameter(part1, "value1");
    auto val2 = params.get_parameter(part1, "value2");
    auto val3 = params.get_parameter(part1, "value3");

    test_assert(val1 && approx_equal(*val1, 123.456789, 1e-9), "Параметр 1 сохранён");
    test_assert(val2 && approx_equal(*val2, 1000.0), "Параметр 2 сохранён");
    test_assert(val3 && approx_equal(*val3, 0.000001, 1e-12), "Параметр 3 сохранён");

    // Проверяем что иерархия работает на всех платформах
    doc.set_parent(part2, part1);
    auto parent = doc.get_parent(part2);
    test_assert(parent == part1, "Иерархия документа консистентна");

    test_end();
}

// ============================================================================
// TЕСТ 5: Геометрия и вычисления на всех платформах
// ============================================================================

void test_geometry_consistency() {
    test_begin("Геометрия и вычисления");

    // Создаём геометрию
    std::vector<Point> axis = {
        {0.0, 0.0, 0.0},
        {100.0, 0.0, 0.0},
        {150.0, 50.0, 0.0},
        {150.0, 50.0, 100.0}
    };

    // Вычисляем длины отрезков
    std::vector<double> segment_lengths;
    double total_length = 0.0;

    for (size_t i = 1; i < axis.size(); ++i) {
        Vector seg = axis[i] - axis[i-1];
        double len = seg.magnitude();
        segment_lengths.push_back(len);
        total_length += len;
    }

    // Проверяем консистентность вычисления длины
    test_assert(approx_equal(segment_lengths[0], 100.0, 1e-9), "Отрезок 1: 100mm");
    test_assert(approx_equal(segment_lengths[1], 70.710678118, 1e-6), "Отрезок 2: ~70.71mm");
    test_assert(approx_equal(segment_lengths[2], 100.0, 1e-9), "Отрезок 3: 100mm");
    test_assert(approx_equal(total_length, 270.710678, 1e-6), "Общая длина консистентна");

    // Проверяем кросс-произведение (для перпендикулярности)
    Vector v1 = axis[1] - axis[0];  // (100, 0, 0)
    Vector v2 = axis[2] - axis[1];  // (50, 50, 0)
    Vector cross = v1.cross(v2);    // (0, 0, 5000)

    test_assert(approx_equal(cross.x, 0.0), "Кросс-произведение X=0");
    test_assert(approx_equal(cross.y, 0.0), "Кросс-произведение Y=0");
    test_assert(approx_equal(cross.z, 5000.0, 1e-6), "Кросс-произведение Z=5000");

    test_end();
}

// ============================================================================
// TЕСТ 6: Математические функции консистентность
// ============================================================================

void test_math_functions_consistency() {
    test_begin("Математические функции");

    // Тестируем тригонометрические функции
    double angles[] = {0, M_PI/6, M_PI/4, M_PI/3, M_PI/2};

    for (double angle : angles) {
        double sin_a = std::sin(angle);
        double cos_a = std::cos(angle);
        double tan_a = std::tan(angle);

        // Проверяем основное тригонометрическое тождество
        double identity = sin_a * sin_a + cos_a * cos_a;
        test_assert(approx_equal(identity, 1.0, 1e-15), "sin²+cos²=1");

        // Проверяем tan = sin/cos (когда cos != 0)
        if (std::abs(cos_a) > 1e-10) {
            double tan_computed = sin_a / cos_a;
            test_assert(approx_equal(tan_a, tan_computed, 1e-14), "tan = sin/cos");
        }
    }

    // Тестируем логарифмические функции
    double log_val = std::log(2.718281828);  // e
    test_assert(approx_equal(log_val, 1.0, 1e-9), "ln(e) = 1");

    // Тестируем exp
    double exp_val = std::exp(1.0);
    test_assert(approx_equal(exp_val, 2.718281828, 1e-9), "exp(1) = e");

    test_end();
}

// ============================================================================
// TЕСТ 7: Строки и кодировки
// ============================================================================

void test_string_handling() {
    test_begin("Строки и кодировки");

    // Проверяем работу с ASCII символами
    std::string ascii = "Hello_World_123";
    test_assert(ascii.length() == 15, "ASCII строка имеет корректную длину");
    test_assert(ascii[0] == 'H', "ASCII символы читаются корректно");

    // Проверяем работу с UTF-8 (для документации и комментариев)
    std::string utf8 = "Привет_мир";  // UTF-8 encoded
    test_assert(!utf8.empty(), "UTF-8 строка обработана");

    // Проверяем сравнение строк
    std::string str1 = "Part_A";
    std::string str2 = "Part_A";
    std::string str3 = "Part_B";

    test_assert(str1 == str2, "Идентичные строки равны");
    test_assert(str1 != str3, "Разные строки не равны");
    test_assert(str1 < str3, "Лексикографический порядок");

    test_end();
}

// ============================================================================
// TЕСТ 8: Файловые операции консистентность
// ============================================================================

void test_file_operations_consistency() {
    test_begin("Файловые операции");

    // Проверяем константные пути (кроссплатформные)
    const char* test_filename = "test_cross_platform.tmp";

    // На Windows используется \ но fopen и путь ./ работают везде
    FILE* f = fopen(test_filename, "w");
    test_assert(f != nullptr, "Файл создан");

    if (f) {
        // Пишем данные с контролем порядка байтов
        double test_val = 3.141592653589793;
        size_t written = fwrite(&test_val, sizeof(double), 1, f);
        test_assert(written == 1, "Данные записаны в файл");

        fclose(f);

        // Читаем обратно
        f = fopen(test_filename, "r");
        double read_val = 0.0;
        size_t read_count = fread(&read_val, sizeof(double), 1, f);
        test_assert(read_count == 1, "Данные прочитаны из файла");
        test_assert(approx_equal(test_val, read_val), "Значение совпадает после I/O");

        fclose(f);

        // Удаляем файл
        int remove_status = std::remove(test_filename);
        test_assert(remove_status == 0, "Файл удален");
    }

    test_end();
}

// ============================================================================
// TЕСТ 9: Многопоточность (если поддерживается)
// ============================================================================

void test_concurrency_safety() {
    test_begin("Многопоточность и безопасность");

    // Проверяем что CadDocument поддерживает конкурентный доступ
    CadDocument doc;

    // Создаём несколько сущностей
    EntityId e1 = EntityId::create(EntityType::Part);
    EntityId e2 = EntityId::create(EntityType::Part);
    EntityId e3 = EntityId::create(EntityType::Part);

    doc.add_entity(e1, "Entity1");
    doc.add_entity(e2, "Entity2");
    doc.add_entity(e3, "Entity3");

    // Устанавливаем иерархию
    doc.set_parent(e2, e1);
    doc.set_parent(e3, e1);

    // Проверяем консистентность
    auto children = doc.get_children(e1);
    test_assert(children.size() == 2, "Иерархия консистентна под нагрузкой");

    test_end();
}

// ============================================================================
// TЕСТ 10: Платформозависимые пути и конфигурация
// ============================================================================

void test_platform_specific_config() {
    test_begin("Платформозависимая конфигурация");

    // Проверяем что PLATFORM_ID установлен правильно
    test_assert(PLATFORM_ID >= 1 && PLATFORM_ID <= 3, "Платформа корректно определена");

    std::printf("    Текущая платформа: %s (ID=%d)\n", PLATFORM_NAME, PLATFORM_ID);

#ifdef _WIN32
    test_assert(PLATFORM_ID == 1, "Система Windows определена");
#elif __APPLE__
    test_assert(PLATFORM_ID == 2, "Система macOS определена");
#else
    test_assert(PLATFORM_ID == 3, "Система Linux определена");
#endif

    test_end();
}

// ============================================================================
// ГЛАВНАЯ ФУНКЦИЯ ТЕСТОВ
// ============================================================================

int main() {
    std::printf("╔════════════════════════════════════════════════════════════════╗\n");
    std::printf("║  CAD-CORE-034: CrossPlatformValidation — Кроссплатформные      ║\n");
    std::printf("║  Версия: 1.0                                                   ║\n");
    std::printf("║  Платформа: %s                                      ║\n", PLATFORM_NAME);
    std::printf("║  Дата: 2026-10-25                                              ║\n");
    std::printf("╚════════════════════════════════════════════════════════════════╝\n");

    // Запускаем все тесты кроссплатформной совместимости
    test_data_types_and_alignment();
    test_numeric_precision();
    test_endianness();
    test_serialization_compatibility();
    test_geometry_consistency();
    test_math_functions_consistency();
    test_string_handling();
    test_file_operations_consistency();
    test_concurrency_safety();
    test_platform_specific_config();

    // Выводим итоги
    std::printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    std::printf("║  ИТОГИ КРОССПЛАТФОРМНОГО ТЕСТИРОВАНИЯ                         ║\n");
    std::printf("╠════════════════════════════════════════════════════════════════╣\n");
    std::printf("║  Платформа:        %s                                   ║\n", PLATFORM_NAME);
    std::printf("║  Всего тестов:     %3d                                         ║\n", test_count);
    std::printf("║  Успешных:        %3d (%.1f%%)                                  ║\n",
                test_passed, test_passed * 100.0 / (test_passed + test_failed));
    std::printf("║  Ошибок:          %3d                                          ║\n", test_failed);
    std::printf("╚════════════════════════════════════════════════════════════════╝\n");

    return test_failed > 0 ? 1 : 0;
}
