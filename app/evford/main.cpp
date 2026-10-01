// SDL selects its callback entry points when this macro is defined before SDL_main.h.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

import std;

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
    constexpr double FixedStepSeconds = 1.0 / 120.0;

    struct App
    {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        evford::World world;
        // Rendering consumes simulation columns into one reusable SDL buffer.
        std::array<SDL_FRect, evford::ParticleCount> rectangles{};
        Uint64 previousTicks = 0;
        std::uint64_t frameLimit = 0;
        std::uint64_t renderedFrames = 0;
        double accumulator = 0.0;
        bool paused = false;
        // Lifecycle events may arrive from a platform thread. Only the iterate callback
        // changes the simulation clock; lifecycle callbacks publish these two flags.
        std::atomic<bool> background{false};
        std::atomic<bool> discardElapsed{false};
        bool minimized = false;
    };

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

    auto ResetClock(App& app) -> void
    {
        app.previousTicks = SDL_GetTicksNS();
        app.accumulator = 0.0;
    }

    auto ResetScene(App& app) -> void
    {
        evford::Reset(app.world);
        ResetClock(app);
    }

    auto ActivatePointer(App& app, float x, float y) -> void
    {
        // Ignore touches in the letterbox. The reset control also works on touch screens.
        if (x < 0.0F || x >= evford::CanvasWidth || y < 0.0F || y >= evford::CanvasHeight)
        {
            return;
        }
        const SDL_FPoint point{.x = x, .y = y};
        if (SDL_PointInRectFloat(&point, &ResetButton))
        {
            ResetScene(app);
        }
        else
        {
            app.paused = !app.paused;
            ResetClock(app);
        }
    }

    auto Render(App& app) -> bool
    {
        auto* renderer = app.renderer;
        if (!SetDrawColor(renderer, BackgroundColor) || !SDL_RenderClear(renderer))
        {
            return false;
        }

        const SDL_FRect field{
            .x = evford::FieldLeft, .y = evford::FieldTop, .w = evford::FieldRight - evford::FieldLeft, .h = evford::FieldBottom - evford::FieldTop};
        if (!SetDrawColor(renderer, FieldColor) || !SDL_RenderFillRect(renderer, &field) || !SetDrawColor(renderer, BorderColor)
            || !SDL_RenderRect(renderer, &field) || !SDL_RenderFillRect(renderer, &ResetButton))
        {
            return false;
        }

        for (auto&& [rectangle, x, y] : std::views::zip(app.rectangles, app.world.x, app.world.y))
        {
            rectangle = {.x = x, .y = y, .w = evford::ParticleSize, .h = evford::ParticleSize};
        }
        for (std::size_t batch = 0; batch < ColorCount; ++batch)
        {
            const auto rectangles = std::span{app.rectangles}.subspan(batch * ParticlesPerColor, ParticlesPerColor);
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
               && SetDrawColor(renderer, StatusColor) && RenderText(renderer, StatusPosition, app.paused ? "PAUSED" : "RUNNING")
               && SDL_RenderPresent(renderer);
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
    ResetScene(app);
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
            return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
            if (!event->key.repeat)
            {
                if (event->key.key == SDLK_ESCAPE)
                {
                    return SDL_APP_SUCCESS;
                }
                if (event->key.key == SDLK_SPACE)
                {
                    app.paused = !app.paused;
                    ResetClock(app);
                }
                else if (event->key.key == SDLK_R)
                {
                    ResetScene(app);
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
                ActivatePointer(app, event->button.x, event->button.y);
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
            ActivatePointer(app, event->tfinger.x, event->tfinger.y);
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
            ResetClock(app);
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            app.minimized = false;
            ResetClock(app);
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
    auto elapsed = static_cast<double>(now - app.previousTicks) / static_cast<double>(SDL_NS_PER_SECOND);
    app.previousTicks = now;
    if (app.discardElapsed.exchange(false))
    {
        elapsed = 0.0;
        app.accumulator = 0.0;
    }
    if (app.background.load() || app.minimized)
    {
        app.accumulator = 0.0;
        return SDL_APP_CONTINUE;
    }
    if (!app.paused)
    {
        app.accumulator += std::min(elapsed, static_cast<double>(evford::MaxFrameSeconds));
        while (app.accumulator >= FixedStepSeconds)
        {
            evford::Advance(app.world, static_cast<float>(FixedStepSeconds));
            app.accumulator -= FixedStepSeconds;
        }
    }
    if (!Render(app))
    {
        return Fail("Render frame");
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
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
    }
    // SDL calls SDL_Quit after this callback.
}
