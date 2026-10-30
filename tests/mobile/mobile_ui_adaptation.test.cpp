#include <gtest/gtest.h>
#include "bendcore/mobile/mobile_ui_adaptation.hpp"

using namespace bendcore::mobile;

class MobileUIAdapterTest : public ::testing::Test {
protected:
    MobileUIAdapter adapter;
};

TEST_F(MobileUIAdapterTest, DetectPhoneDevice) {
    ScreenDimensions dims{.width = 400, .height = 800, .dpi = 326.0f};
    EXPECT_TRUE(adapter.detectDevice(dims));
    EXPECT_EQ(adapter.getDeviceType(), DeviceType::PHONE);
}

TEST_F(MobileUIAdapterTest, DetectTabletDevice) {
    ScreenDimensions dims{.width = 800, .height = 1024, .dpi = 264.0f};
    EXPECT_TRUE(adapter.detectDevice(dims));
    EXPECT_EQ(adapter.getDeviceType(), DeviceType::TABLET);
}

TEST_F(MobileUIAdapterTest, DetectDesktopDevice) {
    ScreenDimensions dims{.width = 1920, .height = 1080, .dpi = 96.0f};
    EXPECT_TRUE(adapter.detectDevice(dims));
    EXPECT_EQ(adapter.getDeviceType(), DeviceType::DESKTOP);
}

TEST_F(MobileUIAdapterTest, GetScreenDimensions) {
    ScreenDimensions dims{.width = 400, .height = 800, .dpi = 326.0f};
    adapter.detectDevice(dims);
    auto result = adapter.getCurrentScreenDimensions();
    EXPECT_EQ(result.width, 400);
    EXPECT_EQ(result.height, 800);
}

TEST_F(MobileUIAdapterTest, RegisterBreakpoint) {
    ResponsiveLayoutConfig bp{.minWidth = 320, .maxWidth = 480, .optimalWidth = 400, 
                               .scaleFactor = 0.75f, .breakpointName = "mobile"};
    EXPECT_TRUE(adapter.registerBreakpoint(bp));
}

TEST_F(MobileUIAdapterTest, GetBreakpointForWidth) {
    ResponsiveLayoutConfig bp{.minWidth = 320, .maxWidth = 480, .optimalWidth = 400, 
                               .scaleFactor = 0.75f, .breakpointName = "mobile"};
    adapter.registerBreakpoint(bp);
    auto result = adapter.getBreakpointForWidth(400);
    EXPECT_EQ(result.breakpointName, "mobile");
}

TEST_F(MobileUIAdapterTest, AdjustLayoutForOrientation) {
    EXPECT_TRUE(adapter.adjustLayoutForOrientation(ScreenOrientation::LANDSCAPE));
}

TEST_F(MobileUIAdapterTest, ScaleUIElements) {
    EXPECT_TRUE(adapter.scaleUIElements(1.5f));
}

TEST_F(MobileUIAdapterTest, OptimizeLayoutForDevice) {
    EXPECT_TRUE(adapter.optimizeLayoutForDevice(DeviceType::PHONE));
}

TEST_F(MobileUIAdapterTest, GetScaledFontSize) {
    adapter.scaleUIElements(1.5f);
    float scaled = adapter.getScaledFontSize(16.0f);
    EXPECT_GT(scaled, 16.0f);
}

TEST_F(MobileUIAdapterTest, GetScaledTouchTargetSize) {
    adapter.scaleUIElements(1.0f);
    float scaled = adapter.getScaledTouchTargetSize(48.0f);
    EXPECT_GT(scaled, 48.0f);
}

TEST_F(MobileUIAdapterTest, AdaptComponentVisibility) {
    EXPECT_TRUE(adapter.adaptComponentVisibility("button1", true));
}

TEST_F(MobileUIAdapterTest, RegisterResponsiveImage) {
    std::vector<std::string> resolutions{"image@1x.png", "image@2x.png", "image@3x.png"};
    EXPECT_TRUE(adapter.registerResponsiveImage("hero_image", resolutions));
}

TEST_F(MobileUIAdapterTest, GetOptimalImageResolution) {
    std::vector<std::string> resolutions{"image@1x.png", "image@2x.png", "image@3x.png"};
    adapter.registerResponsiveImage("hero_image", resolutions);
    ScreenDimensions dims{.width = 400, .height = 800};
    std::string optimal = adapter.getOptimalImageResolution("hero_image", dims);
    EXPECT_FALSE(optimal.empty());
}

TEST_F(MobileUIAdapterTest, SetViewportMetadata) {
    std::string metadata = "width=device-width, initial-scale=1.0";
    EXPECT_TRUE(adapter.setViewportMetadata(metadata));
}

TEST_F(MobileUIAdapterTest, GetViewportMetadata) {
    std::string metadata = adapter.getViewportMetadata();
    EXPECT_FALSE(metadata.empty());
}

TEST_F(MobileUIAdapterTest, ComponentVisibilityMap) {
    adapter.adaptComponentVisibility("nav", false);
    adapter.adaptComponentVisibility("drawer", true);
    auto visibility = adapter.getComponentVisibilityMap();
    EXPECT_GE(visibility.size(), 2);
}

TEST_F(MobileUIAdapterTest, GetAllBreakpoints) {
    auto breakpoints = adapter.getAllBreakpoints();
    EXPECT_GT(breakpoints.size(), 0);
}

TEST_F(MobileUIAdapterTest, RearrangeLayout) {
    EXPECT_TRUE(adapter.rearrangeLayout("layout1"));
}

TEST_F(MobileUIAdapterTest, GetOptimalFont) {
    ScreenDimensions dims{.width = 400, .height = 800};
    adapter.detectDevice(dims);
    std::string font = adapter.getOptimalFont("Arial");
    EXPECT_FALSE(font.empty());
}

TEST_F(MobileUIAdapterTest, MultipleDeviceDetections) {
    ScreenDimensions phone{.width = 400, .height = 800};
    adapter.detectDevice(phone);
    EXPECT_EQ(adapter.getDeviceType(), DeviceType::PHONE);
    
    ScreenDimensions tablet{.width = 800, .height = 1024};
    adapter.detectDevice(tablet);
    EXPECT_EQ(adapter.getDeviceType(), DeviceType::TABLET);
}

TEST_F(MobileUIAdapterTest, BreakpointBoundaries) {
    ResponsiveLayoutConfig bp{.minWidth = 500, .maxWidth = 700};
    adapter.registerBreakpoint(bp);
    
    auto bp_600 = adapter.getBreakpointForWidth(600);
    EXPECT_GE(bp_600.minWidth, 500);
}

TEST_F(MobileUIAdapterTest, ComponentVisibilityTracking) {
    adapter.adaptComponentVisibility("menu", true);
    adapter.adaptComponentVisibility("sidebar", false);
    adapter.adaptComponentVisibility("footer", true);
    
    auto visibility = adapter.getComponentVisibilityMap();
    EXPECT_EQ(visibility["menu"], true);
    EXPECT_EQ(visibility["sidebar"], false);
}

TEST_F(MobileUIAdapterTest, DPIScaling) {
    ScreenDimensions high_dpi{.width = 400, .height = 800, .dpi = 480.0f};
    adapter.detectDevice(high_dpi);
    EXPECT_EQ(adapter.getCurrentScreenDimensions().dpi, 480.0f);
}

TEST_F(MobileUIAdapterTest, ScaleFactor100) {
    adapter.scaleUIElements(1.0f);
    float scaled = adapter.getScaledFontSize(16.0f);
    EXPECT_EQ(scaled, 16.0f);
}

TEST_F(MobileUIAdapterTest, ScaleFactorDoubling) {
    adapter.scaleUIElements(2.0f);
    float scaled = adapter.getScaledFontSize(16.0f);
    EXPECT_EQ(scaled, 32.0f);
}

TEST_F(MobileUIAdapterTest, TouchTargetMinimumSize) {
    adapter.scaleUIElements(1.0f);
    float target = adapter.getScaledTouchTargetSize(44.0f);
    EXPECT_GE(target, 44.0f * 1.5f);  // Minimum touch target is scaled by 1.5x
}

TEST_F(MobileUIAdapterTest, ResponsiveImageResolutions) {
    std::vector<std::string> resolutions{"img@1x.png", "img@2x.png", "img@3x.png"};
    adapter.registerResponsiveImage("test_image", resolutions);
    
    ScreenDimensions small{.width = 400};
    std::string result_small = adapter.getOptimalImageResolution("test_image", small);
    EXPECT_NE(result_small.find("@"), std::string::npos);
}

TEST_F(MobileUIAdapterTest, OrientationChange) {
    adapter.adjustLayoutForOrientation(ScreenOrientation::PORTRAIT);
    auto dims = adapter.getCurrentScreenDimensions();
    EXPECT_EQ(dims.orientation, ScreenOrientation::PORTRAIT);
    
    adapter.adjustLayoutForOrientation(ScreenOrientation::LANDSCAPE);
    dims = adapter.getCurrentScreenDimensions();
    EXPECT_EQ(dims.orientation, ScreenOrientation::LANDSCAPE);
}

TEST_F(MobileUIAdapterTest, BreakpointCount) {
    auto initial_bps = adapter.getAllBreakpoints();
    int initial_count = initial_bps.size();
    
    ResponsiveLayoutConfig new_bp{.minWidth = 1200, .maxWidth = 1920};
    adapter.registerBreakpoint(new_bp);
    
    auto updated_bps = adapter.getAllBreakpoints();
    EXPECT_EQ(updated_bps.size(), initial_count + 1);
}

TEST_F(MobileUIAdapterTest, ComponentVisibilityUpdate) {
    adapter.adaptComponentVisibility("menu", true);
    auto visibility1 = adapter.getComponentVisibilityMap();
    EXPECT_EQ(visibility1["menu"], true);
    
    adapter.adaptComponentVisibility("menu", false);
    auto visibility2 = adapter.getComponentVisibilityMap();
    EXPECT_EQ(visibility2["menu"], false);
}

TEST_F(MobileUIAdapterTest, MultipleResponsiveImages) {
    adapter.registerResponsiveImage("hero", std::vector<std::string>{"hero@1x.png", "hero@2x.png"});
    adapter.registerResponsiveImage("logo", std::vector<std::string>{"logo@1x.png", "logo@2x.png"});
    
    ScreenDimensions dims{.width = 400};
    std::string hero_res = adapter.getOptimalImageResolution("hero", dims);
    std::string logo_res = adapter.getOptimalImageResolution("logo", dims);
    
    EXPECT_FALSE(hero_res.empty());
    EXPECT_FALSE(logo_res.empty());
}

TEST_F(MobileUIAdapterTest, LargeScreenDeviceDetection) {
    ScreenDimensions ultrawide{.width = 2560, .height = 1440};
    adapter.detectDevice(ultrawide);
    EXPECT_EQ(adapter.getDeviceType(), DeviceType::DESKTOP);
}

TEST_F(MobileUIAdapterTest, TabletPortraitOrientation) {
    ScreenDimensions tablet_portrait{.width = 768, .height = 1024, .dpi = 264.0f};
    adapter.detectDevice(tablet_portrait);
    adapter.adjustLayoutForOrientation(ScreenOrientation::PORTRAIT);
    auto dims = adapter.getCurrentScreenDimensions();
    EXPECT_EQ(dims.orientation, ScreenOrientation::PORTRAIT);
}

TEST_F(MobileUIAdapterTest, ResponsiveBreakpointSelection) {
    auto bp_small = adapter.getBreakpointForWidth(350);
    EXPECT_EQ(bp_small.breakpointName, "mobile");

    auto bp_large = adapter.getBreakpointForWidth(900);
    EXPECT_EQ(bp_large.breakpointName, "desktop");
}

