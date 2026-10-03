#include <gtest/gtest.h>

#include <cstdlib>

import std;
import aspire.core;

namespace
{
    // Engine's fixed step and frame-length cap.
    constexpr std::chrono::milliseconds FixedStep{10};
    constexpr std::chrono::milliseconds MaxFrame{50};
    constexpr int MaxStepsPerFrame{5};

    constexpr int FailureCode{3};

    using Log = std::vector<std::string>;

    // Logs "<call> <name>" for each lifecycle hook and frame call, and runs optional callbacks.
    class Recorder : public aspire::core::Service
    {
    public:
        Recorder(Log& log, std::string_view name) : log_{log}
        {
            setName(name);
        }

        auto whenStarted(std::function<void()> x) -> void
        {
            onStart_ = std::move(x);
        }

        auto whenEvent(std::function<void(aspire::core::Event&)> x) -> void
        {
            onEvent_ = std::move(x);
        }

        auto whenUpdated(std::function<void()> x) -> void
        {
            onUpdate_ = std::move(x);
        }

        [[nodiscard]] auto lastDt() const -> float
        {
            return dt_;
        }

        [[nodiscard]] auto lastDtFixed() const -> float
        {
            return dtFixed_;
        }

        [[nodiscard]] auto fixedSteps() const -> int
        {
            return fixedSteps_;
        }

        auto event(aspire::core::Event& x) -> void override
        {
            record("event");

            if (onEvent_)
            {
                onEvent_(x);
            }
        }

        auto update(float x) -> void override
        {
            dt_ = x;
            record("update");

            if (onUpdate_)
            {
                onUpdate_();
            }
        }

        auto updateFixed(float x) -> void override
        {
            dtFixed_ = x;
            ++fixedSteps_;
            record("fixed");
        }

        auto render() -> void override
        {
            record("render");
        }

    protected:
        auto onStartup() -> void override
        {
            record("start");

            if (onStart_)
            {
                onStart_();
            }
        }

        // NOLINTNEXTLINE(bugprone-exception-escape): a throwing test hook should terminate.
        auto onShutdown() noexcept -> void override
        {
            record("stop");
        }

    private:
        auto record(std::string_view x) -> void
        {
            log_.get().push_back(std::string{x} + " " + std::string{getName()});
        }

        std::reference_wrapper<Log> log_;
        std::function<void()> onStart_;
        std::function<void(aspire::core::Event&)> onEvent_;
        std::function<void()> onUpdate_;
        float dt_{};
        float dtFixed_{};
        int fixedSteps_{};
    };

    auto Make(Log& log, std::string_view name) -> std::shared_ptr<Recorder>
    {
        return std::make_shared<Recorder>(log, name);
    }

    // The entries that start with prefix, such as "event".
    auto Filter(const Log& log, std::string_view prefix) -> Log
    {
        Log result;
        std::ranges::copy_if(log, std::back_inserter(result), [prefix](const std::string& x) { return x.starts_with(prefix); });
        return result;
    }

    auto Closed() -> aspire::core::EventWindow
    {
        aspire::core::EventWindow x;
        x.type = aspire::core::EventWindow::Type::Closed;
        return x;
    }

    auto Seconds(std::chrono::nanoseconds x) -> float
    {
        return std::chrono::duration<float>{x}.count();
    }
}

TEST(Engine, servicesTickInChildOrder)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    engine->addChild(Make(log, "a"));
    engine->addChild(Make(log, "b"));

    engine->startup();
    engine->enqueueEvent(aspire::core::EventKeyboard{});
    engine->iterate(FixedStep);

    EXPECT_EQ(log, (Log{"start a", "start b", "event a", "event b", "update a", "update b", "fixed a", "fixed b", "render a", "render b"}));
}

TEST(Engine, plainChildrenStartButDoNotTick)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    const auto plain = std::make_shared<aspire::core::Object>();
    engine->addChild(plain);

    // A Service below a plain child is nested, so it starts but never ticks.
    plain->addChild(Make(log, "nested"));

    engine->startup();
    engine->enqueueEvent(aspire::core::EventKeyboard{});
    engine->iterate(FixedStep);

    EXPECT_TRUE(plain->isStarted());
    EXPECT_EQ(log, (Log{"start nested"}));
}

TEST(Engine, runStartsThenShutsDown)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    auto* raw = engine.get();
    const auto service = Make(log, "s");
    service->whenUpdated([raw] { raw->quit(); });
    engine->addChild(service);

    EXPECT_EQ(engine->run(), EXIT_SUCCESS);
    EXPECT_FALSE(engine->isStarted());

    // run() measures real time, so a slow machine may add fixed steps. Ignore them.
    std::erase_if(log, [](const std::string& x) { return x.starts_with("fixed"); });
    EXPECT_EQ(log, (Log{"start s", "update s", "render s", "stop s"}));
}

TEST(Engine, quitExitCodeIsReturnedByRun)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    auto* raw = engine.get();
    const auto service = Make(log, "s");
    service->whenUpdated([raw] { raw->quit(FailureCode); });
    engine->addChild(service);

    EXPECT_EQ(engine->run(), FailureCode);
    EXPECT_EQ(engine->exitCode(), FailureCode);
}

TEST(Engine, quitDuringStartupEndsBeforeFirstFrame)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    auto* raw = engine.get();
    const auto service = Make(log, "s");
    service->whenStarted([raw] { raw->quit(); });
    engine->addChild(service);

    EXPECT_EQ(engine->run(), EXIT_SUCCESS);
    EXPECT_EQ(log, (Log{"start s", "stop s"}));
}

TEST(Engine, fixedStepsFollowElapsed)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    const auto service = Make(log, "s");
    engine->addChild(service);
    engine->startup();

    // Two and a half steps: two run, and half a step carries over.
    engine->iterate(2 * FixedStep + FixedStep / 2);
    EXPECT_EQ(service->fixedSteps(), 2);

    // The carried half plus another half makes one more.
    engine->iterate(FixedStep / 2);
    EXPECT_EQ(service->fixedSteps(), 3);

    EXPECT_FLOAT_EQ(service->lastDtFixed(), Seconds(FixedStep));
    EXPECT_FLOAT_EQ(service->lastDt(), Seconds(FixedStep / 2));
}

TEST(Engine, elapsedIsClamped)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    const auto service = Make(log, "s");
    engine->addChild(service);
    engine->startup();

    engine->iterate(std::chrono::seconds{1});
    EXPECT_EQ(service->fixedSteps(), MaxStepsPerFrame);
    EXPECT_FLOAT_EQ(service->lastDt(), Seconds(MaxFrame));

    engine->iterate(-FixedStep);
    EXPECT_EQ(service->fixedSteps(), MaxStepsPerFrame);
    EXPECT_FLOAT_EQ(service->lastDt(), 0.0F);
}

TEST(Engine, everyServiceReceivesEveryEvent)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    engine->addChild(Make(log, "a"));
    engine->addChild(Make(log, "b"));
    engine->startup();

    engine->enqueueEvent(aspire::core::EventKeyboard{});
    engine->enqueueEvent(aspire::core::EventMouse{});
    engine->iterate(FixedStep);

    EXPECT_EQ(Filter(log, "event"), (Log{"event a", "event b", "event a", "event b"}));
}

TEST(Engine, handledFlagIsVisibleToLaterServices)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    const auto a = Make(log, "a");
    const auto b = Make(log, "b");
    engine->addChild(a);
    engine->addChild(b);

    auto seenByB = false;
    a->whenEvent([](aspire::core::Event& x) { std::get<aspire::core::EventKeyboard>(x).handled = true; });
    b->whenEvent([&seenByB](aspire::core::Event& x) { seenByB = std::get<aspire::core::EventKeyboard>(x).handled; });

    engine->startup();
    engine->enqueueEvent(aspire::core::EventKeyboard{});
    engine->iterate(FixedStep);

    // Engine still delivers a handled event; b decides what to do with it.
    EXPECT_EQ(Filter(log, "event"), (Log{"event a", "event b"}));
    EXPECT_TRUE(seenByB);
}

TEST(Engine, closedQuitsAndServicesSeeIt)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    engine->addChild(Make(log, "s"));
    engine->startup();

    engine->enqueueEvent(Closed());
    engine->iterate(FixedStep);

    EXPECT_FALSE(engine->running());
    EXPECT_EQ(engine->exitCode(), EXIT_SUCCESS);

    // The rest of the frame still runs.
    EXPECT_EQ(log, (Log{"start s", "event s", "update s", "fixed s", "render s"}));
}

TEST(Engine, closedKeepsFailureExitCode)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    auto* raw = engine.get();
    const auto service = Make(log, "s");
    engine->addChild(service);

    service->whenEvent(
        [raw](aspire::core::Event& x)
        {
            if (std::holds_alternative<aspire::core::EventKeyboard>(x))
            {
                raw->quit(FailureCode);
            }
        });

    engine->startup();
    engine->enqueueEvent(aspire::core::EventKeyboard{});
    engine->enqueueEvent(Closed());
    engine->iterate(FixedStep);

    EXPECT_FALSE(engine->running());
    EXPECT_EQ(engine->exitCode(), FailureCode);
}

TEST(Engine, eventEnqueuedDuringDispatchArrivesNextFrame)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    auto* raw = engine.get();
    const auto service = Make(log, "s");
    engine->addChild(service);

    auto enqueued = false;
    service->whenEvent(
        [raw, &enqueued](aspire::core::Event& /*unused*/)
        {
            if (!enqueued)
            {
                enqueued = true;
                raw->enqueueEvent(aspire::core::EventKeyboard{});
            }
        });

    engine->startup();
    engine->enqueueEvent(aspire::core::EventKeyboard{});

    engine->iterate(FixedStep);
    EXPECT_EQ(std::size(Filter(log, "event")), 1);

    engine->iterate(FixedStep);
    EXPECT_EQ(std::size(Filter(log, "event")), 2);
}

TEST(Engine, serviceAddedDuringUpdateJoinsNextPhase)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    auto* raw = engine.get();
    const auto a = Make(log, "a");
    const auto c = Make(log, "c");
    engine->addChild(a);

    auto added = false;
    a->whenUpdated(
        [raw, &c, &added]
        {
            if (!added)
            {
                added = true;
                raw->addChild(c);
            }
        });

    engine->startup();
    engine->iterate(FixedStep);

    // c starts as soon as it's added, misses this frame's update, and joins from the fixed step.
    EXPECT_EQ(log, (Log{"start a", "update a", "start c", "fixed a", "fixed c", "render a", "render c"}));
}

TEST(Engine, serviceShutDownBySiblingIsSkipped)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    const auto a = Make(log, "a");
    const auto b = Make(log, "b");
    auto* rawB = b.get();
    engine->addChild(a);
    engine->addChild(b);
    a->whenUpdated([rawB] { rawB->shutdown(); });

    engine->startup();
    engine->iterate(FixedStep);

    EXPECT_EQ(log, (Log{"start a", "start b", "update a", "stop b", "fixed a", "render a"}));
    EXPECT_EQ(b->getParent(), engine);
}

TEST(Engine, serviceRemovedBySiblingIsSkipped)
{
    Log log;
    const auto engine = std::make_shared<aspire::core::Engine>();
    const auto a = Make(log, "a");
    const auto b = Make(log, "b");
    auto* rawB = b.get();
    engine->addChild(a);
    engine->addChild(b);
    a->whenUpdated([rawB] { rawB->remove(); });

    engine->startup();
    engine->iterate(FixedStep);

    EXPECT_EQ(log, (Log{"start a", "start b", "update a", "stop b", "fixed a", "render a"}));
    EXPECT_EQ(b->getParent(), nullptr);
}
