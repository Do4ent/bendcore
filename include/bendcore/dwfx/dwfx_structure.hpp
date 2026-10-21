// Структура DWFx (OPC, 3D): сущности и свойства (content.xml), объекты, экземпляры раздела и видимость по видам.
// Чтение XML — pugixml (third_party/pugixml). Геометрия W3D сюда не входит: связь экземпляр → узел → сегмент
// W3D выполняется отдельно (см. пункт 1 плана, раздел «связь с W3D»).
// Ошибки разбора — исключение std::invalid_argument.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bendcore::dwfx {

struct DwfProperty {
    std::string name;
    std::string value;
    std::string category;
};

// <dwf:Entity id label> с дочерними <dwf:Property>. Скрытые свойства и единицы (native:*) сохраняются как есть.
struct DwfEntity {
    std::string id;
    std::string label;
    std::vector<DwfProperty> properties;
};

// <dwf:Object id label entityRef>: объект модели, ссылается на сущность. Объекты образуют дерево:
// вложенный <dwf:Object> — дочерний; parent_id пуст у корневых. Свойства самого объекта — в properties.
struct DwfObject {
    std::string id;
    std::string label;
    std::string entity_ref;
    std::string parent_id;
    std::vector<DwfProperty> properties;
};

// <dwf:Content>: сущности и объекты. Порядок — как в файле.
struct DwfContent {
    std::vector<DwfEntity> entities;
    std::vector<DwfObject> objects;
};

// <dwf:Instance id renderableRef node>: экземпляр раздела; renderableRef — объект из content.xml,
// node — тег узла W3D (TKE_Tag index).
struct DwfInstance {
    std::string id;
    std::string renderable_ref;
    std::uint32_t node = 0;
};

// Видимость экземпляра в одном виде: <InstanceAttributes id><InstanceVisibility state="on|off"/>.
struct DwfInstanceVisibility {
    std::string instance_id;
    bool on = true;
};

// <ModelViewNode id label> в разделе представления.
struct DwfView {
    std::string id;
    std::string label;
    std::vector<DwfInstanceVisibility> visibility;
};

// Раздел модели: экземпляры (sec*.xml с <dwf:Instances>) и виды (представление с <ModelViewNode>).
// Файл может содержать только одно из двух; отсутствующая часть пуста.
struct DwfSection {
    std::vector<DwfInstance> instances;
    std::vector<DwfView> views;
};

// Разбирает content.xml (корень <dwf:Content>).
DwfContent parse_dwf_content(const std::string& xml);

// Разбирает раздел модели (экземпляры и/или виды).
DwfSection parse_dwf_section(const std::string& xml);

}  // namespace bendcore::dwfx
