// Условности разрезов сборочных чертежей (ГОСТ 2.305-2008, разд. 9; ГОСТ 2.109-73): крепёж,
// валы, оси при продольном разрезе и шарики всегда показываются нерассечёнными. Правило детали —
// из свойств компонента сборки (PartProperties::sectionCut) или по умолчанию по виду изделия.
// docs/drawing/VIEWS.md, «Условности разрезов сборок».

#include <bendcore/drawing_v2/drawing_document.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>

#include <array>
#include <cctype>
#include <cmath>
#include <optional>

namespace bendcore::drawing_v2::document {

namespace {

// Строчные буквы для сравнения начала наименования (ASCII и кириллица UTF-8 А–Я, Ё → е).
std::string lower(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            out.push_back(char(std::tolower(c)));
            continue;
        }
        if (c == 0xD0 && i + 1 < s.size()) {
            const unsigned char d = static_cast<unsigned char>(s[i + 1]);
            ++i;
            if (d >= 0x90 && d <= 0x9F) {  // А–П → а–п
                out += {char(0xD0), char(d + 0x20)};
            } else if (d >= 0xA0 && d <= 0xAF) {  // Р–Я → р–я
                out += {char(0xD1), char(d - 0x20)};
            } else if (d == 0x81) {  // Ё → е
                out += {char(0xD0), char(0xB5)};
            } else {
                out += {char(c), char(d)};
            }
            continue;
        }
        if (c == 0xD1 && i + 1 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0x91) {  // ё → е
            out += {char(0xD0), char(0xB5)};
            ++i;
            continue;
        }
        out.push_back(char(c));
    }
    return out;
}

// Текст начинается со слова целиком: «Вал 20», но не «Валик».
bool startsWithWord(const std::string& text, const char* word) {
    const std::string t = lower(text), w = lower(word);
    if (t.compare(0, w.size(), w) != 0) return false;
    if (t.size() == w.size()) return true;
    const unsigned char next = static_cast<unsigned char>(t[w.size()]);
    return !(std::isalpha(next) || next >= 0x80);
}

}  // namespace

SectionRule sectionRule(const assembly_v2::PartProperties& p, const std::string& definitionName) {
    if (p.sectionCut) return *p.sectionCut ? SectionRule::Cut : SectionRule::WholeInLongitudinal;
    const std::string& name = p.name.empty() ? definitionName : p.name;
    for (const std::string* t : {&name, &definitionName})
        for (const char* w : {"Шарик", "Ball"})
            if (startsWithWord(*t, w)) return SectionRule::NeverCut;
    if (p.designation.empty()) return SectionRule::WholeInLongitudinal;  // стандартное изделие
    static constexpr std::array<const char*, 23> kWhole = {
        "Болт", "Винт",  "Шпилька", "Гайка", "Шайба", "Штифт",  "Шпонка", "Заклёпка", "Вал",   "Ось",   "Шпиндель", "Bolt",
        "Screw", "Stud", "Nut",     "Washer", "Pin",  "Key",    "Rivet",  "Shaft",    "Axle",  "Spindle", "Dowel"};
    for (const std::string* t : {&name, &definitionName})
        for (const char* w : kWhole)
            if (startsWithWord(*t, w)) return SectionRule::WholeInLongitudinal;
    return SectionRule::Cut;
}

bool longitudinalSection(const TopoDS_Shape& part, views::Vec3 sight) {
    const double sl = views::length(sight);
    if (part.IsNull() || !(sl > 0)) return true;
    double best = 0;
    std::optional<gp_Dir> axis;
    for (TopExp_Explorer ex(part, TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face f = TopoDS::Face(ex.Current());
        BRepAdaptor_Surface s(f, true);
        if (s.GetType() != GeomAbs_Cylinder) continue;
        GProp_GProps g;
        BRepGProp::SurfaceProperties(f, g);
        if (g.Mass() > best) {
            best = g.Mass();
            axis = s.Cylinder().Axis().Direction();
        }
    }
    if (!axis) return true;
    const double c = std::abs(axis->X() * sight.x + axis->Y() * sight.y + axis->Z() * sight.z) / sl;
    return c <= 0.1;
}

views::SectionParts sectionParts(const ModelSnapshot& model, views::Vec3 sight) {
    views::SectionParts parts;
    if (model.kind != ModelKind::Assembly || !model.assembly || model.shape.isNull()) return parts;
    std::vector<TopoDS_Shape> children;
    for (TopoDS_Iterator it(model.shape.occt()); it.More(); it.Next()) children.push_back(it.Value());
    if (children.size() != model.childOwner.size()) return parts;  // снимок без владельцев — одна деталь
    parts.perChild = true;
    parts.keepWhole.assign(children.size(), false);
    for (std::size_t i = 0; i < children.size(); ++i) {
        const assembly_v2::DefinitionId id = model.childOwner[i];
        if (!model.assembly->hasDefinition(id)) continue;
        const assembly_v2::Definition& def = model.assembly->definition(id);
        switch (sectionRule(def.properties, def.name)) {
            case SectionRule::Cut: break;
            case SectionRule::NeverCut: parts.keepWhole[i] = true; break;
            case SectionRule::WholeInLongitudinal: parts.keepWhole[i] = longitudinalSection(children[i], sight); break;
        }
    }
    return parts;
}

}  // namespace bendcore::drawing_v2::document
