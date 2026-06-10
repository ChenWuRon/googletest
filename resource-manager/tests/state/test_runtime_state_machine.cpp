#include <gtest/gtest.h>
#include "resource_manager/state/runtime_state_machine.h"

using namespace resource_manager;

TEST(RuntimeStateMachineTest, InitialState) {
    RuntimeStateMachine fsm;
    EXPECT_EQ(fsm.current(), RuntimeStateName::Unknown);
}

TEST(RuntimeStateMachineTest, AllValidTransitions) {
    RuntimeStateMachine fsm;

    EXPECT_TRUE(fsm.transition(RuntimeStateName::Discovered));
    EXPECT_EQ(fsm.current(), RuntimeStateName::Discovered);

    EXPECT_TRUE(fsm.transition(RuntimeStateName::Attached));
    EXPECT_EQ(fsm.current(), RuntimeStateName::Attached);

    EXPECT_TRUE(fsm.transition(RuntimeStateName::Running));
    EXPECT_EQ(fsm.current(), RuntimeStateName::Running);

    EXPECT_TRUE(fsm.transition(RuntimeStateName::Lost));
    EXPECT_EQ(fsm.current(), RuntimeStateName::Lost);

    EXPECT_TRUE(fsm.transition(RuntimeStateName::Recovering));
    EXPECT_EQ(fsm.current(), RuntimeStateName::Recovering);

    EXPECT_TRUE(fsm.transition(RuntimeStateName::Running));
    EXPECT_EQ(fsm.current(), RuntimeStateName::Running);

    EXPECT_TRUE(fsm.transition(RuntimeStateName::Lost));
    EXPECT_TRUE(fsm.transition(RuntimeStateName::Recovering));
    EXPECT_TRUE(fsm.transition(RuntimeStateName::Failed));
    EXPECT_EQ(fsm.current(), RuntimeStateName::Failed);
}

TEST(RuntimeStateMachineTest, AllInvalidTransitions) {
    RuntimeStateMachine fsm;

    EXPECT_FALSE(fsm.transition(RuntimeStateName::Unknown));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Attached));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Running));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Lost));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Recovering));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Failed));
    EXPECT_EQ(fsm.current(), RuntimeStateName::Unknown);
}

TEST(RuntimeStateMachineTest, InvalidTransitionsFromEachState) {
    RuntimeStateMachine fsm;

    EXPECT_TRUE(fsm.transition(RuntimeStateName::Discovered));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Unknown));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Running));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Lost));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Recovering));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Failed));

    EXPECT_TRUE(fsm.transition(RuntimeStateName::Attached));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Unknown));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Discovered));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Lost));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Recovering));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Failed));

    EXPECT_TRUE(fsm.transition(RuntimeStateName::Running));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Unknown));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Discovered));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Attached));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Recovering));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Failed));

    EXPECT_TRUE(fsm.transition(RuntimeStateName::Lost));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Unknown));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Discovered));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Attached));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Running));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Failed));

    EXPECT_TRUE(fsm.transition(RuntimeStateName::Recovering));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Unknown));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Discovered));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Attached));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Lost));

    EXPECT_TRUE(fsm.transition(RuntimeStateName::Failed));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Unknown));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Discovered));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Attached));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Running));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Lost));
    EXPECT_FALSE(fsm.transition(RuntimeStateName::Recovering));
}

TEST(RuntimeStateMachineTest, CanTransitionTo) {
    RuntimeStateMachine fsm;
    EXPECT_TRUE(fsm.canTransitionTo(RuntimeStateName::Discovered));
    EXPECT_FALSE(fsm.canTransitionTo(RuntimeStateName::Attached));
    EXPECT_FALSE(fsm.canTransitionTo(RuntimeStateName::Failed));
}

TEST(RuntimeStateMachineTest, Reset) {
    RuntimeStateMachine fsm;
    fsm.transition(RuntimeStateName::Discovered);
    EXPECT_EQ(fsm.current(), RuntimeStateName::Discovered);
    fsm.reset();
    EXPECT_EQ(fsm.current(), RuntimeStateName::Unknown);
}

TEST(RuntimeStateMachineTest, RuntimeStateNameToString) {
    EXPECT_EQ(runtimeStateNameToString(RuntimeStateName::Unknown), "Unknown");
    EXPECT_EQ(runtimeStateNameToString(RuntimeStateName::Discovered), "Discovered");
    EXPECT_EQ(runtimeStateNameToString(RuntimeStateName::Attached), "Attached");
    EXPECT_EQ(runtimeStateNameToString(RuntimeStateName::Running), "Running");
    EXPECT_EQ(runtimeStateNameToString(RuntimeStateName::Lost), "Lost");
    EXPECT_EQ(runtimeStateNameToString(RuntimeStateName::Recovering), "Recovering");
    EXPECT_EQ(runtimeStateNameToString(RuntimeStateName::Failed), "Failed");
}
