#include <gtest/gtest.h>

import aspire.raylib.window;

TEST(WindowTest, ShouldCloseReturnsFalseInitially)
{
    const auto window = aspire::raylib::Window{800, 600, "Test Window"};
    EXPECT_FALSE(window.shouldClose());
}
