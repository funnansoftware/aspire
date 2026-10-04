#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

import std;
import aspire.graphics;

namespace
{
    using aspire::graphics::Color;

    constexpr std::uint8_t Red{10};
    constexpr std::uint8_t Green{20};
    constexpr std::uint8_t Blue{30};
    constexpr std::uint8_t Alpha{40};
}

TEST(Color, roundTripsThroughJson)
{
    const nlohmann::json json = Color{.r = Red, .g = Green, .b = Blue, .a = Alpha};
    EXPECT_EQ(json, nlohmann::json::array({Red, Green, Blue, Alpha}));

    const auto color = json.get<Color>();
    EXPECT_EQ(color.r, Red);
    EXPECT_EQ(color.a, Alpha);
}

TEST(Color, alphaDefaultsToOpaque)
{
    EXPECT_EQ(nlohmann::json::array({Red, Green, Blue}).get<Color>().a, Color::Opaque);
}
