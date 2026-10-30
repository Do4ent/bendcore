#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <ostream>

namespace bendcore::mobile {

// ==============================================================================
// MOBILE UI ADAPTATION - GAP-079
// ==============================================================================
// Responsive design, dynamic layout adjustments, device-specific rendering

enum class ScreenOrientation {
    PORTRAIT,
    LANDSCAPE,
    AUTO
};

inline std::ostream& operator<<(std::ostream& os, ScreenOrientation v) {
    switch (v) {
        case ScreenOrientation::PORTRAIT:  return os << "PORTRAIT";
        case ScreenOrientation::LANDSCAPE: return os << "LANDSCAPE";
        case ScreenOrientation::AUTO:      return os << "AUTO";
    }
    return os << static_cast<int>(v);
}

enum class DeviceType {
    PHONE,
    TABLET,
    DESKTOP
};

inline std::ostream& operator<<(std::ostream& os, DeviceType v) {
    switch (v) {
        case DeviceType::PHONE:   return os << "PHONE";
        case DeviceType::TABLET:  return os << "TABLET";
        case DeviceType::DESKTOP: return os << "DESKTOP";
    }
    return os << static_cast<int>(v);
}

struct ScreenDimensions {
    int32_t width;
    int32_t height;
    float dpi;
    ScreenOrientation orientation;
    DeviceType deviceType;
};

struct ResponsiveLayoutConfig {
    int32_t minWidth;
    int32_t maxWidth;
    int32_t optimalWidth;
    float scaleFactor;
    std::string breakpointName;
};

class MobileUIAdapter {
public:
    MobileUIAdapter();
    ~MobileUIAdapter();

    // Device detection and configuration
    bool detectDevice(const ScreenDimensions& dimensions);
    ScreenDimensions getCurrentScreenDimensions() const;
    DeviceType getDeviceType() const;

    // Responsive layout management
    bool registerBreakpoint(const ResponsiveLayoutConfig& config);
    ResponsiveLayoutConfig getBreakpointForWidth(int32_t width) const;
    std::vector<ResponsiveLayoutConfig> getAllBreakpoints() const;

    // Layout adjustment
    bool adjustLayoutForOrientation(ScreenOrientation orientation);
    bool scaleUIElements(float scaleFactor);
    bool optimizeLayoutForDevice(DeviceType deviceType);

    // Font and text scaling
    float getScaledFontSize(float baseSize) const;
    float getScaledTouchTargetSize(float baseSize) const;
    std::string getOptimalFont(const std::string& baseFont) const;

    // Component adaptation
    bool adaptComponentVisibility(const std::string& componentId, bool shouldShow);
    bool rearrangeLayout(const std::string& layoutId);
    std::map<std::string, bool> getComponentVisibilityMap() const;

    // Responsive image handling
    std::string getOptimalImageResolution(const std::string& imageId, const ScreenDimensions& dims);
    bool registerResponsiveImage(const std::string& imageId, const std::vector<std::string>& resolutions);

    // Viewport configuration
    bool setViewportMetadata(const std::string& metadata);
    std::string getViewportMetadata() const;

private:
    ScreenDimensions currentScreen;
    DeviceType currentDeviceType;
    std::vector<ResponsiveLayoutConfig> breakpoints;
    std::map<std::string, std::vector<std::string>> responsiveImages;
    std::map<std::string, bool> componentVisibility;
    float uiScaleFactor;
};

}  // namespace bendcore::mobile
