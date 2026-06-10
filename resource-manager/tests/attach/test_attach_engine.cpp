#include <gtest/gtest.h>
#include <memory>

#include "resource_manager/attach/attach_engine.h"
#include "resource_manager/driver/mock_cgroup_driver.h"

using namespace resource_manager;

class AttachEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_ = std::make_unique<MockCgroupDriver>();
        mockPtr_ = mock_.get();
    }

    std::unique_ptr<ConfigNode> makeGroup(const std::string& name) {
        return std::make_unique<ConfigNode>(ConfigNodeType::GROUP, name);
    }

    std::unique_ptr<ConfigNode> makeController(const std::string& name) {
        return std::make_unique<ConfigNode>(ConfigNodeType::CONTROLLER, name);
    }

    std::unique_ptr<ConfigNode> makeItem(const std::string& name, const std::string& value) {
        return std::make_unique<ConfigNode>(ConfigNodeType::ITEM, name, value);
    }

    ConfigNode* addController(ConfigNode& group, std::unique_ptr<ConfigNode> ctrl) {
        return group.addChild(std::move(ctrl));
    }

    void addItem(ConfigNode& ctrl, std::unique_ptr<ConfigNode> item) {
        ctrl.addChild(std::move(item));
    }

    std::unique_ptr<MockCgroupDriver> mock_;
    MockCgroupDriver* mockPtr_;
};

TEST_F(AttachEngineTest, SuccessfulAttach) {
    AttachEngine engine(std::move(mock_));

    auto group = makeGroup("web");
    auto* cpuCtrl = addController(*group, makeController("cpu"));
    addItem(*cpuCtrl, makeItem("cpu.max", "100000 100000"));

    RuntimeState state;
    state.updatePid(1234, "web");

    auto err = engine.attach(*group, state);
    EXPECT_FALSE(err.has_value());
    EXPECT_TRUE(mockPtr_->exists("web"));
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::Attached);
    EXPECT_EQ(state.processState().attachedGroupPath, "web");

    auto val = mockPtr_->getValue("web", "cpu.max");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "100000 100000");
}

TEST_F(AttachEngineTest, DuplicateAttach) {
    AttachEngine engine(std::move(mock_));

    auto group = makeGroup("web");
    RuntimeState state;
    state.updatePid(1234, "web");

    auto err1 = engine.attach(*group, state);
    EXPECT_FALSE(err1.has_value());

    auto err2 = engine.attach(*group, state);
    EXPECT_FALSE(err2.has_value());
}

TEST_F(AttachEngineTest, MissingProcess) {
    AttachEngine engine(std::move(mock_));

    auto group = makeGroup("web");
    RuntimeState state;
    // No PID set

    auto err = engine.attach(*group, state);
    EXPECT_FALSE(err.has_value());
}

TEST_F(AttachEngineTest, DriverError) {
    mockPtr_->setNextError(Error(ErrorCode::CgroupCreateFailed, "injected", "test"));
    AttachEngine engine(std::move(mock_));

    auto group = makeGroup("web");
    RuntimeState state;
    state.updatePid(1234, "web");

    auto err = engine.attach(*group, state);
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->code, ErrorCode::CgroupCreateFailed);
}

TEST_F(AttachEngineTest, Detach) {
    AttachEngine engine(std::move(mock_));

    auto group = makeGroup("web");
    RuntimeState state;
    state.updatePid(1234, "web");

    engine.attach(*group, state);
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::Attached);

    auto err = engine.detach(state);
    EXPECT_FALSE(err.has_value());
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::Detached);
    EXPECT_FALSE(mockPtr_->exists("web"));
}

TEST_F(AttachEngineTest, Reattach) {
    AttachEngine engine(std::move(mock_));

    auto group = makeGroup("web");
    auto* cpuCtrl = addController(*group, makeController("cpu"));
    addItem(*cpuCtrl, makeItem("cpu.max", "100000 100000"));

    RuntimeState state;
    state.updatePid(1234, "web");

    auto err = engine.reattach(*group, state);
    EXPECT_FALSE(err.has_value());
    EXPECT_EQ(state.processState().attachStatus, AttachStatus::Attached);
    EXPECT_EQ(state.processState().attachedGroupPath, "web");
}

TEST_F(AttachEngineTest, ReattachWithRetries) {
    MockCgroupDriver* rawPtr;
    {
        auto m = std::make_unique<MockCgroupDriver>();
        rawPtr = m.get();
        AttachEngine engine(std::move(m));

        auto group = makeGroup("web");
        RuntimeState state;
        state.updatePid(1234, "web");

        rawPtr->setNextError(Error(ErrorCode::CgroupCreateFailed, "fail1", "test"));
        rawPtr->setNextError(Error(ErrorCode::CgroupCreateFailed, "fail2", "test"));

        auto err = engine.reattach(*group, state);
        EXPECT_FALSE(err.has_value());
    }
}
