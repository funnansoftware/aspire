#include <gtest/gtest.h>

import aspire.raylib.window;

TEST(WindowTest, ShouldCloseReturnsFalseInitially)
{
    constexpr auto windowWidth = 800;
    constexpr auto windowHeight = 600;
    const auto window = aspire::raylib::Window{windowWidth, windowHeight, "Test Window"};
    EXPECT_FALSE(aspire::raylib::Window::shouldClose());
}
