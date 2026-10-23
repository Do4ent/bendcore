// Экземпляры труб в сборке и одинаковые трубы (5.8, 5.9).
//
// Экземпляры одной детали (команда «Вставить»): у детали один главный экземпляр. Править путь,
// включать адаптивность и привязки к окружению можно только у главного; остальные экземпляры —
// копии только для чтения, они повторяют деталь в своём положении. «Сделать главным» переносит роль,
// «Отвязать» превращает копию в независимую деталь (новую), которая становится главной сама для себя.
//
// Одинаковые трубы (сигнатура, 5.8): трубы с совпавшей сигнатурой получают одну позицию с количеством.
// Сравнение с допуском 0,1 мм и 0,1°, с учётом обратного порядка (труба развёрнута концами);
// зеркальная труба (знаки поворотов инвертированы) — другая позиция.
#pragma once

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "bendcore/bend_output.hpp"
#include "bendcore/tubegeom.hpp"

namespace bendcore::assembly {

struct InstanceError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Данные трубы, входящие в сигнатуру (5.8). Конец-зависимые параметры даны для концов А и Б.
struct TubeSignatureData {
    Path path;
    double od = 0.0, wall = 0.0;
    std::string material, supply_state;      // материал и состояние поставки
    std::string bending_tool, coiling_tool;  // инструмент гибки и навивки
    std::string tooling_instance;            // экземпляр оснастки
    char feed_from = 'A';                    // подача с конца
    bool both_orders = false;
    bool cut_allowed = true;
    std::optional<double> manual_A, manual_B;  // ручные участки заготовки на концах
    double E_A = 0.0, E_B = 0.0;             // глубина вставки
    std::string insulation;
    bool springback = false;
    std::map<int, double> manual_B_st;       // ручные значения B_ст по номеру гиба (по пути)
};

enum class Match { Different, Same, SameReversed, Mirror };
// Сравнение двух труб по сигнатуре. Mirror — совпадают с точностью до зеркала (отдельная позиция).
Match compare(const TubeSignatureData& a, const TubeSignatureData& b, double tol_len = 0.1, double tol_deg = 0.1);

struct Instance {
    std::string id;
    std::string part_id;
    output::Frame placement;
    bool adaptive = false;     // следует за окружением (только главный)
    std::string designation;   // обозначение (у экземпляров одной детали — общее, из детали)
};

struct Part {
    std::string id;
    std::string designation;
    TubeSignatureData data;
    std::string primary;                  // id главного экземпляра
    std::vector<std::string> instances;   // в порядке вставки
};

// Позиция спецификации: группа одинаковых труб.
struct Position {
    int number = 0;
    std::string designation;                  // у первой трубы позиции по порядку в дереве
    std::vector<std::string> instances;       // экземпляры и независимые копии
    int quantity = 0;
    bool designations_differ = false;         // предупреждение «одинаковые трубы с разными обозначениями»
};

class Registry {
public:
    // Новая деталь и её первый экземпляр (он главный).
    void add_part(const std::string& part_id, const std::string& designation, const TubeSignatureData& data);
    // «Вставить»: новый экземпляр детали; первый экземпляр становится главным, следующие — копии.
    void insert(const std::string& part_id, const std::string& instance_id, const output::Frame& placement = {});
    void remove(const std::string& instance_id);  // главный → роль переходит к следующему по порядку

    bool is_primary(const std::string& instance_id) const;
    const std::string& primary_of(const std::string& part_id) const;
    void set_primary(const std::string& instance_id);  // «Сделать главным»; адаптивность переходит вместе с ролью

    bool can_edit(const std::string& instance_id) const { return is_primary(instance_id); }
    // Правка пути через главный экземпляр; возвращает все экземпляры, которые обновятся.
    std::vector<std::string> edit_path(const std::string& instance_id, const Path& path);
    void set_adaptive(const std::string& instance_id, bool adaptive);

    // «Отвязать»: копия становится независимой деталью new_part_id с тем же содержимым.
    void detach(const std::string& instance_id, const std::string& new_part_id);

    // «Привести к одному» (5.8): общее обозначение для всех труб позиции.
    void unify_designation(const std::vector<std::string>& instance_ids, const std::string& designation);

    // Позиции спецификации в порядке дерева (порядок вставки экземпляров).
    std::vector<Position> positions() const;

    const Instance& instance(const std::string& id) const;
    const Part& part(const std::string& id) const;
    std::vector<std::string> tree_order() const { return order_; }

private:
    Instance& inst(const std::string& id);
    std::map<std::string, Part> parts_;
    std::map<std::string, Instance> instances_;
    std::vector<std::string> order_;  // порядок экземпляров в дереве
};

}  // namespace bendcore::assembly
