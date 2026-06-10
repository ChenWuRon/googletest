#include <gtest/gtest.h>
#include "resource_manager/state/runtime_event.h"

using namespace resource_manager;

TEST(RuntimeEventTest, EventCreation) {
    RuntimeEvent event(EventType::ProcessDiscovered, 1234, "discovery");

    EXPECT_EQ(event.type(), EventType::ProcessDiscovered);
    EXPECT_EQ(event.pid(), 1234);
    EXPECT_EQ(event.source(), "discovery");
}

TEST(RuntimeEventTest, EventSerialization) {
    RuntimeEvent event(EventType::ProcessAttached, 5678, "attach_engine");
    auto serialized = event.serialize();

    EXPECT_NE(serialized.find("attach_engine"), std::string::npos);
    EXPECT_NE(serialized.find("ProcessAttached"), std::string::npos);
    EXPECT_NE(serialized.find("pid=5678"), std::string::npos);
}

TEST(RuntimeEventTest, EventTypeToString) {
    EXPECT_EQ(eventTypeToString(EventType::ProcessDiscovered), "ProcessDiscovered");
    EXPECT_EQ(eventTypeToString(EventType::ProcessLost), "ProcessLost");
    EXPECT_EQ(eventTypeToString(EventType::ProcessRestarted), "ProcessRestarted");
    EXPECT_EQ(eventTypeToString(EventType::ProcessAttached), "ProcessAttached");
    EXPECT_EQ(eventTypeToString(EventType::ProcessDetached), "ProcessDetached");
    EXPECT_EQ(eventTypeToString(EventType::RecoveryStarted), "RecoveryStarted");
    EXPECT_EQ(eventTypeToString(EventType::RecoverySucceeded), "RecoverySucceeded");
    EXPECT_EQ(eventTypeToString(EventType::RecoveryFailed), "RecoveryFailed");
}
