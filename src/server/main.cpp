// HTTP-сервис bendcore: POST /v1/tube, GET /v1/health. Без внешних библиотек, один поток, Connection: close.
// Использование: bendcore_server [порт], по умолчанию 8080. Слушает 0.0.0.0.
//   POST /v1/tube?format=csv|json&od=20&wall=0&deflection=0.01   тело запроса — текст файла.
#include "bendcore/tube_service.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

namespace {

using bendcore::server::Response;

constexpr std::size_t kMaxHeader = 64 * 1024;
constexpr std::size_t kMaxBody = 8 * 1024 * 1024;

struct Request {
    std::string method;
    std::string path;
    std::map<std::string, std::string> query;
    std::string body;
};

std::string url_decode(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out += ' ';
        } else if (s[i] == '%' && i + 2 < s.size() && std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
                   std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
            out += static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

// Читает заголовки и тело. Возвращает false, если запрос не разобран или слишком большой.
bool read_request(int fd, Request& req, int& status) {
    std::string buf;
    char chunk[65536];
    std::size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos) {
        const ssize_t n = recv(fd, chunk, sizeof chunk, 0);
        if (n <= 0) return false;
        buf.append(chunk, static_cast<std::size_t>(n));
        headerEnd = buf.find("\r\n\r\n");
        if (headerEnd == std::string::npos && buf.size() > kMaxHeader) {
            status = 431;
            return false;
        }
    }
    const std::string head = buf.substr(0, headerEnd);

    const std::size_t lineEnd = head.find("\r\n");
    const std::string requestLine = head.substr(0, lineEnd);
    const std::size_t sp1 = requestLine.find(' ');
    const std::size_t sp2 = requestLine.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) return false;
    req.method = requestLine.substr(0, sp1);
    const std::string target = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);
    const std::size_t q = target.find('?');
    req.path = target.substr(0, q);
    if (q != std::string::npos) {
        std::size_t pos = q + 1;
        while (pos <= target.size()) {
            std::size_t amp = target.find('&', pos);
            if (amp == std::string::npos) amp = target.size();
            const std::string pair = target.substr(pos, amp - pos);
            const std::size_t eq = pair.find('=');
            if (!pair.empty()) {
                req.query[url_decode(pair.substr(0, eq))] = eq == std::string::npos ? "" : url_decode(pair.substr(eq + 1));
            }
            pos = amp + 1;
        }
    }

    std::size_t length = 0;
    std::size_t lineStart = lineEnd == std::string::npos ? head.size() : lineEnd + 2;
    while (lineStart < head.size()) {
        std::size_t end = head.find("\r\n", lineStart);
        if (end == std::string::npos) end = head.size();
        std::string line = head.substr(lineStart, end - lineStart);
        for (auto& c : line) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (line.rfind("content-length:", 0) == 0) {
            length = static_cast<std::size_t>(std::strtoull(line.c_str() + 15, nullptr, 10));
        }
        lineStart = end + 2;
    }
    if (length > kMaxBody) {
        status = 413;
        return false;
    }
    const std::size_t bodyStart = headerEnd + 4;
    while (buf.size() < bodyStart + length) {
        const ssize_t n = recv(fd, chunk, sizeof chunk, 0);
        if (n <= 0) return false;
        buf.append(chunk, static_cast<std::size_t>(n));
    }
    req.body = buf.substr(bodyStart, length);
    return true;
}

const char* reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        default: return "Error";
    }
}

void send_all(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) return;
        sent += static_cast<std::size_t>(n);
    }
}

void respond(int fd, int status, const std::string& contentType, const std::string& body) {
    std::string head = "HTTP/1.1 " + std::to_string(status) + " " + reason(status) + "\r\n";
    head += "Content-Type: " + contentType + "\r\n";
    head += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    head += "Access-Control-Allow-Origin: *\r\n";
    head += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    head += "Access-Control-Allow-Headers: Content-Type\r\n";
    head += "Connection: close\r\n\r\n";
    send_all(fd, head + body);
}

// Число из параметра запроса; если параметра нет — значение по умолчанию.
bool number_param(const Request& req, const char* name, double fallback, double& out) {
    const auto it = req.query.find(name);
    if (it == req.query.end()) {
        out = fallback;
        return true;
    }
    char* end = nullptr;
    out = std::strtod(it->second.c_str(), &end);
    return end != it->second.c_str() && *end == '\0';
}

void handle(int fd, const Request& req) {
    if (req.method == "OPTIONS") {
        respond(fd, 204, "text/plain", "");
        return;
    }
    if (req.path == "/v1/health") {
        if (req.method != "GET") return respond(fd, 405, "application/json", "{\"ok\":false,\"error\":\"нужен GET\"}");
        respond(fd, 200, "application/json", "{\"ok\":true,\"service\":\"bendcore\"}");
        return;
    }
    if (req.path == "/v1/tube") {
        if (req.method != "POST") return respond(fd, 405, "application/json", "{\"ok\":false,\"error\":\"нужен POST\"}");
        const auto fmt = req.query.find("format");
        const std::string format = fmt == req.query.end() ? "csv" : fmt->second;
        double od = 0.0, wall = 0.0, deflection = 0.0;
        if (!number_param(req, "od", 0.0, od) || !number_param(req, "wall", 0.0, wall) ||
            !number_param(req, "deflection", 0.01, deflection)) {
            return respond(fd, 400, "application/json", "{\"ok\":false,\"error\":\"параметр od, wall или deflection не число\"}");
        }
        const Response res = bendcore::server::tube_response(format, req.body, od, wall, deflection);
        respond(fd, res.status, res.contentType, res.body);
        return;
    }
    respond(fd, 404, "application/json", "{\"ok\":false,\"error\":\"нет такого маршрута\"}");
}

}  // namespace

int main(int argc, char** argv) {
    const int port = argc > 1 ? std::atoi(argv[1]) : 8080;
    if (port <= 0 || port > 65535) {
        std::fprintf(stderr, "порт должен быть от 1 до 65535\n");
        return 2;
    }
    signal(SIGPIPE, SIG_IGN);

    const int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        std::perror("socket");
        return 1;
    }
    int one = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0 || listen(listener, 16) < 0) {
        std::perror("bind/listen");
        close(listener);
        return 1;
    }
    std::printf("bendcore_server: порт %d\n", port);
    std::fflush(stdout);

    for (;;) {
        const int fd = accept(listener, nullptr, nullptr);
        if (fd < 0) {
            if (errno == EINTR) continue;
            std::perror("accept");
            continue;
        }
        Request req;
        int status = 400;
        if (read_request(fd, req, status)) {
            handle(fd, req);
        } else {
            respond(fd, status, "application/json", "{\"ok\":false,\"error\":\"запрос не разобран\"}");
        }
        close(fd);
    }
}
