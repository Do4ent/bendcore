#include <bendcore/kernel/brep_io.hpp>
#include <bendcore/kernel/tessellation.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <random>

#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Poly_Triangulation.hxx>
#include <TopoDS.hxx>

using namespace bendcore::kernel;

namespace {

constexpr double kPi = 3.14159265358979323846;

double faceArea(const TopoDS_Face& face) {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    return props.Mass();
}

// Same volume, same per-index face types and areas as the original.
void expectSameShape(const Shape& original, const Shape& restored, double expectedVolume) {
    ASSERT_FALSE(restored.isNull());
    EXPECT_TRUE(restored.checkValidity().valid);
    EXPECT_NEAR(restored.volumeProperties().value().volume, expectedVolume, expectedVolume * 1e-9);
    const auto a = TopologyIndex::build(original);
    const auto b = TopologyIndex::build(restored);
    ASSERT_TRUE(a.ok());
    ASSERT_TRUE(b.ok());
    ASSERT_EQ(a.value().faces().size(), b.value().faces().size());
    ASSERT_EQ(a.value().edges().size(), b.value().edges().size());
    ASSERT_EQ(a.value().vertices().size(), b.value().vertices().size());
    for (std::size_t i = 0; i < a.value().faces().size(); ++i) {
        EXPECT_EQ(a.value().faces()[i].surface, b.value().faces()[i].surface) << "face " << i;
        EXPECT_NEAR(faceArea(a.value().faces()[i].face), faceArea(b.value().faces()[i].face), 1e-9);
        EXPECT_EQ(a.value().faces()[i].face.Orientation(), b.value().faces()[i].face.Orientation());
    }
    for (std::size_t i = 0; i < a.value().edges().size(); ++i) {
        EXPECT_EQ(a.value().edges()[i].curve, b.value().edges()[i].curve) << "edge " << i;
    }
    for (std::size_t i = 0; i < a.value().vertices().size(); ++i) {
        for (int k = 0; k < 3; ++k) {
            EXPECT_NEAR(a.value().vertices()[i].point[k], b.value().vertices()[i].point[k], 1e-12);
        }
    }
}

struct Sample {
    const char* name;
    Shape shape;
    double volume;
};

std::vector<Sample> samples() {
    return {
        {"box", Shape(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape()), 6000.0},
        {"cylinder", Shape(BRepPrimAPI_MakeCylinder(5.0, 20.0).Shape()), kPi * 25.0 * 20.0},
        {"sphere", Shape(BRepPrimAPI_MakeSphere(7.0).Shape()), 4.0 / 3.0 * kPi * 343.0},
    };
}

bool firstFaceHasTriangulation(const Shape& shape) {
    const auto index = TopologyIndex::build(shape);
    TopLoc_Location location;
    return !BRep_Tool::Triangulation(index.value().faces().front().face, location).IsNull();
}

}  // namespace

TEST(KernelBrepIo, BinaryRoundTripPreservesGeometryAndIndices) {
    for (const Sample& sample : samples()) {
        SCOPED_TRACE(sample.name);
        auto bytes = writeBrepBinary(sample.shape);
        ASSERT_TRUE(bytes.ok()) << bytes.error().message;
        ASSERT_FALSE(bytes.value().empty());
        auto restored = readBrepBinary(bytes.value());
        ASSERT_TRUE(restored.ok()) << restored.error().message;
        expectSameShape(sample.shape, restored.value(), sample.volume);
    }
}

TEST(KernelBrepIo, TextRoundTripPreservesGeometryAndIndices) {
    for (const Sample& sample : samples()) {
        SCOPED_TRACE(sample.name);
        auto text = writeBrepText(sample.shape);
        ASSERT_TRUE(text.ok()) << text.error().message;
        EXPECT_NE(text.value().find("CASCADE Topology"), std::string::npos);
        auto restored = readBrepText(text.value());
        ASSERT_TRUE(restored.ok()) << restored.error().message;
        expectSameShape(sample.shape, restored.value(), sample.volume);
    }
}

TEST(KernelBrepIo, TranslatedShapeKeepsLocation) {
    gp_Trsf trsf;
    trsf.SetTranslation(gp_Vec(100.0, 0.0, -20.0));
    const Shape moved(BRepPrimAPI_MakeBox(1.0, 2.0, 3.0).Shape().Moved(TopLoc_Location(trsf)));
    auto restored = readBrepBinary(writeBrepBinary(moved).value());
    ASSERT_TRUE(restored.ok());
    const auto box = restored.value().boundingBox().value();
    EXPECT_NEAR(box.min[0], 100.0, 1e-7);
    EXPECT_NEAR(box.max[2], -17.0, 1e-7);
}

TEST(KernelBrepIo, TrianglesAreStoredOnlyWhenRequested) {
    const Shape box(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape());
    ASSERT_TRUE(tessellate(box).ok());
    ASSERT_TRUE(firstFaceHasTriangulation(box));

    BrepWriteOptions withTriangles;
    withTriangles.withTriangles = true;
    EXPECT_TRUE(firstFaceHasTriangulation(readBrepBinary(writeBrepBinary(box, withTriangles).value()).value()));
    EXPECT_TRUE(firstFaceHasTriangulation(readBrepText(writeBrepText(box, withTriangles).value()).value()));
    EXPECT_FALSE(firstFaceHasTriangulation(readBrepBinary(writeBrepBinary(box).value()).value()));
    EXPECT_FALSE(firstFaceHasTriangulation(readBrepText(writeBrepText(box).value()).value()));
}

TEST(KernelBrepIo, GarbageInputIsParseError) {
    std::mt19937 rng(12345);
    std::vector<std::uint8_t> noise(4096);
    for (auto& byte : noise) byte = static_cast<std::uint8_t>(rng() & 0xFF);
    EXPECT_EQ(readBrepBinary(noise).error().code, ErrorCode::ParseError);
    EXPECT_EQ(readBrepBinary(nullptr, 0).error().code, ErrorCode::ParseError);
    EXPECT_EQ(readBrepBinary(std::vector<std::uint8_t>{}).error().code, ErrorCode::ParseError);

    EXPECT_EQ(readBrepText("this is not a BRep file\n1 2 3\n").error().code, ErrorCode::ParseError);
    EXPECT_EQ(readBrepText("").error().code, ErrorCode::ParseError);
    EXPECT_EQ(readBrepText(std::string(reinterpret_cast<const char*>(noise.data()), noise.size())).error().code,
              ErrorCode::ParseError);
}

TEST(KernelBrepIo, TruncatedBinaryInputIsParseError) {
    const Shape cylinder(BRepPrimAPI_MakeCylinder(5.0, 20.0).Shape());
    const std::vector<std::uint8_t> bytes = writeBrepBinary(cylinder).value();
    const std::vector<std::uint8_t> half(bytes.begin(), bytes.begin() + bytes.size() / 2);
    EXPECT_EQ(readBrepBinary(half).error().code, ErrorCode::ParseError);
}

TEST(KernelBrepIo, TruncatedTextInputIsParseError) {
    const Shape cylinder(BRepPrimAPI_MakeCylinder(5.0, 20.0).Shape());
    const std::string text = writeBrepText(cylinder).value();
    EXPECT_EQ(readBrepText(text.substr(0, text.size() / 2)).error().code, ErrorCode::ParseError);
}

TEST(KernelBrepIo, NullShapeCannotBeWritten) {
    EXPECT_EQ(writeBrepBinary(Shape()).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(writeBrepText(Shape()).error().code, ErrorCode::InvalidInput);
}
