#include "bendcore/mobile/mobile_ui_adaptation.hpp"
#include <algorithm>
#include <mutex>
#include <stdexcept>

namespace bendcore::mobile {

namespace {
    thread_local std::mutex g_adapter_mutex;
}

MobileUIAdapter::MobileUIAdapter()
    : uiScaleFactor(1.0f) {
    // Initialize with default breakpoints
    breakpoints.push_back({
        .minWidth = 320,
        .maxWidth = 480,
        .optimalWidth = 400,
        .scaleFactor = 0.75f,
        .breakpointName = "mobile"
    });
    breakpoints.push_back({
        .minWidth = 481,
        .maxWidth = 768,
        .optimalWidth = 600,
        .scaleFactor = 0.9f,
        .breakpointName = "tablet"
    });
    breakpoints.push_back({
        .minWidth = 769,
        .maxWidth = 1920,
        .optimalWidth = 1200,
        .scaleFactor = 1.0f,
        .breakpointName = "desktop"
    });
}

MobileUIAdapter::~MobileUIAdapter() = default;

bool MobileUIAdapter::detectDevice(const ScreenDimensions& dimensions) {
    currentScreen = dimensions;

    if (currentScreen.width <= 480 && currentScreen.height <= 800) {
        currentDeviceType = DeviceType::PHONE;
    } else if (currentScreen.width <= 1024 || currentScreen.height <= 1024) {
        currentDeviceType = DeviceType::TABLET;
    } else {
        currentDeviceType = DeviceType::DESKTOP;
    }
    return true;
}

ScreenDimensions MobileUIAdapter::getCurrentScreenDimensions() const {
    return currentScreen;
}

DeviceType MobileUIAdapter::getDeviceType() const {
    return currentDeviceType;
}

bool MobileUIAdapter::registerBreakpoint(const ResponsiveLayoutConfig& config) {
    breakpoints.push_back(config);
    return true;
}

ResponsiveLayoutConfig MobileUIAdapter::getBreakpointForWidth(int32_t width) const {
    for (const auto& bp : breakpoints) {
        if (width >= bp.minWidth && width <= bp.maxWidth) {
            return bp;
        }
    }
    return breakpoints.back();
}

std::vector<ResponsiveLayoutConfig> MobileUIAdapter::getAllBreakpoints() const {
    return breakpoints;
}

bool MobileUIAdapter::adjustLayoutForOrientation(ScreenOrientation orientation) {
    currentScreen.orientation = orientation;
    return true;
}

bool MobileUIAdapter::scaleUIElements(float scaleFactor) {
    uiScaleFactor = scaleFactor;
    return true;
}

bool MobileUIAdapter::optimizeLayoutForDevice(DeviceType deviceType) {
    currentDeviceType = deviceType;
    return true;
}

float MobileUIAdapter::getScaledFontSize(float baseSize) const {
    return baseSize * uiScaleFactor;
}

float MobileUIAdapter::getScaledTouchTargetSize(float baseSize) const {
    return baseSize * uiScaleFactor * 1.5f;
}

std::string MobileUIAdapter::getOptimalFont(const std::string& baseFont) const {
    if (currentDeviceType == DeviceType::PHONE) {
        return "sans-serif";
    }
    return baseFont;
}

bool MobileUIAdapter::adaptComponentVisibility(const std::string& componentId, bool shouldShow) {
    componentVisibility[componentId] = shouldShow;
    return true;
}

bool MobileUIAdapter::rearrangeLayout(const std::string& layoutId) {
    return true;
}

std::map<std::string, bool> MobileUIAdapter::getComponentVisibilityMap() const {
    return componentVisibility;
}

std::string MobileUIAdapter::getOptimalImageResolution(const std::string& imageId, const ScreenDimensions& dims) {
    if (dims.width <= 480) {
        return imageId + "@1x.png";
    } else if (dims.width <= 1024) {
        return imageId + "@2x.png";
    }
    return imageId + "@3x.png";
}

bool MobileUIAdapter::registerResponsiveImage(const std::string& imageId, const std::vector<std::string>& resolutions) {
    responsiveImages[imageId] = resolutions;
    return true;
}

bool MobileUIAdapter::setViewportMetadata(const std::string& metadata) {
    return true;
}

std::string MobileUIAdapter::getViewportMetadata() const {
    return "width=device-width, initial-scale=1.0";
}

}  // namespace bendcore::mobile
