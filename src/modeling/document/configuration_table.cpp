// Таблица параметров детали: заголовки, обозначения, CSV (docs/modeling/PART_DOCUMENT.md,
// раздел «Таблица параметров»). Проверка по документу и подстановка — part_document.cpp.

#include <bendcore/modeling/configuration_table.hpp>

#include <algorithm>
#include <cctype>

namespace bendcore::modeling {

using kernel::Error;
using kernel::ErrorCode;

const char* toString(ConfigurationColumnKind kind) noexcept {
    switch (kind) {
        case ConfigurationColumnKind::Variable: return "variable";
        case ConfigurationColumnKind::Suppression: return "suppression";
        case ConfigurationColumnKind::Material: return "material";
    }
    return "variable";
}

std::optional<ConfigurationColumnKind> configurationColumnKindFromString(std::string_view text) noexcept {
    for (auto k : {ConfigurationColumnKind::Variable, ConfigurationColumnKind::Suppression, ConfigurationColumnKind::Material}) {
        if (text == toString(k)) return k;
    }
    return std::nullopt;
}

std::string columnTitle(const ConfigurationColumn& column) {
    switch (column.kind) {
        case ConfigurationColumnKind::Variable: return column.target;
        case ConfigurationColumnKind::Suppression: return "suppress:" + column.target;
        case ConfigurationColumnKind::Material: return "material";
    }
    return column.target;
}

std::optional<std::size_t> ConfigurationTable::rowOf(std::string_view designation) const {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].designation == designation) return i;
    }
    return std::nullopt;
}

std::optional<std::size_t> ConfigurationTable::columnOf(const ConfigurationColumn& column) const {
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (columns[i] == column) return i;
    }
    return std::nullopt;
}

std::string fullDesignation(const ConfigurationTable& table, std::size_t row) {
    if (row >= table.rows.size()) return {};
    const std::string& d = table.rows[row].designation;
    if (row > 0 && !d.empty() && d.front() == '-') {
        const std::string& base = table.rows.front().designation;
        if (!base.empty() && base.front() != '-') return base + d;
    }
    return d;
}

std::string fullDesignation(const ConfigurationTable& table, std::size_t row, const std::string& base) {
    if (row >= table.rows.size()) return {};
    const std::string& d = table.rows[row].designation;
    if (!d.empty() && d.front() == '-' && !base.empty()) return base + d;
    return fullDesignation(table, row);
}

namespace {

std::string trim(std::string_view s) {
    std::size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
    return std::string(s.substr(b, e - b));
}

std::string asciiLower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Плоское число: необязательный минус, цифры, не более одного разделителя `sep` с цифрами после.
bool isPlainNumber(std::string_view s, char sep) {
    std::size_t i = 0;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
    std::size_t digits = 0, after = 0;
    bool seenSep = false;
    for (; i < s.size(); ++i) {
        const char c = s[i];
        if (c >= '0' && c <= '9') {
            ++digits;
            if (seenSep) ++after;
        } else if (c == sep && !seenSep) {
            seenSep = true;
        } else {
            return false;
        }
    }
    return digits > 0 && (!seenSep || after > 0);
}

std::string quoteCell(const std::string& cell) {
    const bool needs = cell.find_first_of(";\"\r\n") != std::string::npos ||
                       (!cell.empty() && (cell.front() == ' ' || cell.back() == ' ' || cell.front() == '\t' || cell.back() == '\t'));
    if (!needs) return cell;
    std::string out = "\"";
    for (char c : cell) {
        if (c == '"') out += '"';
        out += c;
    }
    out += '"';
    return out;
}

// Строки CSV по RFC 4180 с разделителем «;»; каждая — номер строки файла и ячейки.
struct CsvRecord {
    std::size_t line = 0;
    std::vector<std::string> cells;
    std::vector<bool> quoted;
};

kernel::Result<std::vector<CsvRecord>> splitCsv(std::string_view text) {
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }
    std::vector<CsvRecord> out;
    CsvRecord rec;
    std::string cell;
    bool inQuotes = false, wasQuoted = false;
    std::size_t line = 1;
    rec.line = 1;
    auto endCell = [&] {
        rec.cells.push_back(wasQuoted ? cell : trim(cell));
        rec.quoted.push_back(wasQuoted);
        cell.clear();
        wasQuoted = false;
    };
    auto endRecord = [&] {
        endCell();
        const bool blank = rec.cells.size() == 1 && rec.cells[0].empty() && !rec.quoted[0];
        if (!blank) out.push_back(std::move(rec));
        rec = CsvRecord{};
        rec.line = line;
    };
    std::size_t quoteLine = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') {
                    cell += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                if (c == '\n') ++line;
                cell += c;
            }
            continue;
        }
        if (c == '"') {
            if (!trim(cell).empty()) {
                return Error{ErrorCode::ParseError, "CSV line " + std::to_string(line) + ": quote inside an unquoted cell"};
            }
            cell.clear();
            inQuotes = true;
            wasQuoted = true;
            quoteLine = line;
        } else if (c == ';') {
            endCell();
        } else if (c == '\r') {
            // CRLF: the line ends at '\n'.
        } else if (c == '\n') {
            ++line;
            endRecord();
        } else {
            if (wasQuoted) {
                if (c == ' ' || c == '\t') continue;
                return Error{ErrorCode::ParseError, "CSV line " + std::to_string(line) + ": text after a closing quote"};
            }
            cell += c;
        }
    }
    if (inQuotes) return Error{ErrorCode::ParseError, "CSV line " + std::to_string(quoteLine) + ": unterminated quote"};
    endRecord();
    return out;
}

}  // namespace

std::optional<bool> parseSuppressionCell(std::string_view text) {
    const std::string t = asciiLower(trim(text));
    if (t == "1" || t == "true" || t == "yes" || t == "да" || t == "Да" || t == "ДА") return true;
    if (t == "0" || t == "false" || t == "no" || t == "нет" || t == "Нет" || t == "НЕТ") return false;
    return std::nullopt;
}

std::string configurationTableToCsv(const ConfigurationTable& table, const CsvOptions& options) {
    std::string out;
    if (options.byteOrderMark) out += "\xEF\xBB\xBF";
    out += "designation;name";
    for (const auto& c : table.columns) out += ";" + quoteCell(columnTitle(c));
    out += "\r\n";
    for (const auto& r : table.rows) {
        out += quoteCell(r.designation) + ";" + quoteCell(r.name);
        for (std::size_t k = 0; k < table.columns.size(); ++k) {
            std::string cell = k < r.cells.size() ? r.cells[k] : std::string();
            if (options.decimal == ',' && table.columns[k].kind == ConfigurationColumnKind::Variable &&
                isPlainNumber(cell, '.')) {
                std::replace(cell.begin(), cell.end(), '.', ',');
            }
            out += ";" + quoteCell(cell);
        }
        out += "\r\n";
    }
    return out;
}

kernel::Result<ConfigurationTable> configurationTableFromCsv(std::string_view csv) {
    auto records = splitCsv(csv);
    if (!records) return records.error();
    if (records.value().empty()) return Error{ErrorCode::ParseError, "CSV is empty: expected a header line"};
    const CsvRecord& header = records.value().front();
    if (header.cells.size() < 2) return Error{ErrorCode::ParseError, "CSV line " + std::to_string(header.line) + ": the header needs the columns 'designation;name'"};
    const std::string h0 = asciiLower(header.cells[0]);
    const std::string h1 = asciiLower(header.cells[1]);
    if ((h0 != "designation" && header.cells[0] != "Обозначение" && header.cells[0] != "обозначение") ||
        (h1 != "name" && header.cells[1] != "Наименование" && header.cells[1] != "наименование")) {
        return Error{ErrorCode::ParseError, "CSV line " + std::to_string(header.line) + ": the first two columns must be 'designation' and 'name', found '" +
                                                header.cells[0] + "' and '" + header.cells[1] + "'"};
    }
    ConfigurationTable table;
    for (std::size_t k = 2; k < header.cells.size(); ++k) {
        const std::string& title = header.cells[k];
        ConfigurationColumn col;
        const std::string lower = asciiLower(title);
        auto prefixed = [&](std::string_view prefix) {
            return title.size() > prefix.size() && std::string_view(title).substr(0, prefix.size()) == prefix;
        };
        if (lower == "material" || title == "Материал" || title == "материал") {
            col.kind = ConfigurationColumnKind::Material;
        } else if (lower.rfind("suppress:", 0) == 0 && title.size() > 9) {
            col.kind = ConfigurationColumnKind::Suppression;
            col.target = trim(title.substr(9));
        } else if (prefixed("подавить:") || prefixed("Подавить:")) {
            col.kind = ConfigurationColumnKind::Suppression;
            col.target = trim(title.substr(std::string_view("подавить:").size()));
        } else if (!title.empty()) {
            col.kind = ConfigurationColumnKind::Variable;
            col.target = title;
            if (col.target.front() == '$') col.target.erase(0, 1);
        } else {
            return Error{ErrorCode::ParseError, "CSV line " + std::to_string(header.line) + ": column " + std::to_string(k + 1) + " has an empty title"};
        }
        if (col.kind != ConfigurationColumnKind::Material && col.target.empty()) {
            return Error{ErrorCode::ParseError, "CSV line " + std::to_string(header.line) + ": column " + std::to_string(k + 1) + " '" + title + "' names no feature"};
        }
        if (table.columnOf(col)) return Error{ErrorCode::ParseError, "CSV line " + std::to_string(header.line) + ": column '" + title + "' is repeated"};
        table.columns.push_back(std::move(col));
    }
    for (std::size_t r = 1; r < records.value().size(); ++r) {
        const CsvRecord& rec = records.value()[r];
        if (rec.cells.size() != header.cells.size()) {
            return Error{ErrorCode::ParseError, "CSV line " + std::to_string(rec.line) + ": " +
                                                    std::to_string(rec.cells.size()) + " cells, the header has " +
                                                    std::to_string(header.cells.size())};
        }
        Configuration row;
        row.designation = rec.cells[0];
        row.name = rec.cells[1];
        for (std::size_t k = 2; k < rec.cells.size(); ++k) {
            std::string cell = rec.cells[k];
            if (table.columns[k - 2].kind == ConfigurationColumnKind::Variable && isPlainNumber(cell, ',')) {
                std::replace(cell.begin(), cell.end(), ',', '.');
            }
            row.cells.push_back(std::move(cell));
        }
        table.rows.push_back(std::move(row));
    }
    return table;
}

}  // namespace bendcore::modeling
