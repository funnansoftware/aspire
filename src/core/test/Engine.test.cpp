#include <gtest/gtest.h>

import aspire.core.engine;

TEST(EngineTest, RunReturnsSuccess)
{
    const auto engine = aspire::core::Engine{};
    EXPECT_EQ(engine.run(), EXIT_SUCCESS);
}