#include <gtest/gtest.h>

#include <cstdlib>
#include <nameof.hpp>
#include <nlohmann/json.hpp>

import std;
import aspire.core;
import aspire.graphics;
import aspire.parser;

namespace
{
    using aspire::graphics::Color;
    using aspire::graphics::DrawList;
    using aspire::graphics::Node;
    using aspire::graphics::RenderService;

    using Log = std::vector<std::string>;

    constexpr float Dt{0.016F};
    constexpr float DtFixed{0.01F};
    constexpr int UiLayer{1};
    constexpr Color Teal{.r = 10, .g = 120, .b = 130, .a = Color::Opaque};

    // Records what each frame drew, as strings, since the draw list's text views don't outlive the frame.
    class FakeBackend : public aspire::graphics::RenderBackend
    {
    public:
        auto submit(const DrawList& list, Color clear) -> bool override
        {
            auto& frame = frames.emplace_back();

            for (const auto& item : list.items())
            {
                if (const auto* text = std::get_if<aspire::graphics::DrawText>(&item.primitive); text != nullptr)
                {
                    frame.emplace_back(text->text);
                }
            }

            clears.emplace_back(clear);
            return !fail;
        }

        // NOLINTBEGIN(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)
        std::vector<Log> frames;
        std::vector<Color> clears;
        bool fail{false};
        // NOLINTEND(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)
    };

    // Draws its name, logs "update <name>" and "fixed <name>", and runs an optional callback on update.
    class Recorder : public Node
    {
    public:
        Recorder(Log& log, std::string_view name) : log_{log}
        {
            setName(name);
        }

        auto whenUpdated(std::function<void()> x) -> void
        {
            onUpdate_ = std::move(x);
        }

        auto draw(aspire::graphics::Renderer& x) const -> void override
        {
            x.text(getName(), {});
        }

        auto update([[maybe_unused]] float x) -> void override
        {
            log_.get().push_back("update " + std::string{getName()});

            if (onUpdate_)
            {
                onUpdate_();
            }
        }

        auto updateFixed([[maybe_unused]] float x) -> void override
        {
            log_.get().push_back("fixed " + std::string{getName()});
        }

    private:
        std::reference_wrapper<Log> log_;
        std::function<void()> onUpdate_;
    };

    auto Make(Log& log, std::string_view name) -> std::shared_ptr<Recorder>
    {
        return std::make_shared<Recorder>(log, name);
    }

    // The names drawn, in draw order.
    auto Names(const DrawList& list) -> Log
    {
        Log names;

        for (const auto& item : list.items())
        {
            names.emplace_back(std::get<aspire::graphics::DrawText>(item.primitive).text);
        }

        return names;
    }
}

TEST(RenderService, submitsOneSortedFramePerRender)
{
    Log log;
    FakeBackend backend;
    const auto service = std::make_shared<RenderService>();
    service->setBackend(&backend);

    // The first root is on a higher layer, so it draws after the second.
    const auto ui = Make(log, "ui");
    ui->setLayer(UiLayer);
    service->addChild(ui);
    service->addChild(Make(log, "world"));
    service->startup();

    service->render();
    service->render();

    ASSERT_EQ(std::size(backend.frames), 2);
    EXPECT_EQ(backend.frames.front(), (Log{"world", "ui"}));
}

TEST(RenderService, collectsRootsInChildOrder)
{
    Log log;
    const auto service = std::make_shared<RenderService>();
    const auto a = Make(log, "a");
    a->addChild(Make(log, "a1"));
    service->addChild(a);
    service->addChild(Make(log, "b"));
    service->addChild(std::make_shared<aspire::core::Object>());
    service->startup();

    service->render();

    EXPECT_EQ(Names(service->drawList()), (Log{"a", "a1", "b"}));
}

TEST(RenderService, withoutBackendStillCollects)
{
    Log log;
    const auto service = std::make_shared<RenderService>();
    service->addChild(Make(log, "a"));
    service->startup();

    service->render();

    EXPECT_EQ(Names(service->drawList()), (Log{"a"}));
}

TEST(RenderService, clearColorReachesBackend)
{
    FakeBackend backend;
    const auto service = std::make_shared<RenderService>();
    service->setBackend(&backend);
    service->startup();

    EXPECT_EQ(service->getClearColor().a, Color::Opaque);
    EXPECT_EQ(service->getClearColor().r, 0);

    service->setClearColor(Teal);
    service->render();

    ASSERT_EQ(std::size(backend.clears), 1);
    EXPECT_EQ(backend.clears.front().g, Teal.g);
}

TEST(RenderService, clearColorLoadsFromJson)
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<RenderService>();

    auto json = nlohmann::json::parse(R"({ "type": "RenderService", "clearColor": [10, 120, 130] })");

    const auto service = std::dynamic_pointer_cast<RenderService>(aspire::parser::ReadJson(factory, json));
    ASSERT_NE(service, nullptr);
    EXPECT_EQ(service->getClearColor().b, Teal.b);
}

TEST(RenderService, backendFailureEndsRunWithFailure)
{
    FakeBackend backend;
    backend.fail = true;
    const auto engine = std::make_shared<aspire::core::Engine>();
    const auto service = std::make_shared<RenderService>();
    service->setBackend(&backend);
    engine->addChild(service);

    EXPECT_EQ(engine->run(), EXIT_FAILURE);
    EXPECT_EQ(std::size(backend.frames), 1);
}

TEST(RenderService, updatesEnabledNodesParentsFirst)
{
    Log log;
    const auto service = std::make_shared<RenderService>();
    const auto a = Make(log, "a");
    a->addChild(Make(log, "a1"));
    service->addChild(a);
    service->addChild(Make(log, "b"));
    service->startup();

    service->update(Dt);
    service->updateFixed(DtFixed);

    EXPECT_EQ(log, (Log{"update a", "update a1", "update b", "fixed a", "fixed a1", "fixed b"}));
}

TEST(RenderService, disabledNodeFreezesItsSubtreeButStillDraws)
{
    Log log;
    const auto service = std::make_shared<RenderService>();
    const auto paused = Make(log, "paused");
    paused->setEnabled(false);
    paused->addChild(Make(log, "below"));
    service->addChild(paused);
    service->addChild(Make(log, "running"));
    service->startup();

    service->update(Dt);
    service->render();

    EXPECT_EQ(log, (Log{"update running"}));
    EXPECT_EQ(Names(service->drawList()), (Log{"paused", "below", "running"}));
}

TEST(RenderService, hiddenNodeStillUpdates)
{
    Log log;
    const auto service = std::make_shared<RenderService>();
    const auto hidden = Make(log, "hidden");
    hidden->setVisible(false);
    service->addChild(hidden);
    service->startup();

    service->update(Dt);

    EXPECT_EQ(log, (Log{"update hidden"}));
}

TEST(RenderService, nodeRemovedDuringUpdateIsSkipped)
{
    Log log;
    const auto service = std::make_shared<RenderService>();
    const auto a = Make(log, "a");
    const auto b = Make(log, "b");
    auto* rawB = b.get();
    a->whenUpdated([rawB] { rawB->remove(); });
    service->addChild(a);
    service->addChild(b);
    service->startup();

    service->update(Dt);

    EXPECT_EQ(log, (Log{"update a"}));
}

TEST(RenderService, nodeAddedDuringUpdateStartsNextFrame)
{
    Log log;
    const auto service = std::make_shared<RenderService>();
    auto* rawService = service.get();
    const auto a = Make(log, "a");
    const auto c = Make(log, "c");
    auto added = false;
    a->whenUpdated(
        [rawService, &c, &added]
        {
            if (!added)
            {
                added = true;
                rawService->addChild(c);
            }
        });
    service->addChild(a);
    service->startup();

    service->update(Dt);
    EXPECT_EQ(log, (Log{"update a"}));

    service->update(Dt);
    EXPECT_EQ(log, (Log{"update a", "update a", "update c"}));
}

TEST(RenderService, engineFrameDrivesTheScene)
{
    Log log;
    FakeBackend backend;
    const auto engine = std::make_shared<aspire::core::Engine>();
    const auto service = std::make_shared<RenderService>();
    service->setBackend(&backend);
    service->addChild(Make(log, "a"));
    engine->addChild(service);
    engine->startup();

    engine->iterate(engine->getIntervalFixed());

    EXPECT_EQ(log, (Log{"update a", "fixed a"}));
    ASSERT_EQ(std::size(backend.frames), 1);
    EXPECT_EQ(backend.frames.front(), (Log{"a"}));
}
