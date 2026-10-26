#pragma once

#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <cstdint>

namespace bendcore::mobile {

// ==============================================================================
// TOUCH GESTURES - GAP-080
// ==============================================================================
// Tap, swipe, pinch, rotate, and multi-touch gesture recognition and handling

enum class GestureType {
    TAP,
    DOUBLE_TAP,
    LONG_PRESS,
    SWIPE,
    PINCH,
    ROTATE,
    PAN,
    MULTI_TOUCH
};

enum class SwipeDirection {
    UP,
    DOWN,
    LEFT,
    RIGHT
};

struct TouchPoint {
    float x;
    float y;
    int32_t pointerId;
    int64_t timestamp;
};

struct GestureEvent {
    GestureType type;
    std::vector<TouchPoint> touchPoints;
    int32_t fingerCount;
    float velocity;
    float scale;
    float rotation;
    int64_t startTime;
    int64_t endTime;
};

using GestureCallback = std::function<void(const GestureEvent&)>;

class TouchGestureRecognizer {
public:
    TouchGestureRecognizer();
    ~TouchGestureRecognizer();

    // Touch event handling
    bool handleTouchDown(const std::vector<TouchPoint>& points);
    bool handleTouchMove(const std::vector<TouchPoint>& points);
    bool handleTouchUp(const std::vector<TouchPoint>& points);
    bool handleTouchCancel();

    // Gesture recognition
    GestureType recognizeGesture(const std::vector<TouchPoint>& points);
    std::vector<GestureEvent> getPendingGestures();

    // Gesture configuration
    void setTapThreshold(float pixels);
    void setSwipeThreshold(float pixels);
    void setLongPressDelay(int64_t milliseconds);
    void setDoubleTapWindow(int64_t milliseconds);

    // Gesture callbacks
    void registerGestureCallback(GestureType type, GestureCallback callback);
    void unregisterGestureCallback(GestureType type);
    bool fireGestureCallback(const GestureEvent& event);

    // Swipe detection
    SwipeDirection detectSwipeDirection(const TouchPoint& start, const TouchPoint& end);
    bool isSwipe(const std::vector<TouchPoint>& points, SwipeDirection& direction);

    // Pinch detection
    bool isPinch(const std::vector<TouchPoint>& points, float& scale);
    float calculatePinchScale(const std::vector<TouchPoint>& points);

    // Rotation detection
    bool isRotation(const std::vector<TouchPoint>& points, float& angle);
    float calculateRotationAngle(const std::vector<TouchPoint>& points);

    // Multi-touch handling
    int32_t getActiveTouchCount() const;
    std::vector<TouchPoint> getActiveTouchPoints() const;

    // Gesture validation
    bool validateGestureThresholds(const GestureEvent& event);
    void resetGestureState();

private:
    std::vector<TouchPoint> currentTouches;
    std::vector<TouchPoint> previousTouches;
    std::vector<GestureEvent> pendingGestures;
    std::map<GestureType, GestureCallback> callbacks;

    float tapThreshold;
    float swipeThreshold;
    int64_t longPressDelay;
    int64_t doubleTapWindow;
};

}  // namespace bendcore::mobile
