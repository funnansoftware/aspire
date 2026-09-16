module;

#include <raylib.h>

export module aspire.raylib.window;

import std;
import aspire.core.object;
import aspire.core.engine;
import aspire.core.event;
import aspire.core.overloaded;

export namespace aspire::raylib
{
    class Window : public aspire::core::Object
    {
    public:
        Window(int width, int height, const char* title)
        {
            using Key = aspire::core::EventKeyboard::Key;
            keyMap_[KEY_NULL] = Key::Unknown;
            keyMap_[KEY_A] = Key::A;
            keyMap_[KEY_B] = Key::B;
            keyMap_[KEY_C] = Key::C;
            keyMap_[KEY_D] = Key::D;
            keyMap_[KEY_E] = Key::E;
            keyMap_[KEY_F] = Key::F;
            keyMap_[KEY_G] = Key::G;
            keyMap_[KEY_H] = Key::H;
            keyMap_[KEY_I] = Key::I;
            keyMap_[KEY_J] = Key::J;
            keyMap_[KEY_K] = Key::K;
            keyMap_[KEY_L] = Key::L;
            keyMap_[KEY_M] = Key::M;
            keyMap_[KEY_N] = Key::N;
            keyMap_[KEY_O] = Key::O;
            keyMap_[KEY_P] = Key::P;
            keyMap_[KEY_Q] = Key::Q;
            keyMap_[KEY_R] = Key::R;
            keyMap_[KEY_S] = Key::S;
            keyMap_[KEY_T] = Key::T;
            keyMap_[KEY_U] = Key::U;
            keyMap_[KEY_V] = Key::V;
            keyMap_[KEY_W] = Key::W;
            keyMap_[KEY_X] = Key::X;
            keyMap_[KEY_Y] = Key::Y;
            keyMap_[KEY_Z] = Key::Z;
            keyMap_[KEY_SPACE] = Key::Space;
            keyMap_[KEY_ESCAPE] = Key::Escape;
            keyMap_[KEY_ENTER] = Key::Enter;
            keyMap_[KEY_TAB] = Key::Tab;
            keyMap_[KEY_BACKSPACE] = Key::Backspace;
            keyMap_[KEY_INSERT] = Key::Insert;
            keyMap_[KEY_DELETE] = Key::Delete;
            keyMap_[KEY_RIGHT] = Key::Right;
            keyMap_[KEY_LEFT] = Key::Left;
            keyMap_[KEY_DOWN] = Key::Down;
            keyMap_[KEY_UP] = Key::Up;
            keyMap_[KEY_PAGE_UP] = Key::Page_Up;
            keyMap_[KEY_PAGE_DOWN] = Key::Page_Down;
            keyMap_[KEY_HOME] = Key::Home;
            keyMap_[KEY_END] = Key::End;
            keyMap_[KEY_CAPS_LOCK] = Key::Caps_Lock;
            keyMap_[KEY_SCROLL_LOCK] = Key::Scroll_Lock;
            keyMap_[KEY_NUM_LOCK] = Key::Num_Lock;
            keyMap_[KEY_PRINT_SCREEN] = Key::Print_Screen;
            keyMap_[KEY_PAUSE] = Key::Pause;
            keyMap_[KEY_F1] = Key::F1;
            keyMap_[KEY_F2] = Key::F2;
            keyMap_[KEY_F3] = Key::F3;
            keyMap_[KEY_F4] = Key::F4;
            keyMap_[KEY_F5] = Key::F5;
            keyMap_[KEY_F6] = Key::F6;
            keyMap_[KEY_F7] = Key::F7;
            keyMap_[KEY_F8] = Key::F8;
            keyMap_[KEY_F9] = Key::F9;
            keyMap_[KEY_F10] = Key::F10;
            keyMap_[KEY_F11] = Key::F11;
            keyMap_[KEY_F12] = Key::F12;
            keyMap_[KEY_LEFT_SHIFT] = Key::LeftShift;
            keyMap_[KEY_LEFT_CONTROL] = Key::LeftControl;
            keyMap_[KEY_LEFT_ALT] = Key::LeftAlt;
            keyMap_[KEY_LEFT_SUPER] = Key::LeftSuper;
            keyMap_[KEY_RIGHT_SHIFT] = Key::RightShift;
            keyMap_[KEY_RIGHT_CONTROL] = Key::RightControl;
            keyMap_[KEY_RIGHT_ALT] = Key::RightAlt;
            keyMap_[KEY_RIGHT_SUPER] = Key::RightSuper;
            keyMap_[KEY_MENU] = Key::Kb_Menu;

            using Button = aspire::core::EventMouse::Button;
            mouseButtonMap_[MOUSE_BUTTON_LEFT] = Button::Left;
            mouseButtonMap_[MOUSE_BUTTON_RIGHT] = Button::Right;
            mouseButtonMap_[MOUSE_BUTTON_MIDDLE] = Button::Middle;
            mouseButtonMap_[MOUSE_BUTTON_SIDE] = Button::Side;
            mouseButtonMap_[MOUSE_BUTTON_EXTRA] = Button::Extra;
            mouseButtonMap_[MOUSE_BUTTON_FORWARD] = Button::Back;

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

        auto beginDraw(Color x) const -> void
        {
            BeginTextureMode(target_);
            ClearBackground(x);
        }

        auto endDraw() const -> void
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
        auto onStartup() -> void override
        {
            engine_ = getParent<aspire::core::Engine>();
        }

        auto onRenderPre() const -> void override
        {
            beginDraw(BLACK);
        }

        auto onRenderPost() const -> void override
        {
            endDraw();
        }

        auto translateEvents() const -> void
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

            for (const auto& [rayKey, aspireKey] : keyMap_)
            {
                if (IsKeyPressed(rayKey))
                {
                    engine->enqueueEvent(aspire::core::EventKeyboard{.type = aspire::core::EventKeyboard::Type::KeyPressed, .key = aspireKey});
                }

                if (IsKeyReleased(rayKey))
                {
                    engine->enqueueEvent(aspire::core::EventKeyboard{.type = aspire::core::EventKeyboard::Type::KeyReleased, .key = aspireKey});
                }

                if (IsKeyPressedRepeat(rayKey))
                {
                    engine->enqueueEvent(aspire::core::EventKeyboard{.type = aspire::core::EventKeyboard::Type::KeyRepeated, .key = aspireKey});
                }
            }

            auto position = GetMousePosition();
            position = {.x = position.x / scale_.x, .y = position.y / scale_.y};

            const auto delta = GetMouseDelta();
            const auto scroll = GetMouseWheelMoveV();
            std::optional<aspire::core::EventMouse::Type> mouseEventType;
            auto button = aspire::core::EventMouse::Button::Unknown;

            for (const auto& [rayMouseButton, aspireMouseButton] : mouseButtonMap_)
            {
                if (IsMouseButtonPressed(rayMouseButton))
                {
                    mouseEventType = aspire::core::EventMouse::Type::ButtonPressed;
                    button = aspireMouseButton;
                }
                else if (IsMouseButtonReleased(rayMouseButton))
                {
                    mouseEventType = aspire::core::EventMouse::Type::ButtonReleased;
                    button = aspireMouseButton;
                }
                else if (delta.x != 0.0F || delta.y != 0.0F)
                {
                    mouseEventType = aspire::core::EventMouse::Type::Moved;
                }
                else if (scroll.x != 0.0F || scroll.y != 0.0F)
                {
                    mouseEventType = aspire::core::EventMouse::Type::Scrolled;
                }
            }

            if (mouseEventType.has_value())
            {
                engine->enqueueEvent(aspire::core::EventMouse{.position = {.x = position.x, .y = position.y},
                                                              .scroll = {.x = scroll.x, .y = scroll.y},
                                                              .delta = {.x = delta.x, .y = delta.y},
                                                              .type = mouseEventType.value(),
                                                              .button = button});
            }
        }

    private:
        std::flat_map<int, aspire::core::EventKeyboard::Key> keyMap_;
        std::flat_map<int, aspire::core::EventMouse::Button> mouseButtonMap_;
        std::weak_ptr<aspire::core::Engine> engine_;
        RenderTexture2D target_{};
        Vector2 scale_{.x = 1.0, .y = 1.0};
    };
}
