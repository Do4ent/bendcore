// Структура DWFx: самодостаточная проверка разбора на маленьком XML и, если задан BENDCORE_DWFX_SAMPLE
// (путь к .dwfx), золотые числа для образца. Образец выбирается по номеру документа в пути (8 цифр):
// счётчики, связи ID и видимость по видам совпадают с независимой проверкой по архиву.
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
        "<dwf:Object id=\"O1\" label=\"Сборка\" entityRef=\"E1\">"
        "<dwf:Object id=\"O2\" label=\"Тело\" entityRef=\"E1\"><dwf:Property name=\"Mass\" value=\"1\" category=\"c\"/></dwf:Object>"
        "</dwf:Object>"
        "</dwf:Objects>"
        "</dwf:Content>";
    const DwfContent c = parse_dwf_content(content);
    check(c.entities.size() == 1 && c.objects.size() == 2, "маленький content: 1 сущность и 2 объекта");
    check(c.entities[0].label == "Труба", "маленький content: метка сущности в UTF-8");
    check(c.entities[0].properties.size() == 2, "маленький content: два свойства сущности");
    check(c.entities[0].properties[1].value == "ГОСТ & т.п.", "маленький content: XML-сущности раскрыты");
    check(c.objects[0].parent_id.empty() && c.objects[1].parent_id == "O1", "дерево: вложенный объект помнит родителя");
    check(c.objects[1].properties.size() == 1, "объект: свойство объекта читается");

    const std::string section =
        "<dwf:SectionContent xmlns:dwf=\"DWF-ContentResource:1.0\" version=\"1.0\">"
        "<dwf:Instances>"
        "<dwf:Instance id=\"I1\" renderableRef=\"O1\" node=\"119305\"/>"
        "</dwf:Instances>"
        "</dwf:SectionContent>";
    const DwfSection s = parse_dwf_section(section);
    check(s.instances.size() == 1 && s.instances[0].node == 119305, "маленький раздел: экземпляр и node");
    check(s.instances[0].renderable_ref == "O1", "маленький раздел: renderableRef");

    const std::string view =
        "<ContentPresentation version=\"1,0\"><Presentations><Presentation id=\"P\" label=\"Published Views\"><Views>"
        "<View id=\"V\"><ModelViewNode id=\"M\" label=\"Вид\"><ModelScene>"
        "<InstanceAttributes id=\"I1\"><InstanceVisibility state=\"off\"/></InstanceAttributes>"
        "<InstanceAttributes id=\"I2\"><InstanceVisibility state=\"on\"/></InstanceAttributes>"
        "</ModelScene></ModelViewNode></View></Views></Presentation></Presentations></ContentPresentation>";
    const DwfSection vs = parse_dwf_section(view);
    check(vs.instances.empty() && vs.views.size() == 1, "маленький вид: один вид, без экземпляров");
    check(vs.views[0].label == "Вид" && vs.views[0].visibility.size() == 2, "маленький вид: два экземпляра");
    check(!vs.views[0].visibility[0].on && vs.views[0].visibility[1].on, "маленький вид: on/off");

    bool threw = false;
    try {
        parse_dwf_content("<dwf:Other/>");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "нет корня dwf:Content — исключение");
}

// Золотые значения для одного образца.
struct GoldenView {
    const char* label;
    std::size_t total, on, off;
};
struct Golden {
    const char* key;  // 8 цифр номера документа в имени файла
    std::size_t entities, objects, instances;
    std::size_t entity_props, object_props;
    std::vector<GoldenView> views;
};

// Значения получены независимой проверкой по архиву (Python, xml.etree) и совпадают с С++-разбором.
static const std::vector<Golden> kGolden = {
    {"80004806", 276, 604, 604, 6662, 3134,
     {{"Master", 553, 542, 11}, {"Default", 553, 541, 12}, {"Onderdelen", 553, 457, 96},
      {"Sparepart 1", 553, 492, 61}, {"Sparepart 2", 553, 518, 35}}},
    {"80005311", 331, 679, 679, 7514, 3964,
     {{"[Primary]", 626, 626, 0}, {"Default", 626, 598, 28}, {"Without service panels", 626, 568, 58},
      {"Sparte parts 1", 626, 555, 71}, {"Spare parts 2", 626, 515, 111}}},
    {"10206248", 73, 392, 392, 1228, 951,
     {{"[Primary]", 382, 382, 0}, {"Default", 382, 353, 29}, {"Holes", 382, 12, 370},
      {"Hole diameters", 382, 10, 372}, {"Omega", 382, 358, 24}, {"Clamps", 382, 358, 24}}},
    {"80005313", 278, 731, 731, 6306, 3449,
     {{"[Primary]", 688, 688, 0}, {"Default", 688, 659, 29}, {"Without service panels", 688, 594, 94},
      {"Spareparts 1", 688, 578, 110}, {"Spareparts 2", 688, 529, 159}}},
};

static std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Текст записи архива, путь которой оканчивается на suffix (только первая найденная).
static std::string find_entry(const ZipArchive& zip, const std::string& suffix) {
    for (const auto& e : zip.entries()) {
        if (e.path.size() >= suffix.size() && e.path.compare(e.path.size() - suffix.size(), suffix.size(), suffix) == 0) {
            const auto bytes = zip.extract(e);
            return std::string(bytes.begin(), bytes.end());
        }
    }
    return {};
}

static void check_sample(const char* path, const Golden& g) {
    std::printf("образец %s (%s)\n", g.key, path);
    const ZipArchive zip(read_file(path));
    const DwfContent c = parse_dwf_content(find_entry(zip, ".content.xml"));
    check(c.entities.size() == g.entities, "образец: число сущностей");
    check(c.objects.size() == g.objects, "образец: число объектов");
    std::size_t props_e = 0, props_o = 0, roots = 0;
    for (const auto& e : c.entities) props_e += e.properties.size();
    for (const auto& o : c.objects) {
        props_o += o.properties.size();
        if (o.parent_id.empty()) ++roots;
    }
    check(props_e == g.entity_props, "образец: свойства сущностей");
    check(props_o == g.object_props, "образец: свойства объектов");
    check(roots == 1, "образец: одно корневое дерево объектов");

    // Разделы: экземпляры (в разделе с <dwf:Instances>) и виды (в разделе с <ModelViewNode>).
    DwfSection inst, views;
    for (const auto& e : zip.entries()) {
        if (e.path.find("/sections/") == std::string::npos) continue;
        const auto bytes = zip.extract(e);
        const std::string text(bytes.begin(), bytes.end());
        if (text.find("<dwf:Instances>") != std::string::npos) inst = parse_dwf_section(text);
        if (text.find("<ModelViewNode") != std::string::npos) views = parse_dwf_section(text);
    }
    check(inst.instances.size() == g.instances, "образец: число экземпляров");
    check(views.views.size() == g.views.size(), "образец: число видов");

    // Связи: renderableRef → объект → сущность.
    std::unordered_map<std::string, std::size_t> object_by_id;
    for (std::size_t i = 0; i < c.objects.size(); ++i) object_by_id[c.objects[i].id] = i;
    std::unordered_set<std::string> entity_ids;
    for (const auto& e : c.entities) entity_ids.insert(e.id);
    std::size_t renderable_ok = 0, entity_ok = 0;
    for (const auto& i : inst.instances) {
        const auto it = object_by_id.find(i.renderable_ref);
        if (it == object_by_id.end()) continue;
        ++renderable_ok;
        if (entity_ids.count(c.objects[it->second].entity_ref)) ++entity_ok;
    }
    check(renderable_ok == g.instances, "образец: renderableRef → объект");
    check(entity_ok == g.instances, "образец: объект → сущность");

    // Видимость по видам: видов ровно столько, сколько ожидается; экземпляры без записи видимости не учитываются.
    for (const auto& x : g.views) {
        std::size_t total = 0, on = 0, off = 0;
        for (const auto& v : views.views) {
            if (v.label != x.label) continue;
            total = v.visibility.size();
            for (const auto& a : v.visibility) (a.on ? on : off) += 1;
        }
        std::printf("  вид %-24s всего %zu, on %zu, off %zu\n", x.label, total, on, off);
        check(total == x.total && on == x.on && off == x.off, "образец: видимость вида");
    }
}

int main() {
    check_small();
    if (const char* sample = std::getenv("BENDCORE_DWFX_SAMPLE")) {
        const std::string path = sample;
        bool matched = false;
        for (const auto& g : kGolden) {
            if (path.find(g.key) == std::string::npos) continue;
            matched = true;
            check_sample(sample, g);
        }
        if (!matched) {
            std::printf("образец без золотых значений: %s\n", sample);
            ++failures;
        }
    } else {
        std::printf("пропуск образца: задайте BENDCORE_DWFX_SAMPLE\n");
    }
    if (failures == 0) std::printf("OK dwfx_structure\n");
    return failures == 0 ? 0 : 1;
}
