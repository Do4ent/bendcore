#include <bendcore/kernel/step_io.hpp>

#include <gtest/gtest.h>

#include <map>

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Trsf.hxx>

using namespace bendcore::kernel;

namespace {

constexpr double kPi = 3.14159265358979323846;

std::map<SurfaceType, int> surfaceHistogram(const Shape& shape) {
    std::map<SurfaceType, int> histogram;
    for (const FaceInfo& face : TopologyIndex::build(shape).value().faces()) ++histogram[face.surface];
    return histogram;
}

Shape roundTrip(const Shape& shape, StepLengthUnit unit, std::string* text = nullptr) {
    StepWriteOptions options;
    options.fileUnit = unit;
    auto written = writeStep(shape, options);
    EXPECT_TRUE(written.ok()) << (written.ok() ? "" : written.error().message);
    if (!written.ok()) return Shape();
    if (text) *text = written.value();
    auto read = readStep(written.value());
    EXPECT_TRUE(read.ok()) << (read.ok() ? "" : read.error().message);
    return read.ok() ? read.value() : Shape();
}

}  // namespace

TEST(KernelStepIo, PrimitivesSurviveRoundTrip) {
    struct Case {
        const char* name;
        Shape shape;
        double volume;
        std::map<SurfaceType, int> surfaces;
    };
    const std::vector<Case> cases = {
        {"box", Shape(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape()), 6000.0, {{SurfaceType::Plane, 6}}},
        {"cylinder", Shape(BRepPrimAPI_MakeCylinder(5.0, 20.0).Shape()), kPi * 25.0 * 20.0,
         {{SurfaceType::Plane, 2}, {SurfaceType::Cylinder, 1}}},
        {"sphere", Shape(BRepPrimAPI_MakeSphere(7.0).Shape()), 4.0 / 3.0 * kPi * 343.0, {{SurfaceType::Sphere, 1}}},
    };
    for (const Case& c : cases) {
        SCOPED_TRACE(c.name);
        const Shape restored = roundTrip(c.shape, StepLengthUnit::Millimetre);
        ASSERT_FALSE(restored.isNull());
        EXPECT_TRUE(restored.checkValidity().valid);
        EXPECT_NEAR(restored.volumeProperties().value().volume, c.volume, c.volume * 1e-6);
        EXPECT_EQ(restored.counts().faces, c.shape.counts().faces);
        EXPECT_EQ(restored.counts().solids, 1u);
        EXPECT_EQ(surfaceHistogram(restored), c.surfaces);
    }
}

TEST(KernelStepIo, FileUnitIsConvertedBackToMillimetres) {
    gp_Trsf shift;
    shift.SetTranslation(gp_Vec(100.0, 200.0, 300.0));
    const Shape box(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape().Moved(TopLoc_Location(shift)));

    struct Case {
        StepLengthUnit unit;
        const char* marker;     // unit entity the file must declare
        const char* corner;     // far corner (110, 220, 330) mm written in the file unit
    };
    const std::vector<Case> cases = {
        {StepLengthUnit::Millimetre, "SI_UNIT(.MILLI.,.METRE.)", "(110.,220.,330.)"},
        {StepLengthUnit::Metre, "SI_UNIT($,.METRE.)", "(0.11,0.22,0.33)"},
        {StepLengthUnit::Inch, "'INCH'", nullptr},
    };
    for (const Case& c : cases) {
        SCOPED_TRACE(c.marker);
        std::string text;
        const Shape restored = roundTrip(box, c.unit, &text);
        ASSERT_FALSE(restored.isNull());
        EXPECT_NE(text.find(c.marker), std::string::npos);
        if (c.corner) EXPECT_NE(text.find(c.corner), std::string::npos);
        EXPECT_NEAR(restored.volumeProperties().value().volume, 6000.0, 6000.0 * 1e-6);
        const BoundingBox bbox = restored.boundingBox().value();
        EXPECT_NEAR(bbox.min[0], 100.0, 1e-5);
        EXPECT_NEAR(bbox.min[1], 200.0, 1e-5);
        EXPECT_NEAR(bbox.min[2], 300.0, 1e-5);
        EXPECT_NEAR(bbox.max[0], 110.0, 1e-5);
        EXPECT_NEAR(bbox.max[1], 220.0, 1e-5);
        EXPECT_NEAR(bbox.max[2], 330.0, 1e-5);
    }
}

TEST(KernelStepIo, CompoundOfSolidsIsReadBack) {
    gp_Trsf shift;
    shift.SetTranslation(gp_Vec(50.0, 0.0, 0.0));
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape());
    builder.Add(compound, BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, 0), 3.0).Shape().Moved(TopLoc_Location(shift)));
    const Shape restored = roundTrip(Shape(compound), StepLengthUnit::Millimetre);
    ASSERT_FALSE(restored.isNull());
    EXPECT_EQ(restored.counts().solids, 2u);
    const double expected = 1000.0 + 4.0 / 3.0 * kPi * 27.0;
    EXPECT_NEAR(restored.volumeProperties().value().volume, expected, expected * 1e-6);
    EXPECT_NEAR(restored.boundingBox().value().max[0], 53.0, 1e-5);
}

TEST(KernelStepIo, GarbageInputIsParseError) {
    EXPECT_EQ(readStep("").error().code, ErrorCode::ParseError);
    EXPECT_EQ(readStep("definitely not a STEP file\n\x01\x02\x03").error().code, ErrorCode::ParseError);
    const char* empty =
        "ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION((''),'2;1');\n"
        "FILE_NAME('x','2026-01-01T00:00:00',(''),(''),'','','');\n"
        "FILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));\nENDSEC;\nDATA;\nENDSEC;\n"
        "END-ISO-10303-21;\n";
    EXPECT_EQ(readStep(empty).error().code, ErrorCode::ParseError);
}

TEST(KernelStepIo, NullShapeCannotBeWritten) {
    EXPECT_EQ(writeStep(Shape()).error().code, ErrorCode::InvalidInput);
}
