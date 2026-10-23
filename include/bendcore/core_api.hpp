// Единая точка входа ядра для интерфейса: команда + аргументы JSON → результат JSON.
// Один и тот же диспетчер работает в нативном процессе bendcore_cli (Electron) и в WebAssembly
// (браузер и мобильная версия). Состояние сессии (реестр экземпляров) хранится по id документа.
//
// Ответ: {"ok": true, "data": …} или {"ok": false, "error": "текст для пользователя"}.
#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "bendcore/tubegeom.hpp"

namespace bendcore::api {

using json = nlohmann::json;

class Core {
public:
    Core();
    ~Core();
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    json call(const std::string& command, const json& args);
    // Строка запроса {"id", "command", "args"} → строка ответа с тем же id.
    std::string handle_line(const std::string& line);

    struct State;

private:
    State* state_;
};

// Путь в JSON (как в import_json): [{"kind":"straight","length":…}, {"kind":"bend","radius","angle","rotation"}, …].
Path path_from_json(const json& j);
json path_to_json(const Path& p);

}  // namespace bendcore::api
