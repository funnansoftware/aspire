// SDL selects its callback entry points when this macro is defined before SDL_main.h.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <nameof.hpp>

import std;
import aspire.core;
import aspire.graphics;
import aspire.parser;
import aspire.sdl;
import sl.character;
import sl.level;
import sl.viewworld;

namespace
{
    constexpr int WindowWidth{1280};
    constexpr int WindowHeight{720};

    // The game draws at a quarter of the window's size, and SDL scales it up by a whole number.
    constexpr int CanvasWidth{320};
    constexpr int CanvasHeight{180};

    // The world sits in the right half of the canvas, and the character starts near its top-left corner.
    constexpr aspire::core::Vec2 WorldPosition{.x = CanvasWidth / 2.0F, .y = 0.0F};
    constexpr aspire::core::Vec2 CharacterStart{.x = 50.0F, .y = 50.0F};

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

    // Registers every type the data files name.
    auto Register(aspire::core::ObjectFactory& factory) -> void
    {
        factory.registerObject<aspire::core::Object>();
        factory.registerObject<aspire::core::Data>();
        factory.registerObject<aspire::core::Database>();
        factory.registerObject<aspire::graphics::Sprite>();
        factory.registerObject<sl::Character>();
        factory.registerObject<sl::Level>();
        factory.registerObject<sl::ViewWorld>();
    }

    // Reads a data file next to the executable, logging which one is missing or broken.
    auto Read(const aspire::core::ObjectFactory& factory, const std::filesystem::path& base, const std::filesystem::path& file)
        -> std::shared_ptr<aspire::core::Object>
    {
        const auto path = base / file;

        if (!std::filesystem::exists(path))
        {
            Log(SDL_LOG_PRIORITY_ERROR, std::format("Missing data file: {}", path.generic_string()));
            return nullptr;
        }

        auto object = aspire::parser::ReadFile(factory, path);

        if (object == nullptr)
        {
            Log(SDL_LOG_PRIORITY_ERROR, std::format("Unreadable data file: {}", path.generic_string()));
        }

        return object;
    }

    // Builds the scene: the world, holding the level and the hero.
    auto Scene(const aspire::core::ObjectFactory& factory, const std::filesystem::path& base) -> std::shared_ptr<aspire::graphics::RenderService>
    {
        const auto level = Read(factory, base, "database/levels/level_1.json");
        const auto hero = std::dynamic_pointer_cast<aspire::graphics::Sprite>(Read(factory, base, "database/textures/hero.json"));

        if (level == nullptr || hero == nullptr)
        {
            return nullptr;
        }

        hero->setPosition(CharacterStart);

        const auto world = std::make_shared<sl::ViewWorld>();
        world->setPosition(WorldPosition);
        world->addChild(level);
        world->addChild(hero);

        auto scene = std::make_shared<aspire::graphics::RenderService>();
        scene->addChild(world);
        return scene;
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
            Log(SDL_LOG_PRIORITY_INFO, "Usage: srd-lite [--frames=N]  (N must be positive; omit for interactive mode)");
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
        if (std::make_error_code(parsed.ec) || parsed.ptr != last || frameLimit == 0)
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
    SDL_SetAppMetadata("srd-lite", "0.1.0", "org.aspire.srd-lite");
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return Fail("Initialize SDL video");
    }
    if (!SDL_CreateWindowAndRenderer("Not D&D Like", WindowWidth, WindowHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &app.window,
                                     &app.renderer))
    {
        return Fail("Create window and renderer");
    }
    if (!SDL_SetRenderLogicalPresentation(app.renderer, CanvasWidth, CanvasHeight, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE))
    {
        return Fail("Set logical presentation");
    }
    // The callback-rate hint still limits desktop work if a backend cannot enable vsync.
    if (!SDL_SetRenderVSync(app.renderer, 1))
    {
        Log(SDL_LOG_PRIORITY_INFO, std::format("Vsync unavailable: {}", SDL_GetError()));
    }

    // Data and textures are copied next to the executable (preloaded at the root on the web).
    const auto* basePath = SDL_GetBasePath();
    const std::filesystem::path base = basePath != nullptr ? basePath : "";

    app.backend = std::make_unique<aspire::sdl::RenderBackend>(app.renderer);
    app.backend->setAssetRoot(base);

    aspire::core::ObjectFactory factory;
    Register(factory);
    const auto database = Read(factory, base, "database/database.json");
    const auto game = Read(factory, base, "config/Game.json");
    const auto scene = Scene(factory, base);

    if (database == nullptr || game == nullptr || scene == nullptr)
    {
        return SDL_APP_FAILURE;
    }

    scene->setBackend(app.backend.get());

    // The database and game data aren't services: they start and shut down with the Engine but don't tick.
    app.engine = std::make_shared<aspire::core::Engine>();
    app.engine->addChild(database);
    app.engine->addChild(game);
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
        case SDL_EVENT_KEY_DOWN:
            // Escape quits, as it did under raylib.
            if (event->key.key == SDLK_ESCAPE)
            {
                app.engine->quit();
                return SDL_APP_SUCCESS;
            }
            break;
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
        case SDL_EVENT_DID_ENTER_BACKGROUND:
            app.background.store(true);
            app.discardElapsed.store(true);
            return SDL_APP_CONTINUE;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
            app.discardElapsed.store(true);
            app.background.store(false);
            return SDL_APP_CONTINUE;
        case SDL_EVENT_WINDOW_MINIMIZED:
            app.minimized = true;
            return SDL_APP_CONTINUE;
        case SDL_EVENT_WINDOW_RESTORED:
            app.minimized = false;
            app.discardElapsed.store(true);
            return SDL_APP_CONTINUE;
        default:
            break;
    }

    // Input arrives on the main thread: translate it in render coordinates and queue it for the scene.
    if (auto translated = aspire::sdl::ToEvent(*event, app.renderer); translated.has_value())
    {
        app.engine->enqueueEvent(std::move(*translated));
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
