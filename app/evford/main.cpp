// SDL selects its callback entry points when this macro is defined before SDL_main.h.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdlib>

import std;
import aspire.core;
import aspire.graphics;
import aspire.sdl;

#include "world.hpp"

namespace
{
    using aspire::core::Vec2;
    using aspire::graphics::Color;
    using aspire::graphics::Rect;

    constexpr std::size_t ColorCount = 4;
    constexpr std::size_t ParticlesPerColor = evford::ParticleCount / ColorCount;
    static_assert(evford::ParticleCount % ColorCount == 0);
    constexpr std::array<Color, ColorCount> Colors{{{.r = 107, .g = 222, .b = 195, .a = 255},
                                                    {.r = 119, .g = 184, .b = 255, .a = 255},
                                                    {.r = 195, .g = 164, .b = 255, .a = 255},
                                                    {.r = 255, .g = 185, .b = 132, .a = 255}}};
    constexpr Color BackgroundColor{.r = 14, .g = 20, .b = 30, .a = 255};
    constexpr Color FieldColor{.r = 21, .g = 31, .b = 44, .a = 255};
    constexpr Color BorderColor{.r = 48, .g = 65, .b = 84, .a = 255};
    constexpr Color TitleColor{.r = 228, .g = 237, .b = 247, .a = 255};
    constexpr Color TextColor{.r = 150, .g = 171, .b = 194, .a = 255};
    constexpr Color StatusColor{.r = 107, .g = 222, .b = 195, .a = 255};
    constexpr Rect Field{
        .x = evford::FieldLeft, .y = evford::FieldTop, .w = evford::FieldRight - evford::FieldLeft, .h = evford::FieldBottom - evford::FieldTop};
    constexpr Rect ResetButton{.x = 816.0F, .y = 28.0F, .w = 112.0F, .h = 40.0F};
    constexpr Vec2 TitlePosition{.x = 32.0F, .y = 32.0F};
    constexpr Vec2 ResetLabelPosition{.x = 844.0F, .y = 44.0F};
    constexpr Vec2 SummaryPosition{.x = 32.0F, .y = 56.0F};
    constexpr Vec2 ControlsPosition{.x = 32.0F, .y = 496.0F};
    constexpr Vec2 StatusPosition{.x = 864.0F, .y = 496.0F};

    // 1/120 s isn't a whole number of nanoseconds; truncating it is fine.
    constexpr auto FixedStep = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>{1.0 / 120.0});

    auto Log(SDL_LogPriority priority, std::string_view message) -> void
    {
        // SDL logging is printf-style, so preformatted text goes through a fixed format.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "%.*s", static_cast<int>(std::size(message)), std::data(message));
    }

    auto Fail(const char* operation) -> SDL_AppResult
    {
        Log(SDL_LOG_PRIORITY_ERROR, std::format("{}: {}", operation, SDL_GetError()));
        return SDL_APP_FAILURE;
    }

    // The particle field as a scene node: advances the world each fixed step and draws it, with its labels.
    // Disabling the node pauses it: a disabled node keeps drawing but gets no updates.
    class ParticleField : public aspire::graphics::Node
    {
    public:
        auto reseed() -> void
        {
            evford::Reset(world_);
        }

        auto updateFixed(float x) -> void override
        {
            evford::Advance(world_, x);
        }

        auto draw(aspire::graphics::Renderer& x) const -> void override
        {
            x.rect(Field, FieldColor);
            x.outline(Field, BorderColor);
            x.rect(ResetButton, BorderColor);

            for (std::size_t i = 0; i < evford::ParticleCount; ++i)
            {
                x.rect({.x = world_.x.at(i), .y = world_.y.at(i), .w = evford::ParticleSize, .h = evford::ParticleSize},
                       Colors.at(i / ParticlesPerColor));
            }

            // SDL's built-in debug font keeps this example entirely asset-free.
            x.text("EVFORD / PARTICLE FIELD", TitlePosition, TitleColor);
            x.text("RESET", ResetLabelPosition, TitleColor);
            x.text("1024 particles. Four color batches. One shared world.", SummaryPosition, TextColor);
            x.text("SPACE / CLICK / TAP  pause     R  reset     ESC  quit", ControlsPosition, TextColor);
            x.text(getEnabled() ? "RUNNING" : "PAUSED", StatusPosition, StatusColor);
        }

    protected:
        auto onStartup() -> void override
        {
            reseed();
            setEnabled(true);
        }

    private:
        evford::World world_;
    };

    // Turns input into actions on the field. RenderService doesn't route input to nodes yet, so a service does it.
    class Controls : public aspire::core::Service
    {
    public:
        explicit Controls(std::shared_ptr<ParticleField> field) : field_{std::move(field)}
        {
        }

        auto event(aspire::core::Event& x) -> void override
        {
            if (const auto* key = std::get_if<aspire::core::EventKeyboard>(&x); key != nullptr)
            {
                if (key->type == aspire::core::EventKeyboard::Type::KeyPressed)
                {
                    pressKey(key->key);
                }
            }
            else if (const auto* mouse = std::get_if<aspire::core::EventMouse>(&x); mouse != nullptr)
            {
                if (mouse->type == aspire::core::EventMouse::Type::ButtonPressed && mouse->button == aspire::core::EventMouse::Button::Left)
                {
                    pressPointer(mouse->position);
                }
            }
        }

        auto update([[maybe_unused]] float x) -> void override
        {
        }

        auto updateFixed([[maybe_unused]] float x) -> void override
        {
        }

        auto render() -> void override
        {
        }

    private:
        auto togglePause() const -> void
        {
            field_->setEnabled(!field_->getEnabled());
        }

        auto quit() const -> void
        {
            if (const auto engine = getParent<aspire::core::Engine>(); engine != nullptr)
            {
                engine->quit();
            }
        }

        auto pressKey(aspire::core::EventKeyboard::Key x) const -> void
        {
            switch (x)
            {
                case aspire::core::EventKeyboard::Key::Space:
                    togglePause();
                    break;
                case aspire::core::EventKeyboard::Key::R:
                    field_->reseed();
                    break;
                case aspire::core::EventKeyboard::Key::Escape:
                    quit();
                    break;
                default:
                    break;
            }
        }

        auto pressPointer(Vec2 x) const -> void
        {
            // Ignore presses in the letterbox. The reset control also works on touch screens.
            if (x.x < 0.0F || x.x >= evford::CanvasWidth || x.y < 0.0F || x.y >= evford::CanvasHeight)
            {
                return;
            }

            const auto inReset =
                x.x >= ResetButton.x && x.x < ResetButton.x + ResetButton.w && x.y >= ResetButton.y && x.y < ResetButton.y + ResetButton.h;

            if (inReset)
            {
                field_->reseed();
            }
            else
            {
                togglePause();
            }
        }

        std::shared_ptr<ParticleField> field_;
    };

    // The SDL host: owns the window, renderer and Engine, and drives Engine from SDL's callbacks.
    struct App
    {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        std::unique_ptr<aspire::sdl::RenderBackend> backend;
        std::shared_ptr<aspire::core::Engine> engine;
        Uint64 previousTicks = 0;
        std::uint64_t frameLimit = 0;
        std::uint64_t renderedFrames = 0;
        // Lifecycle events may arrive from a platform thread. Only the iterate callback
        // reads the clock; lifecycle callbacks publish these two flags.
        std::atomic<bool> background{false};
        std::atomic<bool> discardElapsed{false};
        bool minimized = false;
    };
}

// NOLINTNEXTLINE(readability-identifier-naming)
auto SDL_AppInit(void** appstate, int argc, char** argv) -> SDL_AppResult
{
    *appstate = nullptr;
    std::uint64_t frameLimit = 0;
    for (const std::string_view argument : std::span{argv, static_cast<std::size_t>(argc)} | std::views::drop(1))
    {
        constexpr std::string_view prefix = "--frames=";
        if (argument == "--help")
        {
            Log(SDL_LOG_PRIORITY_INFO, "Usage: evford [--frames=N]  (N must be positive; omit for interactive mode)");
            return SDL_APP_SUCCESS;
        }
        if (argument.substr(0, std::size(prefix)) != prefix)
        {
            Log(SDL_LOG_PRIORITY_ERROR, std::format("Unknown argument: {}", argument));
            return SDL_APP_FAILURE;
        }
        const auto value = argument.substr(std::size(prefix));
        const auto* const first = std::data(value);
        const auto* const last = std::next(first, std::ssize(value));
        const auto parsed = std::from_chars(first, last, frameLimit);
        if (parsed.ec != std::errc{} || parsed.ptr != last || frameLimit == 0)
        {
            Log(SDL_LOG_PRIORITY_ERROR, "--frames requires a positive integer");
            return SDL_APP_FAILURE;
        }
    }

    auto state = std::unique_ptr<App>(new (std::nothrow) App);
    if (state == nullptr)
    {
        Log(SDL_LOG_PRIORITY_ERROR, "Unable to allocate application state");
        return SDL_APP_FAILURE;
    }
    auto& app = *state;
    *appstate = state.release(); // SDL_AppQuit also runs after partially completed initialization.
    app.frameLimit = frameLimit;
    SDL_SetAppMetadata("Evford", "0.1.0", "org.aspire.evford");
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return Fail("Initialize SDL video");
    }
    if (!SDL_CreateWindowAndRenderer("Evford - particle field", evford::CanvasWidth, evford::CanvasHeight,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &app.window, &app.renderer))
    {
        return Fail("Create window and renderer");
    }
    if (!SDL_SetRenderLogicalPresentation(app.renderer, evford::CanvasWidth, evford::CanvasHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX))
    {
        return Fail("Set logical presentation");
    }
    // The callback-rate hint still limits desktop work if a backend cannot enable vsync.
    if (!SDL_SetRenderVSync(app.renderer, 1))
    {
        Log(SDL_LOG_PRIORITY_INFO, std::format("Vsync unavailable: {}", SDL_GetError()));
    }

    app.backend = std::make_unique<aspire::sdl::RenderBackend>(app.renderer);

    const auto scene = std::make_shared<aspire::graphics::RenderService>();
    scene->setBackend(app.backend.get());
    scene->setClearColor(BackgroundColor);
    const auto field = std::make_shared<ParticleField>();
    scene->addChild(field);

    // Controls come first, so they see each event before the scene.
    app.engine = std::make_shared<aspire::core::Engine>();
    app.engine->setIntervalFixed(FixedStep);
    app.engine->addChild(std::make_shared<Controls>(field));
    app.engine->addChild(scene);
    app.engine->startup();
    app.previousTicks = SDL_GetTicksNS();
    return SDL_APP_CONTINUE;
}

// NOLINTNEXTLINE(readability-identifier-naming)
auto SDL_AppEvent(void* appstate, SDL_Event* event) -> SDL_AppResult
{
    auto& app = *static_cast<App*>(appstate);
    switch (event->type)
    {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            app.engine->quit();
            return SDL_APP_SUCCESS;
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
        case SDL_EVENT_DID_ENTER_BACKGROUND:
            app.background.store(true);
            app.discardElapsed.store(true);
            break;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
            app.discardElapsed.store(true);
            app.background.store(false);
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            app.minimized = true;
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            app.minimized = false;
            app.discardElapsed.store(true);
            break;
        default:
            // Input arrives on the main thread: translate it in render coordinates and queue it for the services.
            if (auto translated = aspire::sdl::ToEvent(*event, app.renderer); translated.has_value())
            {
                app.engine->enqueueEvent(std::move(*translated));
            }
            break;
    }
    return SDL_APP_CONTINUE;
}

// NOLINTNEXTLINE(readability-identifier-naming)
auto SDL_AppIterate(void* appstate) -> SDL_AppResult
{
    auto& app = *static_cast<App*>(appstate);
    const auto now = SDL_GetTicksNS();
    auto elapsed = std::chrono::nanoseconds{static_cast<std::int64_t>(now - app.previousTicks)};
    app.previousTicks = now;

    // The first frame after the app resumes advances nothing.
    if (app.discardElapsed.exchange(false))
    {
        elapsed = std::chrono::nanoseconds::zero();
    }
    if (app.background.load() || app.minimized)
    {
        return SDL_APP_CONTINUE;
    }

    app.engine->iterate(elapsed);

    if (!app.engine->running())
    {
        return app.engine->exitCode() == EXIT_SUCCESS ? SDL_APP_SUCCESS : SDL_APP_FAILURE;
    }

    ++app.renderedFrames;
    return app.frameLimit != 0 && app.renderedFrames >= app.frameLimit ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

// NOLINTNEXTLINE(readability-identifier-naming)
auto SDL_AppQuit(void* appstate, SDL_AppResult /*result*/) -> void
{
    const std::unique_ptr<App> app(static_cast<App*>(appstate));
    if (app != nullptr)
    {
        // Shut the services down, then release the backend's textures, while the renderer is still valid.
        if (app->engine != nullptr)
        {
            app->engine->shutdown();
            app->engine.reset();
        }
        app->backend.reset();
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
    }
    // SDL calls SDL_Quit after this callback.
}
