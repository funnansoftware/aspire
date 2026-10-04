#include <gtest/gtest.h>

#include <nameof.hpp>
#include <nlohmann/json.hpp>

import std;
import aspire.core;
import aspire.graphics;
import aspire.parser;

namespace
{
    constexpr aspire::graphics::Color Gold{.r = 255, .g = 210, .b = 70, .a = aspire::graphics::Color::Opaque};
    constexpr aspire::core::Vec2 Offset{.x = 10.0F, .y = 20.0F};
}

TEST(Label, drawsItsTextAtItsOrigin)
{
    const auto label = std::make_shared<aspire::graphics::Label>();
    label->setText("HELLO");
    label->setColor(Gold);
    label->setPosition(Offset);
    label->startup();

    aspire::graphics::DrawList list;
    aspire::graphics::Collect(*label, list);

    const auto items = list.items();
    ASSERT_EQ(std::size(items), 1);
    const auto& drawn = std::get<aspire::graphics::DrawText>(items.front().primitive);
    EXPECT_EQ(drawn.text, "HELLO");
    EXPECT_FLOAT_EQ(drawn.position.x, Offset.x);
    EXPECT_EQ(drawn.color.b, Gold.b);
}

TEST(Label, loadsFromJson)
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<aspire::graphics::Label>();

    auto json = nlohmann::json::parse(R"({ "type": "Label", "text": "HELLO", "color": [255, 210, 70] })");

    const auto label = std::dynamic_pointer_cast<aspire::graphics::Label>(aspire::parser::ReadJson(factory, json));
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->getText(), "HELLO");
    EXPECT_EQ(label->getColor().g, Gold.g);
}
