#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

import std;

#include "world.hpp"

namespace
{
    constexpr std::size_t colorCount = 4;
    constexpr std::size_t particlesPerColor = evford::particleCount / colorCount;
    static_assert(evford::particleCount % colorCount == 0);
    constexpr std::array<SDL_Color, colorCount> colors{{{107, 222, 195, 255}, {119, 184, 255, 255}, {195, 164, 255, 255}, {255, 185, 132, 255}}};
    constexpr SDL_FRect resetButton{816.0F, 28.0F, 112.0F, 40.0F};
    constexpr double fixedStepSeconds = 1.0 / 120.0;

    struct App
    {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        evford::World world;
        // Rendering consumes simulation columns into one reusable SDL buffer.
        std::array<SDL_FRect, evford::particleCount> rectangles{};
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

    SDL_AppResult fail(const char* operation)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", operation, SDL_GetError());
        return SDL_APP_FAILURE;
    }

    void resetClock(App& app)
    {
        app.previousTicks = SDL_GetTicksNS();
        app.accumulator = 0.0;
    }

    void resetScene(App& app)
    {
        evford::reset(app.world);
        resetClock(app);
    }

    void activatePointer(App& app, float x, float y)
    {
        // Ignore touches in the letterbox. The reset control also works on touch screens.
        if (x < 0.0F || x >= evford::canvasWidth || y < 0.0F || y >= evford::canvasHeight)
        {
            return;
        }
        const SDL_FPoint point{x, y};
        if (SDL_PointInRectFloat(&point, &resetButton))
        {
            resetScene(app);
        }
        else
        {
            app.paused = !app.paused;
            resetClock(app);
        }
    }

    bool render(App& app)
    {
        auto* renderer = app.renderer;
        if (!SDL_SetRenderDrawColor(renderer, 14, 20, 30, 255) || !SDL_RenderClear(renderer))
        {
            return false;
        }

        const SDL_FRect field{evford::fieldLeft, evford::fieldTop, evford::fieldRight - evford::fieldLeft, evford::fieldBottom - evford::fieldTop};
        if (!SDL_SetRenderDrawColor(renderer, 21, 31, 44, 255) || !SDL_RenderFillRect(renderer, &field)
            || !SDL_SetRenderDrawColor(renderer, 48, 65, 84, 255) || !SDL_RenderRect(renderer, &field) || !SDL_RenderFillRect(renderer, &resetButton))
        {
            return false;
        }

        for (std::size_t i = 0; i < evford::particleCount; ++i)
        {
            app.rectangles[i] = {app.world.x[i], app.world.y[i], evford::particleSize, evford::particleSize};
        }
        for (std::size_t batch = 0; batch < colorCount; ++batch)
        {
            const auto& color = colors[batch];
            if (!SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a)
                || !SDL_RenderFillRects(renderer, app.rectangles.data() + batch * particlesPerColor, static_cast<int>(particlesPerColor)))
            {
                return false;
            }
        }

        // SDL's built-in debug font keeps this example entirely asset-free.
        return SDL_SetRenderDrawColor(renderer, 228, 237, 247, 255) && SDL_RenderDebugText(renderer, 32.0F, 32.0F, "EVFORD / PARTICLE FIELD")
               && SDL_RenderDebugText(renderer, 844.0F, 44.0F, "RESET") && SDL_SetRenderDrawColor(renderer, 150, 171, 194, 255)
               && SDL_RenderDebugText(renderer, 32.0F, 56.0F, "1024 particles. Four color batches. One shared world.")
               && SDL_RenderDebugText(renderer, 32.0F, 496.0F, "SPACE / CLICK / TAP  pause     R  reset     ESC  quit")
               && SDL_SetRenderDrawColor(renderer, 107, 222, 195, 255)
               && SDL_RenderDebugText(renderer, 864.0F, 496.0F, app.paused ? "PAUSED" : "RUNNING") && SDL_RenderPresent(renderer);
    }
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{
    *appstate = nullptr;
    std::uint64_t frameLimit = 0;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view argument(argv[i]);
        constexpr std::string_view prefix = "--frames=";
        if (argument == "--help")
        {
            SDL_Log("Usage: evford [--frames=N]  (N must be positive; omit for interactive mode)");
            return SDL_APP_SUCCESS;
        }
        if (argument.substr(0, prefix.size()) != prefix)
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown argument: %s", argv[i]);
            return SDL_APP_FAILURE;
        }
        const auto value = argument.substr(prefix.size());
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), frameLimit);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || frameLimit == 0)
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "--frames requires a positive integer");
            return SDL_APP_FAILURE;
        }
    }

    auto* app = new (std::nothrow) App;
    if (app == nullptr)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to allocate application state");
        return SDL_APP_FAILURE;
    }
    *appstate = app; // SDL_AppQuit also runs after partially completed initialization.
    app->frameLimit = frameLimit;
    SDL_SetAppMetadata("Evford", "0.1.0", "org.aspire.evford");
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return fail("Initialize SDL video");
    }
    if (!SDL_CreateWindowAndRenderer("Evford - particle field", evford::canvasWidth, evford::canvasHeight,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &app->window, &app->renderer))
    {
        return fail("Create window and renderer");
    }
    if (!SDL_SetRenderLogicalPresentation(app->renderer, evford::canvasWidth, evford::canvasHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX))
    {
        return fail("Set logical presentation");
    }
    // The callback-rate hint still limits desktop work if a backend cannot enable vsync.
    if (!SDL_SetRenderVSync(app->renderer, 1))
    {
        SDL_Log("Vsync unavailable: %s", SDL_GetError());
    }
    resetScene(*app);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
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
                    resetClock(app);
                }
                else if (event->key.key == SDLK_R)
                {
                    resetScene(app);
                }
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            // Touch produces its own event; ignore its synthetic mouse counterpart.
            if (event->button.button == SDL_BUTTON_LEFT && event->button.which != SDL_TOUCH_MOUSEID)
            {
                if (!SDL_ConvertEventToRenderCoordinates(app.renderer, event))
                {
                    return fail("Convert mouse coordinates");
                }
                activatePointer(app, event->button.x, event->button.y);
            }
            break;
        case SDL_EVENT_FINGER_DOWN:
            if (event->tfinger.touchID == SDL_MOUSE_TOUCHID)
            {
                break;
            }
            if (!SDL_ConvertEventToRenderCoordinates(app.renderer, event))
            {
                return fail("Convert touch coordinates");
            }
            activatePointer(app, event->tfinger.x, event->tfinger.y);
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
            resetClock(app);
            break;
        case SDL_EVENT_WINDOW_RESTORED:
            app.minimized = false;
            resetClock(app);
            break;
        default:
            break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
    auto& app = *static_cast<App*>(appstate);
    const auto now = SDL_GetTicksNS();
    auto elapsed = static_cast<double>(now - app.previousTicks) / 1000000000.0;
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
        app.accumulator += std::min(elapsed, static_cast<double>(evford::maxFrameSeconds));
        while (app.accumulator >= fixedStepSeconds)
        {
            evford::advance(app.world, static_cast<float>(fixedStepSeconds));
            app.accumulator -= fixedStepSeconds;
        }
    }
    if (!render(app))
    {
        return fail("Render frame");
    }
    ++app.renderedFrames;
    return app.frameLimit != 0 && app.renderedFrames >= app.frameLimit ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult)
{
    auto* app = static_cast<App*>(appstate);
    if (app != nullptr)
    {
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
        delete app;
    }
    // SDL calls SDL_Quit after this callback.
}
