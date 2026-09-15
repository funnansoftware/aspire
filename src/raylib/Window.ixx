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

        Window(const Window&) = delete;
        auto operator=(const Window&) -> Window& = delete;

        Window(Window&&) = delete;
        auto operator=(Window&&) -> Window& = delete;

        [[nodiscard]] static auto shouldClose() -> bool
        {
            return WindowShouldClose();
        }
    };
}
