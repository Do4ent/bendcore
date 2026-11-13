#include <gtest/gtest.h>
#include <memory>
#include "../include/bendcore/mobile/touch_gestures.hpp"

class TouchGesturesTest : public ::testing::Test {
protected:
    std::unique_ptr<bendcore::mobile::TouchGestures> gestures;

    void SetUp() override {
        gestures = std::make_unique<bendcore::mobile::TouchGestures>();
    }
};

TEST_F(TouchGesturesTest, InitializeGestures) {
    EXPECT_NE(gestures.get(), nullptr);
}

TEST_F(TouchGesturesTest, DetectTap) {
    bool detected = gestures->detectTap(100, 200);
    EXPECT_TRUE(detected);
}

TEST_F(TouchGesturesTest, DetectDoubleTap) {
    bool detected = gestures->detectDoubleTap(100, 200);
    EXPECT_TRUE(detected);
}

TEST_F(TouchGesturesTest, DetectLongPress) {
    bool detected = gestures->detectLongPress(100, 200);
    EXPECT_TRUE(detected);
}

TEST_F(TouchGesturesTest, DetectSwipe) {
    bool detected = gestures->detectSwipe(100, 200, 150, 250, "down");
    EXPECT_TRUE(detected);
}

TEST_F(TouchGesturesTest, DetectPinch) {
    bool detected = gestures->detectPinch(1.5);
    EXPECT_TRUE(detected);
}

TEST_F(TouchGesturesTest, DetectRotation) {
    bool detected = gestures->detectRotation(45.0);
    EXPECT_TRUE(detected);
}

TEST_F(TouchGesturesTest, DetectPan) {
    bool detected = gestures->detectPan(100, 200, 150, 250);
    EXPECT_TRUE(detected);
}

TEST_F(TouchGesturesTest, MultiTouchSupport) {
    bool supported = gestures->supportsMultiTouch();
    EXPECT_TRUE(supported);
}

TEST_F(TouchGesturesTest, RegisterGestureHandler) {
    bool registered = gestures->registerGestureHandler("tap", []() {});
    EXPECT_TRUE(registered);
}

TEST_F(TouchGesturesTest, HandleGesture) {
    gestures->registerGestureHandler("tap", []() {});
    bool handled = gestures->handleGesture("tap", 100, 200);
    EXPECT_TRUE(handled);
}

TEST_F(TouchGesturesTest, RemoveGestureHandler) {
    gestures->registerGestureHandler("tap", []() {});
    bool removed = gestures->removeGestureHandler("tap");
    EXPECT_TRUE(removed);
}

TEST_F(TouchGesturesTest, CustomGesturePattern) {
    bool pattern = gestures->defineCustomGesture("circle", {{0, 0}, {100, 0}, {100, 100}});
    EXPECT_TRUE(pattern);
}

TEST_F(TouchGesturesTest, GestureContext) {
    gestures->detectTap(100, 200);
    auto context = gestures->getGestureContext();
    EXPECT_NE(context.gesture_type, "");
}

TEST_F(TouchGesturesTest, DisableTouchFeedback) {
    bool disabled = gestures->enableTouchFeedback(false);
    EXPECT_TRUE(disabled);
}

TEST_F(TouchGesturesTest, EnablePressureSensitivity) {
    bool enabled = gestures->enablePressureSensitivity(true);
    EXPECT_TRUE(enabled);
}

TEST_F(TouchGesturesTest, DetectEdgeSwipe) {
    bool detected = gestures->detectEdgeSwipe("left");
    EXPECT_TRUE(detected);
}

TEST_F(TouchGesturesTest, GestureVelocity) {
    auto velocity = gestures->getGestureVelocity();
    EXPECT_GE(velocity, 0.0f);
}

TEST_F(TouchGesturesTest, TouchPoint) {
    auto points = gestures->getTouchPoints();
    EXPECT_GE(points.size(), 0);
}

TEST_F(TouchGesturesTest, CancelGesture) {
    gestures->detectTap(100, 200);
    bool cancelled = gestures->cancelCurrentGesture();
    EXPECT_TRUE(cancelled);
}

TEST_F(TouchGesturesTest, RecognizeGesture) {
    bool recognized = gestures->recognizeGesture({{0, 0}, {100, 100}});
    EXPECT_TRUE(recognized);
}

TEST_F(TouchGesturesTest, GestureConfidence) {
    gestures->detectTap(100, 200);
    auto confidence = gestures->getGestureConfidence();
    EXPECT_GE(confidence, 0.0f);
}

TEST_F(TouchGesturesTest, CombinedGestures) {
    bool detected = gestures->detectCombinedGesture({"pinch", "rotate"});
    EXPECT_TRUE(detected);
}

TEST_F(TouchGesturesTest, GestureExclusivity) {
    bool exclusive = gestures->enableGestureExclusivity(true);
    EXPECT_TRUE(exclusive);
}

TEST_F(TouchGesturesTest, GestureTimeout) {
    gestures->setGestureTimeout(500);
    auto timeout = gestures->getGestureTimeout();
    EXPECT_EQ(timeout, 500);
}

TEST_F(TouchGesturesTest, GestureThreshold) {
    gestures->setTapThreshold(100);
    auto threshold = gestures->getTapThreshold();
    EXPECT_EQ(threshold, 100);
}

TEST_F(TouchGesturesTest, PrimaryTouchFinger) {
    gestures->detectTap(100, 200);
    auto primary = gestures->getPrimaryTouchFinger();
    EXPECT_NE(primary, -1);
}

TEST_F(TouchGesturesTest, GestureHistory) {
    gestures->detectTap(100, 200);
    gestures->detectSwipe(100, 200, 150, 250, "right");
    auto history = gestures->getGestureHistory(5);
    EXPECT_GT(history.size(), 0);
}

TEST_F(TouchGesturesTest, GesturesVersion) {
    auto version = gestures->getGestureVersion();
    EXPECT_FALSE(version.empty());
}

TEST_F(TouchGesturesTest, ClearGestures) {
    gestures->registerGestureHandler("tap", []() {});
    bool cleared = gestures->clearAllGestureHandlers();
    EXPECT_TRUE(cleared);
}

TEST_F(TouchGesturesTest, SimulateGesture) {
    bool simulated = gestures->simulateGesture("tap", 100, 200);
    EXPECT_TRUE(simulated);
}

TEST_F(TouchGesturesTest, GestureRecording) {
    auto record = gestures->startRecordingGesture();
    gestures->detectTap(100, 200);
    auto recorded = gestures->stopRecordingGesture();
    EXPECT_TRUE(recorded);
}

TEST_F(TouchGesturesTest, PlayRecordedGesture) {
    auto recorded = gestures->startRecordingGesture();
    gestures->detectTap(100, 200);
    gestures->stopRecordingGesture();
    bool played = gestures->playRecordedGesture(recorded);
    EXPECT_TRUE(played);
}

TEST_F(TouchGesturesTest, AsyncGestureDetection) {
    auto future = gestures->detectGestureAsync(100, 200);
    EXPECT_TRUE(future.valid());
}
