// Структура DWFx: самодостаточная проверка разбора на маленьком XML и, если задан BENDCORE_DWFX_SAMPLE
// (путь к source.dwfx), золотые числа по образцу 80004806 — счётчики, связи ID и видимость по видам.
#include "bendcore/dwfx/dwfx_structure.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bendcore/dwfx/zip_reader.hpp"

using namespace bendcore::dwfx;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

static void check_small() {
    const std::string content =
        "<?xml version=\"1.0\"?>"
        "<dwf:Content xmlns:dwf=\"DWF-Content:1.0\" version=\"1.0\">"
        "<dwf:Entities>"
        "<dwf:Entity id=\"E1\" label=\"Труба\">"
        "<dwf:Property name=\"Mass\" value=\"2 kg\" category=\"Physical\"/>"
        "<dwf:Property name=\"Описание\" value=\"ГОСТ &amp; т.п.\" category=\"Design\"/>"
        "</dwf:Entity>"
        "</dwf:Entities>"
        "<dwf:Objects>"
        "<dwf:Object id=\"O1\" label=\"Тело\" entityRef=\"E1\"/>"
        "</dwf:Objects>"
        "</dwf:Content>";
    const DwfContent c = parse_dwf_content(content);
    check(c.entities.size() == 1 && c.objects.size() == 1, "маленький content: 1 сущность и 1 объект");
    check(c.entities[0].label == "Труба", "маленький content: метка сущности в UTF-8");
    check(c.entities[0].properties.size() == 2, "маленький content: два свойства");
    check(c.entities[0].properties[1].value == "ГОСТ & т.п.", "маленький content: XML-сущности раскрыты");
    check(c.objects[0].entity_ref == "E1", "маленький content: ссылка объекта на сущность");

    const std::string section =
        "<dwf:SectionContent xmlns:dwf=\"DWF-ContentResource:1.0\" version=\"1.0\">"
        "<dwf:Instances>"
        "<dwf:Instance id=\"I1\" renderableRef=\"O1\" node=\"119305\"/>"
        "</dwf:Instances>"
        "</dwf:SectionContent>";
    const DwfSection s = parse_dwf_section(section);
    check(s.instances.size() == 1 && s.instances[0].node == 119305, "маленький раздел: экземпляр и node");
    check(s.instances[0].renderable_ref == "O1", "маленький раздел: renderableRef");

    bool threw = false;
    try {
        parse_dwf_content("<dwf:Other/>");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "нет корня dwf:Content — исключение");
}

static std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static std::string find_suffix(const ZipArchive& zip, const std::string& suffix) {
    for (const auto& e : zip.entries()) {
        if (e.path.size() >= suffix.size() && e.path.compare(e.path.size() - suffix.size(), suffix.size(), suffix) == 0) {
            const auto bytes = zip.extract(e);
            return std::string(bytes.begin(), bytes.end());
        }
    }
    return {};
}

static void check_sample(const char* path) {
    const ZipArchive zip(read_file(path));
    const std::string content_xml = find_suffix(zip, ".content.xml");
    check(!content_xml.empty(), "образец: есть content.xml");
    const DwfContent c = parse_dwf_content(content_xml);
    check(c.entities.size() == 276, "образец: 276 сущностей");
    check(c.objects.size() == 604, "образец: 604 объекта");
    std::size_t props = 0, object_props = 0, roots = 0;
    for (const auto& e : c.entities) props += e.properties.size();
    for (const auto& o : c.objects) {
        object_props += o.properties.size();
        if (o.parent_id.empty()) ++roots;
    }
    std::printf("образец: свойств сущностей %zu, объектов %zu, корневых объектов %zu\n", props, object_props, roots);
    check(props + object_props == 9796, "образец: 9796 свойств (сущности + объекты)");

    // Разделы: экземпляры (604) и виды (5). Пути разделов ищем по имени файла.
    std::string instances_xml, views_xml;
    for (const auto& e : zip.entries()) {
        if (e.path.size() < 4 || e.path.compare(e.path.size() - 4, 4, ".xml") != 0) continue;
        if (e.path.find("content.xml") != std::string::npos) continue;
        const auto bytes = zip.extract(e);
        const std::string text(bytes.begin(), bytes.end());
        if (text.find("<dwf:Instances>") != std::string::npos) instances_xml = text;
        if (text.find("<ModelViewNode") != std::string::npos) views_xml = text;
    }
    const DwfSection inst = parse_dwf_section(instances_xml);
    check(inst.instances.size() == 604, "образец: 604 экземпляра");
    const DwfSection views = parse_dwf_section(views_xml);
    check(views.views.size() == 5, "образец: 5 видов");

    // Связи: renderableRef → объект, entityRef → сущность; узлы — в диапазоне тегов W3D (120048).
    std::unordered_map<std::string, std::size_t> object_by_id;
    for (std::size_t i = 0; i < c.objects.size(); ++i) object_by_id[c.objects[i].id] = i;
    std::unordered_set<std::string> entity_ids;
    for (const auto& e : c.entities) entity_ids.insert(e.id);
    std::size_t renderable_hits = 0, entity_hits = 0, node_in_range = 0;
    for (const auto& i : inst.instances) {
        const auto it = object_by_id.find(i.renderable_ref);
        if (it != object_by_id.end()) {
            ++renderable_hits;
            if (entity_ids.count(c.objects[it->second].entity_ref)) ++entity_hits;
        }
        if (i.node < 120048) ++node_in_range;
    }
    check(renderable_hits == 604, "образец: renderableRef → объект 604/604");
    check(entity_hits == 604, "образец: entityRef → сущность 604/604");
    check(node_in_range == 604, "образец: node внутри диапазона тегов 120048");

    // Видимость по видам: (всего, on, off) как в проверке по разметке sec1334.xml.
    const struct {
        const char* label;
        std::size_t total, on, off;
    } expect[] = {
        {"Master", 553, 542, 11},  {"Default", 553, 541, 12},    {"Onderdelen", 553, 457, 96},
        {"Sparepart 1", 553, 492, 61}, {"Sparepart 2", 553, 518, 35},
    };
    for (const auto& x : expect) {
        std::size_t total = 0, on = 0, off = 0;
        for (const auto& v : views.views) {
            if (v.label != x.label) continue;
            total = v.visibility.size();
            for (const auto& a : v.visibility) (a.on ? on : off) += 1;
        }
        std::printf("образец: вид %-12s всего %zu, on %zu, off %zu\n", x.label, total, on, off);
        check(total == x.total && on == x.on && off == x.off, "образец: видимость вида");
    }
}

int main() {
    check_small();
    if (const char* sample = std::getenv("BENDCORE_DWFX_SAMPLE")) {
        check_sample(sample);
    } else {
        std::printf("пропуск образца: задайте BENDCORE_DWFX_SAMPLE\n");
    }
    if (failures == 0) std::printf("OK dwfx_structure\n");
    return failures == 0 ? 0 : 1;
}
