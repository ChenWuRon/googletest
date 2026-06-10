#include <gtest/gtest.h>
#include "resource_manager/mode/mode.h"

using namespace resource_manager;

TEST(ModeTest, DefaultConstruction) {
    Mode m;
    EXPECT_EQ(m.service, ServiceType::None);
    EXPECT_EQ(m.ns, NamespaceType::None);
    EXPECT_EQ(m.entity, EntityType::None);
}
