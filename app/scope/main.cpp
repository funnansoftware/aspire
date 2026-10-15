// SDL selects its callback entry points when this macro is defined before SDL_main.h.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

import std;
import aspire.sdl;
import scope.painter;
import scope.world;

namespace
{
    constexpr const char* WindowTitle = "Scope - SDL3";
    constexpr int WindowWidth = 1280;
    constexpr int WindowHeight = 720;
    constexpr SDL_WindowFlags WindowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    constexpr double NanosecondsPerSecond = 1e9;
    constexpr std::string_view FontFile = "fonts/B612Mono-Regular.ttf";

    // How often the frame rate in the readouts is worked out.
    constexpr double RateSeconds = 0.5;

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

    auto Failed(const char* operation) -> bool
    {
        Log(SDL_LOG_PRIORITY_ERROR, std::format("{}: {}", operation, SDL_GetError()));
        return false;
    }

    struct Options
    {
        std::uint64_t frames{};
        std::string capture;
        // Runs unthrottled, without vsync, to measure how fast frames can be made.
        bool benchmark{};
        // Starts frozen, at the same moment every run, so captures can be compared.
        bool paused{};
        bool noGlow{};
    };

    // Parses the command line. Empty if the app should exit, with `result` saying how.
    auto Parse(std::span<char*> arguments, SDL_AppResult& result) -> std::optional<Options>
    {
        constexpr std::string_view framesPrefix = "--frames=";
        constexpr std::string_view capturePrefix = "--capture=";
        Options options;

        const auto reject = [&result](std::string_view message) -> std::optional<Options>
        {
            Log(SDL_LOG_PRIORITY_ERROR, message);
            result = SDL_APP_FAILURE;
            return std::nullopt;
        };

        for (const std::string_view argument : arguments | std::views::drop(1))
        {
            if (argument == "--help")
            {
                Log(SDL_LOG_PRIORITY_INFO,
                    "Usage: scope [--paused] [--no-glow] [--frames=N [--capture=FILE.png] [--benchmark]]  (N must be positive; omit for "
                    "interactive mode)");
                result = SDL_APP_SUCCESS;
                return std::nullopt;
            }

            if (argument == "--benchmark")
            {
                options.benchmark = true;
                continue;
            }

            if (argument == "--paused")
            {
                options.paused = true;
                continue;
            }

            if (argument == "--no-glow")
            {
                options.noGlow = true;
                continue;
            }

            if (argument.starts_with(capturePrefix))
            {
                options.capture = argument.substr(std::size(capturePrefix));
                continue;
            }

            if (!argument.starts_with(framesPrefix))
            {
                return reject(std::format("Unknown argument: {}", argument));
            }

            const auto value = argument.substr(std::size(framesPrefix));
            const auto* const first = std::data(value);
            const auto* const last = std::next(first, std::ssize(value));
            const auto parsed = std::from_chars(first, last, options.frames);

            if (std::make_error_code(parsed.ec) || parsed.ptr != last || options.frames == 0)
            {
                return reject("--frames requires a positive integer");
            }
        }

        if ((!std::empty(options.capture) || options.benchmark) && options.frames == 0)
        {
            return reject("--capture and --benchmark need --frames");
        }

        return options;
    }

    // The SDL host: owns the window, its renderer and the painter that draws into it, and drives the scope from SDL's
    // callbacks.
    struct App
    {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        std::unique_ptr<scope::Painter> painter;
        scope::Scope scope;
        Options options;
        Uint64 previousTicks = 0;
        Uint64 startTicks = 0;
        std::uint64_t renderedFrames = 0;
        double paintMilliseconds = 0.0;
        // The frames made since the frame rate was last worked out, and when that was.
        Uint64 rateTicks = 0;
        std::uint32_t rateFrames = 0;
        // Lifecycle events may arrive from a platform thread. Only the iterate callback
        // reads the clock; lifecycle callbacks publish these two flags.
        std::atomic<bool> background{false};
        std::atomic<bool> discardElapsed{false};
        bool minimized = false;
    };

    // Creates the window with a renderer: SDL's GPU renderer where the platform has one.
    auto OpenWindow(App& app) -> bool
    {
        app.window = SDL_CreateWindow(WindowTitle, WindowWidth, WindowHeight, WindowFlags);

        if (app.window == nullptr)
        {
            return Failed("Create window");
        }

        app.renderer = aspire::sdl::CreateRenderer(app.window);

        if (app.renderer == nullptr)
        {
            return Failed("Create renderer");
        }

        // The callback-rate hint still limits desktop work if a backend cannot enable vsync.
        if (!SDL_SetRenderVSync(app.renderer, app.options.benchmark ? 0 : 1))
        {
            Log(SDL_LOG_PRIORITY_INFO, std::format("Vsync unavailable: {}", SDL_GetError()));
        }

        return true;
    }

    // Says what draws the scope, for the line under its title: SDL's and SDL_ttf's versions, and SDL's renderer.
    auto Caption(const App& app) -> std::string
    {
        const auto sdl = SDL_GetVersion();
        const auto ttf = TTF_Version();
        auto renderer = std::string{SDL_GetRendererName(app.renderer)};
        std::ranges::transform(renderer, std::begin(renderer), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return std::format("SDL {}.{}.{} / SDL_TTF {}.{}.{} / {}", SDL_VERSIONNUM_MAJOR(sdl), SDL_VERSIONNUM_MINOR(sdl), SDL_VERSIONNUM_MICRO(sdl),
                           SDL_VERSIONNUM_MAJOR(ttf), SDL_VERSIONNUM_MINOR(ttf), SDL_VERSIONNUM_MICRO(ttf), renderer);
    }

    // Loads the readout font from next to the executable into the painter. The scope still runs without it,
    // unlabelled.
    auto LoadFont(scope::Painter& painter) -> void
    {
        const auto* basePath = SDL_GetBasePath();
        const auto path = (std::filesystem::path{basePath != nullptr ? basePath : ""} / FontFile).string();
        std::size_t size = 0;
        auto* data = SDL_LoadFile(path.c_str(), &size);

        if (data == nullptr)
        {
            Log(SDL_LOG_PRIORITY_WARN, std::format("Load font {}: {}", path, SDL_GetError()));
            return;
        }

        if (!painter.loadFont({static_cast<const char*>(data), size}))
        {
            Log(SDL_LOG_PRIORITY_WARN, std::format("Load font {}: {}", path, SDL_GetError()));
        }

        SDL_free(data);
    }

    // Saves the renderer's frame, before it's presented, if this is the frame to capture.
    auto Capture(const App& app) -> bool
    {
        if (std::empty(app.options.capture) || app.options.frames == 0 || app.renderedFrames + 1 < app.options.frames)
        {
            return true;
        }

        auto* surface = SDL_RenderReadPixels(app.renderer, nullptr);
        const auto saved = surface != nullptr && SDL_SavePNG(surface, app.options.capture.c_str());
        SDL_DestroySurface(surface);

        if (!saved)
        {
            return Failed("Capture frame");
        }

        Log(SDL_LOG_PRIORITY_INFO, std::format("Captured {}", app.options.capture));
        return true;
    }

    // Has the painter draw a frame, and presents it.
    auto Present(App& app) -> bool
    {
        const auto painted = app.painter->paint(app.scope);
        app.paintMilliseconds += app.painter->getMilliseconds();

        if (!painted)
        {
            return Failed("Paint frame");
        }

        return Capture(app) && (SDL_RenderPresent(app.renderer) || Failed("Present frame"));
    }

    // Counts a frame, and every so often works out the frame rate the readouts show.
    auto CountFrame(App& app, Uint64 now) -> void
    {
        ++app.rateFrames;
        const auto seconds = static_cast<double>(now - app.rateTicks) / NanosecondsPerSecond;

        if (seconds >= RateSeconds)
        {
            app.painter->setFramesPerSecond(static_cast<double>(app.rateFrames) / seconds);
            app.rateFrames = 0;
            app.rateTicks = now;
        }
    }

    auto KeyPressed(App& app, SDL_Keycode key) -> SDL_AppResult
    {
        switch (key)
        {
            case SDLK_UP:
                app.scope.stepRange(1);
                break;
            case SDLK_DOWN:
                app.scope.stepRange(-1);
                break;
            case SDLK_A:
                app.scope.toggleCycling();
                break;
            case SDLK_P:
                app.scope.togglePalette();
                break;
            case SDLK_G:
                app.scope.toggleGlow();
                break;
            case SDLK_SPACE:
                app.scope.togglePause();
                break;
            case SDLK_ESCAPE:
                return SDL_APP_SUCCESS;
            default:
                break;
        }

        return SDL_APP_CONTINUE;
    }
}

// NOLINTNEXTLINE(readability-identifier-naming)
auto SDL_AppInit(void** appstate, int argc, char** argv) -> SDL_AppResult
{
    *appstate = nullptr;
    auto result = SDL_APP_CONTINUE;
    auto options = Parse(std::span{argv, static_cast<std::size_t>(argc)}, result);

    if (!options.has_value())
    {
        return result;
    }

    auto state = std::unique_ptr<App>(new (std::nothrow) App);
    if (state == nullptr)
    {
        Log(SDL_LOG_PRIORITY_ERROR, "Unable to allocate application state");
        return SDL_APP_FAILURE;
    }
    auto& app = *state;
    *appstate = state.release(); // SDL_AppQuit also runs after partially completed initialization.
    app.options = std::move(*options);
    if (app.options.paused)
    {
        // Frozen at the start, with the range held, every run shows the same frame.
        app.scope.togglePause();
        app.scope.toggleCycling();
    }
    if (app.options.noGlow)
    {
        app.scope.toggleGlow();
    }
    SDL_SetAppMetadata("Scope", "0.1.0", "org.aspire.scope");
#ifndef SDL_PLATFORM_EMSCRIPTEN
    // A benchmark iterates as fast as it can; otherwise, at the display's usual rate. On the web, any rate makes SDL
    // pace frames with timers, so it's left to the browser's animation frames, which follow the display.
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, app.options.benchmark ? "0" : "60");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return Fail("Initialize SDL video");
    }
    if (!OpenWindow(app))
    {
        return SDL_APP_FAILURE;
    }

    app.previousTicks = SDL_GetTicksNS();
    app.startTicks = app.previousTicks;
    app.rateTicks = app.previousTicks;

    const auto caption = Caption(app);
    Log(SDL_LOG_PRIORITY_INFO, caption);
    app.painter = std::make_unique<scope::Painter>(app.renderer);
    app.painter->setCaption(caption);
    LoadFont(*app.painter);
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
        case SDL_EVENT_KEY_DOWN:
            if (!event->key.repeat)
            {
                return KeyPressed(app, event->key.key);
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            // Scrolling up zooms in, to a shorter range. A trackpad sends many fractional scrolls per swipe, so only
            // whole ticks step it, or one swipe would run through every range at once.
            if (event->wheel.integer_y != 0)
            {
                app.scope.stepRange(event->wheel.integer_y > 0 ? -1 : 1);
            }
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
    auto elapsed = static_cast<double>(now - app.previousTicks) / NanosecondsPerSecond;
    app.previousTicks = now;

    // The first frame after the app resumes advances nothing.
    if (app.discardElapsed.exchange(false))
    {
        elapsed = 0.0;
    }
    if (app.background.load() || app.minimized)
    {
        return SDL_APP_CONTINUE;
    }

    app.scope.advance(static_cast<float>(elapsed));

    if (!Present(app))
    {
        return SDL_APP_FAILURE;
    }

    ++app.renderedFrames;
    CountFrame(app, now);

    if (app.options.frames == 0 || app.renderedFrames < app.options.frames)
    {
        return SDL_APP_CONTINUE;
    }

    const auto frames = static_cast<double>(app.renderedFrames);
    const auto seconds = static_cast<double>(SDL_GetTicksNS() - app.startTicks) / NanosecondsPerSecond;
    Log(SDL_LOG_PRIORITY_INFO, std::format("Painted {} frames in {:.2f} ms each, at {:.1f} frames per second", app.renderedFrames,
                                           app.paintMilliseconds / frames, frames / seconds));
    return SDL_APP_SUCCESS;
}

// NOLINTNEXTLINE(readability-identifier-naming)
auto SDL_AppQuit(void* appstate, SDL_AppResult /*result*/) -> void
{
    const std::unique_ptr<App> app(static_cast<App*>(appstate));
    if (app != nullptr)
    {
        // The painter's textures and text go before the renderer they were made with.
        app->painter.reset();
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
    }
    // SDL calls SDL_Quit after this callback.
}
