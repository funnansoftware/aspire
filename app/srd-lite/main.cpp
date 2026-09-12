#include <raylib.h>

auto main() -> int
try
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "Hello Raylib");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Hello Raylib", 190, 200, 20, LIGHTGRAY);
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