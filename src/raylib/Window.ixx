module;

#include <raylib.h>

export module aspire.raylib.window;

export namespace aspire::raylib
{
    class Window
    {
    public:
        Window(int width, int height, const char* title)
        {
            InitWindow(width, height, title);
        }

        ~Window()
        {
            CloseWindow();
        }

        auto shouldClose() const -> bool
        {
            return WindowShouldClose();
        }
    };
}