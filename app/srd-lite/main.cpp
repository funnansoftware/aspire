#include <raylib.h>
#include <memory>
#include <nameof.hpp>
#include <ranges>

import aspire;

auto main() -> int
try
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<aspire::raylib::Texture>();

    auto engine = std::make_shared<aspire::core::Engine>();

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "Hello Raylib");
    SetTargetFPS(60);

    const auto texturePath = std::filesystem::path("D:/dev/aspire/assets/kenney/1-bit-pack/Tilesheet/colored-transparent.png");
    const auto texture = LoadTexture(texturePath.string().c_str());

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
        // DrawTextureRec(texture, Rectangle{18, 18, 16, 16}, Vector2{30, 30}, WHITE);
        // DrawTexture(texture, 0, 0, WHITE);

        for (auto [index, drawable] : std::views::enumerate(engine->getChildren<aspire::raylib::Drawable>()))
        {
            auto* t = dynamic_cast<aspire::raylib::Texture*>(drawable.get());
            t->setPosition(Vector2{static_cast<float>(index * 32), static_cast<float>(index * 32)});
            drawable->draw();
        }

        EndTextureMode();

        BeginDrawing();
        ClearBackground(bg);

        DrawTexturePro(target.texture, Rectangle{0, 0, static_cast<float>(target.texture.width), static_cast<float>(-target.texture.height)},
                       Rectangle{0, 0, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())}, Vector2{0, 0}, 0, WHITE);

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