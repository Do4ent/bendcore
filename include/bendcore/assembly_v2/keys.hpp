#pragma once

// Шпонки призматические ГОСТ 23360-78 библиотеки стандартных изделий (docs/assembly/FASTENERS.md,
// раздел 9): сечения b×h с глубинами пазов вала t1 и втулки t2 по диаметру вала, фаски s, ряд
// длин; модели исполнений 1 (оба конца скруглены), 2 (оба плоские), 3 (один скруглён) с фасками
// s × 45° по продольным кромкам сечения; атрибуты «key.*», свойства для BOM и спецификации.
//
// Таблица записана по памяти без доступа к официальному тексту стандарта и ТРЕБУЕТ СВЕРКИ
// (keyTableInfo().verified == false, docs/STANDARDS_VERIFICATION.md).

#include <bendcore/assembly_v2/fasteners.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bendcore::assembly_v2::fasteners {

enum class KeyExecution {
    Rounded = 1,     // исполнение 1: оба конца скруглены радиусом b/2
    Flat = 2,        // исполнение 2: оба конца плоские
    OneRounded = 3,  // исполнение 3: конец x = −l/2 скруглён, x = +l/2 плоский
};

struct KeyRow {
    double dOver = 0;  // диаметр вала: свыше …
    double dUpTo = 0;  // … до (включительно)
    double b = 0;      // ширина
    double h = 0;      // высота
    double t1 = 0;     // глубина паза вала
    double t2 = 0;     // глубина паза втулки
    double sMin = 0;   // фаска s (или радиус): наименьшая …
    double sMax = 0;   // … наибольшая
    double lMin = 0;   // длина: наименьшая …
    double lMax = 0;   // … наибольшая (из ряда keyLengthSeries)
};

// Сечение для диаметра вала d (dOver < d ≤ dUpTo). Ошибки: Unsupported — d вне 6…110.
kernel::Result<KeyRow> keyRowForShaft(double d);
// Сечение b×h. Ошибки: Unsupported — нет в таблице.
kernel::Result<KeyRow> keyRow(double b, double h);
const std::vector<KeyRow>& keyTable();

// Глубины пазов вала t1 и втулки t2 для вала диаметром d (по сечению keyRowForShaft).
struct KeySeatDepths {
    double b = 0, h = 0;
    double t1 = 0, t2 = 0;
};
kernel::Result<KeySeatDepths> keySeatDepths(double d);

// Ряд длин шпонок l (мм).
const std::vector<double>& keyLengthSeries();

struct KeyTableInfo {
    std::string standard;  // «ГОСТ 23360-78»
    std::string source;
    std::string verification;
    bool verified = false;
};
KeyTableInfo keyTableInfo();

struct KeySpec {
    KeyExecution execution = KeyExecution::Rounded;
    double b = 0, h = 0, l = 0;
    // Фаска s × 45°; пусто — наименьшая по таблице (sMin). Должна быть в [sMin, sMax].
    std::optional<double> chamfer;
};

// Роли граней шпонки: "Key:<роль>".
namespace key_roles {
inline constexpr const char* kFeature = "Key";
inline constexpr const char* kBottom = "Bottom";   // z = 0 (опора на дно паза вала)
inline constexpr const char* kTop = "Top";         // z = h
inline constexpr const char* kSide = "Side";       // рабочие грани Side1 (y = −b/2), Side2 (y = +b/2)
inline constexpr const char* kEnd = "End";         // торцы: End1 (x = −l/2), End2 (x = +l/2) — плоские или скруглённые
}  // namespace key_roles

std::string keyFaceName(const std::string& role);

// Атрибуты «key.*» (на всех гранях): key.standard, key.execution, key.b, key.h, key.l, key.s,
// key.t1, key.t2, key.designation.
modeling::ElementAttributes keyAttributes(const KeySpec& spec, const KeyRow& row, double s,
                                          const std::string& designation);

// Модель: длина вдоль X (x ∈ [−l/2, l/2]), ширина вдоль Y, высота z ∈ [0, h].
struct Key {
    KeySpec spec;
    KeyRow row;
    double s = 0;  // фаска модели
    TopoDS_Shape shape;
    std::shared_ptr<const modeling::NamedShape> elementMap;
    FastenerProperties properties;  // name «Шпонка», designation «Шпонка 2-10×8×32 ГОСТ 23360-78»
    std::vector<std::pair<std::string, double>> dimensions;  // b, h, l, s, t1, t2

    std::optional<double> dimension(const std::string& key) const;
};

// Ошибки: Unsupported — сечения нет в таблице; InvalidInput — длина не из ряда или вне
// [lMin, lMax] сечения, фаска вне [sMin, sMax]; OperationFailed — OCCT.
kernel::Result<Key> makeKey(const KeySpec& spec);

PartData toPartData(const Key& key);
PartProperties toPartProperties(const Key& key);
kernel::Result<DefinitionId> addKeyPart(AssemblyDocument& document, const Key& key);

}  // namespace bendcore::assembly_v2::fasteners
