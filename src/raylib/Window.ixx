module;

#include <raylib.h>

export module aspire.raylib.window;
import aspire.core.object;

export namespace aspire::raylib
{
    class Window : public aspire::core::Object
    {
    public:
        Window(int width, int height, const char* title)
        {
            SetConfigFlags(FLAG_WINDOW_RESIZABLE);
            InitWindow(width, height, title);

            // Initialize the render texture with the current scale.
            setScale(scale_);
        }

        ~Window()
        {
            UnloadRenderTexture(target_);
            CloseWindow();
        }

        Window(const Window&) = delete;
        auto operator=(const Window&) -> Window& = delete;

        Window(Window&&) = delete;
        auto operator=(Window&&) -> Window& = delete;

        auto beginDraw(Color x) -> void
        {
            BeginTextureMode(target_);
            ClearBackground(x);
        }

        auto endDraw() -> void
        {
            EndTextureMode();

            BeginDrawing();
            ClearBackground(BLACK);

            DrawTexturePro(
                target_.texture,
                Rectangle{.x = 0, .y = 0, .width = static_cast<float>(target_.texture.width), .height = static_cast<float>(-target_.texture.height)},
                Rectangle{.x = 0, .y = 0, .width = static_cast<float>(width()), .height = static_cast<float>(height())}, Vector2{.x = 0, .y = 0}, 0,
                WHITE);

            EndDrawing();
            PollInputEvents();
            SwapScreenBuffer();
        }

        auto width() const -> int
        {
            return GetScreenWidth();
        }

        auto height() const -> int
        {
            return GetScreenHeight();
        }

        auto setScale(Vector2 scale) noexcept -> void
        {
            scale_ = scale;

            UnloadRenderTexture(target_);
            target_ = LoadRenderTexture(static_cast<int>(width() / scale_.x), static_cast<int>(height() / scale_.y));
        }

        [[nodiscard]] auto getScale() const noexcept -> Vector2
        {
            return scale_;
        }

        [[nodiscard]] static auto shouldClose() -> bool
        {
            return WindowShouldClose();
        }

    private:
        RenderTexture2D target_{};
        Vector2 scale_{1.0, 1.0};
    };
}
