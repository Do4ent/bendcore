#include <bendcore/kernel/xcaf_io.hpp>

#include "occt_failure.hpp"
#include "occt_fault_guard.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <map>
#include <sstream>
#include <utility>

#include <DESTEP_Parameters.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_InterfaceModel.hxx>
#include <NCollection_HSequence.hxx>
#include <NCollection_Sequence.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_ColorRGBA.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPConstruct_Tool.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Failure.hxx>
#include <StepBasic_Product.hxx>
#include <StepBasic_ProductDefinition.hxx>
#include <StepBasic_ProductDefinitionFormation.hxx>
#include <StepData_StepModel.hxx>
#include <StepRepr_NextAssemblyUsageOccurrence.hxx>
#include <TCollection_AsciiString.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TCollection_HAsciiString.hxx>
#include <TDF_Attribute.hxx>
#include <TDF_Label.hxx>
#include <TDF_Tool.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>
#include <TransferBRep.hxx>
#include <Transfer_Binder.hxx>
#include <Transfer_TransientProcess.hxx>
#include <UnitsMethods_LengthUnit.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_ColorType.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_LayerTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XSControl_TransferReader.hxx>
#include <XSControl_WorkSession.hxx>

namespace bendcore::kernel {

using detail::failureText;

namespace {

// ------------------------------------------------------------------ XCAF document guard

// Temporary XCAF document, closed on scope exit (the application keeps a list of open
// documents, so an unclosed one would leak).
class TempXcafDocument {
public:
    TempXcafDocument() : app_(XCAFApp_Application::GetApplication()) {
        app_->NewDocument("MDTV-XCAF", doc_);
        // Model space of bendcore is millimetres: the reader converts the file unit to the
        // document unit, and the writer takes the local unit of the model from it.
        XCAFDoc_DocumentTool::SetLengthUnit(doc_, 1.0, UnitsMethods_LengthUnit_Millimeter);
    }
    ~TempXcafDocument() {
        try {
            if (!doc_.IsNull() && doc_->IsOpened()) {
                app_->Close(doc_);
            }
        } catch (const Standard_Failure&) {
            // Close() failing leaves the document to the application; nothing to report.
        }
    }
    TempXcafDocument(const TempXcafDocument&) = delete;
    TempXcafDocument& operator=(const TempXcafDocument&) = delete;

    const Handle(TDocStd_Document)& doc() const { return doc_; }
    Handle(XCAFDoc_ShapeTool) shapes() const { return XCAFDoc_DocumentTool::ShapeTool(doc_->Main()); }
    Handle(XCAFDoc_ColorTool) colors() const { return XCAFDoc_DocumentTool::ColorTool(doc_->Main()); }
    Handle(XCAFDoc_LayerTool) layers() const { return XCAFDoc_DocumentTool::LayerTool(doc_->Main()); }

private:
    Handle(XCAFApp_Application) app_;
    Handle(TDocStd_Document) doc_;
};

// ------------------------------------------------------------------ conversions

std::string toUtf8(const TCollection_ExtendedString& text) {
    if (text.IsEmpty()) {
        return {};
    }
    std::string out(static_cast<std::size_t>(text.LengthOfCString()) + 1, '\0');
    char* buffer = out.data();
    const int written = text.ToUTF8CString(buffer);
    out.resize(static_cast<std::size_t>(std::max(written, 0)));
    return out;
}

TCollection_ExtendedString fromUtf8(const std::string& text) {
    return TCollection_ExtendedString(text.c_str(), /*isMultiByte=*/true);
}

std::string labelName(const TDF_Label& label) {
    Handle(TDataStd_Name) name;
    if (label.FindAttribute(TDataStd_Name::GetID(), name)) {
        return toUtf8(name->Get());
    }
    return {};
}

std::string labelEntry(const TDF_Label& label) {
    TCollection_AsciiString entry;
    TDF_Tool::Entry(label, entry);
    return entry.ToCString();
}

AssemblyColor fromOcct(const Quantity_ColorRGBA& rgba) {
    AssemblyColor c;
    rgba.GetRGB().Values(c.r, c.g, c.b, Quantity_TOC_sRGB);
    c.alpha = rgba.Alpha();
    return c;
}

Quantity_ColorRGBA toOcct(const AssemblyColor& c) {
    return Quantity_ColorRGBA(Quantity_Color(c.r, c.g, c.b, Quantity_TOC_sRGB), static_cast<float>(c.alpha));
}

// Surface colour of a label: XCAFDoc_ColorSurf, else XCAFDoc_ColorGen.
std::optional<AssemblyColor> surfaceColor(const Handle(XCAFDoc_ColorTool)& colors, const TDF_Label& label) {
    Quantity_ColorRGBA rgba;
    if (colors->GetColor(label, XCAFDoc_ColorSurf, rgba) || colors->GetColor(label, XCAFDoc_ColorGen, rgba)) {
        return fromOcct(rgba);
    }
    return std::nullopt;
}

std::vector<std::string> labelLayers(const Handle(XCAFDoc_LayerTool)& layers, const TDF_Label& label) {
    std::vector<std::string> out;
    Handle(NCollection_HSequence<TCollection_ExtendedString>) names;
    if (layers->GetLayers(label, names) && !names.IsNull()) {
        for (int i = 1; i <= names->Length(); ++i) {
            out.push_back(toUtf8(names->Value(i)));
        }
    }
    return out;
}

// Millimetres per unit for the names STEPControl_Reader::FileUnits reports: SI units are
// spelled "<prefix>metre", conversion-based units carry their NAME from the file.
std::optional<double> unitNameToMm(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    static const std::map<std::string, double> kUnits = {
        {"metre", 1000.0},      {"meter", 1000.0},          {"millimetre", 1.0},
        {"millimeter", 1.0},    {"centimetre", 10.0},       {"centimeter", 10.0},
        {"decimetre", 100.0},   {"kilometre", 1.0e6},       {"micrometre", 1.0e-3},
        {"nanometre", 1.0e-6},  {"inch", 25.4},             {"foot", 304.8},
        {"feet", 304.8},        {"yard", 914.4},            {"mile", 1609344.0},
        {"mil", 0.0254},
    };
    const auto it = kUnits.find(name);
    if (it == kUnits.end()) {
        return std::nullopt;
    }
    return it->second;
}

// ------------------------------------------------------------------ PRODUCT and NAUO attributes

// STEPCAFControl_Reader gives a label only one name (PRODUCT name, or its id when the name
// is empty; NAUO description, name or id). The other attributes are read from the STEP
// model directly and attached to the labels the reader created.
struct ProductAttributes {
    std::string name;
    std::string id;
    std::string description;
};

struct OccurrenceAttributes {
    std::string id;
    std::string name;
    std::string description;
    std::optional<std::string> referenceDesignator;
};

struct StepAttributes {
    std::map<std::string, ProductAttributes> products;        // by prototype label entry
    std::map<std::string, OccurrenceAttributes> occurrences;  // by component label entry
};

// STEP strings are decoded by the parser into UTF-8 unless the code page is NoConversion
// (then every byte is one character), as STEPCAFControl_Reader::convertName does.
std::string stepText(const Handle(TCollection_HAsciiString)& text, bool multiByte) {
    if (text.IsNull()) {
        return {};
    }
    return toUtf8(TCollection_ExtendedString(text->String(), multiByte));
}

Handle(TCollection_HAsciiString) stepString(const std::string& utf8) {
    return new TCollection_HAsciiString(utf8.c_str());
}

StepAttributes collectStepAttributes(STEPCAFControl_Reader& reader, const Handle(XCAFDoc_ShapeTool)& shapes) {
    StepAttributes out;
    const Handle(XSControl_WorkSession) ws = reader.ChangeReader().WS();
    const Handle(Interface_InterfaceModel) model = ws->Model();
    const Handle(Transfer_TransientProcess) tp = ws->TransferReader()->TransientProcess();
    if (model.IsNull() || tp.IsNull()) {
        return out;
    }
    const Handle(StepData_StepModel) stepModel = Handle(StepData_StepModel)::DownCast(model);
    const bool multiByte = stepModel.IsNull() || stepModel->SourceCodePage() != Resource_FormatType_NoConversion;
    const auto& shapeLabels = reader.GetShapeLabelMap();
    STEPConstruct_Tool tool(ws);
    for (int i = 1; i <= model->NbEntities(); ++i) {
        const Handle(Standard_Transient) entity = model->Value(i);
        if (const auto nauo = Handle(StepRepr_NextAssemblyUsageOccurrence)::DownCast(entity); !nauo.IsNull()) {
            const TDF_Label label = STEPCAFControl_Reader::FindInstance(nauo, shapes, tool, shapeLabels);
            if (label.IsNull()) {
                continue;
            }
            OccurrenceAttributes a;
            a.id = stepText(nauo->Id(), multiByte);
            a.name = stepText(nauo->Name(), multiByte);
            if (nauo->HasDescription()) {
                a.description = stepText(nauo->Description(), multiByte);
            }
            if (nauo->HasReferenceDesignator() && !nauo->ReferenceDesignator().IsNull()) {
                a.referenceDesignator = stepText(nauo->ReferenceDesignator(), multiByte);
            }
            out.occurrences[labelEntry(label)] = std::move(a);
            continue;
        }
        const auto pd = Handle(StepBasic_ProductDefinition)::DownCast(entity);
        if (pd.IsNull() || pd->Formation().IsNull() || pd->Formation()->OfProduct().IsNull()) {
            continue;
        }
        const Handle(Transfer_Binder) binder = tp->Find(pd);
        if (binder.IsNull() || !binder->HasResult()) {
            continue;
        }
        const TopoDS_Shape shape = TransferBRep::ShapeResult(tp, binder);
        if (shape.IsNull()) {
            continue;
        }
        TDF_Label label;
        if (shapeLabels.IsBound(shape)) {
            label = shapeLabels.Find(shape);
        }
        if (label.IsNull()) {
            shapes->Search(shape, label, true, true, false);
        }
        if (label.IsNull()) {
            continue;
        }
        const Handle(StepBasic_Product) product = pd->Formation()->OfProduct();
        ProductAttributes a;
        a.name = stepText(product->Name(), multiByte);
        a.id = stepText(product->Id(), multiByte);
        a.description = stepText(product->Description(), multiByte);
        out.products.emplace(labelEntry(label), std::move(a));
    }
    return out;
}

// The writer names labels with these markers; after the transfer the PRODUCT and NAUO
// entities carrying a marker get the real name, id, description and reference designator.
// Markers make the match exact even when several definitions share a name.
std::string definitionMarker(std::size_t definition) {
    return "bendcore-xcaf-definition-" + std::to_string(definition);
}

std::string componentMarker(std::size_t definition, std::size_t component) {
    return "bendcore-xcaf-component-" + std::to_string(definition) + "-" + std::to_string(component);
}

// ------------------------------------------------------------------ structure checks

bool colorInRange(const AssemblyColor& c) {
    for (double v : {c.r, c.g, c.b, c.alpha}) {
        if (!std::isfinite(v) || v < 0.0 || v > 1.0) {
            return false;
        }
    }
    return true;
}

std::string defLabel(const AssemblyDocument& doc, std::size_t i) {
    return "definition #" + std::to_string(i) + " '" + doc.definitions[i].name + "'";
}

// Face counts of the part definitions (0 for assemblies), computed during the check.
Result<std::vector<std::size_t>> checkStructure(const AssemblyDocument& doc) {
    using R = Result<std::vector<std::size_t>>;
    const std::size_t n = doc.definitions.size();
    if (n == 0) {
        return R::failure(ErrorCode::InvalidInput, "assembly document has no definitions");
    }
    if (doc.roots.empty()) {
        return R::failure(ErrorCode::InvalidInput, "assembly document has no roots");
    }
    std::vector<std::size_t> faceCounts(n, 0);
    std::vector<int> uses(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        const AssemblyDefinition& d = doc.definitions[i];
        if (d.color && !colorInRange(*d.color)) {
            return R::failure(ErrorCode::InvalidInput, defLabel(doc, i) + ": colour outside [0, 1]");
        }
        if (d.kind == DefinitionKind::Part) {
            if (d.shape.isNull()) {
                return R::failure(ErrorCode::InvalidInput, defLabel(doc, i) + ": part shape is null");
            }
            if (!d.components.empty()) {
                return R::failure(ErrorCode::InvalidInput, defLabel(doc, i) + ": a part cannot have components");
            }
            auto index = TopologyIndex::build(d.shape);
            if (!index) {
                return R::failure(ErrorCode::InvalidInput, defLabel(doc, i) + ": " + index.error().message);
            }
            faceCounts[i] = index.value().faces().size();
            std::optional<std::uint32_t> previous;
            for (const FaceColor& fc : d.faceColors) {
                if (fc.faceIndex >= faceCounts[i]) {
                    return R::failure(ErrorCode::InvalidInput,
                                      defLabel(doc, i) + ": face index " + std::to_string(fc.faceIndex) +
                                          " out of range (" + std::to_string(faceCounts[i]) + " faces)");
                }
                if (previous && fc.faceIndex <= *previous) {
                    return R::failure(ErrorCode::InvalidInput,
                                      defLabel(doc, i) + ": face colours must have ascending unique indices");
                }
                if (!colorInRange(fc.color)) {
                    return R::failure(ErrorCode::InvalidInput,
                                      defLabel(doc, i) + ": colour of face " + std::to_string(fc.faceIndex) +
                                          " outside [0, 1]");
                }
                previous = fc.faceIndex;
            }
        } else {
            if (!d.shape.isNull()) {
                return R::failure(ErrorCode::InvalidInput,
                                  defLabel(doc, i) + ": an assembly takes its shape from its components");
            }
            if (d.components.empty()) {
                return R::failure(ErrorCode::InvalidInput, defLabel(doc, i) + ": assembly has no components");
            }
            if (!d.faceColors.empty()) {
                return R::failure(ErrorCode::InvalidInput, defLabel(doc, i) + ": face colours belong to parts");
            }
            for (std::size_t k = 0; k < d.components.size(); ++k) {
                const AssemblyComponent& c = d.components[k];
                if (c.definition >= n) {
                    return R::failure(ErrorCode::InvalidInput,
                                      defLabel(doc, i) + ", component #" + std::to_string(k) +
                                          ": definition index " + std::to_string(c.definition) + " out of range");
                }
                if (c.color && !colorInRange(*c.color)) {
                    return R::failure(ErrorCode::InvalidInput,
                                      defLabel(doc, i) + ", component #" + std::to_string(k) +
                                          ": colour outside [0, 1]");
                }
                ++uses[c.definition];
            }
        }
    }
    std::vector<bool> isRoot(n, false);
    for (std::size_t r : doc.roots) {
        if (r >= n) {
            return R::failure(ErrorCode::InvalidInput, "root index " + std::to_string(r) + " out of range");
        }
        if (isRoot[r]) {
            return R::failure(ErrorCode::InvalidInput, defLabel(doc, r) + " is listed twice as a root");
        }
        if (uses[r] > 0) {
            return R::failure(ErrorCode::InvalidInput, defLabel(doc, r) + " is a root but also used by a component");
        }
        isRoot[r] = true;
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (!isRoot[i] && uses[i] == 0) {
            return R::failure(ErrorCode::InvalidInput, defLabel(doc, i) + " is neither a root nor used");
        }
    }
    // Cycle check over every definition (0 = unvisited, 1 = on stack, 2 = done).
    std::vector<int> state(n, 0);
    for (std::size_t start = 0; start < n; ++start) {
        if (state[start] != 0) {
            continue;
        }
        std::vector<std::pair<std::size_t, std::size_t>> stack{{start, 0}};
        state[start] = 1;
        while (!stack.empty()) {
            auto& [def, next] = stack.back();
            const auto& comps = doc.definitions[def].components;
            if (next < comps.size()) {
                const std::size_t child = comps[next++].definition;
                if (state[child] == 1) {
                    return R::failure(ErrorCode::InvalidInput,
                                      "assembly cycle through " + defLabel(doc, child));
                }
                if (state[child] == 0) {
                    state[child] = 1;
                    stack.emplace_back(child, 0);
                }
            } else {
                state[def] = 2;
                stack.pop_back();
            }
        }
    }
    return faceCounts;
}

// ------------------------------------------------------------------ XCAF -> structure

class XcafToDocument {
public:
    XcafToDocument(const TempXcafDocument& xcaf, const StepAttributes& attributes, AssemblyDocument& out)
        : shapes_(xcaf.shapes()), colors_(xcaf.colors()), layers_(xcaf.layers()), attributes_(attributes), out_(out) {}

    Result<std::size_t> convert(const TDF_Label& label) {
        using R = Result<std::size_t>;
        const std::string entry = labelEntry(label);
        if (const auto it = done_.find(entry); it != done_.end()) {
            return it->second;
        }
        if (onStack_.count(entry) != 0) {
            return R::failure(ErrorCode::OperationFailed, "XCAF assembly cycle at label " + entry);
        }
        onStack_[entry] = true;

        AssemblyDefinition def;
        def.name = labelName(label);
        if (const auto it = attributes_.products.find(entry); it != attributes_.products.end()) {
            if (!it->second.name.empty()) {
                def.name = it->second.name;
            }
            def.productId = it->second.id;
            def.description = it->second.description;
        } else {
            def.productId = std::string();
        }
        def.color = surfaceColor(colors_, label);
        def.layers = labelLayers(layers_, label);
        if (XCAFDoc_ShapeTool::IsAssembly(label)) {
            def.kind = DefinitionKind::Assembly;
            NCollection_Sequence<TDF_Label> comps;
            XCAFDoc_ShapeTool::GetComponents(label, comps, false);
            for (int i = 1; i <= comps.Length(); ++i) {
                const TDF_Label& compLabel = comps.Value(i);
                TDF_Label referred;
                if (!XCAFDoc_ShapeTool::GetReferredShape(compLabel, referred)) {
                    return R::failure(ErrorCode::OperationFailed,
                                      "component " + labelEntry(compLabel) + " of '" + def.name +
                                          "' refers to no shape");
                }
                auto child = convert(referred);
                if (!child) {
                    return child;
                }
                AssemblyComponent c;
                c.name = labelName(compLabel);
                if (const auto it = attributes_.occurrences.find(labelEntry(compLabel));
                    it != attributes_.occurrences.end()) {
                    // NAUO name first; the reader prefers the description, which is a separate
                    // attribute here.
                    if (!it->second.name.empty()) {
                        c.name = it->second.name;
                    }
                    c.instanceId = it->second.id;
                    c.description = it->second.description;
                    c.referenceDesignator = it->second.referenceDesignator;
                }
                c.definition = child.value();
                c.location = XCAFDoc_ShapeTool::GetLocation(compLabel);
                c.color = surfaceColor(colors_, compLabel);
                c.layers = labelLayers(layers_, compLabel);
                NCollection_Sequence<Handle(TDF_Attribute)> shuo;
                if (XCAFDoc_ShapeTool::GetAllComponentSHUO(compLabel, shuo) && shuo.Length() > 0) {
                    out_.warnings.push_back("component '" + c.name + "' of '" + def.name + "': " +
                                            std::to_string(shuo.Length()) +
                                            " path-specific (SHUO) style(s) ignored");
                }
                def.components.push_back(std::move(c));
            }
        } else {
            def.kind = DefinitionKind::Part;
            const TopoDS_Shape shape = XCAFDoc_ShapeTool::GetShape(label);
            if (shape.IsNull()) {
                return R::failure(ErrorCode::OperationFailed, "part '" + def.name + "' has no shape");
            }
            def.shape = Shape(shape);
            auto faces = faceColorsOf(label, def.shape);
            if (!faces) {
                return Result<std::size_t>(faces.error());
            }
            def.faceColors = std::move(faces).value();
        }
        const std::size_t index = out_.definitions.size();
        out_.definitions.push_back(std::move(def));
        onStack_.erase(entry);
        done_[entry] = index;
        return index;
    }

private:
    // Colours of sub-shape labels: a face label gives that face; a solid, shell or compound
    // label gives all its faces, unless a face has its own colour.
    Result<std::vector<FaceColor>> faceColorsOf(const TDF_Label& label, const Shape& part) {
        using R = Result<std::vector<FaceColor>>;
        NCollection_Sequence<TDF_Label> subs;
        if (!XCAFDoc_ShapeTool::GetSubShapes(label, subs) || subs.Length() == 0) {
            return std::vector<FaceColor>{};
        }
        auto index = TopologyIndex::build(part);
        if (!index) {
            return R(index.error());
        }
        std::map<std::uint32_t, AssemblyColor> own;
        std::map<std::uint32_t, AssemblyColor> inherited;
        for (int i = 1; i <= subs.Length(); ++i) {
            const std::optional<AssemblyColor> color = surfaceColor(colors_, subs.Value(i));
            if (!color) {
                continue;
            }
            const TopoDS_Shape sub = XCAFDoc_ShapeTool::GetShape(subs.Value(i));
            if (sub.IsNull()) {
                continue;
            }
            if (sub.ShapeType() == TopAbs_FACE) {
                if (const auto fi = index.value().faceIndex(sub)) {
                    own[*fi] = *color;
                }
                continue;
            }
            for (TopExp_Explorer ex(sub, TopAbs_FACE); ex.More(); ex.Next()) {
                if (const auto fi = index.value().faceIndex(ex.Current())) {
                    inherited[*fi] = *color;
                }
            }
        }
        for (const auto& [fi, color] : inherited) {
            own.emplace(fi, color);  // keeps the face's own colour when present
        }
        std::vector<FaceColor> out;
        out.reserve(own.size());
        for (const auto& [fi, color] : own) {
            out.push_back({fi, color});
        }
        return out;
    }

    Handle(XCAFDoc_ShapeTool) shapes_;
    Handle(XCAFDoc_ColorTool) colors_;
    Handle(XCAFDoc_LayerTool) layers_;
    const StepAttributes& attributes_;
    AssemblyDocument& out_;
    std::map<std::string, std::size_t> done_;
    std::map<std::string, bool> onStack_;
};

// ------------------------------------------------------------------ structure -> XCAF

class DocumentToXcaf {
public:
    DocumentToXcaf(const AssemblyDocument& doc, const TempXcafDocument& xcaf)
        : doc_(doc),
          shapes_(xcaf.shapes()),
          colors_(xcaf.colors()),
          layers_(xcaf.layers()),
          labels_(doc.definitions.size()) {}

    // Creates the definitions reachable from the roots, children before parents, the roots
    // in the order of AssemblyDocument::roots (free shapes keep that order in the file).
    Result<bool> build() {
        for (std::size_t r : doc_.roots) {
            auto made = make(r);
            if (!made) {
                return made;
            }
        }
        shapes_->UpdateAssemblies();
        return true;
    }

private:
    Result<bool> make(std::size_t i) {
        using R = Result<bool>;
        if (!labels_[i].IsNull()) {
            return true;
        }
        const AssemblyDefinition& d = doc_.definitions[i];
        TDF_Label label;
        if (d.kind == DefinitionKind::Part) {
            label = shapes_->AddShape(d.shape.occt(), /*makeAssembly=*/false);
            if (label.IsNull()) {
                return R::failure(ErrorCode::OperationFailed, defLabel(doc_, i) + ": XCAF rejected the shape");
            }
            if (!d.faceColors.empty()) {
                auto index = TopologyIndex::build(d.shape);
                if (!index) {
                    return R(index.error());
                }
                for (const FaceColor& fc : d.faceColors) {
                    const TDF_Label sub = shapes_->AddSubShape(label, index.value().faces()[fc.faceIndex].face);
                    if (sub.IsNull()) {
                        return R::failure(ErrorCode::OperationFailed,
                                          defLabel(doc_, i) + ": XCAF rejected face " + std::to_string(fc.faceIndex));
                    }
                    colors_->SetColor(sub, toOcct(fc.color), XCAFDoc_ColorSurf);
                }
            }
        } else {
            for (const AssemblyComponent& c : d.components) {
                auto child = make(c.definition);
                if (!child) {
                    return child;
                }
            }
            label = shapes_->NewShape();
            for (const AssemblyComponent& c : d.components) {
                const TDF_Label comp = shapes_->AddComponent(label, labels_[c.definition], c.location);
                if (comp.IsNull()) {
                    return R::failure(ErrorCode::OperationFailed,
                                      defLabel(doc_, i) + ": XCAF rejected component '" + c.name + "'");
                }
                const auto k = static_cast<std::size_t>(&c - d.components.data());
                TDataStd_Name::Set(comp, fromUtf8(componentMarker(i, k)));
                if (c.color) {
                    colors_->SetColor(comp, toOcct(*c.color), XCAFDoc_ColorSurf);
                }
            }
        }
        TDataStd_Name::Set(label, fromUtf8(definitionMarker(i)));
        if (d.color) {
            colors_->SetColor(label, toOcct(*d.color), XCAFDoc_ColorSurf);
        }
        for (const std::string& layer : d.layers) {
            layers_->SetLayer(label, fromUtf8(layer));
        }
        labels_[i] = label;
        return true;
    }

    const AssemblyDocument& doc_;
    Handle(XCAFDoc_ShapeTool) shapes_;
    Handle(XCAFDoc_ColorTool) colors_;
    Handle(XCAFDoc_LayerTool) layers_;
    std::vector<TDF_Label> labels_;
};

// Replaces the markers of DocumentToXcaf in the transferred model by the attributes of the
// document. Every definition must have produced a PRODUCT and every component a NAUO.
Result<bool> applyStepAttributes(const Handle(Interface_InterfaceModel)& model, const AssemblyDocument& doc) {
    using R = Result<bool>;
    if (model.IsNull()) {
        return R::failure(ErrorCode::OperationFailed, "STEP writer has no model after the transfer");
    }
    std::map<std::string, std::size_t> definitions;
    std::map<std::string, std::pair<std::size_t, std::size_t>> components;
    for (std::size_t i = 0; i < doc.definitions.size(); ++i) {
        definitions[definitionMarker(i)] = i;
        for (std::size_t k = 0; k < doc.definitions[i].components.size(); ++k) {
            components[componentMarker(i, k)] = {i, k};
        }
    }
    std::vector<bool> productSeen(doc.definitions.size(), false);
    std::map<std::pair<std::size_t, std::size_t>, int> nauoSeen;
    for (int e = 1; e <= model->NbEntities(); ++e) {
        const Handle(Standard_Transient) entity = model->Value(e);
        if (const auto nauo = Handle(StepRepr_NextAssemblyUsageOccurrence)::DownCast(entity); !nauo.IsNull()) {
            if (nauo->Name().IsNull()) {
                continue;
            }
            const auto it = components.find(nauo->Name()->String().ToCString());
            if (it == components.end()) {
                continue;
            }
            const AssemblyComponent& c = doc.definitions[it->second.first].components[it->second.second];
            const Handle(TCollection_HAsciiString) id = c.instanceId.empty() ? nauo->Id() : stepString(c.instanceId);
            nauo->Init(id, stepString(c.name), true, stepString(c.description), nauo->RelatingProductDefinitionAP242(),
                       nauo->RelatedProductDefinitionAP242(), c.referenceDesignator.has_value(),
                       c.referenceDesignator ? stepString(*c.referenceDesignator) : Handle(TCollection_HAsciiString)());
            ++nauoSeen[it->second];
            continue;
        }
        const auto product = Handle(StepBasic_Product)::DownCast(entity);
        if (product.IsNull() || product->Name().IsNull()) {
            continue;
        }
        const auto it = definitions.find(product->Name()->String().ToCString());
        if (it == definitions.end()) {
            continue;
        }
        const AssemblyDefinition& d = doc.definitions[it->second];
        product->SetName(stepString(d.name));
        product->SetId(stepString(d.productId.value_or(d.name)));
        product->SetDescription(stepString(d.description));
        productSeen[it->second] = true;
    }
    for (std::size_t i = 0; i < doc.definitions.size(); ++i) {
        if (!productSeen[i]) {
            return R::failure(ErrorCode::OperationFailed, defLabel(doc, i) + ": no PRODUCT was written for it");
        }
        for (std::size_t k = 0; k < doc.definitions[i].components.size(); ++k) {
            const auto it = nauoSeen.find({i, k});
            if (it == nauoSeen.end() || it->second != 1) {
                return R::failure(ErrorCode::OperationFailed,
                                  defLabel(doc, i) + ", component #" + std::to_string(k) +
                                      ": expected one NEXT_ASSEMBLY_USAGE_OCCURRENCE, found " +
                                      std::to_string(it == nauoSeen.end() ? 0 : it->second));
            }
        }
    }
    return true;
}

UnitsMethods_LengthUnit toOcctUnit(StepLengthUnit unit) {
    switch (unit) {
        case StepLengthUnit::Millimetre: return UnitsMethods_LengthUnit_Millimeter;
        case StepLengthUnit::Metre: return UnitsMethods_LengthUnit_Meter;
        case StepLengthUnit::Inch: return UnitsMethods_LengthUnit_Inch;
    }
    return UnitsMethods_LengthUnit_Millimeter;
}

DESTEP_Parameters::WriteMode_StepSchema toOcctSchema(StepSchema schema) {
    switch (schema) {
        case StepSchema::AP214: return DESTEP_Parameters::WriteMode_StepSchema_AP214IS;
        case StepSchema::AP242: return DESTEP_Parameters::WriteMode_StepSchema_AP242DIS;
    }
    return DESTEP_Parameters::WriteMode_StepSchema_AP214IS;
}

}  // namespace

// ====================================================================== read

Result<AssemblyDocument> readStepAssembly(std::string_view data) {
    using R = Result<AssemblyDocument>;
    if (data.empty()) {
        return R::failure(ErrorCode::ParseError, "STEP input is empty");
    }
    try {
        // Under the fault guard: corrupted entity references can leave null handles that OCCT
        // dereferences without a check (docs/kernel/OCCT_BUILD.md, раздел 6).
        return detail::callConvertingFaults<R>([&]() -> R {
            TempXcafDocument xcaf;
            std::istringstream in{std::string(data)};
            STEPCAFControl_Reader reader;
            reader.SetNameMode(true);
            reader.SetColorMode(true);
            reader.SetLayerMode(true);
            const IFSelect_ReturnStatus status = reader.ReadStream("memory.step", in);
            if (status != IFSelect_RetDone) {
                return R::failure(ErrorCode::ParseError,
                                  "STEP input could not be parsed (IFSelect status " + std::to_string(status) + ")");
            }
            if (reader.NbRootsForTransfer() <= 0) {
                return R::failure(ErrorCode::ParseError, "STEP input has no transferable shape roots");
            }
            if (!reader.Transfer(xcaf.doc())) {
                return R::failure(ErrorCode::OperationFailed, "STEP roots could not be transferred to XCAF");
            }

            AssemblyDocument out;
            NCollection_Sequence<TCollection_AsciiString> lengthNames;
            NCollection_Sequence<TCollection_AsciiString> angleNames;
            NCollection_Sequence<TCollection_AsciiString> solidAngleNames;
            reader.ChangeReader().FileUnits(lengthNames, angleNames, solidAngleNames);
            for (int i = 1; i <= lengthNames.Length(); ++i) {
                out.fileLengthUnits.emplace_back(lengthNames.Value(i).ToCString());
            }
            if (out.fileLengthUnits.size() == 1) {
                out.fileUnitMm = unitNameToMm(out.fileLengthUnits.front());
            }

            NCollection_Sequence<TDF_Label> free;
            xcaf.shapes()->GetFreeShapes(free);
            if (free.Length() == 0) {
                return R::failure(ErrorCode::OperationFailed, "STEP transfer produced no shapes");
            }
            const StepAttributes attributes = collectStepAttributes(reader, xcaf.shapes());
            XcafToDocument converter(xcaf, attributes, out);
            for (int i = 1; i <= free.Length(); ++i) {
                auto root = converter.convert(free.Value(i));
                if (!root) {
                    return R(root.error());
                }
                out.roots.push_back(root.value());
            }
            return out;
        });
    } catch (const Standard_Failure& failure) {
        return R::failure(ErrorCode::ParseError, "STEP input is malformed: " + failureText(failure));
    } catch (const std::exception& e) {
        return R::failure(ErrorCode::ParseError, std::string("STEP input is malformed: ") + e.what());
    }
}

// ====================================================================== write

Result<std::string> writeStepAssembly(const AssemblyDocument& document, const AssemblyWriteOptions& options) {
    using R = Result<std::string>;
    try {
        auto checked = checkStructure(document);
        if (!checked) {
            return R(checked.error());
        }
        // STEPCAFControl_Writer writes PRESENTATION_LAYER_ASSIGNMENT only for part shapes and
        // their sub-shapes; layers of assemblies and components would be dropped silently.
        for (std::size_t i = 0; i < document.definitions.size(); ++i) {
            const AssemblyDefinition& d = document.definitions[i];
            if (d.kind == DefinitionKind::Assembly && !d.layers.empty()) {
                return R::failure(ErrorCode::Unsupported,
                                  defLabel(document, i) + ": STEP layers of an assembly cannot be written");
            }
            for (const AssemblyComponent& c : d.components) {
                if (!c.layers.empty()) {
                    return R::failure(ErrorCode::Unsupported, defLabel(document, i) + ", component '" + c.name +
                                                                  "': STEP layers of a component cannot be written");
                }
            }
        }
        TempXcafDocument xcaf;
        DocumentToXcaf builder(document, xcaf);
        auto built = builder.build();
        if (!built) {
            return R(built.error());
        }

        STEPCAFControl_Writer writer;
        writer.SetNameMode(true);
        writer.SetColorMode(true);
        writer.SetLayerMode(true);
        DESTEP_Parameters params;
        params.InitFromStatic();
        params.WriteSchema = toOcctSchema(options.schema);
        params.WriteUnit = toOcctUnit(options.fileUnit);
        if (!writer.Transfer(xcaf.doc(), params, STEPControl_AsIs)) {
            return R::failure(ErrorCode::OperationFailed, "STEPCAFControl_Writer could not transfer the document");
        }
        if (auto applied = applyStepAttributes(writer.ChangeWriter().WS()->Model(), document); !applied) {
            return R(applied.error());
        }
        std::ostringstream out;
        const IFSelect_ReturnStatus status = writer.WriteStream(out);
        if (status != IFSelect_RetDone || !out) {
            return R::failure(ErrorCode::IoError,
                              "STEP write failed (IFSelect status " + std::to_string(status) + ")");
        }
        return out.str();
    } catch (const Standard_Failure& failure) {
        return R::failure(ErrorCode::OperationFailed, failureText(failure));
    } catch (const std::exception& e) {
        return R::failure(ErrorCode::OperationFailed, e.what());
    }
}

// ====================================================================== flatten

Result<std::vector<PlacedPart>> flattenAssembly(const AssemblyDocument& document) {
    using R = Result<std::vector<PlacedPart>>;
    try {
        auto checked = checkStructure(document);
        if (!checked) {
            return R(checked.error());
        }
        std::vector<PlacedPart> out;
        struct Frame {
            std::size_t definition;
            std::vector<std::size_t> path;
            std::string name;
            TopLoc_Location location;
            std::optional<AssemblyColor> color;
        };
        for (std::size_t r = 0; r < document.roots.size(); ++r) {
            const std::size_t rootDef = document.roots[r];
            std::vector<Frame> stack;
            stack.push_back({rootDef, {}, document.definitions[rootDef].name, TopLoc_Location(),
                             document.definitions[rootDef].color});
            while (!stack.empty()) {
                Frame f = std::move(stack.back());
                stack.pop_back();
                const AssemblyDefinition& d = document.definitions[f.definition];
                if (d.kind == DefinitionKind::Part) {
                    PlacedPart p;
                    p.root = r;
                    p.componentPath = std::move(f.path);
                    p.definition = f.definition;
                    p.path = std::move(f.name);
                    p.location = f.location;
                    p.shape = Shape(d.shape.occt().Moved(f.location));
                    p.color = f.color;
                    p.faceColors = d.faceColors;
                    out.push_back(std::move(p));
                    continue;
                }
                // Reverse push keeps the output in component order (depth first).
                for (std::size_t k = d.components.size(); k-- > 0;) {
                    const AssemblyComponent& c = d.components[k];
                    const AssemblyDefinition& child = document.definitions[c.definition];
                    Frame next;
                    next.definition = c.definition;
                    next.path = f.path;
                    next.path.push_back(k);
                    next.name = f.name + "/" + (c.name.empty() ? child.name : c.name);
                    next.location = f.location * c.location;
                    next.color = f.color;
                    if (child.color) {
                        next.color = child.color;
                    }
                    if (c.color) {
                        next.color = c.color;
                    }
                    stack.push_back(std::move(next));
                }
            }
        }
        return out;
    } catch (const Standard_Failure& failure) {
        return R::failure(ErrorCode::OperationFailed, failureText(failure));
    }
}

}  // namespace bendcore::kernel
