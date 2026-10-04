#include <gtest/gtest.h>

#include <nameof.hpp>
#include <nlohmann/json.hpp>

import std;
import aspire.core;
import aspire.graphics;
import aspire.parser;

namespace
{
    using aspire::graphics::Rect;

    constexpr Rect Region{.x = 17.0F, .y = 34.0F, .w = 16.0F, .h = 16.0F};
    constexpr aspire::core::Vec2 Offset{.x = 10.0F, .y = 20.0F};
    constexpr aspire::graphics::Color Tint{.r = 200, .g = 100, .b = 50, .a = aspire::graphics::Color::Opaque};
}

TEST(Sprite, drawsItsRegionAtItsOrigin)
{
    const auto sprite = std::make_shared<aspire::graphics::Sprite>();
    sprite->setSource("tiles.png");
    sprite->setRegion(Region);
    sprite->setTint(Tint);
    sprite->setPosition(Offset);
    sprite->startup();

    aspire::graphics::DrawList list;
    aspire::graphics::Collect(*sprite, list);

    const auto items = list.items();
    ASSERT_EQ(std::size(items), 1);
    const auto& drawn = std::get<aspire::graphics::DrawSprite>(items.front().primitive);
    EXPECT_EQ(drawn.source, "tiles.png");
    EXPECT_FLOAT_EQ(drawn.region.x, Region.x);
    EXPECT_FLOAT_EQ(drawn.bounds.x, Offset.x);
    EXPECT_FLOAT_EQ(drawn.bounds.y, Offset.y);
    EXPECT_FLOAT_EQ(drawn.bounds.w, Region.w);
    EXPECT_FLOAT_EQ(drawn.bounds.h, Region.h);
    EXPECT_EQ(drawn.tint.r, Tint.r);
}

TEST(Sprite, loadsFromJson)
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<aspire::graphics::Sprite>();

    auto json = nlohmann::json::parse(R"({ "type": "Sprite", "source": "tiles.png", "region": [17, 34, 16, 16], "tint": [200, 100, 50] })");

    const auto sprite = std::dynamic_pointer_cast<aspire::graphics::Sprite>(aspire::parser::ReadJson(factory, json));
    ASSERT_NE(sprite, nullptr);
    EXPECT_EQ(sprite->getSource(), "tiles.png");
    EXPECT_FLOAT_EQ(sprite->getRegion().y, Region.y);
    EXPECT_EQ(sprite->getTint().g, Tint.g);
}
