#include <gtest/gtest.h>

#include <nameof.hpp>
#include <nlohmann/json.hpp>

import std;
import aspire.core;
import aspire.graphics;
import aspire.parser;

// Input routing through RenderService::event().
namespace
{
    using aspire::core::EventKeyboard;
    using aspire::core::EventMouse;
    using aspire::core::Vec2;
    using aspire::graphics::Node;
    using aspire::graphics::Rect;
    using aspire::graphics::RenderService;

    using Log = std::vector<std::string>;

    constexpr Rect Box{.x = 0.0F, .y = 0.0F, .w = 10.0F, .h = 10.0F};
    constexpr Rect Elsewhere{.x = 20.0F, .y = 20.0F, .w = 5.0F, .h = 5.0F};
    constexpr Rect Large{.x = 0.0F, .y = 0.0F, .w = 100.0F, .h = 100.0F};
    constexpr Vec2 InBox{.x = 5.0F, .y = 5.0F};
    constexpr Vec2 OutsideBox{.x = 50.0F, .y = 50.0F};
    constexpr Vec2 Offset{.x = 100.0F, .y = 40.0F};
    constexpr Vec2 Double{.x = 2.0F, .y = 2.0F};
    constexpr Vec2 Movement{.x = 4.0F, .y = -6.0F};
    constexpr Vec2 InChild{.x = 1.0F, .y = 2.0F};
    constexpr int Front{1};

    // Logs "<mouse|key> <name>" for each event it receives, keeps the last local position, and optionally handles
    // events or runs a callback.
    class Input : public Node
    {
    public:
        Input(Log& log, std::string_view name, std::optional<Rect> bounds = Box) : log_{log}
        {
            setName(name);
            setBounds(bounds);
        }

        auto handle(bool x) -> void
        {
            handle_ = x;
        }

        auto whenEvent(std::function<void()> x) -> void
        {
            onEvent_ = std::move(x);
        }

        [[nodiscard]] auto lastMouse() const -> const EventMouse&
        {
            return lastMouse_;
        }

        auto eventMouse(EventMouse& x) -> void override
        {
            lastMouse_ = x;
            record("mouse", x.handled);
        }

        auto eventKeyboard(EventKeyboard& x) -> void override
        {
            record("key", x.handled);
        }

    private:
        auto record(std::string_view kind, bool& handled) -> void
        {
            log_.get().push_back(std::string{kind} + " " + std::string{getName()});
            handled = handle_;

            if (onEvent_)
            {
                onEvent_();
            }
        }

        std::reference_wrapper<Log> log_;
        std::function<void()> onEvent_;
        EventMouse lastMouse_;
        bool handle_{false};
    };

    auto Make(Log& log, std::string_view name, std::optional<Rect> bounds = Box) -> std::shared_ptr<Input>
    {
        return std::make_shared<Input>(log, name, bounds);
    }

    auto Mouse(EventMouse::Type type, Vec2 position) -> aspire::core::Event
    {
        EventMouse x;
        x.type = type;
        x.button = EventMouse::Button::Left;
        x.position = position;
        x.delta = Movement;
        return x;
    }

    auto Press(Vec2 position) -> aspire::core::Event
    {
        return Mouse(EventMouse::Type::ButtonPressed, position);
    }

    auto Key() -> aspire::core::Event
    {
        EventKeyboard x;
        x.type = EventKeyboard::Type::KeyPressed;
        x.key = EventKeyboard::Key::Space;
        return x;
    }

    auto Handled(const aspire::core::Event& x) -> bool
    {
        return std::visit(aspire::core::Overloaded{[](const std::unique_ptr<aspire::core::EventUser>& e) { return e->handled; },
                                                   [](const auto& e) { return e.handled; }},
                          x);
    }

    // A started service with the given roots.
    auto Scene(std::initializer_list<std::shared_ptr<Node>> roots) -> std::shared_ptr<RenderService>
    {
        auto service = std::make_shared<RenderService>();

        for (const auto& root : roots)
        {
            service->addChild(root);
        }

        service->startup();
        return service;
    }
}

TEST(Input, pressReachesTheNodeItHits)
{
    Log log;
    const auto service = Scene({Make(log, "hit"), Make(log, "missed", Elsewhere)});

    auto event = Press(InBox);
    service->event(event);

    EXPECT_EQ(log, (Log{"mouse hit"}));
}

TEST(Input, nodeWithoutBoundsIsNeverPressed)
{
    Log log;
    const auto service = Scene({Make(log, "a", std::nullopt)});

    auto event = Press(InBox);
    service->event(event);

    EXPECT_TRUE(std::empty(log));
}

TEST(Input, positionAndMovementArriveInLocalCoordinates)
{
    Log log;
    const auto parent = Make(log, "parent", std::nullopt);
    parent->setPosition(Offset);
    parent->setScale(Double);
    const auto child = Make(log, "child");
    child->setPosition(InBox);
    parent->addChild(child);
    const auto service = Scene({parent});

    // The child's origin is at Offset + InBox * 2 on screen, and one local unit is two screen pixels.
    const Vec2 screen{.x = Offset.x + ((InBox.x + InChild.x) * Double.x), .y = Offset.y + ((InBox.y + InChild.y) * Double.y)};
    auto event = Press(screen);
    service->event(event);

    ASSERT_EQ(log, (Log{"mouse child"}));
    EXPECT_FLOAT_EQ(child->lastMouse().position.x, InChild.x);
    EXPECT_FLOAT_EQ(child->lastMouse().position.y, InChild.y);
    EXPECT_FLOAT_EQ(child->lastMouse().delta.x, Movement.x / Double.x);
    EXPECT_FLOAT_EQ(child->lastMouse().delta.y, Movement.y / Double.y);
}

TEST(Input, childrenAreHitBeforeTheirParent)
{
    Log log;
    const auto parent = Make(log, "parent");
    parent->addChild(Make(log, "child"));
    const auto service = Scene({parent});

    auto event = Press(InBox);
    service->event(event);

    EXPECT_EQ(log, (Log{"mouse child", "mouse parent"}));
}

TEST(Input, higherLayerIsHitFirstWhateverTheTreeOrder)
{
    Log log;
    const auto top = Make(log, "top");
    top->setLayer(Front);
    const auto service = Scene({top, Make(log, "later")});

    auto event = Press(InBox);
    service->event(event);

    EXPECT_EQ(log, (Log{"mouse top", "mouse later"}));
}

TEST(Input, handlingStopsRoutingAndMarksTheEvent)
{
    Log log;
    const auto parent = Make(log, "parent");
    const auto child = Make(log, "child");
    child->handle(true);
    parent->addChild(child);
    const auto service = Scene({parent});

    auto event = Press(InBox);
    service->event(event);

    EXPECT_EQ(log, (Log{"mouse child"}));
    EXPECT_TRUE(Handled(event));
}

TEST(Input, alreadyHandledEventIsNotRouted)
{
    Log log;
    const auto service = Scene({Make(log, "a")});

    auto event = Press(InBox);
    std::get<EventMouse>(event).handled = true;
    service->event(event);

    EXPECT_TRUE(std::empty(log));
}

TEST(Input, pressOutsideTheClipMissesEvenInsideTheBounds)
{
    Log log;
    const auto node = Make(log, "a", Large);
    node->setClip(Box);
    const auto service = Scene({node});

    auto outside = Press(OutsideBox);
    service->event(outside);
    EXPECT_TRUE(std::empty(log));

    auto inside = Press(InBox);
    service->event(inside);
    EXPECT_EQ(log, (Log{"mouse a"}));
}

TEST(Input, disabledHiddenAndUnstartedNodesAreSkipped)
{
    Log log;
    const auto disabled = Make(log, "disabled");
    disabled->setEnabled(false);
    disabled->addChild(Make(log, "underDisabled"));
    const auto hidden = Make(log, "hidden");
    hidden->setVisible(false);
    const auto stopped = Make(log, "stopped");
    const auto service = Scene({disabled, hidden, stopped, Make(log, "kept")});
    stopped->shutdown();

    auto event = Press(InBox);
    service->event(event);

    EXPECT_EQ(log, (Log{"mouse kept"}));
}

TEST(Input, movementReachesNodesThePointerIsNotOver)
{
    Log log;
    const auto service = Scene({Make(log, "under"), Make(log, "elsewhere", std::nullopt)});

    auto event = Mouse(EventMouse::Type::Moved, OutsideBox);
    service->event(event);

    EXPECT_EQ(log, (Log{"mouse elsewhere", "mouse under"}));
}

TEST(Input, keysGoToTheFocusFirstThenTreeOrder)
{
    Log log;
    const auto a = Make(log, "a");
    const auto b = Make(log, "b");
    const auto c = Make(log, "c");
    const auto service = Scene({a, b, c});
    service->setFocus(b);

    auto event = Key();
    service->event(event);

    EXPECT_EQ(log, (Log{"key b", "key a", "key c"}));
}

TEST(Input, handledKeyStopsRouting)
{
    Log log;
    const auto a = Make(log, "a");
    a->handle(true);
    const auto service = Scene({a, Make(log, "b")});

    auto event = Key();
    service->event(event);

    EXPECT_EQ(log, (Log{"key a"}));
    EXPECT_TRUE(Handled(event));
}

TEST(Input, focusOnAnIneligibleNodeIsIgnored)
{
    Log log;
    const auto a = Make(log, "a");
    const auto b = Make(log, "b");
    const auto service = Scene({a, b});
    service->setFocus(b);
    b->setEnabled(false);

    auto event = Key();
    service->event(event);

    EXPECT_EQ(log, (Log{"key a"}));
}

TEST(Input, focusIsLostWhenTheNodeIsDestroyed)
{
    Log log;
    auto focused = Make(log, "focused");
    const auto service = Scene({});
    service->setFocus(focused);
    focused.reset();

    EXPECT_EQ(service->getFocus(), nullptr);
}

TEST(Input, nodeRemovedByAnEarlierHandlerIsSkipped)
{
    Log log;
    const auto front = Make(log, "front");
    front->setLayer(Front);
    const auto back = Make(log, "back");
    auto* rawBack = back.get();
    front->whenEvent([rawBack] { rawBack->remove(); });
    const auto service = Scene({front, back});

    auto event = Press(InBox);
    service->event(event);

    EXPECT_EQ(log, (Log{"mouse front"}));
}

TEST(Input, engineFrameDeliversQueuedInput)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    const auto service = std::make_shared<RenderService>();
    service->addChild(Make(log, "a"));
    engine->addChild(service);
    engine->startup();

    engine->enqueueEvent(Press(InBox));
    engine->iterate(engine->getIntervalFixed());

    EXPECT_EQ(log, (Log{"mouse a"}));
}
