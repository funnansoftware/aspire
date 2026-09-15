#include <raylib.h>
#include <nameof.hpp>

import std;
import aspire;

auto main() -> int
try
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<aspire::raylib::Texture>();
    factory.registerObject<aspire::raylib::TileMap>();

    auto engine = std::make_shared<aspire::core::Engine>();

    constexpr auto windowWidth = 1280;
    constexpr auto windowHeight = 720;
    constexpr auto targetFramesPerSecond = 60;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(windowWidth, windowHeight, "Hello Raylib");
    SetTargetFPS(targetFramesPerSecond);

    constexpr auto gameScreenWidth = 320;
    constexpr auto gameScreenHeight = 180;
    const auto target = LoadRenderTexture(gameScreenWidth, gameScreenHeight);
    const auto bg = Color{.r = 71, .g = 45, .b = 60, .a = 255};

    auto object = aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/levels/level_1.json");
    auto* map = dynamic_cast<aspire::raylib::TileMap*>(object.get());
    map->setPosition(Vector2{.x = gameScreenWidth * 0.5, .y = 0});
    engine->addChild(object);

    auto character = aspire::parser::json::ReadFile(factory, "D:/dev/aspire/app/srd-lite/database/characters/hero.json");
    auto* texture = dynamic_cast<aspire::raylib::Texture*>(character.get());
    texture->setPosition(Vector2{.x = gameScreenWidth * 0.55, .y = 50});
    engine->addChild(character);

    while (!WindowShouldClose())
    {
        // Render Game View.
        BeginTextureMode(target);
        ClearBackground(bg);

        for (const auto& [index, drawable] : std::views::enumerate(engine->getChildren<aspire::raylib::Drawable>()))
        {
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

    return EXIT_SUCCESS;
}
catch (...)
{
    return EXIT_FAILURE;
}
