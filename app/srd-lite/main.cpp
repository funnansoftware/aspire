#include <raylib.h>
#include <nameof.hpp>

import std;
import aspire;

auto main() -> int
try
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<aspire::raylib::Texture>();

    auto engine = std::make_shared<aspire::core::Engine>();

    constexpr auto windowWidth = 800;
    constexpr auto windowHeight = 600;
    constexpr auto targetFramesPerSecond = 60;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(windowWidth, windowHeight, "Hello Raylib");
    SetTargetFPS(targetFramesPerSecond);

    constexpr auto gameScreenWidth = 640;
    constexpr auto gameScreenHeight = 360;
    const auto target = LoadRenderTexture(gameScreenWidth, gameScreenHeight);
    const auto bg = Color{.r = 71, .g = 45, .b = 60, .a = 255};

    engine->addChild(aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/tiles/ground_0.jsonc"));
    engine->addChild(aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/tiles/ground_1.jsonc"));
    engine->addChild(aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/tiles/ground_2.jsonc"));
    engine->addChild(aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/tiles/ground_3.jsonc"));
    engine->addChild(aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/tiles/ground_4.jsonc"));

    // return engine.run();

    while (!WindowShouldClose())
    {
        // Render Game View.
        BeginTextureMode(target);
        ClearBackground(bg);
        constexpr auto tileSpacing = 32;
        for (const auto& [index, drawable] : std::views::enumerate(engine->getChildren<aspire::raylib::Drawable>()))
        {
            auto* t = dynamic_cast<aspire::raylib::Texture*>(drawable.get());
            t->setPosition(Vector2{.x = static_cast<float>(index * tileSpacing), .y = static_cast<float>(index * tileSpacing)});
            drawable->draw();
        }

        EndTextureMode();

        BeginDrawing();
        ClearBackground(bg);

        DrawTexturePro(
            target.texture,
            Rectangle{.x = 0, .y = 0, .width = static_cast<float>(target.texture.width), .height = static_cast<float>(-target.texture.height)},
            Rectangle{.x = 0, .y = 0, .width = static_cast<float>(GetScreenWidth()), .height = static_cast<float>(GetScreenHeight())},
            Vector2{.x = 0, .y = 0}, 0, WHITE);

        EndDrawing();
        PollInputEvents();
        SwapScreenBuffer();
    }

    CloseWindow();

    return 0;
}
catch (...)
{
    return 1;
}
