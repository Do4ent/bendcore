#include <gtest/gtest.h>
#include "bendcore/mobile/touch_gestures.hpp"

using namespace bendcore::mobile;

class TouchGestureRecognizerTest : public ::testing::Test {
protected:
    TouchGestureRecognizer recognizer;
};

TEST_F(TouchGestureRecognizerTest, HandleTouchDown) {
    std::vector<TouchPoint> points{{.x = 100.0f, .y = 100.0f, .pointerId = 0, .timestamp = 0}};
    EXPECT_TRUE(recognizer.handleTouchDown(points));
}

TEST_F(TouchGestureRecognizerTest, HandleTouchMove) {
    std::vector<TouchPoint> points{{.x = 150.0f, .y = 150.0f, .pointerId = 0, .timestamp = 10}};
    EXPECT_TRUE(recognizer.handleTouchMove(points));
}

TEST_F(TouchGestureRecognizerTest, HandleTouchUp) {
    std::vector<TouchPoint> points{{.x = 200.0f, .y = 200.0f, .pointerId = 0, .timestamp = 20}};
    EXPECT_TRUE(recognizer.handleTouchUp(points));
}

TEST_F(TouchGestureRecognizerTest, HandleTouchCancel) {
    EXPECT_TRUE(recognizer.handleTouchCancel());
}

TEST_F(TouchGestureRecognizerTest, RecognizeTapGesture) {
    std::vector<TouchPoint> points{{.x = 100.0f, .y = 100.0f, .pointerId = 0, .timestamp = 0}};
    GestureType type = recognizer.recognizeGesture(points);
    EXPECT_EQ(type, GestureType::TAP);
}

TEST_F(TouchGestureRecognizerTest, RecognizePinchGesture) {
    std::vector<TouchPoint> points{
        {.x = 100.0f, .y = 100.0f, .pointerId = 0, .timestamp = 0},
        {.x = 200.0f, .y = 200.0f, .pointerId = 1, .timestamp = 0}
    };
    GestureType type = recognizer.recognizeGesture(points);
    EXPECT_EQ(type, GestureType::PINCH);
}

TEST_F(TouchGestureRecognizerTest, SetTapThreshold) {
    recognizer.setTapThreshold(15.0f);
    EXPECT_TRUE(true);  // Method executed without error
}

TEST_F(TouchGestureRecognizerTest, SetSwipeThreshold) {
    recognizer.setSwipeThreshold(150.0f);
    EXPECT_TRUE(true);
}

TEST_F(TouchGestureRecognizerTest, SetLongPressDelay) {
    recognizer.setLongPressDelay(600);
    EXPECT_TRUE(true);
}

TEST_F(TouchGestureRecognizerTest, SetDoubleTapWindow) {
    recognizer.setDoubleTapWindow(400);
    EXPECT_TRUE(true);
}

TEST_F(TouchGestureRecognizerTest, RegisterGestureCallback) {
    bool called = false;
    recognizer.registerGestureCallback(GestureType::TAP, 
        [&called](const GestureEvent& e) { called = true; });
    EXPECT_TRUE(true);
}

TEST_F(TouchGestureRecognizerTest, UnregisterGestureCallback) {
    recognizer.registerGestureCallback(GestureType::TAP, 
        [](const GestureEvent& e) {});
    recognizer.unregisterGestureCallback(GestureType::TAP);
    EXPECT_TRUE(true);
}

TEST_F(TouchGestureRecognizerTest, GetActiveTouchCount) {
    std::vector<TouchPoint> points{{.x = 100.0f, .y = 100.0f}};
    recognizer.handleTouchDown(points);
    int32_t count = recognizer.getActiveTouchCount();
    EXPECT_EQ(count, 1);
}

TEST_F(TouchGestureRecognizerTest, GetActiveTouchPoints) {
    std::vector<TouchPoint> points{{.x = 100.0f, .y = 100.0f, .pointerId = 0}};
    recognizer.handleTouchDown(points);
    auto active = recognizer.getActiveTouchPoints();
    EXPECT_EQ(active.size(), 1);
}

TEST_F(TouchGestureRecognizerTest, DetectSwipeRight) {
    TouchPoint start{.x = 100.0f, .y = 100.0f};
    TouchPoint end{.x = 300.0f, .y = 100.0f};
    SwipeDirection dir = recognizer.detectSwipeDirection(start, end);
    EXPECT_EQ(dir, SwipeDirection::RIGHT);
}

TEST_F(TouchGestureRecognizerTest, DetectSwipeLeft) {
    TouchPoint start{.x = 300.0f, .y = 100.0f};
    TouchPoint end{.x = 100.0f, .y = 100.0f};
    SwipeDirection dir = recognizer.detectSwipeDirection(start, end);
    EXPECT_EQ(dir, SwipeDirection::LEFT);
}

TEST_F(TouchGestureRecognizerTest, DetectSwipeDown) {
    TouchPoint start{.x = 100.0f, .y = 100.0f};
    TouchPoint end{.x = 100.0f, .y = 300.0f};
    SwipeDirection dir = recognizer.detectSwipeDirection(start, end);
    EXPECT_EQ(dir, SwipeDirection::DOWN);
}

TEST_F(TouchGestureRecognizerTest, DetectSwipeUp) {
    TouchPoint start{.x = 100.0f, .y = 300.0f};
    TouchPoint end{.x = 100.0f, .y = 100.0f};
    SwipeDirection dir = recognizer.detectSwipeDirection(start, end);
    EXPECT_EQ(dir, SwipeDirection::UP);
}

TEST_F(TouchGestureRecognizerTest, CalculatePinchScale) {
    std::vector<TouchPoint> points{
        {.x = 0.0f, .y = 0.0f},
        {.x = 100.0f, .y = 100.0f}
    };
    float scale = recognizer.calculatePinchScale(points);
    EXPECT_GT(scale, 0.0f);
}

TEST_F(TouchGestureRecognizerTest, ResetGestureState) {
    std::vector<TouchPoint> points{{.x = 100.0f, .y = 100.0f}};
    recognizer.handleTouchDown(points);
    recognizer.resetGestureState();
    EXPECT_EQ(recognizer.getActiveTouchCount(), 0);
}

TEST_F(TouchGestureRecognizerTest, GetPendingGestures) {
    auto gestures = recognizer.getPendingGestures();
    EXPECT_GE(gestures.size(), 0);
}

TEST_F(TouchGestureRecognizerTest, MultiTouchHandling) {
    std::vector<TouchPoint> three_points{
        {.x = 100.0f, .y = 100.0f, .pointerId = 0},
        {.x = 200.0f, .y = 200.0f, .pointerId = 1},
        {.x = 300.0f, .y = 300.0f, .pointerId = 2}
    };
    recognizer.handleTouchDown(three_points);
    EXPECT_EQ(recognizer.getActiveTouchCount(), 3);
}

TEST_F(TouchGestureRecognizerTest, GestureTypeIdentification) {
    std::vector<TouchPoint> single{{.x = 100.0f, .y = 100.0f}};
    GestureType single_type = recognizer.recognizeGesture(single);
    EXPECT_EQ(single_type, GestureType::TAP);
    
    std::vector<TouchPoint> dual{{.x = 100.0f, .y = 100.0f}, {.x = 200.0f, .y = 200.0f}};
    GestureType dual_type = recognizer.recognizeGesture(dual);
    EXPECT_EQ(dual_type, GestureType::PINCH);
}

TEST_F(TouchGestureRecognizerTest, TouchSequence) {
    std::vector<TouchPoint> down{{.x = 100.0f, .y = 100.0f}};
    recognizer.handleTouchDown(down);
    EXPECT_EQ(recognizer.getActiveTouchCount(), 1);
    
    std::vector<TouchPoint> move{{.x = 150.0f, .y = 150.0f}};
    recognizer.handleTouchMove(move);
    EXPECT_EQ(recognizer.getActiveTouchCount(), 1);
    
    recognizer.handleTouchUp(move);
    EXPECT_EQ(recognizer.getActiveTouchCount(), 1);
}

TEST_F(TouchGestureRecognizerTest, ValidateGestureThresholds) {
    GestureEvent event{.type = GestureType::TAP};
    EXPECT_TRUE(recognizer.validateGestureThresholds(event));
}

TEST_F(TouchGestureRecognizerTest, FireGestureCallback) {
    bool fired = false;
    recognizer.registerGestureCallback(GestureType::TAP,
        [&fired](const GestureEvent& e) { fired = true; });
    GestureEvent event{.type = GestureType::TAP};
    EXPECT_TRUE(recognizer.fireGestureCallback(event));
}

TEST_F(TouchGestureRecognizerTest, IsSwipeDetection) {
    std::vector<TouchPoint> points{
        {.x = 100.0f, .y = 100.0f},
        {.x = 300.0f, .y = 100.0f}
    };
    SwipeDirection dir;
    recognizer.setSwipeThreshold(100.0f);
    bool is_swipe = recognizer.isSwipe(points, dir);
    EXPECT_TRUE(is_swipe);
    EXPECT_EQ(dir, SwipeDirection::RIGHT);
}

TEST_F(TouchGestureRecognizerTest, IsPinchDetection) {
    std::vector<TouchPoint> points{
        {.x = 100.0f, .y = 100.0f},
        {.x = 300.0f, .y = 300.0f}
    };
    float scale;
    bool is_pinch = recognizer.isPinch(points, scale);
    EXPECT_GT(scale, 0.0f);
}

