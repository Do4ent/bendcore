// Сервис тела трубы (tube_response): успешный ответ на примере, ошибки формата и данных.
#include "bendcore/tube_service.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using bendcore::server::Response;
using bendcore::server::tube_response;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static std::size_t count(const std::string& text, const std::string& word) {
    std::size_t n = 0;
    for (std::size_t pos = text.find(word); pos != std::string::npos; pos = text.find(word, pos + word.size())) ++n;
    return n;
}

static std::string read_file(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main() {
    const std::string csv = read_file(std::string(BENDCORE_EXAMPLES_DIR) + "/tube_path.csv");
    check(!csv.empty(), "пример CSV читается");

    // Пример: ручной порт P1 плюс два концевых порта; сетка и объём есть.
    const Response ok = tube_response("csv", csv, 16.0, 0.0, 0.05);
    std::printf("пример (сплошной): статус %d, длина ответа %zu\n", ok.status, ok.body.size());
    check(ok.status == 200, "пример: статус 200");
    check(ok.body.find("\"ok\":true") != std::string::npos, "пример: ok=true");
    check(count(ok.body, "\"name\":") == 3, "пример: три порта (P1 и два концевых)");
    check(ok.body.find("\"mesh\":{\"vertices\":[") != std::string::npos, "пример: есть сетка");
    check(ok.body.find("\"valid\":true") != std::string::npos, "пример: сплошное тело валидно");

    // Со стенкой: ответ 200 и поле valid. Спираль со стенкой пока невалидна (ограничение ядра).
    const Response hollow = tube_response("csv", csv, 16.0, 2.0, 0.05);
    check(hollow.status == 200 && hollow.body.find("\"valid\":") != std::string::npos, "пример со стенкой: ответ 200");

    // Формат, отклонение и файл без пути — ошибки 400 с полем error.
    const Response badFormat = tube_response("xml", csv, 16.0, 2.0, 0.05);
    check(badFormat.status == 400 && badFormat.body.find("\"ok\":false") != std::string::npos, "формат xml — 400");
    const Response badDeflection = tube_response("csv", csv, 16.0, 2.0, 0.0);
    check(badDeflection.status == 400, "отклонение 0 — 400");
    const Response onlyPorts = tube_response("csv", "kind,x,y,z,dx,dy,dz,od\nport,0,0,0,1,0,0,20\n", 20.0, 0.0, 0.05);
    check(onlyPorts.status == 400 && onlyPorts.body.find("нет пути") != std::string::npos, "только порты — 400");
    const Response badCsv = tube_response("csv", "kind,length\nstraight,abc\n", 20.0, 0.0, 0.05);
    check(badCsv.status == 400 && badCsv.body.find("строка 2") != std::string::npos, "ошибка CSV указывает строку");
    const Response badWall = tube_response("csv", csv, 16.0, 16.0, 0.05);
    check(badWall.status == 400, "стенка не меньше радиуса — 400");

    if (failures == 0) std::printf("OK server\n");
    return failures == 0 ? 0 : 1;
}
