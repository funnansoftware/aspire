#include <gtest/gtest.h>

#include <nameof.hpp>
#include <nlohmann/json.hpp>

import std;
import aspire.core;
import aspire.graphics;
import aspire.parser;

namespace
{
    using aspire::graphics::DrawList;
    using aspire::graphics::Node;
    using aspire::graphics::Rect;

    constexpr aspire::core::Vec2 Offset{.x = 10.0F, .y = 20.0F};
    constexpr aspire::core::Vec2 Double{.x = 2.0F, .y = 2.0F};
    constexpr aspire::core::Vec2 ChildPosition{.x = 5.0F, .y = 5.0F};
    constexpr Rect Box{.x = 0.0F, .y = 0.0F, .w = 100.0F, .h = 100.0F};
    constexpr int UiLayer{2};
    constexpr float InFront{-1.0F};

    // A 16x16 tile from a tilesheet, drawn at the node's origin, and a small rectangle inside it.
    constexpr Rect TileRegion{.x = 1.0F, .y = 2.0F, .w = 16.0F, .h = 16.0F};
    constexpr Rect TileBounds{.x = 0.0F, .y = 0.0F, .w = 16.0F, .h = 16.0F};
    constexpr Rect Small{.x = 1.0F, .y = 1.0F, .w = 2.0F, .h = 2.0F};

    // Far from Near, so their intersection is empty.
    constexpr Rect Near{.x = 0.0F, .y = 0.0F, .w = 10.0F, .h = 10.0F};
    constexpr Rect Far{.x = 20.0F, .y = 20.0F, .w = 10.0F, .h = 10.0F};

    // Places a child's clip so it overhangs its parent's (Box at Offset) on the right and bottom.
    constexpr aspire::core::Vec2 Overhang{.x = 50.0F, .y = 50.0F};

    // Draws its name as text at its origin, so tests can read back what was drawn, where, and in which order.
    class Label : public Node
    {
    public:
        explicit Label(std::string_view name = {})
        {
            setName(name);
        }

        auto draw(aspire::graphics::Renderer& x) const -> void override
        {
            x.text(getName(), {}, aspire::graphics::White, order_);
        }

        auto setOrder(float x) -> void
        {
            order_ = x;
        }

    private:
        float order_{};
    };

    auto Make(std::string_view name) -> std::shared_ptr<Label>
    {
        return std::make_shared<Label>(name);
    }

    // Collects a fresh frame from root.
    auto Draw(const Node& root, DrawList& list) -> void
    {
        list.clear();
        aspire::graphics::Collect(root, list);
    }

    auto Text(const aspire::graphics::DrawItem& x) -> const aspire::graphics::DrawText&
    {
        return std::get<aspire::graphics::DrawText>(x.primitive);
    }

    auto Names(const DrawList& list) -> std::vector<std::string>
    {
        std::vector<std::string> names;

        for (const auto& item : list.items())
        {
            names.emplace_back(Text(item).text);
        }

        return names;
    }

    auto ItemAt(const DrawList& list, std::size_t x) -> const aspire::graphics::DrawItem&
    {
        return *std::next(std::begin(list.items()), static_cast<std::ptrdiff_t>(x));
    }

    auto ClipOf(const DrawList& list, const aspire::graphics::DrawItem& x) -> Rect
    {
        return *std::next(std::begin(list.clips()), static_cast<std::ptrdiff_t>(x.clip));
    }

    auto ExpectRect(Rect actual, Rect expected) -> void
    {
        EXPECT_FLOAT_EQ(actual.x, expected.x);
        EXPECT_FLOAT_EQ(actual.y, expected.y);
        EXPECT_FLOAT_EQ(actual.w, expected.w);
        EXPECT_FLOAT_EQ(actual.h, expected.h);
    }
}

TEST(Collect, drawsParentsFirstThenChildrenInOrder)
{
    const auto root = Make("root");
    const auto a = Make("a");
    root->addChild(a);
    a->addChild(Make("a1"));
    root->addChild(Make("b"));
    root->startup();

    DrawList list;
    Draw(*root, list);

    EXPECT_EQ(Names(list), (std::vector<std::string>{"root", "a", "a1", "b"}));

    std::uint32_t expected{};

    for (const auto& item : list.items())
    {
        EXPECT_EQ(item.sequence, expected++);
    }
}

TEST(Collect, positionAndScaleComposeDownTheTree)
{
    const auto root = Make("root");
    root->setPosition(Offset);
    root->setScale(Double);
    const auto child = Make("child");
    child->setPosition(ChildPosition);
    root->addChild(child);
    root->startup();

    DrawList list;
    Draw(*root, list);

    const auto items = list.items();
    const auto& drawn = Text(items.back());
    EXPECT_FLOAT_EQ(drawn.position.x, Offset.x + (ChildPosition.x * Double.x));
    EXPECT_FLOAT_EQ(drawn.position.y, Offset.y + (ChildPosition.y * Double.y));
    EXPECT_FLOAT_EQ(drawn.scale.x, Double.x);
}

TEST(Collect, spriteAndRectBoundsAreInScreenCoordinates)
{
    class Shapes : public Node
    {
    public:
        auto draw(aspire::graphics::Renderer& x) const -> void override
        {
            x.sprite("tiles.png", TileRegion, TileBounds);
            x.rect(Small, aspire::graphics::White);
            x.outline(Small, aspire::graphics::White);
        }
    };

    const auto node = std::make_shared<Shapes>();
    node->setPosition(Offset);
    node->setScale(Double);
    node->startup();

    DrawList list;
    Draw(*node, list);
    ASSERT_EQ(std::size(list.items()), 3);

    // Keep the span: clang's -Wdangling-gsl flags front() and back() on the temporary items() returns.
    const auto items = list.items();
    const auto& sprite = std::get<aspire::graphics::DrawSprite>(items.front().primitive);
    EXPECT_EQ(sprite.source, "tiles.png");
    ExpectRect(sprite.region, TileRegion);
    ExpectRect(sprite.bounds, {.x = Offset.x, .y = Offset.y, .w = TileBounds.w * Double.x, .h = TileBounds.h * Double.y});

    const auto& rect = std::get<aspire::graphics::DrawRect>(std::next(std::begin(items))->primitive);
    EXPECT_TRUE(rect.filled);
    ExpectRect(rect.bounds,
               {.x = Offset.x + (Small.x * Double.x), .y = Offset.y + (Small.y * Double.y), .w = Small.w * Double.x, .h = Small.h * Double.y});

    // An outline has the same screen bounds, unfilled.
    const auto& outline = std::get<aspire::graphics::DrawRect>(items.back().primitive);
    EXPECT_FALSE(outline.filled);
    ExpectRect(outline.bounds, rect.bounds);
}

TEST(Collect, clipsIntersectDownTheTree)
{
    const auto root = Make("root");
    root->setPosition(Offset);
    root->setClip(Box);
    const auto child = Make("child");
    child->setPosition(Overhang);
    child->setClip(Box);
    root->addChild(child);
    const auto grandchild = Make("grandchild");
    child->addChild(grandchild);
    root->startup();

    DrawList list;
    Draw(*root, list);
    ASSERT_EQ(std::size(list.items()), 3);

    ExpectRect(ClipOf(list, ItemAt(list, 0)), {.x = Offset.x, .y = Offset.y, .w = Box.w, .h = Box.h});

    // The child's own clip starts at Offset + Overhang and is cut off where the parent's ends.
    ExpectRect(ClipOf(list, ItemAt(list, 1)),
               {.x = Offset.x + Overhang.x, .y = Offset.y + Overhang.y, .w = Box.w - Overhang.x, .h = Box.h - Overhang.y});

    // A node without a clip of its own keeps its parent's.
    EXPECT_EQ(ItemAt(list, 2).clip, ItemAt(list, 1).clip);
}

TEST(Collect, emptyClipIntersectionIsKept)
{
    const auto root = Make("root");
    root->setClip(Near);
    const auto child = Make("child");
    child->setClip(Far);
    root->addChild(child);
    root->startup();

    DrawList list;
    Draw(*root, list);

    // The child still draws; its clip just covers nothing, so the backend draws nothing for it.
    ASSERT_EQ(std::size(list.items()), 2);
    const auto items = list.items();
    EXPECT_FLOAT_EQ(ClipOf(list, items.back()).w, 0.0F);
}

TEST(Collect, nodesWithoutClipHaveClipZero)
{
    const auto root = Make("root");
    root->startup();

    DrawList list;
    Draw(*root, list);

    const auto items = list.items();
    EXPECT_EQ(items.front().clip, 0);
}

TEST(Collect, layerIsInheritedUnlessSet)
{
    const auto root = Make("root");
    const auto ui = Make("ui");
    ui->setLayer(UiLayer);
    root->addChild(ui);
    ui->addChild(Make("button"));
    const auto overlay = Make("overlay");
    overlay->setLayer(0);
    ui->addChild(overlay);
    root->startup();

    DrawList list;
    Draw(*root, list);

    ASSERT_EQ(std::size(list.items()), 4);
    EXPECT_EQ(ItemAt(list, 0).layer, 0);
    EXPECT_EQ(ItemAt(list, 1).layer, UiLayer);
    EXPECT_EQ(ItemAt(list, 2).layer, UiLayer);
    EXPECT_EQ(ItemAt(list, 3).layer, 0);
}

TEST(DrawList, sortsByLayerThenOrderThenSequence)
{
    const auto root = Make("root");
    const auto ui = Make("ui");
    ui->setLayer(1);
    const auto behind = Make("behind");
    behind->setOrder(InFront);
    const auto first = Make("first");
    const auto second = Make("second");
    root->addChild(ui);
    root->addChild(first);
    root->addChild(behind);
    root->addChild(second);
    root->startup();

    DrawList list;
    Draw(*root, list);
    list.sort();

    // Layer 0 before layer 1; within layer 0, negative order first, then ties in tree order.
    EXPECT_EQ(Names(list), (std::vector<std::string>{"behind", "root", "first", "second", "ui"}));
}

TEST(Collect, skipsNodesThatArentStarted)
{
    const auto root = Make("root");
    const auto stopped = Make("stopped");
    root->addChild(stopped);
    stopped->addChild(Make("below"));
    root->addChild(Make("kept"));
    root->startup();
    stopped->shutdown();

    DrawList list;
    Draw(*root, list);

    EXPECT_EQ(Names(list), (std::vector<std::string>{"root", "kept"}));
}

TEST(Collect, drawsNothingFromAnUnstartedRoot)
{
    const auto root = Make("root");

    DrawList list;
    Draw(*root, list);

    EXPECT_TRUE(std::empty(list.items()));
}

TEST(Collect, invisibleNodeHidesItsSubtree)
{
    const auto root = Make("root");
    const auto hidden = Make("hidden");
    hidden->setVisible(false);
    root->addChild(hidden);
    hidden->addChild(Make("below"));
    root->startup();

    DrawList list;
    Draw(*root, list);

    EXPECT_EQ(Names(list), (std::vector<std::string>{"root"}));
}

TEST(Collect, skipsNodesUnderPlainObjects)
{
    const auto root = Make("root");
    const auto plain = std::make_shared<aspire::core::Object>();
    root->addChild(plain);
    plain->addChild(Make("nested"));
    root->startup();

    DrawList list;
    Draw(*root, list);

    EXPECT_EQ(Names(list), (std::vector<std::string>{"root"}));
}

TEST(DrawList, collectingAgainReusesStorage)
{
    const auto root = Make("root");
    root->setClip(Box);
    root->addChild(Make("a"));
    root->addChild(Make("b"));
    root->startup();

    DrawList list;
    Draw(*root, list);
    const auto* items = std::data(list.items());
    const auto* clips = std::data(list.clips());

    Draw(*root, list);
    EXPECT_EQ(std::data(list.items()), items);
    EXPECT_EQ(std::data(list.clips()), clips);
}

TEST(Node, loadsFromJson)
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<Node>();

    auto json = nlohmann::json::parse(R"({
        "type": "Node",
        "position": [1, 2],
        "scale": [3, 4],
        "layer": 5,
        "visible": false,
        "enabled": false,
        "clip": [0, 0, 8, 8],
        "bounds": [1, 2, 3, 4]
    })");

    const auto node = std::dynamic_pointer_cast<Node>(aspire::parser::ReadJson(factory, json));
    ASSERT_NE(node, nullptr);
    EXPECT_FLOAT_EQ(node->getPosition().y, 2.0F);
    EXPECT_FLOAT_EQ(node->getScale().x, 3.0F);
    EXPECT_EQ(node->getLayer(), 5);
    EXPECT_FALSE(node->isVisible());
    EXPECT_FALSE(node->getEnabled());
    const auto clip = node->getClip();
    ASSERT_TRUE(clip.has_value());
    EXPECT_FLOAT_EQ(clip.value_or(Rect{}).w, 8.0F);
    EXPECT_FLOAT_EQ(node->getBounds().value_or(Rect{}).h, 4.0F);
}

TEST(Node, nullLayerAndClipInJsonMeanUnset)
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<Node>();

    auto json = nlohmann::json::parse(R"({ "type": "Node", "layer": null, "clip": null })");

    const auto node = std::dynamic_pointer_cast<Node>(aspire::parser::ReadJson(factory, json));
    ASSERT_NE(node, nullptr);
    EXPECT_FALSE(node->getLayer().has_value());
    EXPECT_FALSE(node->getClip().has_value());
}

TEST(ChildState, composesInheritsAndNarrows)
{
    // The parent clips to twice Box; the node at Overhang (screen offset Offset + Overhang * Double) clips to Box.
    constexpr Rect parentClip{.x = 0.0F, .y = 0.0F, .w = Box.w * Double.x, .h = Box.h * Double.y};
    const aspire::graphics::DrawState parent{.transform = {.offset = Offset, .scale = Double}, .clip = 1, .clipRect = parentClip, .layer = UiLayer};

    Node node;
    node.setPosition(Overhang);
    const aspire::core::Vec2 origin{.x = Offset.x + (Overhang.x * Double.x), .y = Offset.y + (Overhang.y * Double.y)};

    // Without a layer or clip of its own, the node inherits both.
    auto state = aspire::graphics::ChildState(parent, node);
    EXPECT_FLOAT_EQ(state.transform.offset.x, origin.x);
    EXPECT_FLOAT_EQ(state.transform.scale.x, Double.x);
    EXPECT_EQ(state.layer, UiLayer);
    EXPECT_EQ(state.clip, 1);
    ExpectRect(state.clipRect.value_or(Rect{}), parentClip);

    // Its own clip narrows the parent's; the clip index is still the parent's, since only Collect allocates them.
    node.setLayer(0);
    node.setClip(Box);
    state = aspire::graphics::ChildState(parent, node);
    EXPECT_EQ(state.layer, 0);
    EXPECT_EQ(state.clip, 1);
    ExpectRect(state.clipRect.value_or(Rect{}), {.x = origin.x, .y = origin.y, .w = parentClip.w - origin.x, .h = parentClip.h - origin.y});
}

TEST(Node, hitTestUsesBounds)
{
    Node node;
    EXPECT_FALSE(node.hitTest({}));

    node.setBounds(Box);
    EXPECT_TRUE(node.hitTest({.x = Box.w / 2, .y = Box.h / 2}));
    EXPECT_FALSE(node.hitTest({.x = Box.w, .y = Box.h}));
}

TEST(Node, hitTestCanBeOverridden)
{
    // A node that's hit everywhere left of x = 0, with no bounds at all.
    class LeftHalf : public Node
    {
    public:
        [[nodiscard]] auto hitTest(aspire::core::Vec2 x) const -> bool override
        {
            return x.x < 0.0F;
        }
    };

    const LeftHalf node;
    EXPECT_TRUE(node.hitTest({.x = -1.0F, .y = 0.0F}));
    EXPECT_FALSE(node.hitTest({.x = 1.0F, .y = 0.0F}));
}

TEST(Node, layerAndClipDefaultToUnset)
{
    const Node node;
    EXPECT_FALSE(node.getLayer().has_value());
    EXPECT_FALSE(node.getClip().has_value());
    EXPECT_TRUE(node.isVisible());
    EXPECT_TRUE(node.getEnabled());
}
