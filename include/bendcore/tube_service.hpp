// Сервис тела трубы: вход — текст CSV или JSON, выход — JSON с объёмом, портами и сеткой.
// Без сетевого кода: HTTP-обёртка в src/server/main.cpp. Нужен OCCT (BENDCORE_WITH_OCCT).
#pragma once

#include <string>

namespace bendcore::server {

struct Response {
    int status = 200;
    std::string contentType = "application/json; charset=utf-8";
    std::string body;
};

// format — "csv" или "json"; text — содержимое файла; od, wall — мм; deflection — отклонение сетки, мм (0 < d ≤ 10).
// Файл без пути (только порты) — ошибка 400: тело не строится. Ошибки данных — 400, прочие — 500.
Response tube_response(const std::string& format, const std::string& text, double od, double wall, double deflection);

}  // namespace bendcore::server
