// STEP / IGES file reader (Q3 2027, Task 1, Subtask 2)
// Uses the OCCT XCAF readers (names, assemblies) on every platform when the
// library is built with OCCT (BENDCORE_WITH_OCCT); otherwise read() reports
// that OCCT is unavailable.
#include <bendcore/step_reader.hpp>
#include <bendcore/occt_config.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef BENDCORE_WITH_OCCT
#include <BRepBndLib.hxx>
#include <BRepTools.hxx>
#include <Bnd_Box.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <IGESCAFControl_Reader.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <Standard_Failure.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDF_Label.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_Sequence.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#endif

namespace bendcore::step {

STEPReader::STEPReader() = default;
STEPReader::~STEPReader() = default;

std::string STEPReader::get_file_extension(const std::string& file_path) {
    size_t dot_pos = file_path.rfind('.');
    if (dot_pos == std::string::npos) {
        return "";
    }
    std::string ext = file_path.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

bool STEPReader::is_supported_format(const std::string& file_path) {
    std::string ext = get_file_extension(file_path);
    return ext == "step" || ext == "stp" || ext == "iges" || ext == "igs";
}

#ifdef BENDCORE_WITH_OCCT
namespace {

int count_subshapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum type) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> map;
    TopExp::MapShapes(shape, type, map);
    return map.Extent();
}

std::string xyz(double x, double y, double z) {
    std::ostringstream out;
    out << x << ' ' << y << ' ' << z;
    return out.str();
}

}  // namespace
#endif

ImportResult STEPReader::read(const std::string& file_path) {
    ImportResult result;
    shapes_.clear();
    properties_.clear();
    warning_count_ = 0;

    if (!is_supported_format(file_path)) {
        result.error_message = "Unsupported file format (expected .step, .stp, .iges, .igs)";
        return result;
    }
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file_path, ec)) {
        result.error_message = "File not found: " + file_path;
        return result;
    }

#ifdef BENDCORE_WITH_OCCT
    const std::string ext = get_file_extension(file_path);
    const bool is_iges = ext == "iges" || ext == "igs";
    Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
    Handle(TDocStd_Document) doc;
    try {
        if (occt::initialize_occt() != occt::OCCTErrorCode::SUCCESS) {
            result.error_message = occt::get_occt_info().error_message;
            return result;
        }
        app->NewDocument("MDTV-XCAF", doc);

        bool transferred = false;
        if (is_iges) {
            IGESCAFControl_Reader reader;
            reader.SetNameMode(true);
            if (reader.ReadFile(file_path.c_str()) != IFSelect_RetDone) {
                result.error_message = "Failed to read IGES file: " +
                    occt::occt_error_to_string(occt::OCCTErrorCode::INVALID_FORMAT);
                app->Close(doc);
                return result;
            }
            transferred = reader.Transfer(doc);
        } else {
            STEPCAFControl_Reader reader;
            reader.SetNameMode(true);
            if (reader.ReadFile(file_path.c_str()) != IFSelect_RetDone) {
                result.error_message = "Failed to read STEP file: " +
                    occt::occt_error_to_string(occt::OCCTErrorCode::INVALID_FORMAT);
                app->Close(doc);
                return result;
            }
            transferred = reader.Transfer(doc);
        }
        if (!transferred) {
            result.error_message = "Failed to transfer file data to document";
            app->Close(doc);
            return result;
        }

        // Top-level (free) shapes of the document: one entry per part/assembly
        Handle(XCAFDoc_ShapeTool) shape_tool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
        NCollection_Sequence<TDF_Label> labels;
        shape_tool->GetFreeShapes(labels);

        int solids = 0, faces = 0, edges = 0;
        Bnd_Box box;
        for (int i = 1; i <= labels.Length(); ++i) {
            const TDF_Label& label = labels.Value(i);
            const TopoDS_Shape shape = XCAFDoc_ShapeTool::GetShape(label);
            if (shape.IsNull()) {
                ++warning_count_;
                continue;
            }
            const std::string id = "shape_" + std::to_string(shapes_.size());

            std::ostringstream brep;
            BRepTools::Write(shape, brep);
            shapes_[id] = brep.str();

            Handle(TDataStd_Name) name;
            if (label.FindAttribute(TDataStd_Name::GetID(), name)) {
                properties_[id + "_name"] = TCollection_AsciiString(name->Get()).ToCString();
            }
            solids += count_subshapes(shape, TopAbs_SOLID);
            faces += count_subshapes(shape, TopAbs_FACE);
            edges += count_subshapes(shape, TopAbs_EDGE);
            BRepBndLib::Add(shape, box);
        }
        app->Close(doc);

        properties_["format"] = is_iges ? "IGES" : "STEP";
        properties_["file_name"] = std::filesystem::path(file_path).filename().string();
        properties_["file_path"] = file_path;
        properties_["file_extension"] = ext;
        properties_["solid_count"] = std::to_string(solids);
        properties_["face_count"] = std::to_string(faces);
        properties_["edge_count"] = std::to_string(edges);
        if (!box.IsVoid()) {
            double x0, y0, z0, x1, y1, z1;
            box.Get(x0, y0, z0, x1, y1, z1);
            properties_["bbox_min"] = xyz(x0, y0, z0);
            properties_["bbox_max"] = xyz(x1, y1, z1);
        }

        result.success = true;
        result.shape_count = static_cast<int>(shapes_.size());
        result.warning_count = warning_count_;
        for (const auto& [shape_id, data] : shapes_) result.shape_ids.push_back(shape_id);
        result.properties = properties_;
    } catch (const Standard_Failure& e) {
        if (!doc.IsNull()) app->Close(doc);
        result.error_message = "OCCT error: ";
        result.error_message += e.what();
    } catch (const std::exception& e) {
        if (!doc.IsNull()) app->Close(doc);
        result.error_message = std::string("Error: ") + e.what();
    }
    return result;
#else
    result.error_message = "STEP/IGES reading requires BendCore built with OCCT (BENDCORE_WITH_OCCT)";
    return result;
#endif
}

std::string STEPReader::get_shape_data(const std::string& shape_id) const {
    auto it = shapes_.find(shape_id);
    return it != shapes_.end() ? it->second : std::string();
}

std::vector<std::string> STEPReader::get_property_keys() const {
    std::vector<std::string> keys;
    for (const auto& [key, value] : properties_) keys.push_back(key);
    return keys;
}

std::string STEPReader::get_property(const std::string& key) const {
    auto it = properties_.find(key);
    return it != properties_.end() ? it->second : std::string();
}

}  // namespace bendcore::step
