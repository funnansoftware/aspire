#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

import std;
import aspire.core;

namespace
{
    constexpr float X{1.5F};
    constexpr float Y{-2.0F};
}

TEST(Vec2, writesJsonAsArray)
{
    const nlohmann::json json = aspire::core::Vec2{.x = X, .y = Y};
    EXPECT_EQ(json, nlohmann::json::array({X, Y}));
}

TEST(Vec2, readsJsonArray)
{
    const auto x = nlohmann::json::array({X, Y}).get<aspire::core::Vec2>();
    EXPECT_FLOAT_EQ(x.x, X);
    EXPECT_FLOAT_EQ(x.y, Y);
}

TEST(Vec2, canBeAProperty)
{
    static_assert(aspire::core::JsonSerializable<aspire::core::Vec2>);

    aspire::core::Vec2 value;
    aspire::core::TemplateProperty<aspire::core::Vec2> property("position", value);
    property.setValueString("[3, 4]");
    EXPECT_FLOAT_EQ(value.x, 3.0F);
    EXPECT_FLOAT_EQ(value.y, 4.0F);
}
