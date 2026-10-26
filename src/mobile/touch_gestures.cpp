#include "bendcore/mobile/touch_gestures.hpp"
#include <algorithm>
#include <cmath>

namespace bendcore::mobile {

TouchGestureRecognizer::TouchGestureRecognizer()
    : tapThreshold(10.0f),
      swipeThreshold(100.0f),
      longPressDelay(500),
      doubleTapWindow(300) {}

TouchGestureRecognizer::~TouchGestureRecognizer() = default;

bool TouchGestureRecognizer::handleTouchDown(const std::vector<TouchPoint>& points) {
    previousTouches = currentTouches;
    currentTouches = points;
    return true;
}

bool TouchGestureRecognizer::handleTouchMove(const std::vector<TouchPoint>& points) {
    previousTouches = currentTouches;
    currentTouches = points;
    return true;
}

bool TouchGestureRecognizer::handleTouchUp(const std::vector<TouchPoint>& points) {
    previousTouches = currentTouches;
    currentTouches = points;
    return true;
}

bool TouchGestureRecognizer::handleTouchCancel() {
    currentTouches.clear();
    previousTouches.clear();
    return true;
}

GestureType TouchGestureRecognizer::recognizeGesture(const std::vector<TouchPoint>& points) {
    if (points.empty()) return GestureType::TAP;

    if (points.size() == 1) {
        return GestureType::TAP;
    } else if (points.size() == 2) {
        return GestureType::PINCH;
    }
    return GestureType::MULTI_TOUCH;
}

std::vector<GestureEvent> TouchGestureRecognizer::getPendingGestures() {
    return pendingGestures;
}

void TouchGestureRecognizer::setTapThreshold(float pixels) {
    tapThreshold = pixels;
}

void TouchGestureRecognizer::setSwipeThreshold(float pixels) {
    swipeThreshold = pixels;
}

void TouchGestureRecognizer::setLongPressDelay(int64_t milliseconds) {
    longPressDelay = milliseconds;
}

void TouchGestureRecognizer::setDoubleTapWindow(int64_t milliseconds) {
    doubleTapWindow = milliseconds;
}

void TouchGestureRecognizer::registerGestureCallback(GestureType type, GestureCallback callback) {
    callbacks[type] = callback;
}

void TouchGestureRecognizer::unregisterGestureCallback(GestureType type) {
    callbacks.erase(type);
}

bool TouchGestureRecognizer::fireGestureCallback(const GestureEvent& event) {
    auto it = callbacks.find(event.type);
    if (it != callbacks.end()) {
        it->second(event);
        return true;
    }
    return false;
}

SwipeDirection TouchGestureRecognizer::detectSwipeDirection(const TouchPoint& start, const TouchPoint& end) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;

    if (std::abs(dx) > std::abs(dy)) {
        return dx > 0 ? SwipeDirection::RIGHT : SwipeDirection::LEFT;
    }
    return dy > 0 ? SwipeDirection::DOWN : SwipeDirection::UP;
}

bool TouchGestureRecognizer::isSwipe(const std::vector<TouchPoint>& points, SwipeDirection& direction) {
    if (points.size() < 2) return false;

    float distance = std::sqrt(std::pow(points.back().x - points.front().x, 2) +
                               std::pow(points.back().y - points.front().y, 2));
    if (distance > swipeThreshold) {
        direction = detectSwipeDirection(points.front(), points.back());
        return true;
    }
    return false;
}

bool TouchGestureRecognizer::isPinch(const std::vector<TouchPoint>& points, float& scale) {
    if (points.size() < 2) return false;
    scale = calculatePinchScale(points);
    return std::abs(scale - 1.0f) > 0.1f;
}

float TouchGestureRecognizer::calculatePinchScale(const std::vector<TouchPoint>& points) {
    if (points.size() < 2) return 1.0f;

    float distance = std::sqrt(std::pow(points[1].x - points[0].x, 2) +
                               std::pow(points[1].y - points[0].y, 2));
    return distance > 0 ? 1.0f : 1.0f;
}

bool TouchGestureRecognizer::isRotation(const std::vector<TouchPoint>& points, float& angle) {
    angle = calculateRotationAngle(points);
    return std::abs(angle) > 5.0f;
}

float TouchGestureRecognizer::calculateRotationAngle(const std::vector<TouchPoint>& points) {
    if (points.size() < 2) return 0.0f;
    return 0.0f;
}

int32_t TouchGestureRecognizer::getActiveTouchCount() const {
    return currentTouches.size();
}

std::vector<TouchPoint> TouchGestureRecognizer::getActiveTouchPoints() const {
    return currentTouches;
}

bool TouchGestureRecognizer::validateGestureThresholds(const GestureEvent& event) {
    return true;
}

void TouchGestureRecognizer::resetGestureState() {
    currentTouches.clear();
    previousTouches.clear();
    pendingGestures.clear();
}

}  // namespace bendcore::mobile
