// Damaged BRep and STEP input must give an explicit error, never a crash (docs/kernel/KERNEL_CORE.md,
// "Повреждённые файлы"; docs/kernel/OCCT_BUILD.md, раздел 6). Each case is a minimal, deterministic
// version of a defect class found by the fuzz test (tests/regression/fuzz, docs/qa/QA.md, раздел 7):
//  * BRep subshape / location index out of range — needs OCCT built with Release checks
//    (OCCT_RELEASE_CHECKS=1: Standard_OutOfRange instead of an out-of-bounds read);
//  * a keyword longer than OCCT's 255-byte buffer — needs OCCT built as C++20
//    (OCCT_CXX_STANDARD=C++20: `stream >> char[255]` is bounded; C++17 smashes the stack);
//  * a null handle OCCT dereferences (trimmed curve on an unknown basis, STEP reference to an
//    entity of the wrong type) — the kernel's fault guard (SIGSEGV → Standard_Failure);
//  * characters no STEP token has outside strings, oversize STEP — the kernel's preflight.

#include <bendcore/kernel/brep_io.hpp>
#include <bendcore/kernel/step_io.hpp>
#include <bendcore/kernel/xcaf_io.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include <BRepPrimAPI_MakeBox.hxx>

using namespace bendcore::kernel;

namespace {

// A straight edge from (0,0,0) to (10,0,0), as BRepTools::Write prints it (TopTools format V3).
const char* const kEdgeText =
    "\nCASCADE Topology V3, (c) Open Cascade\n"
    "Locations 0\n"
    "Curve2ds 0\n"
    "Curves 1\n"
    "@CURVE@\n"
    "Polygon3D 0\n"
    "PolygonOnTriangulations 0\n"
    "Surfaces 0\n"
    "Triangulations 0\n"
    "\n"
    "TShapes 3\n"
    "Ve\n1e-07\n0 0 0\n0 0\n\n0101101\n*\n"
    "Ve\n1e-07\n10 0 0\n0 0\n\n0101101\n*\n"
    "Ed\n 1e-07 1 1 0\n1  1 0 0 10\n0\n\n1101000\n@SUBSHAPES@ *\n"
    "\n+1 0 \n";

std::string edgeText(const std::string& curve = "1 0 0 0 1 0 0 ", const std::string& subshapes = "+3 0 -2 0") {
    std::string text = kEdgeText;
    text.replace(text.find("@CURVE@"), 7, curve);
    text.replace(text.find("@SUBSHAPES@"), 11, subshapes);
    return text;
}

std::string boxStep() {
    auto text = writeStep(Shape(BRepPrimAPI_MakeBox(100.0, 60.0, 20.0).Shape()));
    EXPECT_TRUE(text.ok());
    return text.ok() ? text.value() : std::string();
}

// Replaces the first occurrence of `from` (which must exist) in `text`.
std::string replaced(std::string text, const std::string& from, const std::string& to) {
    const auto at = text.find(from);
    EXPECT_NE(at, std::string::npos) << from;
    if (at != std::string::npos) text.replace(at, from.size(), to);
    return text;
}

template <typename T>
std::string messageOf(const Result<T>& r) {
    return r.ok() ? std::string("<ok>") : r.error().message;
}

}  // namespace

// ---------------------------------------------------------------- BRep text

TEST(KernelMalformedInput, ValidEdgeTextIsTheReference) {
    auto edge = readBrepText(edgeText());
    ASSERT_TRUE(edge.ok()) << messageOf(edge);
    EXPECT_EQ(edge.value().counts().edges, 1);
    EXPECT_EQ(edge.value().counts().vertices, 2);
}

TEST(KernelMalformedInput, SubshapeIndexOutOfRangeIsParseError) {
    // 3 TShapes; the edge refers to shape 9. Without OCCT's Release checks this read past the array.
    auto r = readBrepText(edgeText("1 0 0 0 1 0 0 ", "+9 0 -2 0"));
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::ParseError);
    EXPECT_NE(r.error().message.find("Standard_OutOfRange"), std::string::npos) << r.error().message;
}

TEST(KernelMalformedInput, LocationIndexOutOfRangeIsParseError) {
    // "Locations 0", the vertex reference uses location 7.
    auto r = readBrepText(edgeText("1 0 0 0 1 0 0 ", "+3 7 -2 0"));
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::ParseError);
    EXPECT_NE(r.error().message.find("Standard_OutOfRange"), std::string::npos) << r.error().message;
}

TEST(KernelMalformedInput, TrimmedCurveOnUnknownBasisIsParseError) {
    // Type 8 = trimmed curve; its basis has the unknown type 99, GeomTools returns a null curve
    // and Geom_TrimmedCurve dereferences it (no check in OCCT): the fault guard turns the SIGSEGV
    // into an error.
    auto r = readBrepText(edgeText("8 0 10\n99 0 0 0 1 0 0 "));
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::ParseError);
}

TEST(KernelMalformedInput, OverlongKeywordInTextIsParseError) {
    // OCCT reads section keywords into char[255] with `>>`: a 100 000-character word in place of
    // "TShapes" overflowed the buffer when OCCT was compiled as C++17.
    auto r = readBrepText(replaced(edgeText(), "TShapes 3", std::string(100000, 'T') + " 3"));
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::ParseError);
}

// ---------------------------------------------------------------- BRep binary

TEST(KernelMalformedInput, OverlongKeywordInBinaryIsParseError) {
    auto bin = writeBrepBinary(Shape(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape()));
    ASSERT_TRUE(bin.ok());
    std::string bytes(bin.value().begin(), bin.value().end());
    ASSERT_TRUE(readBrepBinary(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()).ok());
    for (const char* keyword : {"TShapes", "Locations", "Curves", "Surfaces"}) {
        const std::string damaged = replaced(bytes, keyword, std::string(4096, 'K'));
        auto r = readBrepBinary(reinterpret_cast<const std::uint8_t*>(damaged.data()), damaged.size());
        ASSERT_FALSE(r.ok()) << keyword;
        EXPECT_EQ(r.error().code, ErrorCode::ParseError) << keyword;
    }
}

// ---------------------------------------------------------------- STEP

TEST(KernelMalformedInput, CharacterOutsideStepTokensIsParseErrorWithPosition) {
    const std::string step = replaced(boxStep(), "VECTOR('',", "VECTOR('',[");
    auto r = readStep(step);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::ParseError);
    EXPECT_NE(r.error().message.find("'['"), std::string::npos) << r.error().message;
    EXPECT_NE(r.error().message.find("line "), std::string::npos) << r.error().message;
}

TEST(KernelMalformedInput, TwoHundredThousandBracketsAreRefusedBeforeParsing) {
    // The fuzz input that made OCCT's reader allocate more than 8 GiB (every '[' became a
    // separate parameter). Refused by the preflight in well under a second.
    const auto t0 = std::chrono::steady_clock::now();
    auto r = readStep(replaced(boxStep(), "VECTOR('',", "VECTOR(''," + std::string(200000, '[')));
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::ParseError);
    EXPECT_LT(s, 2.0);
    // Inside a string the same characters are text and the file reads normally (OCCT's string
    // end rule: an apostrophe followed by blanks and , or )).
    auto quoted = readStep(replaced(boxStep(), "VECTOR('',", "VECTOR('" + std::string(200000, '[') + "',"));
    ASSERT_TRUE(quoted.ok()) << messageOf(quoted);
}

TEST(KernelMalformedInput, SpecialCharactersInStringsAndCommentsAreAccepted) {
    std::string step = replaced(boxStep(), "PLANE('',", "PLANE('face [1] {a} it''s ~ok @ % ?',");
    step = replaced(step, "DATA;", "/* comment with [brackets] and ~ ? */\nDATA;");
    auto r = readStep(step);
    ASSERT_TRUE(r.ok()) << messageOf(r);
    EXPECT_NEAR(r.value().volumeProperties().value().volume, 100.0 * 60.0 * 20.0, 1e-6 * 120000.0);
    EXPECT_EQ(r.value().counts().faces, 6);
}

TEST(KernelMalformedInput, OversizeStepIsInvalidInput) {
    const std::string step = boxStep();
    StepReadOptions options;
    options.maxInputBytes = step.size() - 1;
    auto r = readStep(step, options);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::InvalidInput);
    EXPECT_NE(r.error().message.find("exceeds the limit"), std::string::npos) << r.error().message;
    options.maxInputBytes = step.size();
    EXPECT_TRUE(readStep(step, options).ok());
    EXPECT_EQ(kStepDefaultMaxInputBytes, std::size_t(256) * 1024 * 1024);
}

TEST(KernelMalformedInput, WrongTypeReferenceDoesNotCrash) {
    // The direction of a VECTOR refers to a CARTESIAN_POINT: OCCT leaves a null StepGeom_Direction
    // and StepToGeom dereferences it. The SIGSEGV happens inside OCCT's own per-entity
    // OCC_CATCH_SIGNALS point (the kernel's handler longjmps there), OCCT records a transfer
    // failure for that edge and goes on; the result is an explicit outcome either way.
    std::string step = boxStep();
    // First LINE of the file: "#a = LINE('',#p,#v); #p = CARTESIAN_POINT; #v = VECTOR('',#d,1.)".
    const auto vectorAt = step.find("= VECTOR('',#");
    ASSERT_NE(vectorAt, std::string::npos);
    const auto lineStart = step.rfind('\n', vectorAt) + 1;
    const auto pointAt = step.rfind("= CARTESIAN_POINT", vectorAt);
    ASSERT_NE(pointAt, std::string::npos);
    const auto pointStart = step.rfind('\n', pointAt) + 1;
    const std::string pointName = step.substr(pointStart, step.find(' ', pointStart) - pointStart);
    const auto refAt = step.find('#', vectorAt + 2);
    const auto refEnd = step.find(',', refAt);
    step.replace(refAt, refEnd - refAt, pointName);
    ASSERT_NE(step.substr(lineStart, 40).find("VECTOR"), std::string::npos);
    auto r = readStep(step);
    if (r.ok()) {
        EXPECT_LE(r.value().counts().faces, 6);
    } else {
        EXPECT_TRUE(r.error().code == ErrorCode::ParseError || r.error().code == ErrorCode::OperationFailed)
            << r.error().message;
    }
}

TEST(KernelMalformedInput, XcafReaderUsesTheSamePreflight) {
    auto r = readStepAssembly(replaced(boxStep(), "VECTOR('',", "VECTOR('',[["));
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::ParseError);
    EXPECT_NE(r.error().message.find("'['"), std::string::npos) << r.error().message;
}
