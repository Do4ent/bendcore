#include "bendcore/dwfx/dwfx_structure.hpp"

#include <cstdint>
#include <cstdlib>
#include <stdexcept>

#include "pugixml.hpp"

namespace bendcore::dwfx {
namespace {

void load_document(const std::string& xml, pugi::xml_document& doc) {
    const pugi::xml_parse_result result = doc.load_buffer(xml.data(), xml.size(), pugi::parse_default);
    if (!result) {
        throw std::invalid_argument(std::string("DWFx XML не разобран: ") + result.description());
    }
}

std::string attr(const pugi::xml_node& node, const char* name) { return node.attribute(name).as_string(); }

}  // namespace

DwfContent parse_dwf_content(const std::string& xml) {
    pugi::xml_document doc;
    load_document(xml, doc);
    const pugi::xml_node root = doc.child("dwf:Content");
    if (!root) throw std::invalid_argument("DWFx content.xml: нет корневого элемента dwf:Content");
    DwfContent out;

    for (const pugi::xml_node entities : root.children("dwf:Entities")) {
        for (const pugi::xml_node e : entities.children("dwf:Entity")) {
            DwfEntity entity;
            entity.id = attr(e, "id");
            entity.label = attr(e, "label");
            for (const pugi::xml_node p : e.children("dwf:Property")) {
                entity.properties.push_back({attr(p, "name"), attr(p, "value"), attr(p, "category")});
            }
            out.entities.push_back(std::move(entity));
        }
    }

    // Дерево объектов: <dwf:Object> может содержать вложенные <dwf:Object>; порядок — обход в глубину.
    // parent_id по значению: ссылка на элемент вектора стала бы висячей после его роста.
    const auto walk = [&out](const pugi::xml_node node, std::string parent_id, auto&& self) -> void {
        for (const pugi::xml_node o : node.children("dwf:Object")) {
            DwfObject object;
            object.id = attr(o, "id");
            object.label = attr(o, "label");
            object.entity_ref = attr(o, "entityRef");
            object.parent_id = parent_id;
            for (const pugi::xml_node p : o.children("dwf:Property")) {
                object.properties.push_back({attr(p, "name"), attr(p, "value"), attr(p, "category")});
            }
            out.objects.push_back(std::move(object));
            self(o, out.objects.back().id, self);
        }
    };
    for (const pugi::xml_node objects : root.children("dwf:Objects")) {
        walk(objects, std::string(), walk);
    }
    return out;
}

DwfSection parse_dwf_section(const std::string& xml) {
    pugi::xml_document doc;
    load_document(xml, doc);
    DwfSection out;

    // Экземпляры — в <dwf:SectionContent>; у файла представления корень <ContentPresentation>, там их нет.
    for (const pugi::xml_node instances : doc.child("dwf:SectionContent").children("dwf:Instances")) {
        for (const pugi::xml_node i : instances.children("dwf:Instance")) {
            DwfInstance inst;
            inst.id = attr(i, "id");
            inst.renderable_ref = attr(i, "renderableRef");
            const long long node = std::strtoll(attr(i, "node").c_str(), nullptr, 10);
            if (node < 0) throw std::invalid_argument("DWFx: отрицательный node у экземпляра " + inst.id);
            inst.node = static_cast<std::uint32_t>(node);
            out.instances.push_back(std::move(inst));
        }
    }

    // Виды: <ModelViewNode> → <ModelScene> → <InstanceAttributes> → <InstanceVisibility state>.
    for (const pugi::xpath_node xpath_view : doc.select_nodes("//ModelViewNode")) {
        const pugi::xml_node view = xpath_view.node();
        DwfView v;
        v.id = attr(view, "id");
        v.label = attr(view, "label");
        for (const pugi::xpath_node xpath_attr : view.select_nodes(".//InstanceAttributes")) {
            const pugi::xml_node a = xpath_attr.node();
            const pugi::xml_node vis = a.child("InstanceVisibility");
            if (!vis) continue;
            v.visibility.push_back({attr(a, "id"), attr(vis, "state") != "off"});
        }
        out.views.push_back(std::move(v));
    }
    return out;
}

}  // namespace bendcore::dwfx
