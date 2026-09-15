module;

#include <raylib.h>

export module aspire.raylib.window;

import std;
import aspire.core.object;
import aspire.core.engine;
import aspire.core.event;
import aspire.core.overloaded;
import aspire.raylib.drawable;

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

        ~Window() override
        {
            UnloadRenderTexture(target_);
            CloseWindow();
        }

        Window(const Window&) = delete;
        auto operator=(const Window&) -> Window& = delete;

        Window(Window&&) = delete;
        auto operator=(Window&&) -> Window& = delete;

        // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
        auto width() const -> int
        {
            // NOLINTNEXTLINE([misc-multiple-inheritance)
            return GetScreenWidth();
        }

        // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
        auto height() const -> int
        {
            // NOLINTNEXTLINE([misc-multiple-inheritance)
            return GetScreenHeight();
        }

        auto setScale(Vector2 scale) noexcept -> void
        {
            scale_ = scale;

            UnloadRenderTexture(target_);

            const auto scaledWidth = static_cast<int>(static_cast<float>(width()) / scale_.x);
            const auto scaledHeight = static_cast<int>(static_cast<float>(height()) / scale_.y);
            target_ = LoadRenderTexture(scaledWidth, scaledHeight);
        }

        [[nodiscard]] auto getScale() const noexcept -> Vector2
        {
            return scale_;
        }

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
            translateEvents();
        }

    protected:
        auto onEvent(aspire::core::Event& e) -> void override
        {
            std::visit(aspire::core::Overloaded{[this](aspire::core::EventStartup&) { engine_ = getParent<aspire::core::Engine>(); },
                                                [this](aspire::core::EventRender&)
                                                {
                                                    beginDraw(BLACK);

                                                    for (auto& drawable : getChildren<Drawable>())
                                                    {
                                                        drawable->draw();
                                                    }

                                                    endDraw();
                                                },
                                                [](auto&&) {}},
                       e);
        }

        auto translateEvents() -> void
        {
            auto engine = engine_.lock();

            if (engine == nullptr)
            {
                return;
            }

            if (WindowShouldClose())
            {
                engine->enqueueEvent(aspire::core::EventWindow{.type = aspire::core::EventWindow::Type::Closed});
            }
        }

    private:
        std::weak_ptr<aspire::core::Engine> engine_;
        RenderTexture2D target_{};
        Vector2 scale_{.x = 1.0, .y = 1.0};
    };
}
