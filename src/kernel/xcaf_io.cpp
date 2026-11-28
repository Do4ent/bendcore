#include <bendcore/kernel/xcaf_io.hpp>
#include <bendcore/kernel/step_io.hpp>

#include "occt_failure.hpp"
#include "occt_fault_guard.hpp"
#include "step_preflight.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <utility>

#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <DESTEP_Parameters.hxx>
#include <IGESCAFControl_Reader.hxx>
#include <IGESCAFControl_Writer.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESData_GlobalSection.hxx>
#include <IGESData_IGESModel.hxx>
#include <Interface_Static.hxx>
#include <ShapeFix_Solid.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Shell.hxx>
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
          labels_(doc.definitions.size()),
          componentLabels_(doc.definitions.size()) {}

    // Replaces the markers by the names of the definitions and components (writers that take
    // the names from the labels and need no attribute pass afterwards: IGES).
    void useDocumentNames() {
        for (std::size_t i = 0; i < labels_.size(); ++i) {
            if (labels_[i].IsNull()) {
                continue;
            }
            TDataStd_Name::Set(labels_[i], fromUtf8(doc_.definitions[i].name));
            for (std::size_t k = 0; k < componentLabels_[i].size(); ++k) {
                TDataStd_Name::Set(componentLabels_[i][k], fromUtf8(doc_.definitions[i].components[k].name));
            }
        }
    }

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
                componentLabels_[i].push_back(comp);
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
    std::vector<std::vector<TDF_Label>> componentLabels_;
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
    if (data.size() > kStepDefaultMaxInputBytes) {
        return R::failure(ErrorCode::InvalidInput, *detail::preflightStep(data, kStepDefaultMaxInputBytes));
    }
    if (auto invalid = detail::findInvalidStepCharacter(data)) {
        return R::failure(ErrorCode::ParseError, std::move(*invalid));
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
        return R::failure(detail::faultOr(failure, ErrorCode::ParseError), "STEP input is malformed: " + failureText(failure));
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

// ====================================================================== IGES

namespace {

// Temporary file holding the IGES text for the reader (OCCT reads IGES only from a file).
class TempIgesFile {
public:
    explicit TempIgesFile(std::string_view data) {
        static std::atomic<unsigned long> counter{0};
        std::error_code ec;
        const std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
        if (ec) {
            error_ = "no temporary directory: " + ec.message();
            return;
        }
        std::random_device rd;
        for (int attempt = 0; attempt < 8 && path_.empty(); ++attempt) {
            const std::filesystem::path candidate =
                dir / ("bendcore-iges-" + std::to_string(rd()) + "-" + std::to_string(++counter) + ".igs");
            if (std::filesystem::exists(candidate, ec)) {
                continue;
            }
            std::ofstream out(candidate, std::ios::binary | std::ios::trunc);
            if (!out) {
                error_ = "cannot create " + candidate.string();
                continue;
            }
            out.write(data.data(), static_cast<std::streamsize>(data.size()));
            out.close();
            if (!out) {
                error_ = "cannot write " + candidate.string();
                std::filesystem::remove(candidate, ec);
                continue;
            }
            path_ = candidate.string();
        }
    }
    ~TempIgesFile() {
        if (!path_.empty()) {
            std::error_code ec;
            std::filesystem::remove(path_, ec);
        }
    }
    TempIgesFile(const TempIgesFile&) = delete;
    TempIgesFile& operator=(const TempIgesFile&) = delete;

    const std::string& path() const { return path_; }
    const std::string& error() const { return error_; }

private:
    std::string path_;
    std::string error_;
};

// Millimetres per unit of the IGES global section unit flag (IGES 5.3, global parameter 14).
std::optional<double> igesUnitFlagToMm(int flag) {
    switch (flag) {
        case 1: return 25.4;        // inch
        case 2: return 1.0;         // millimetre
        case 4: return 304.8;       // foot
        case 5: return 1609344.0;   // mile
        case 6: return 1000.0;      // metre
        case 7: return 1.0e6;       // kilometre
        case 8: return 0.0254;      // mil
        case 9: return 1.0e-3;      // micron
        case 10: return 10.0;       // centimetre
        case 11: return 2.54e-5;    // microinch
        default: return std::nullopt;
    }
}

const char* igesUnitName(StepLengthUnit unit) {
    switch (unit) {
        case StepLengthUnit::Metre: return "M";
        case StepLengthUnit::Inch: return "INCH";
        case StepLengthUnit::Millimetre: break;
    }
    return "MM";
}

bool hasSolid(const TopoDS_Shape& shape) { return TopExp_Explorer(shape, TopAbs_SOLID).More(); }

// Sews a part that arrived as loose faces; closed shells become solids. Face colours follow
// the faces through the sewing history. Returns false (shape unchanged) when sewing gives a
// shell that is not closed.
bool sewIntoSolids(AssemblyDefinition& def, double tolerance, std::vector<std::string>& warnings) {
    const TopoDS_Shape input = def.shape.occt();
    if (!TopExp_Explorer(input, TopAbs_FACE).More()) {
        return false;
    }
    BRepBuilderAPI_Sewing sewing(tolerance);
    sewing.Add(input);
    sewing.Perform();
    const TopoDS_Shape sewn = sewing.SewedShape();
    if (sewn.IsNull()) {
        return false;
    }
    BRep_Builder builder;
    TopoDS_Compound solids;
    builder.MakeCompound(solids);
    int closed = 0;
    int open = 0;
    for (TopExp_Explorer ex(sewn, TopAbs_SHELL); ex.More(); ex.Next()) {
        const TopoDS_Shell shell = TopoDS::Shell(ex.Current());
        if (!BRep_Tool::IsClosed(shell)) {
            ++open;
            continue;
        }
        BRepBuilderAPI_MakeSolid makeSolid(shell);
        if (!makeSolid.IsDone()) {
            ++open;
            continue;
        }
        ShapeFix_Solid fix(makeSolid.Solid());
        fix.Perform();  // orients the shell outward
        builder.Add(solids, fix.Solid());
        ++closed;
    }
    if (closed == 0 || open != 0) {
        warnings.push_back("part '" + def.name + "': sewing gave " + std::to_string(closed + open) + " shell(s), " +
                           std::to_string(open) + " not closed; kept as faces without a solid");
        return false;
    }
    TopoDS_Shape result = solids;
    if (closed == 1) {
        TopoDS_Iterator it(solids);
        result = it.Value();
    }
    std::vector<FaceColor> colors;
    if (!def.faceColors.empty()) {
        auto before = TopologyIndex::build(def.shape);
        auto after = TopologyIndex::build(Shape(result));
        if (before && after) {
            for (const FaceColor& fc : def.faceColors) {
                const TopoDS_Shape oldFace = before.value().faces()[fc.faceIndex].face;
                const TopoDS_Shape newFace =
                    sewing.IsModifiedSubShape(oldFace) ? sewing.ModifiedSubShape(oldFace) : oldFace;
                for (TopExp_Explorer f(newFace, TopAbs_FACE); f.More(); f.Next()) {
                    if (const auto idx = after.value().faceIndex(f.Current())) {
                        colors.push_back({*idx, fc.color});
                    }
                }
            }
            std::sort(colors.begin(), colors.end(),
                      [](const FaceColor& a, const FaceColor& b) { return a.faceIndex < b.faceIndex; });
            colors.erase(std::unique(colors.begin(), colors.end(),
                                     [](const FaceColor& a, const FaceColor& b) { return a.faceIndex == b.faceIndex; }),
                         colors.end());
        }
    }
    warnings.push_back("part '" + def.name + "': faces without a solid were sewn into " + std::to_string(closed) +
                       " solid(s)");
    def.shape = Shape(result);
    def.faceColors = std::move(colors);
    return true;
}

// A root part of several solids becomes an assembly of one part per solid (IGES writes the
// bodies of an assembly as separate solid entities under one root). Names come from the XCAF
// sub-shape labels of the solids, colours from the faces: a solid whose faces share one colour
// gets it as the part colour.
void splitSolidsIntoParts(AssemblyDocument& out, std::size_t index, const TDF_Label& label) {
    std::vector<TopoDS_Shape> bodies;
    for (TopExp_Explorer ex(out.definitions[index].shape.occt(), TopAbs_SOLID); ex.More(); ex.Next()) {
        bodies.push_back(ex.Current());
    }
    if (bodies.size() < 2) {
        return;
    }
    std::map<std::uint32_t, AssemblyColor> faceColor;
    for (const FaceColor& fc : out.definitions[index].faceColors) {
        faceColor[fc.faceIndex] = fc.color;
    }
    auto whole = TopologyIndex::build(out.definitions[index].shape);
    if (!whole) {
        return;
    }
    NCollection_Sequence<TDF_Label> subs;
    XCAFDoc_ShapeTool::GetSubShapes(label, subs);
    const std::string baseName = out.definitions[index].name;
    std::vector<AssemblyComponent> components;
    for (std::size_t k = 0; k < bodies.size(); ++k) {
        AssemblyDefinition part;
        part.kind = DefinitionKind::Part;
        part.name = baseName + "_" + std::to_string(k + 1);
        for (int i = 1; i <= subs.Length(); ++i) {
            if (XCAFDoc_ShapeTool::GetShape(subs.Value(i)).IsSame(bodies[k])) {
                const std::string name = labelName(subs.Value(i));
                if (!name.empty()) {
                    part.name = name;
                }
            }
        }
        part.shape = Shape(bodies[k]);
        part.productId = std::string();
        auto local = TopologyIndex::build(part.shape);
        if (local) {
            for (std::uint32_t f = 0; f < local.value().faces().size(); ++f) {
                const auto wholeIndex = whole.value().faceIndex(local.value().faces()[f].face);
                if (!wholeIndex) {
                    continue;
                }
                if (const auto it = faceColor.find(*wholeIndex); it != faceColor.end()) {
                    part.faceColors.push_back({f, it->second});
                }
            }
        }
        const bool uniform =
            !part.faceColors.empty() && local && part.faceColors.size() == local.value().faces().size() &&
            std::all_of(part.faceColors.begin(), part.faceColors.end(), [&](const FaceColor& fc) {
                const AssemblyColor& c0 = part.faceColors.front().color;
                return fc.color.r == c0.r && fc.color.g == c0.g && fc.color.b == c0.b && fc.color.alpha == c0.alpha;
            });
        if (uniform) {
            part.color = part.faceColors.front().color;
            part.faceColors.clear();
        } else if (!part.color) {
            part.color = out.definitions[index].color;
        }
        AssemblyComponent c;
        c.name = part.name;
        c.definition = out.definitions.size();
        components.push_back(std::move(c));
        out.definitions.push_back(std::move(part));
    }
    AssemblyDefinition& assembly = out.definitions[index];
    assembly.kind = DefinitionKind::Assembly;
    assembly.shape = Shape();
    assembly.faceColors.clear();
    assembly.components = std::move(components);
}

}  // namespace

Result<AssemblyDocument> readIgesAssembly(std::string_view data, const IgesReadOptions& options) {
    using R = Result<AssemblyDocument>;
    if (data.empty()) {
        return R::failure(ErrorCode::ParseError, "IGES input is empty");
    }
    if (data.size() > options.maxInputBytes) {
        return R::failure(ErrorCode::InvalidInput, "IGES input of " + std::to_string(data.size()) +
                                                       " bytes exceeds the limit of " +
                                                       std::to_string(options.maxInputBytes) + " bytes");
    }
    // IGES is fixed-format text of 80-column records; column 73 holds the section letter. A
    // first record that is not a start (S) or global (G) record is not IGES.
    {
        const std::size_t eol = data.find_first_of("\r\n");
        const std::string_view first = data.substr(0, eol);
        if (first.size() < 73 || (first[72] != 'S' && first[72] != 'G')) {
            return R::failure(ErrorCode::ParseError, "IGES input does not start with a start or global section record");
        }
    }
    try {
        return detail::callConvertingFaults<R>([&]() -> R {
            TempIgesFile file(data);
            if (file.path().empty()) {
                return R::failure(ErrorCode::IoError, "IGES temporary file: " + file.error());
            }
            TempXcafDocument xcaf;
            IGESCAFControl_Reader reader;
            reader.SetNameMode(true);
            reader.SetColorMode(true);
            reader.SetLayerMode(true);
            const IFSelect_ReturnStatus status = reader.ReadFile(file.path().c_str());
            if (status != IFSelect_RetDone) {
                return R::failure(ErrorCode::ParseError,
                                  "IGES input could not be parsed (IFSelect status " + std::to_string(status) + ")");
            }
            if (reader.NbRootsForTransfer() <= 0) {
                return R::failure(ErrorCode::ParseError, "IGES input has no transferable entities");
            }
            if (!reader.Transfer(xcaf.doc())) {
                return R::failure(ErrorCode::OperationFailed, "IGES entities could not be transferred to XCAF");
            }
            AssemblyDocument out;
            const Handle(IGESData_IGESModel) model = Handle(IGESData_IGESModel)::DownCast(reader.Model());
            if (!model.IsNull()) {
                const IGESData_GlobalSection& global = model->GlobalSection();
                if (!global.UnitName().IsNull()) {
                    out.fileLengthUnits.emplace_back(global.UnitName()->ToCString());
                }
                out.fileUnitMm = igesUnitFlagToMm(global.UnitFlag());
            }
            NCollection_Sequence<TDF_Label> free;
            xcaf.shapes()->GetFreeShapes(free);
            if (free.Length() == 0) {
                return R::failure(ErrorCode::OperationFailed, "IGES transfer produced no shapes");
            }
            const StepAttributes noAttributes;
            XcafToDocument converter(xcaf, noAttributes, out);
            for (int i = 1; i <= free.Length(); ++i) {
                auto root = converter.convert(free.Value(i));
                if (!root) {
                    return R(root.error());
                }
                out.roots.push_back(root.value());
            }
            if (options.sewingTolerance > 0.0) {
                for (AssemblyDefinition& def : out.definitions) {
                    if (def.kind == DefinitionKind::Part && !hasSolid(def.shape.occt())) {
                        sewIntoSolids(def, options.sewingTolerance, out.warnings);
                    }
                }
            }
            if (options.splitSolids) {
                for (int i = 1; i <= free.Length(); ++i) {
                    const std::size_t index = out.roots[static_cast<std::size_t>(i - 1)];
                    if (out.definitions[index].kind == DefinitionKind::Part) {
                        splitSolidsIntoParts(out, index, free.Value(i));
                    }
                }
            }
            return out;
        });
    } catch (const Standard_Failure& failure) {
        return R::failure(detail::faultOr(failure, ErrorCode::ParseError), "IGES input is malformed: " + failureText(failure));
    } catch (const std::exception& e) {
        return R::failure(ErrorCode::ParseError, std::string("IGES input is malformed: ") + e.what());
    }
}

Result<std::string> writeIgesAssembly(const AssemblyDocument& document, const IgesWriteOptions& options) {
    using R = Result<std::string>;
    try {
        auto checked = checkStructure(document);
        if (!checked) {
            return R(checked.error());
        }
        TempXcafDocument xcaf;
        DocumentToXcaf builder(document, xcaf);
        auto built = builder.build();
        if (!built) {
            return R(built.error());
        }
        builder.useDocumentNames();
        // IGESControl_Writer takes the unit and the B-rep mode from the static parameters.
        IGESControl_Controller::Init();
        Interface_Static::SetCVal("write.iges.unit", igesUnitName(options.fileUnit));
        Interface_Static::SetIVal("write.iges.brep.mode", options.mode == IgesWriteMode::BRep ? 1 : 0);
        IGESCAFControl_Writer writer;
        writer.SetNameMode(true);
        writer.SetColorMode(true);
        writer.SetLayerMode(false);
        if (!writer.Transfer(xcaf.doc())) {
            return R::failure(ErrorCode::OperationFailed, "IGESCAFControl_Writer could not transfer the document");
        }
        std::ostringstream out;
        if (!writer.Write(out) || !out) {
            return R::failure(ErrorCode::IoError, "IGES write failed");
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
