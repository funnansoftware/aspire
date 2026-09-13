#include <raylib.h>

import std;

auto main() -> int
try
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "Hello Raylib");
    SetTargetFPS(60);

    const auto texturePath = std::filesystem::path("D:/dev/aspire/assets/kenney/1-bit-pack/Tilesheet/colored-transparent.png");
    const auto texture = LoadTexture(texturePath.string().c_str());

    constexpr auto gameScreenWidth = 640;
    constexpr auto gameScreenHeight = 360;
    const auto target = LoadRenderTexture(gameScreenWidth, gameScreenHeight);
    const auto bg = Color{.r = 71, .g = 45, .b = 60, .a = 255};

    while (!WindowShouldClose())
    {
        // Render Game View.
        BeginTextureMode(target);
        ClearBackground(bg);
        // DrawTextureRec(texture, Rectangle{18, 18, 16, 16}, Vector2{30, 30}, WHITE);
        DrawTexture(texture, 0, 0, WHITE);
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