#pragma once

// STEP with assembly structure, names, colours and layers (XCAF), through memory buffers
// only (docs/kernel/XCAF_IO.md).
//
// readStepAssembly() reads STEP text with STEPCAFControl_Reader into a temporary XCAF
// document and converts it into AssemblyDocument, a plain value structure that does not
// depend on OCAF: part and assembly definitions (prototypes), components (occurrences)
// with TopLoc_Location, names, colours of definitions, components and faces, layers and
// the length units of the file, PRODUCT id and description of the definitions, NAUO id,
// description and reference designator of the components. writeStepAssembly() builds an XCAF document from such a
// structure and writes it with STEPCAFControl_Writer as AP214 or AP242.
//
// Lengths are millimetres inside bendcore; colours are sRGB components in [0, 1], the
// space of STEP COLOUR_RGB.

#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>
#include <bendcore/kernel/step_io.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <TopLoc_Location.hxx>

namespace bendcore::kernel {

struct AssemblyColor {
    double r = 0.0;  // sRGB, 0..1
    double g = 0.0;
    double b = 0.0;
    double alpha = 1.0;  // 1 = opaque; STEP SURFACE_STYLE_TRANSPARENT stores 1 - alpha
};

// Surface colour of one face of a part; faceIndex is the TopologyIndex of the part shape.
struct FaceColor {
    std::uint32_t faceIndex = 0;
    AssemblyColor color;
};

// Occurrence of a definition inside an assembly definition.
struct AssemblyComponent {
    std::string name;            // NEXT_ASSEMBLY_USAGE_OCCURRENCE name
    std::size_t definition = 0;  // index into AssemblyDocument::definitions
    TopLoc_Location location;    // placement relative to the parent assembly
    std::optional<AssemblyColor> color;  // overrides the colour of the definition
    // Read only: STEPCAFControl_Writer cannot write layers of an occurrence, so
    // writeStepAssembly() rejects a non-empty list with Unsupported.
    std::vector<std::string> layers;
    // NAUO id. On write an empty id lets OCCT number the occurrences ('1', '2', ...);
    // readStepAssembly() always fills it from the file.
    std::string instanceId;
    std::string description;  // NAUO description
    // NAUO reference_designator (optional attribute; '$' in the file when absent).
    std::optional<std::string> referenceDesignator;
};

enum class DefinitionKind { Part, Assembly };

// Prototype: a part (shape in its own coordinates) or an assembly (list of components).
struct AssemblyDefinition {
    DefinitionKind kind = DefinitionKind::Part;
    std::string name;  // PRODUCT name
    Shape shape;                                // Part only
    std::vector<AssemblyComponent> components;  // Assembly only
    std::optional<AssemblyColor> color;         // surface colour of the whole definition
    std::vector<FaceColor> faceColors;          // Part only; ascending faceIndex, unique
    std::vector<std::string> layers;            // written for parts only (see writeStepAssembly)
    // PRODUCT id (designation, part number). On write nullopt gives id = name (the OCCT
    // default); readStepAssembly() always fills it from the file.
    std::optional<std::string> productId;
    std::string description;  // PRODUCT description
};

struct AssemblyDocument {
    std::vector<AssemblyDefinition> definitions;
    // Top-level (free) definitions in file order. Every definition is either a root or
    // used by at least one component, never both.
    std::vector<std::size_t> roots;
    // Filled by readStepAssembly(): length unit names of the file as OCCT reports them
    // (e.g. "millimetre", "metre", "INCH"), and millimetres per file unit when all
    // representations use one unit that is known (nullopt otherwise). Ignored on write.
    std::vector<std::string> fileLengthUnits;
    std::optional<double> fileUnitMm;
    // Information that was present in the file but has no place in this structure
    // (path-specific SHUO styles); empty when nothing was dropped.
    std::vector<std::string> warnings;
};

enum class StepSchema { AP214, AP242 };

struct AssemblyWriteOptions {
    StepSchema schema = StepSchema::AP214;
    StepLengthUnit fileUnit = StepLengthUnit::Millimetre;
};

// Errors: empty input, unparsable text, no transferable roots -> ParseError; transfer that
// yields no shape -> OperationFailed.
Result<AssemblyDocument> readStepAssembly(std::string_view data);

// Errors: inconsistent structure (index out of range, cycle, a definition that is neither a
// root nor used, null part shape, empty assembly, colour outside [0, 1], face index out of
// range or repeated) -> InvalidInput; layers on an assembly or a component (STEP cannot
// carry them through STEPCAFControl_Writer) -> Unsupported; OCCT transfer failure ->
// OperationFailed; stream write failure -> IoError.
Result<std::string> writeStepAssembly(const AssemblyDocument& document,
                                      const AssemblyWriteOptions& options = {});

// One part occurrence in world coordinates, found by walking the tree from a root.
struct PlacedPart {
    std::size_t root = 0;                  // index into AssemblyDocument::roots
    std::vector<std::size_t> componentPath;  // component index at each assembly level
    std::size_t definition = 0;            // the part definition
    std::string path;                      // names joined by '/', root name first
    TopLoc_Location location;              // world placement (product of the path)
    Shape shape;                           // part shape moved by location
    // Effective surface colour, resolved like XCAFPrs_DocumentExplorer: inherited from the
    // parent, replaced by the colour of the definition, replaced by that of the component.
    std::optional<AssemblyColor> color;
    std::vector<FaceColor> faceColors;  // face colours of the part (override `color`)
};

// Errors: the same structural checks as writeStepAssembly() -> InvalidInput.
Result<std::vector<PlacedPart>> flattenAssembly(const AssemblyDocument& document);

}  // namespace bendcore::kernel
