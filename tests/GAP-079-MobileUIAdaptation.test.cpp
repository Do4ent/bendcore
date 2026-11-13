#include <gtest/gtest.h>
#include <memory>
#include "../include/bendcore/mobile/mobile_ui_adaptation.hpp"

class MobileUIAdaptationTest : public ::testing::Test {
protected:
    std::unique_ptr<bendcore::mobile::MobileUIAdaptation> mobile_ui;

    void SetUp() override {
        mobile_ui = std::make_unique<bendcore::mobile::MobileUIAdaptation>();
    }
};

TEST_F(MobileUIAdaptationTest, InitializeUI) {
    EXPECT_NE(mobile_ui.get(), nullptr);
}

TEST_F(MobileUIAdaptationTest, AdaptToScreenSize) {
    auto adapted = mobile_ui->adaptToScreenSize(1080, 1920);
    EXPECT_TRUE(adapted);
}

TEST_F(MobileUIAdaptationTest, AdaptToOrientation) {
    auto adapted = mobile_ui->adaptToOrientation("portrait");
    EXPECT_TRUE(adapted);
}

TEST_F(MobileUIAdaptationTest, AdaptToDevice) {
    auto adapted = mobile_ui->adaptToDevice("smartphone");
    EXPECT_TRUE(adapted);
}

TEST_F(MobileUIAdaptationTest, AdaptToDeviceType) {
    auto adapted = mobile_ui->adaptToDevice("tablet");
    EXPECT_TRUE(adapted);
}

TEST_F(MobileUIAdaptationTest, GetResponsiveLayout) {
    auto layout = mobile_ui->getResponsiveLayout("portrait");
    EXPECT_FALSE(layout.empty());
}

TEST_F(MobileUIAdaptationTest, TouchOptimizedButtons) {
    auto btn = mobile_ui->createTouchOptimizedButton("Action", 48, 48);
    EXPECT_FALSE(btn.empty());
}

TEST_F(MobileUIAdaptationTest, TouchOptimizedInputs) {
    auto input = mobile_ui->createTouchOptimizedInput("text", 44);
    EXPECT_FALSE(input.empty());
}

TEST_F(MobileUIAdaptationTest, ResponsiveText) {
    auto text = mobile_ui->getResponsiveTextSize("body");
    EXPECT_GT(text, 0);
}

TEST_F(MobileUIAdaptationTest, SafeAreaInsets) {
    auto insets = mobile_ui->getSafeAreaInsets();
    EXPECT_GE(insets.top, 0);
}

TEST_F(MobileUIAdaptationTest, NavigationDrawer) {
    auto drawer = mobile_ui->createNavigationDrawer({{"Home", "/"}, {"Settings", "/settings"}});
    EXPECT_FALSE(drawer.empty());
}

TEST_F(MobileUIAdaptationTest, BottomNavigationBar) {
    auto navbar = mobile_ui->createBottomNavigationBar({{"Home", "home"}, {"Profile", "profile"}});
    EXPECT_FALSE(navbar.empty());
}

TEST_F(MobileUIAdaptationTest, FloatingActionButton) {
    auto fab = mobile_ui->createFloatingActionButton("Add");
    EXPECT_FALSE(fab.empty());
}

TEST_F(MobileUIAdaptationTest, ResponsiveGrid) {
    auto grid = mobile_ui->createResponsiveGrid(2);
    EXPECT_FALSE(grid.empty());
}

TEST_F(MobileUIAdaptationTest, MobileKeyboard) {
    auto keyboard = mobile_ui->showKeyboard("text");
    EXPECT_TRUE(keyboard);
}

TEST_F(MobileUIAdaptationTest, StatusBarAdaptation) {
    auto adapted = mobile_ui->adaptStatusBar("light");
    EXPECT_TRUE(adapted);
}

TEST_F(MobileUIAdaptationTest, OverflowMenu) {
    auto menu = mobile_ui->createOverflowMenu({{"Edit", "edit"}, {"Delete", "delete"}});
    EXPECT_FALSE(menu.empty());
}

TEST_F(MobileUIAdaptationTest, TabletSplitView) {
    auto split = mobile_ui->createSplitView(true);
    EXPECT_FALSE(split.empty());
}

TEST_F(MobileUIAdaptationTest, ResponsiveFonts) {
    auto font_size = mobile_ui->getResponsiveFontSize(16);
    EXPECT_GT(font_size, 0);
}

TEST_F(MobileUIAdaptationTest, TouchTargetSize) {
    auto size = mobile_ui->getRecommendedTouchTargetSize();
    EXPECT_GE(size, 44);
}

TEST_F(MobileUIAdaptationTest, ColorAdaptation) {
    auto color = mobile_ui->adaptColor("#FF0000", "dark_mode");
    EXPECT_FALSE(color.empty());
}

TEST_F(MobileUIAdaptationTest, IconAdaptation) {
    auto icon = mobile_ui->adaptIcon("icon_name", 24);
    EXPECT_FALSE(icon.empty());
}

TEST_F(MobileUIAdaptationTest, ScrollableContent) {
    auto scrollable = mobile_ui->createScrollableContainer();
    EXPECT_FALSE(scrollable.empty());
}

TEST_F(MobileUIAdaptationTest, SnapPoints) {
    auto snap = mobile_ui->addSnapPoints({0.5, 0.75, 1.0});
    EXPECT_TRUE(snap);
}

TEST_F(MobileUIAdaptationTest, AdaptiveSpacing) {
    auto spacing = mobile_ui->getAdaptiveSpacing("regular");
    EXPECT_GT(spacing, 0);
}

TEST_F(MobileUIAdaptationTest, DensityAdaptation) {
    auto adapted = mobile_ui->adaptDensity("comfortable");
    EXPECT_TRUE(adapted);
}

TEST_F(MobileUIAdaptationTest, LanguageAdaptation) {
    auto adapted = mobile_ui->adaptToLanguage("ar");
    EXPECT_TRUE(adapted);
}

TEST_F(MobileUIAdaptationTest, AccessibilityAdaptation) {
    auto adapted = mobile_ui->enableAccessibilityFeatures(true);
    EXPECT_TRUE(adapted);
}

TEST_F(MobileUIAdaptationTest, UIVersion) {
    auto version = mobile_ui->getUIAdaptationVersion();
    EXPECT_FALSE(version.empty());
}

TEST_F(MobileUIAdaptationTest, AsyncUIAdaptation) {
    auto future = mobile_ui->adaptUIAsync("landscape");
    EXPECT_TRUE(future.valid());
}

TEST_F(MobileUIAdaptationTest, PreviewUI) {
    auto preview = mobile_ui->previewUI("portrait", 1080, 1920);
    EXPECT_FALSE(preview.empty());
}

TEST_F(MobileUIAdaptationTest, UIAnimations) {
    auto animated = mobile_ui->enableUIAnimations(true);
    EXPECT_TRUE(animated);
}

TEST_F(MobileUIAdaptationTest, DarkModeAdaptation) {
    auto adapted = mobile_ui->adaptToDarkMode(true);
    EXPECT_TRUE(adapted);
}
