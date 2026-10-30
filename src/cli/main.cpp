// bendcore_cli: ядро как отдельный процесс для Electron. Протокол — строки JSON:
//   запрос  {"id": 1, "command": "bend.lra", "args": {...}}
//   ответ   {"id": 1, "ok": true, "data": {...}}  или  {"id": 1, "ok": false, "error": "..."}
// Один запрос — одна строка stdin, один ответ — одна строка stdout. Пустая строка игнорируется.
#include <cstdio>
#include <iostream>
#include <string>

#include "bendcore/core_api.hpp"

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--version") {
        std::cout << "bendcore_cli api 1\n";
        return 0;
    }
    std::ios::sync_with_stdio(false);
    bendcore::api::Core core;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::cout << core.handle_line(line) << '\n' << std::flush;
    }
    return 0;
}
