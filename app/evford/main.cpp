// SDL selects its callback entry points when this macro is defined before SDL_main.h.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdlib>

import std;
import aspire.core;

#include "world.hpp"

namespace
{
    constexpr std::size_t ColorCount = 4;
    constexpr std::size_t ParticlesPerColor = evford::ParticleCount / ColorCount;
    static_assert(evford::ParticleCount % ColorCount == 0);
    constexpr std::array<SDL_Color, ColorCount> Colors{{{.r = 107, .g = 222, .b = 195, .a = 255},
                                                        {.r = 119, .g = 184, .b = 255, .a = 255},
                                                        {.r = 195, .g = 164, .b = 255, .a = 255},
                                                        {.r = 255, .g = 185, .b = 132, .a = 255}}};
    constexpr SDL_Color BackgroundColor{.r = 14, .g = 20, .b = 30, .a = 255};
    constexpr SDL_Color FieldColor{.r = 21, .g = 31, .b = 44, .a = 255};
    constexpr SDL_Color BorderColor{.r = 48, .g = 65, .b = 84, .a = 255};
    constexpr SDL_Color TitleColor{.r = 228, .g = 237, .b = 247, .a = 255};
    constexpr SDL_Color TextColor{.r = 150, .g = 171, .b = 194, .a = 255};
    constexpr SDL_Color StatusColor{.r = 107, .g = 222, .b = 195, .a = 255};
    constexpr SDL_FRect ResetButton{.x = 816.0F, .y = 28.0F, .w = 112.0F, .h = 40.0F};
    constexpr SDL_FPoint TitlePosition{.x = 32.0F, .y = 32.0F};
    constexpr SDL_FPoint ResetLabelPosition{.x = 844.0F, .y = 44.0F};
    constexpr SDL_FPoint SummaryPosition{.x = 32.0F, .y = 56.0F};
    constexpr SDL_FPoint ControlsPosition{.x = 32.0F, .y = 496.0F};
    constexpr SDL_FPoint StatusPosition{.x = 864.0F, .y = 496.0F};

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

    auto SetDrawColor(SDL_Renderer* renderer, SDL_Color color) -> bool
    {
        return SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    }

    auto RenderText(SDL_Renderer* renderer, SDL_FPoint position, const char* text) -> bool
    {
        return SDL_RenderDebugText(renderer, position.x, position.y, text);
    }

    // The particle field as an Engine service: input, fixed-step simulation, and drawing.
    class Simulation : public aspire::core::Service
    {
    public:
        // The renderer must outlive the service's last frame. The host shuts Engine down before destroying it.
        explicit Simulation(SDL_Renderer* renderer) : renderer_{renderer}
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

        auto update(float /*unused*/) -> void override
        {
        }

        auto updateFixed(float x) -> void override
        {
            // Engine drains its fixed steps every frame, so steps skipped while paused don't build up.
            if (!paused_)
            {
                evford::Advance(world_, x);
            }
        }

        auto render() -> void override
        {
            if (!draw())
            {
                Log(SDL_LOG_PRIORITY_ERROR, std::format("Render frame: {}", SDL_GetError()));
                quit(EXIT_FAILURE);
            }
        }

    protected:
        auto onStartup() -> void override
        {
            evford::Reset(world_);
            paused_ = false;
        }

    private:
        auto quit(int x) const -> void
        {
            if (const auto engine = getParent<aspire::core::Engine>(); engine != nullptr)
            {
                engine->quit(x);
            }
        }

        auto pressKey(aspire::core::EventKeyboard::Key x) -> void
        {
            switch (x)
            {
                case aspire::core::EventKeyboard::Key::Space:
                    paused_ = !paused_;
                    break;
                case aspire::core::EventKeyboard::Key::R:
                    evford::Reset(world_);
                    break;
                case aspire::core::EventKeyboard::Key::Escape:
                    quit(EXIT_SUCCESS);
                    break;
                default:
                    break;
            }
        }

        auto pressPointer(aspire::core::Vec2 x) -> void
        {
            // Ignore presses in the letterbox. The reset control also works on touch screens.
            if (x.x < 0.0F || x.x >= evford::CanvasWidth || x.y < 0.0F || x.y >= evford::CanvasHeight)
            {
                return;
            }

            const SDL_FPoint point{.x = x.x, .y = x.y};

            if (SDL_PointInRectFloat(&point, &ResetButton))
            {
                evford::Reset(world_);
            }
            else
            {
                paused_ = !paused_;
            }
        }

        auto draw() -> bool
        {
            auto* renderer = renderer_;
            if (!SetDrawColor(renderer, BackgroundColor) || !SDL_RenderClear(renderer))
            {
                return false;
            }

            const SDL_FRect field{.x = evford::FieldLeft,
                                  .y = evford::FieldTop,
                                  .w = evford::FieldRight - evford::FieldLeft,
                                  .h = evford::FieldBottom - evford::FieldTop};
            if (!SetDrawColor(renderer, FieldColor) || !SDL_RenderFillRect(renderer, &field) || !SetDrawColor(renderer, BorderColor)
                || !SDL_RenderRect(renderer, &field) || !SDL_RenderFillRect(renderer, &ResetButton))
            {
                return false;
            }

            // Rendering consumes simulation columns into one reusable SDL buffer. Not std::views::zip: with
            // aspire.core imported, libc++'s zip_view hits ambiguous partial specializations under clang 22.
            std::ranges::transform(world_.x, world_.y, std::begin(rectangles_),
                                   [](float x, float y) { return SDL_FRect{.x = x, .y = y, .w = evford::ParticleSize, .h = evford::ParticleSize}; });
            for (std::size_t batch = 0; batch < ColorCount; ++batch)
            {
                const auto rectangles = std::span{rectangles_}.subspan(batch * ParticlesPerColor, ParticlesPerColor);
                if (!SetDrawColor(renderer, Colors.at(batch))
                    || !SDL_RenderFillRects(renderer, std::data(rectangles), static_cast<int>(std::size(rectangles))))
                {
                    return false;
                }
            }

            // SDL's built-in debug font keeps this example entirely asset-free.
            return SetDrawColor(renderer, TitleColor) && RenderText(renderer, TitlePosition, "EVFORD / PARTICLE FIELD")
                   && RenderText(renderer, ResetLabelPosition, "RESET") && SetDrawColor(renderer, TextColor)
                   && RenderText(renderer, SummaryPosition, "1024 particles. Four color batches. One shared world.")
                   && RenderText(renderer, ControlsPosition, "SPACE / CLICK / TAP  pause     R  reset     ESC  quit")
                   && SetDrawColor(renderer, StatusColor) && RenderText(renderer, StatusPosition, paused_ ? "PAUSED" : "RUNNING")
                   && SDL_RenderPresent(renderer);
        }

        SDL_Renderer* renderer_;
        evford::World world_;
        std::array<SDL_FRect, evford::ParticleCount> rectangles_{};
        bool paused_{false};
    };

    // The SDL host: owns the window, renderer and Engine, and drives Engine from SDL's callbacks.
    struct App
    {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
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

    // Only the keys evford uses. Full SDL event translation belongs to the future aspire.sdl.
    auto ToKey(SDL_Keycode x) -> std::optional<aspire::core::EventKeyboard::Key>
    {
        switch (x)
        {
            case SDLK_SPACE:
                return aspire::core::EventKeyboard::Key::Space;
            case SDLK_R:
                return aspire::core::EventKeyboard::Key::R;
            case SDLK_ESCAPE:
                return aspire::core::EventKeyboard::Key::Escape;
            default:
                return std::nullopt;
        }
    }

    auto KeyPressed(aspire::core::EventKeyboard::Key x) -> aspire::core::EventKeyboard
    {
        aspire::core::EventKeyboard event;
        event.type = aspire::core::EventKeyboard::Type::KeyPressed;
        event.key = x;
        return event;
    }

    // Mouse clicks and touches both arrive as a left-button press, in render coordinates.
    auto PointerPressed(float x, float y) -> aspire::core::EventMouse
    {
        aspire::core::EventMouse event;
        event.type = aspire::core::EventMouse::Type::ButtonPressed;
        event.button = aspire::core::EventMouse::Button::Left;
        event.position = {.x = x, .y = y};
        return event;
    }
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

    app.engine = std::make_shared<aspire::core::Engine>();
    app.engine->setIntervalFixed(FixedStep);
    app.engine->addChild(std::make_shared<Simulation>(app.renderer));
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
        case SDL_EVENT_KEY_DOWN:
            if (!event->key.repeat)
            {
                if (const auto key = ToKey(event->key.key); key.has_value())
                {
                    app.engine->enqueueEvent(KeyPressed(*key));
                }
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            // Touch produces its own event; ignore its synthetic mouse counterpart.
            if (event->button.button == SDL_BUTTON_LEFT && event->button.which != SDL_TOUCH_MOUSEID)
            {
                if (!SDL_ConvertEventToRenderCoordinates(app.renderer, event))
                {
                    return Fail("Convert mouse coordinates");
                }
                app.engine->enqueueEvent(PointerPressed(event->button.x, event->button.y));
            }
            break;
        case SDL_EVENT_FINGER_DOWN:
            if (event->tfinger.touchID == SDL_MOUSE_TOUCHID)
            {
                break;
            }
            if (!SDL_ConvertEventToRenderCoordinates(app.renderer, event))
            {
                return Fail("Convert touch coordinates");
            }
            app.engine->enqueueEvent(PointerPressed(event->tfinger.x, event->tfinger.y));
            break;
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
        // Shut the services down while the renderer they draw with is still valid.
        if (app->engine != nullptr)
        {
            app->engine->shutdown();
            app->engine.reset();
        }
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
    }
    // SDL calls SDL_Quit after this callback.
}
