// SDL selects its callback entry points when this macro is defined before SDL_main.h.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

import std;
import scope.world;
import scopegpu.shaders;
import scopegpu2d.painter;

namespace
{
    constexpr const char* WindowTitle = "Scope - SDL3 GPU 2D";
    constexpr int WindowWidth = 1280;
    constexpr int WindowHeight = 720;
    constexpr SDL_WindowFlags WindowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    constexpr double NanosecondsPerSecond = 1e9;
    constexpr double NanosecondsPerMillisecond = 1e6;
    constexpr std::string_view FontFile = "fonts/B612Mono-Regular.ttf";

    // How many frames' times are made room for up front; a longer run grows the room as it goes.
    constexpr std::uint64_t MostTimedFrames = 1U << 20U;

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
        // Holds the range, so the same tracks stay inside the scale ring as they fly.
        bool noCycle{};
        // Shows this many generated tracks in place of the usual ones, or the usual ones if none.
        std::size_t tracks{};
        // Draws into a texture rather than a window, as where there's no display to show one.
        bool offscreen{};
        int width{WindowWidth};
        int height{WindowHeight};
    };

    // Parses a positive whole number.
    auto ParsePositive(std::string_view x, auto& value) -> bool
    {
        const auto* const first = std::data(x);
        const auto* const last = std::next(first, std::ssize(x));
        const auto parsed = std::from_chars(first, last, value);
        return !std::make_error_code(parsed.ec) && parsed.ptr == last && value > 0;
    }

    // Parses the value of --size, a width and height such as 1920x1080.
    auto ParseSize(std::string_view x, Options& options) -> bool
    {
        const auto by = x.find('x');
        return by != std::string_view::npos && ParsePositive(x.substr(0, by), options.width) && ParsePositive(x.substr(by + 1), options.height);
    }

    // An argument that turns an option on.
    struct Switch
    {
        std::string_view name;
        bool Options::* option;
    };

    constexpr auto Switches = std::to_array<Switch>({
        {.name = "--benchmark", .option = &Options::benchmark},
        {.name = "--paused", .option = &Options::paused},
        {.name = "--no-glow", .option = &Options::noGlow},
        {.name = "--no-cycle", .option = &Options::noCycle},
        {.name = "--offscreen", .option = &Options::offscreen},
    });

    // Turns on the option an argument names. False if it names none.
    auto ParseSwitch(std::string_view argument, Options& options) -> bool
    {
        const auto found = std::ranges::find(Switches, argument, &Switch::name);

        if (found == std::end(Switches))
        {
            return false;
        }

        options.*(found->option) = true;
        return true;
    }

    // Parses the command line. Empty if the app should exit, with `result` saying how.
    auto Parse(std::span<char*> arguments, SDL_AppResult& result) -> std::optional<Options>
    {
        constexpr std::string_view framesPrefix = "--frames=";
        constexpr std::string_view capturePrefix = "--capture=";
        constexpr std::string_view sizePrefix = "--size=";
        constexpr std::string_view tracksPrefix = "--tracks=";
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
                    "Usage: scopegpu2d [--paused] [--no-glow] [--no-cycle] [--tracks=N] [--size=WxH] [--offscreen] [--frames=N [--capture=FILE.png] "
                    "[--benchmark]]  (N must be positive; omit for interactive mode)");
                result = SDL_APP_SUCCESS;
                return std::nullopt;
            }

            if (ParseSwitch(argument, options))
            {
                continue;
            }

            if (argument.starts_with(tracksPrefix))
            {
                if (!ParsePositive(argument.substr(std::size(tracksPrefix)), options.tracks))
                {
                    return reject("--tracks requires a positive integer");
                }

                continue;
            }

            if (argument.starts_with(capturePrefix))
            {
                options.capture = argument.substr(std::size(capturePrefix));
                continue;
            }

            if (argument.starts_with(sizePrefix))
            {
                if (!ParseSize(argument.substr(std::size(sizePrefix)), options))
                {
                    return reject("--size requires a width and height, such as --size=1920x1080");
                }

                continue;
            }

            if (!argument.starts_with(framesPrefix))
            {
                return reject(std::format("Unknown argument: {}", argument));
            }

            if (!ParsePositive(argument.substr(std::size(framesPrefix)), options.frames))
            {
                return reject("--frames requires a positive integer");
            }
        }

        if ((!std::empty(options.capture) || options.benchmark || options.offscreen) && options.frames == 0)
        {
            return reject("--capture, --benchmark and --offscreen need --frames");
        }

        return options;
    }

    // The SDL host: owns the window, its renderer and the painter that draws into it, and drives the scope from SDL's
    // callbacks.
    struct App
    {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        // Offscreen, the texture each frame's drawn into.
        SDL_Texture* frame = nullptr;
        std::unique_ptr<scopegpu2d::Painter> painter;
        scope::Scope scope;
        Options options;
        Uint64 previousTicks = 0;
        Uint64 startTicks = 0;
        std::uint64_t renderedFrames = 0;
        double paintMilliseconds = 0.0;
        // SDL's renderer records most of a frame, submits it and waits for the swapchain as it presents.
        double presentMilliseconds = 0.0;
        // How long each frame took from the one before.
        std::vector<double> frameMilliseconds;
        // The frames made since the frame rate was last worked out, and when that was.
        Uint64 rateTicks = 0;
        std::uint32_t rateFrames = 0;
        // Lifecycle events may arrive from a platform thread. Only the iterate callback
        // reads the clock; lifecycle callbacks publish these two flags.
        std::atomic<bool> background{false};
        std::atomic<bool> discardElapsed{false};
        bool minimized = false;
    };

    // Creates the window with SDL's GPU renderer, the one renderer that takes shaders of the app's own, or offscreen,
    // the renderer alone and a texture to draw into. In a window, the renderer draws into a backbuffer of its own and
    // copies it to the window as it presents, with one frame in flight, as it does for scope.
    auto OpenWindow(App& app) -> bool
    {
        if (!app.options.offscreen)
        {
            app.window = SDL_CreateWindow(WindowTitle, app.options.width, app.options.height, WindowFlags);

            if (app.window == nullptr)
            {
                return Failed("Create window");
            }
        }

        app.renderer = SDL_CreateGPURenderer(nullptr, app.window);

        if (app.renderer == nullptr)
        {
            // Without a GPU backend that takes the app's shaders, there's nothing to run them on, and the smoke test
            // skips; any other failure is the renderer's own. Asking overwrites SDL's error, so it's kept first.
            const auto error = std::string{SDL_GetError()};
            const auto* failure = SDL_GPUSupportsShaderFormats(scopegpu::ShaderFormats(), nullptr) ? "Create GPU renderer" : "No GPU renderer";
            Log(SDL_LOG_PRIORITY_ERROR, std::format("{}: {}", failure, error));
            return false;
        }

        if (app.options.offscreen)
        {
            app.frame = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_TARGET, app.options.width, app.options.height);
            return app.frame != nullptr || Failed("Create frame");
        }

        // The callback-rate hint still limits desktop work if a backend cannot enable vsync.
        if (!SDL_SetRenderVSync(app.renderer, app.options.benchmark ? 0 : 1))
        {
            Log(SDL_LOG_PRIORITY_INFO, std::format("Vsync unavailable: {}", SDL_GetError()));
        }

        return true;
    }

    // Says what draws the scope, for the line under its title: SDL's and SDL_ttf's versions, and SDL's renderer with
    // shaders of the app's own.
    auto Caption(const App& app) -> std::string
    {
        const auto sdl = SDL_GetVersion();
        const auto ttf = TTF_Version();
        auto renderer = std::string{SDL_GetRendererName(app.renderer)};
        std::ranges::transform(renderer, std::begin(renderer), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return std::format("SDL {}.{}.{} / SDL_TTF {}.{}.{} / {} + SHADERS", SDL_VERSIONNUM_MAJOR(sdl), SDL_VERSIONNUM_MINOR(sdl),
                           SDL_VERSIONNUM_MICRO(sdl), SDL_VERSIONNUM_MAJOR(ttf), SDL_VERSIONNUM_MINOR(ttf), SDL_VERSIONNUM_MICRO(ttf), renderer);
    }

    // Names the adapter the renderer's device runs on, so a benchmark can tell which it measured. SDL's renderer picks
    // its device itself, and follows hints that a device made with SDL_CreateGPUDevice doesn't.
    auto Adapter(const App& app) -> std::string
    {
        const auto* name =
            SDL_GetStringProperty(SDL_GetGPUDeviceProperties(SDL_GetGPURendererDevice(app.renderer)), SDL_PROP_GPU_DEVICE_NAME_STRING, nullptr);
        return name != nullptr ? name : "an unnamed adapter";
    }

    // Loads the readout font from next to the executable into the painter. The scope still runs without it,
    // unlabelled.
    auto LoadFont(scopegpu2d::Painter& painter) -> void
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

        if (!Capture(app))
        {
            return false;
        }

        const auto presenting = SDL_GetTicksNS();

        // Offscreen, presenting only submits the frame: there's no swapchain to wait for, and nothing else stops the
        // frames queueing up ahead of the GPU. The frame before is waited for instead, before this one's submitted,
        // which keeps one frame in flight as the renderer does in a window.
        if (app.options.offscreen && !SDL_WaitForGPUIdle(SDL_GetGPURendererDevice(app.renderer)))
        {
            return Failed("Wait for the GPU");
        }

        const auto presented = SDL_RenderPresent(app.renderer);
        app.presentMilliseconds += static_cast<double>(SDL_GetTicksNS() - presenting) / NanosecondsPerMillisecond;
        return presented || Failed("Present frame");
    }

    // Describes how long frames took, from one to the next: the median, the slow tail, and the slowest.
    auto FrameTimes(std::vector<double> milliseconds) -> std::string
    {
        constexpr double median = 0.5;
        constexpr double slow = 0.95;
        constexpr double slowest = 0.99;

        if (std::empty(milliseconds))
        {
            return "Frame time unmeasured";
        }

        std::ranges::sort(milliseconds);
        const auto at = [&milliseconds](double share)
        { return milliseconds.at(static_cast<std::size_t>(share * static_cast<double>(std::size(milliseconds) - 1))); };
        return std::format("Frame time p50 {:.3f} ms, p95 {:.3f} ms, p99 {:.3f} ms, max {:.3f} ms", at(median), at(slow), at(slowest),
                           milliseconds.back());
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
        // A run of a set length keeps the scene it started with, so a stray key can't change what's measured or
        // captured. It can still be quit.
        if (app.options.frames != 0 && key != SDLK_ESCAPE)
        {
            return SDL_APP_CONTINUE;
        }

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
    app.frameMilliseconds.reserve(static_cast<std::size_t>(std::min(app.options.frames, MostTimedFrames)));
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
    if (app.options.noCycle && app.scope.getRange().getCycling())
    {
        app.scope.toggleCycling();
    }
    if (app.options.tracks != 0)
    {
        app.scope.setTracks(scope::Traffic(app.options.tracks));
    }
    SDL_SetAppMetadata("Scope GPU 2D", "0.1.0", "org.aspire.scopegpu2d");
#ifndef SDL_PLATFORM_EMSCRIPTEN
    // A benchmark iterates as fast as it can; otherwise, at the display's usual rate.
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, app.options.benchmark ? "0" : "60");
#endif
#ifdef SDL_PLATFORM_WINDOWS
    // The window's own background can show along an edge for a composition while it's dragged smaller, so it's the
    // scope's background rather than black, on every erase: erased only the first time, the parts the window later
    // grows into stay black. The color hint comes with the repo's SDL patches.
    const auto& background = scope::BackgroundColor;
    SDL_SetHint(SDL_HINT_WINDOWS_ERASE_BACKGROUND_COLOR, std::format("#{:02X}{:02X}{:02X}", background.r, background.g, background.b).c_str());
    SDL_SetHint(SDL_HINT_WINDOWS_ERASE_BACKGROUND_MODE, "always");
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
    Log(SDL_LOG_PRIORITY_INFO, std::format("Drawing on {}", Adapter(app)));
    app.painter = scopegpu2d::Painter::create(app.renderer, app.frame);

    if (app.painter == nullptr)
    {
        return Fail("Create painter");
    }

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
            // whole ticks step it, or one swipe would run through every range at once. A run of a set length
            // keeps its range, as it ignores keys.
            if (event->wheel.integer_y != 0 && app.options.frames == 0)
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
    const auto interval = static_cast<double>(now - app.previousTicks) / NanosecondsPerMillisecond;
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

    // From one frame to the next, for a run of a set length: the first follows start-up instead.
    if (app.options.frames != 0 && app.renderedFrames > 0)
    {
        app.frameMilliseconds.push_back(interval);
    }

    ++app.renderedFrames;
    CountFrame(app, now);

    if (app.options.frames == 0 || app.renderedFrames < app.options.frames)
    {
        return SDL_APP_CONTINUE;
    }

    // The last frame's counted once it's drawn, not once it's submitted.
    if (app.options.offscreen && !SDL_WaitForGPUIdle(SDL_GetGPURendererDevice(app.renderer)))
    {
        return Fail("Wait for the GPU");
    }

    const auto frames = static_cast<double>(app.renderedFrames);
    const auto seconds = static_cast<double>(SDL_GetTicksNS() - app.startTicks) / NanosecondsPerSecond;
    Log(SDL_LOG_PRIORITY_INFO, std::format("Painted {} frames in {:.2f} ms each, at {:.1f} frames per second", app.renderedFrames,
                                           app.paintMilliseconds / frames, frames / seconds));
    Log(SDL_LOG_PRIORITY_INFO, std::format("Present {:.3f} ms per frame", app.presentMilliseconds / frames));
    Log(SDL_LOG_PRIORITY_INFO, FrameTimes(app.frameMilliseconds));
    return SDL_APP_SUCCESS;
}

// NOLINTNEXTLINE(readability-identifier-naming)
auto SDL_AppQuit(void* appstate, SDL_AppResult /*result*/) -> void
{
    const std::unique_ptr<App> app(static_cast<App*>(appstate));
    if (app != nullptr)
    {
        // The painter's textures, render states and text go before the renderer they were made with.
        app->painter.reset();
        SDL_DestroyTexture(app->frame);
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
    }
    // SDL calls SDL_Quit after this callback.
}
