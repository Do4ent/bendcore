// STEP File Reader Implementation (Q3 2027, Task 1, Subtask 2)
// С OCCT (BENDCORE_WITH_OCCT): читает STEP/IGES на любой платформе, каждое тело (solid)
// сохраняется как B-rep текст BRepTools — формат, который принимает GeometryAnalyzer::analyze.
// Без OCCT read() возвращает ошибку с причиной.
#include <bendcore/step_reader.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#ifdef BENDCORE_WITH_OCCT
  #include <bendcore/occt_config.hpp>

  #include <BRepTools.hxx>
  #include <IFSelect_ReturnStatus.hxx>
  #include <IGESControl_Reader.hxx>
  #include <STEPControl_Reader.hxx>
  #include <Standard_Failure.hxx>
  #include <TopAbs_ShapeEnum.hxx>
  #include <TopExp_Explorer.hxx>
  #include <TopoDS_Shape.hxx>
#endif

namespace bendcore::step {

STEPReader::STEPReader() = default;
STEPReader::~STEPReader() = default;

std::string STEPReader::get_file_extension(const std::string& file_path) {
    const std::string ext = std::filesystem::path(file_path).extension().string();
    if (ext.size() < 2) {
        return "";
    }
    std::string out = ext.substr(1);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool STEPReader::is_supported_format(const std::string& file_path) {
    const std::string ext = get_file_extension(file_path);
    return ext == "step" || ext == "stp" || ext == "iges" || ext == "igs";
}

#ifdef BENDCORE_WITH_OCCT
namespace {

// Читает файл в одну форму (compound при нескольких корнях). Пустая форма — ошибка чтения.
template <class Reader>
TopoDS_Shape read_with(Reader& reader, const std::string& file_path, int& roots) {
    if (reader.ReadFile(file_path.c_str()) != IFSelect_RetDone) {
        return {};
    }
    roots = reader.TransferRoots();
    return reader.OneShape();
}

std::string brep_text(const TopoDS_Shape& shape) {
    std::ostringstream out;
    BRepTools::Write(shape, out);
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

    const std::string ext = get_file_extension(file_path);
    properties_["file_path"] = file_path;
    properties_["file_name"] = std::filesystem::path(file_path).filename().string();
    properties_["file_extension"] = ext;
    const auto size = std::filesystem::file_size(file_path, ec);
    if (!ec) {
        properties_["file_size"] = std::to_string(size);
    }

#ifdef BENDCORE_WITH_OCCT
    try {
        occt::initialize_occt();

        int roots = 0;
        TopoDS_Shape shape;
        if (ext == "iges" || ext == "igs") {
            IGESControl_Reader reader;
            shape = read_with(reader, file_path, roots);
            properties_["format"] = "IGES";
        } else {
            STEPControl_Reader reader;
            shape = read_with(reader, file_path, roots);
            properties_["format"] = "STEP";
        }
        properties_["root_count"] = std::to_string(roots);

        if (shape.IsNull()) {
            result.error_message = "Failed to read file: " +
                occt::occt_error_to_string(occt::OCCTErrorCode::INVALID_FORMAT);
            result.properties = properties_;
            return result;
        }

        // Тела по отдельности; если тел нет (только оболочки/поверхности) — вся форма целиком.
        int index = 0;
        for (TopExp_Explorer it(shape, TopAbs_SOLID); it.More(); it.Next()) {
            shapes_["solid_" + std::to_string(index++)] = brep_text(it.Current());
        }
        if (shapes_.empty()) {
            shapes_["shape_0"] = brep_text(shape);
            ++warning_count_;
            properties_["warning"] = "no solids found; whole shape exported";
        }
        properties_["solid_count"] = std::to_string(index);

        result.success = true;
        result.shape_count = static_cast<int>(shapes_.size());
        result.warning_count = warning_count_;
        for (const auto& [shape_id, data] : shapes_) {
            result.shape_ids.push_back(shape_id);
        }
        result.properties = properties_;
    } catch (const Standard_Failure& e) {
        result.error_message = "OCCT error: ";
        result.error_message += e.GetMessageString() ? e.GetMessageString() : "Unknown error";
    } catch (const std::exception& e) {
        result.error_message = std::string("Error: ") + e.what();
    }
    return result;
#else
    result.error_message = "STEP file reading requires BendCore built with OCCT (BENDCORE_WITH_OCCT)";
    result.properties = properties_;
    return result;
#endif
}

std::string STEPReader::get_shape_data(const std::string& shape_id) const {
    auto it = shapes_.find(shape_id);
    return it != shapes_.end() ? it->second : std::string();
}

std::vector<std::string> STEPReader::get_property_keys() const {
    std::vector<std::string> keys;
    keys.reserve(properties_.size());
    for (const auto& [key, value] : properties_) {
        keys.push_back(key);
    }
    return keys;
}

std::string STEPReader::get_property(const std::string& key) const {
    auto it = properties_.find(key);
    return it != properties_.end() ? it->second : std::string();
}

}  // namespace bendcore::step
