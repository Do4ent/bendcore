#include <gtest/gtest.h>
#include <memory>
#include "../include/bendcore/analytics/usage_tracking.hpp"

class UsageTrackingTest : public ::testing::Test {
protected:
    std::unique_ptr<bendcore::analytics::UsageTracking> tracker;

    void SetUp() override {
        tracker = std::make_unique<bendcore::analytics::UsageTracking>();
    }
};

TEST_F(UsageTrackingTest, InitializeTracking) {
    EXPECT_NE(tracker.get(), nullptr);
}

TEST_F(UsageTrackingTest, TrackEvent) {
    tracker->trackEvent("user1", "design1", "open");
    auto events = tracker->getUserEvents("user1");
    EXPECT_GT(events.size(), 0);
}

TEST_F(UsageTrackingTest, TrackFeatureUsage) {
    tracker->trackFeatureUsage("user1", "bend_tool");
    auto features = tracker->getFeatureUsageCount("bend_tool");
    EXPECT_GT(features, 0);
}

TEST_F(UsageTrackingTest, TrackSessionDuration) {
    tracker->startSession("user1", "session1");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto duration = tracker->getSessionDuration("user1", "session1");
    EXPECT_GE(duration, 100);
}

TEST_F(UsageTrackingTest, TrackUserJourney) {
    tracker->trackEvent("user1", "project1", "create");
    tracker->trackEvent("user1", "project1", "edit");
    tracker->trackEvent("user1", "project1", "save");
    auto journey = tracker->getUserJourney("user1", "project1");
    EXPECT_EQ(journey.size(), 3);
}

TEST_F(UsageTrackingTest, GetUserActivitySummary) {
    tracker->trackEvent("user1", "design1", "open");
    tracker->trackEvent("user1", "design1", "edit");
    auto summary = tracker->getActivitySummary("user1");
    EXPECT_FALSE(summary.empty());
}

TEST_F(UsageTrackingTest, TrackCustomEvent) {
    tracker->trackCustomEvent("user1", "custom_action", {{"key", "value"}});
    auto custom = tracker->getCustomEvents("user1");
    EXPECT_GT(custom.size(), 0);
}

TEST_F(UsageTrackingTest, GetTopFeatures) {
    tracker->trackFeatureUsage("user1", "bend_tool");
    tracker->trackFeatureUsage("user1", "bend_tool");
    tracker->trackFeatureUsage("user1", "straight_tool");
    auto top = tracker->getTopFeatures(1);
    EXPECT_EQ(top[0], "bend_tool");
}

TEST_F(UsageTrackingTest, GetUserRetention) {
    tracker->startSession("user1", "session1");
    tracker->startSession("user1", "session2");
    auto retention = tracker->getUserRetention("user1");
    EXPECT_GT(retention, 0.0f);
}

TEST_F(UsageTrackingTest, TrackConversion) {
    tracker->trackEvent("user1", "project1", "view");
    tracker->trackEvent("user1", "project1", "create");
    auto conversion = tracker->getConversionRate("view", "create");
    EXPECT_GE(conversion, 0.0f);
}

TEST_F(UsageTrackingTest, GetActiveUsers) {
    tracker->startSession("user1", "session1");
    tracker->startSession("user2", "session2");
    auto active = tracker->getActiveUserCount();
    EXPECT_GE(active, 2);
}

TEST_F(UsageTrackingTest, TrackErrorEvents) {
    tracker->trackErrorEvent("user1", "design1", "geometry_error");
    auto errors = tracker->getErrorEvents("user1", "design1");
    EXPECT_GT(errors.size(), 0);
}

TEST_F(UsageTrackingTest, GetUserSegmentation) {
    tracker->trackEvent("user1", "design1", "open");
    tracker->trackEvent("user2", "design2", "open");
    auto segments = tracker->getUserSegments();
    EXPECT_GT(segments.size(), 0);
}

TEST_F(UsageTrackingTest, TrackSearchQueries) {
    tracker->trackSearchQuery("user1", "bend radius");
    auto queries = tracker->getSearchHistory("user1");
    EXPECT_GT(queries.size(), 0);
}

TEST_F(UsageTrackingTest, GetPopularDesigns) {
    tracker->trackEvent("user1", "design1", "open");
    tracker->trackEvent("user2", "design1", "open");
    tracker->trackEvent("user3", "design1", "open");
    auto popular = tracker->getMostPopularDesigns(1);
    EXPECT_EQ(popular[0], "design1");
}

TEST_F(UsageTrackingTest, TrackAbandonmentRate) {
    tracker->trackEvent("user1", "design1", "open");
    auto abandon_rate = tracker->getAbandonmentRate("design1");
    EXPECT_GE(abandon_rate, 0.0f);
}

TEST_F(UsageTrackingTest, GenerateUsageReport) {
    tracker->trackEvent("user1", "design1", "open");
    auto report = tracker->generateUsageReport();
    EXPECT_FALSE(report.empty());
}

TEST_F(UsageTrackingTest, ExportEventData) {
    tracker->trackEvent("user1", "design1", "open");
    auto exported = tracker->exportEventData("csv");
    EXPECT_FALSE(exported.empty());
}

TEST_F(UsageTrackingTest, TrackClickstream) {
    tracker->trackClickstream("user1", {"/home", "/projects", "/design1", "/settings"});
    auto stream = tracker->getClickstream("user1");
    EXPECT_EQ(stream.size(), 4);
}

TEST_F(UsageTrackingTest, GetCohortAnalysis) {
    tracker->trackEvent("user1", "design1", "open");
    tracker->trackEvent("user2", "design1", "open");
    auto cohort = tracker->getCohortAnalysis("open");
    EXPECT_GT(cohort.size(), 0);
}

TEST_F(UsageTrackingTest, TrackFunnelAnalysis) {
    tracker->trackEvent("user1", "step1", "enter");
    tracker->trackEvent("user1", "step2", "enter");
    tracker->trackEvent("user1", "step3", "complete");
    auto funnel = tracker->getFunnelAnalysis({"step1", "step2", "step3"});
    EXPECT_GT(funnel.size(), 0);
}

TEST_F(UsageTrackingTest, GetAttributionModel) {
    tracker->trackEvent("user1", "channel1", "view");
    tracker->trackEvent("user1", "channel2", "click");
    tracker->trackEvent("user1", "channel3", "convert");
    auto attribution = tracker->getAttributionModel("user1");
    EXPECT_FALSE(attribution.empty());
}

TEST_F(UsageTrackingTest, TrackABTest) {
    tracker->trackABTestEvent("user1", "test_a", "variant_a");
    tracker->trackABTestEvent("user2", "test_a", "variant_b");
    auto ab_results = tracker->getABTestResults("test_a");
    EXPECT_GT(ab_results.size(), 0);
}

TEST_F(UsageTrackingTest, GetUserLifetimeValue) {
    tracker->startSession("user1", "session1");
    tracker->trackEvent("user1", "design1", "open");
    auto ltv = tracker->getUserLifetimeValue("user1");
    EXPECT_GE(ltv, 0.0f);
}

TEST_F(UsageTrackingTest, TrackDeviceInfo) {
    tracker->trackDeviceInfo("user1", "desktop", "Windows", "Chrome");
    auto devices = tracker->getUserDevices("user1");
    EXPECT_GT(devices.size(), 0);
}

TEST_F(UsageTrackingTest, GetGeographicDistribution) {
    tracker->trackGeoLocation("user1", "US", "New York");
    tracker->trackGeoLocation("user2", "UK", "London");
    auto geo = tracker->getGeographicDistribution();
    EXPECT_GT(geo.size(), 0);
}

TEST_F(UsageTrackingTest, TrackSessionProperties) {
    tracker->setSessionProperty("user1", "session1", "premium", true);
    auto props = tracker->getSessionProperties("user1", "session1");
    EXPECT_FALSE(props.empty());
}

TEST_F(UsageTrackingTest, GetTrackingVersion) {
    auto version = tracker->getTrackingVersion();
    EXPECT_FALSE(version.empty());
}

TEST_F(UsageTrackingTest, AsyncEventTracking) {
    auto future = tracker->trackEventAsync("user1", "design1", "open");
    EXPECT_TRUE(future.valid());
}

TEST_F(UsageTrackingTest, BulkEventTracking) {
    std::vector<bendcore::analytics::TrackingEvent> events;
    events.push_back({"user1", "design1", "open"});
    events.push_back({"user1", "design1", "edit"});
    auto bulk = tracker->trackBulkEvents(events);
    EXPECT_TRUE(bulk);
}

TEST_F(UsageTrackingTest, RealtimeEventStream) {
    auto stream = tracker->getRealtimeEventStream();
    tracker->trackEvent("user1", "design1", "open");
    EXPECT_TRUE(stream.valid());
}

TEST_F(UsageTrackingTest, EventDeduplication) {
    tracker->trackEvent("user1", "design1", "open");
    tracker->trackEvent("user1", "design1", "open");
    auto events = tracker->getUserEvents("user1");
    EXPECT_EQ(events.size(), 1);
}
