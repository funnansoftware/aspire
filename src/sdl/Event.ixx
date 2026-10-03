module;

#include <SDL3/SDL.h>

export module aspire.sdl:event;

import std;
import aspire.core;

namespace aspire::sdl
{
    using Key = aspire::core::EventKeyboard::Key;
    using Button = aspire::core::EventMouse::Button;

    struct KeyMapping
    {
        SDL_Keycode from;
        Key to;
    };

    // Sorted by SDL keycode at compile time, so ToKey() can binary search.
    template <std::size_t N>
    constexpr auto SortByKeycode(std::array<KeyMapping, N> x) -> std::array<KeyMapping, N>
    {
        std::ranges::sort(x, {}, &KeyMapping::from);
        return x;
    }

    // SDL keycodes follow the keyboard layout: Key::A is the key that types 'a', wherever it sits.
    // Listed in aspire's Key order for reading; SortByKeycode() puts them in lookup order.
    constexpr auto KeyMappings = SortByKeycode(std::array{
        KeyMapping{.from = SDLK_APOSTROPHE, .to = Key::Apostrophe},
        KeyMapping{.from = SDLK_COMMA, .to = Key::Comma},
        KeyMapping{.from = SDLK_MINUS, .to = Key::Minus},
        KeyMapping{.from = SDLK_PERIOD, .to = Key::Period},
        KeyMapping{.from = SDLK_SLASH, .to = Key::Slash},
        KeyMapping{.from = SDLK_0, .to = Key::Zero},
        KeyMapping{.from = SDLK_1, .to = Key::One},
        KeyMapping{.from = SDLK_2, .to = Key::Two},
        KeyMapping{.from = SDLK_3, .to = Key::Three},
        KeyMapping{.from = SDLK_4, .to = Key::Four},
        KeyMapping{.from = SDLK_5, .to = Key::Five},
        KeyMapping{.from = SDLK_6, .to = Key::Six},
        KeyMapping{.from = SDLK_7, .to = Key::Seven},
        KeyMapping{.from = SDLK_8, .to = Key::Eight},
        KeyMapping{.from = SDLK_9, .to = Key::Nine},
        KeyMapping{.from = SDLK_SEMICOLON, .to = Key::Semicolon},
        KeyMapping{.from = SDLK_EQUALS, .to = Key::Equal},
        KeyMapping{.from = SDLK_A, .to = Key::A},
        KeyMapping{.from = SDLK_B, .to = Key::B},
        KeyMapping{.from = SDLK_C, .to = Key::C},
        KeyMapping{.from = SDLK_D, .to = Key::D},
        KeyMapping{.from = SDLK_E, .to = Key::E},
        KeyMapping{.from = SDLK_F, .to = Key::F},
        KeyMapping{.from = SDLK_G, .to = Key::G},
        KeyMapping{.from = SDLK_H, .to = Key::H},
        KeyMapping{.from = SDLK_I, .to = Key::I},
        KeyMapping{.from = SDLK_J, .to = Key::J},
        KeyMapping{.from = SDLK_K, .to = Key::K},
        KeyMapping{.from = SDLK_L, .to = Key::L},
        KeyMapping{.from = SDLK_M, .to = Key::M},
        KeyMapping{.from = SDLK_N, .to = Key::N},
        KeyMapping{.from = SDLK_O, .to = Key::O},
        KeyMapping{.from = SDLK_P, .to = Key::P},
        KeyMapping{.from = SDLK_Q, .to = Key::Q},
        KeyMapping{.from = SDLK_R, .to = Key::R},
        KeyMapping{.from = SDLK_S, .to = Key::S},
        KeyMapping{.from = SDLK_T, .to = Key::T},
        KeyMapping{.from = SDLK_U, .to = Key::U},
        KeyMapping{.from = SDLK_V, .to = Key::V},
        KeyMapping{.from = SDLK_W, .to = Key::W},
        KeyMapping{.from = SDLK_X, .to = Key::X},
        KeyMapping{.from = SDLK_Y, .to = Key::Y},
        KeyMapping{.from = SDLK_Z, .to = Key::Z},
        KeyMapping{.from = SDLK_LEFTBRACKET, .to = Key::Left_Bracket},
        KeyMapping{.from = SDLK_BACKSLASH, .to = Key::Backslash},
        KeyMapping{.from = SDLK_RIGHTBRACKET, .to = Key::Right_Bracket},
        KeyMapping{.from = SDLK_GRAVE, .to = Key::Grave},
        KeyMapping{.from = SDLK_SPACE, .to = Key::Space},
        KeyMapping{.from = SDLK_ESCAPE, .to = Key::Escape},
        KeyMapping{.from = SDLK_RETURN, .to = Key::Enter},
        KeyMapping{.from = SDLK_TAB, .to = Key::Tab},
        KeyMapping{.from = SDLK_BACKSPACE, .to = Key::Backspace},
        KeyMapping{.from = SDLK_INSERT, .to = Key::Insert},
        KeyMapping{.from = SDLK_DELETE, .to = Key::Delete},
        KeyMapping{.from = SDLK_RIGHT, .to = Key::Right},
        KeyMapping{.from = SDLK_LEFT, .to = Key::Left},
        KeyMapping{.from = SDLK_DOWN, .to = Key::Down},
        KeyMapping{.from = SDLK_UP, .to = Key::Up},
        KeyMapping{.from = SDLK_PAGEUP, .to = Key::Page_Up},
        KeyMapping{.from = SDLK_PAGEDOWN, .to = Key::Page_Down},
        KeyMapping{.from = SDLK_HOME, .to = Key::Home},
        KeyMapping{.from = SDLK_END, .to = Key::End},
        KeyMapping{.from = SDLK_CAPSLOCK, .to = Key::Caps_Lock},
        KeyMapping{.from = SDLK_SCROLLLOCK, .to = Key::Scroll_Lock},
        KeyMapping{.from = SDLK_NUMLOCKCLEAR, .to = Key::Num_Lock},
        KeyMapping{.from = SDLK_PRINTSCREEN, .to = Key::Print_Screen},
        KeyMapping{.from = SDLK_PAUSE, .to = Key::Pause},
        KeyMapping{.from = SDLK_F1, .to = Key::F1},
        KeyMapping{.from = SDLK_F2, .to = Key::F2},
        KeyMapping{.from = SDLK_F3, .to = Key::F3},
        KeyMapping{.from = SDLK_F4, .to = Key::F4},
        KeyMapping{.from = SDLK_F5, .to = Key::F5},
        KeyMapping{.from = SDLK_F6, .to = Key::F6},
        KeyMapping{.from = SDLK_F7, .to = Key::F7},
        KeyMapping{.from = SDLK_F8, .to = Key::F8},
        KeyMapping{.from = SDLK_F9, .to = Key::F9},
        KeyMapping{.from = SDLK_F10, .to = Key::F10},
        KeyMapping{.from = SDLK_F11, .to = Key::F11},
        KeyMapping{.from = SDLK_F12, .to = Key::F12},
        KeyMapping{.from = SDLK_LSHIFT, .to = Key::LeftShift},
        KeyMapping{.from = SDLK_LCTRL, .to = Key::LeftControl},
        KeyMapping{.from = SDLK_LALT, .to = Key::LeftAlt},
        KeyMapping{.from = SDLK_LGUI, .to = Key::LeftSuper},
        KeyMapping{.from = SDLK_RSHIFT, .to = Key::RightShift},
        KeyMapping{.from = SDLK_RCTRL, .to = Key::RightControl},
        KeyMapping{.from = SDLK_RALT, .to = Key::RightAlt},
        KeyMapping{.from = SDLK_RGUI, .to = Key::RightSuper},
        KeyMapping{.from = SDLK_APPLICATION, .to = Key::Kb_Menu},
        KeyMapping{.from = SDLK_KP_0, .to = Key::Kp_0},
        KeyMapping{.from = SDLK_KP_1, .to = Key::Kp_1},
        KeyMapping{.from = SDLK_KP_2, .to = Key::Kp_2},
        KeyMapping{.from = SDLK_KP_3, .to = Key::Kp_3},
        KeyMapping{.from = SDLK_KP_4, .to = Key::Kp_4},
        KeyMapping{.from = SDLK_KP_5, .to = Key::Kp_5},
        KeyMapping{.from = SDLK_KP_6, .to = Key::Kp_6},
        KeyMapping{.from = SDLK_KP_7, .to = Key::Kp_7},
        KeyMapping{.from = SDLK_KP_8, .to = Key::Kp_8},
        KeyMapping{.from = SDLK_KP_9, .to = Key::Kp_9},
        KeyMapping{.from = SDLK_KP_PERIOD, .to = Key::KpDecimal},
        KeyMapping{.from = SDLK_KP_DIVIDE, .to = Key::KpDivide},
        KeyMapping{.from = SDLK_KP_MULTIPLY, .to = Key::KpMultiply},
        KeyMapping{.from = SDLK_KP_MINUS, .to = Key::KpSubtract},
        KeyMapping{.from = SDLK_KP_PLUS, .to = Key::KpAdd},
        KeyMapping{.from = SDLK_KP_ENTER, .to = Key::KpEnter},
        KeyMapping{.from = SDLK_KP_EQUALS, .to = Key::KpEqual},
        KeyMapping{.from = SDLK_AC_BACK, .to = Key::BACK},
        KeyMapping{.from = SDLK_MENU, .to = Key::MENU},
        KeyMapping{.from = SDLK_VOLUMEUP, .to = Key::VOLUME_UP},
        KeyMapping{.from = SDLK_VOLUMEDOWN, .to = Key::VOLUME_DOWN},
    });

    static_assert(std::ranges::adjacent_find(KeyMappings, {}, &KeyMapping::from) == std::end(KeyMappings), "Each SDL keycode maps to one aspire key");

    auto ToKey(SDL_Keycode x) -> std::optional<Key>
    {
        const auto found = std::ranges::lower_bound(KeyMappings, x, {}, &KeyMapping::from);

        if (found == std::end(KeyMappings) || found->from != x)
        {
            return std::nullopt;
        }

        return found->to;
    }

    // SDL's X1 and X2 are the side buttons, which raylib (and so aspire) calls Side and Extra.
    auto ToButton(Uint8 x) -> std::optional<Button>
    {
        switch (x)
        {
            case SDL_BUTTON_LEFT:
                return Button::Left;
            case SDL_BUTTON_MIDDLE:
                return Button::Middle;
            case SDL_BUTTON_RIGHT:
                return Button::Right;
            case SDL_BUTTON_X1:
                return Button::Side;
            case SDL_BUTTON_X2:
                return Button::Extra;
            default:
                return std::nullopt;
        }
    }

    // SDL timestamps count nanoseconds from SDL_GetTicksNS()'s start; aspire's use steady_clock.
    auto ToTimePoint(Uint64 x) -> std::chrono::steady_clock::time_point
    {
        const auto now = SDL_GetTicksNS();
        const auto age = now > x ? now - x : Uint64{0};
        return std::chrono::steady_clock::now() - std::chrono::nanoseconds{static_cast<std::int64_t>(age)};
    }

    // Finger positions are normalized to their window. This scales them to window coordinates, the units of mouse events.
    auto WindowSize(SDL_WindowID x) -> std::optional<aspire::core::Vec2>
    {
        auto* const window = SDL_GetWindowFromID(x);
        auto width = 0;
        auto height = 0;

        if (window == nullptr || !SDL_GetWindowSize(window, &width, &height))
        {
            return std::nullopt;
        }

        return aspire::core::Vec2{.x = static_cast<float>(width), .y = static_cast<float>(height)};
    }

    auto WindowEvent(aspire::core::EventWindow::Type type, Uint64 timestamp) -> aspire::core::Event
    {
        aspire::core::EventWindow event;
        event.timestamp = ToTimePoint(timestamp);
        event.type = type;
        return event;
    }

    auto MouseEvent(aspire::core::EventMouse::Type type, aspire::core::Vec2 position, Uint64 timestamp) -> aspire::core::EventMouse
    {
        aspire::core::EventMouse event;
        event.timestamp = ToTimePoint(timestamp);
        event.type = type;
        event.position = position;
        return event;
    }

    // SDL_Event is a C union; reading the member that matches its type is how SDL is used.
    // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)

    auto TranslateKey(const SDL_KeyboardEvent& x) -> std::optional<aspire::core::Event>
    {
        const auto key = ToKey(x.key);

        if (!key.has_value())
        {
            return std::nullopt;
        }

        aspire::core::EventKeyboard event;
        event.timestamp = ToTimePoint(x.timestamp);
        event.key = *key;

        if (!x.down)
        {
            event.type = aspire::core::EventKeyboard::Type::KeyReleased;
        }
        else if (x.repeat)
        {
            event.type = aspire::core::EventKeyboard::Type::KeyRepeated;
        }
        else
        {
            event.type = aspire::core::EventKeyboard::Type::KeyPressed;
        }

        return event;
    }

    auto TranslateButton(const SDL_MouseButtonEvent& x) -> std::optional<aspire::core::Event>
    {
        const auto button = ToButton(x.button);

        if (!button.has_value())
        {
            return std::nullopt;
        }

        const auto type = x.down ? aspire::core::EventMouse::Type::ButtonPressed : aspire::core::EventMouse::Type::ButtonReleased;
        auto event = MouseEvent(type, {.x = x.x, .y = x.y}, x.timestamp);
        event.button = *button;
        return event;
    }

    auto TranslateWheel(const SDL_MouseWheelEvent& x) -> aspire::core::Event
    {
        const auto sign = x.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0F : 1.0F;
        auto event = MouseEvent(aspire::core::EventMouse::Type::Scrolled, {.x = x.mouse_x, .y = x.mouse_y}, x.timestamp);
        event.scroll = {.x = x.x * sign, .y = x.y * sign};
        return event;
    }

    // A finger acts as the left mouse button. scale is the window size, or 1 when the position is already converted.
    auto TranslateFinger(const SDL_TouchFingerEvent& x, aspire::core::Vec2 scale) -> aspire::core::Event
    {
        auto type = aspire::core::EventMouse::Type::Moved;

        if (x.type == SDL_EVENT_FINGER_DOWN)
        {
            type = aspire::core::EventMouse::Type::ButtonPressed;
        }
        else if (x.type == SDL_EVENT_FINGER_UP)
        {
            type = aspire::core::EventMouse::Type::ButtonReleased;
        }

        auto event = MouseEvent(type, {.x = x.x * scale.x, .y = x.y * scale.y}, x.timestamp);
        event.button = Button::Left;
        event.delta = {.x = x.dx * scale.x, .y = x.dy * scale.y};
        return event;
    }

    auto Translate(const SDL_Event& x, bool converted) -> std::optional<aspire::core::Event>
    {
        using WindowType = aspire::core::EventWindow::Type;

        switch (x.type)
        {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                return WindowEvent(WindowType::Closed, x.common.timestamp);
            case SDL_EVENT_WINDOW_RESIZED:
                return WindowEvent(WindowType::Resized, x.common.timestamp);
            case SDL_EVENT_WINDOW_MOVED:
                return WindowEvent(WindowType::Moved, x.common.timestamp);
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                return WindowEvent(WindowType::FocusGained, x.common.timestamp);
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                return WindowEvent(WindowType::FocusLost, x.common.timestamp);
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                return TranslateKey(x.key);
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                // Touches arrive as finger events; drop the mouse events SDL synthesizes from them.
                return x.button.which == SDL_TOUCH_MOUSEID ? std::nullopt : TranslateButton(x.button);
            case SDL_EVENT_MOUSE_MOTION:
            {
                if (x.motion.which == SDL_TOUCH_MOUSEID)
                {
                    return std::nullopt;
                }

                auto event = MouseEvent(aspire::core::EventMouse::Type::Moved, {.x = x.motion.x, .y = x.motion.y}, x.motion.timestamp);
                event.delta = {.x = x.motion.xrel, .y = x.motion.yrel};
                return event;
            }
            case SDL_EVENT_MOUSE_WHEEL:
                return x.wheel.which == SDL_TOUCH_MOUSEID ? std::nullopt : std::optional{TranslateWheel(x.wheel)};
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION:
            {
                // The mouse arrives as mouse events; drop the finger events SDL synthesizes from it.
                if (x.tfinger.touchID == SDL_MOUSE_TOUCHID)
                {
                    return std::nullopt;
                }

                const auto scale = converted ? std::optional{aspire::core::Vec2{.x = 1.0F, .y = 1.0F}} : WindowSize(x.tfinger.windowID);

                if (!scale.has_value())
                {
                    return std::nullopt;
                }

                return TranslateFinger(x.tfinger, *scale);
            }
            default:
                return std::nullopt;
        }
    }

    // NOLINTEND(cppcoreguidelines-pro-type-union-access)
}

export namespace aspire::sdl
{
    /// @brief Translates an SDL event into an aspire event.
    ///
    /// Covers windows (close requests and quit become `EventWindow::Type::Closed`), keys, mouse buttons, motion and
    /// the wheel, and touch. A finger acts as the left mouse button, and the mouse events SDL synthesizes from
    /// touches are dropped, as are the finger events it synthesizes from the mouse. Keys use SDL keycodes, so they
    /// follow the keyboard layout.
    ///
    /// Positions are in window coordinates, or in render coordinates when `renderer` is given.
    ///
    /// @param x The event to translate.
    /// @param renderer The renderer whose coordinates positions should use, or null for window coordinates.
    /// @return The translated event, or `std::nullopt` if the event has no aspire equivalent (lifecycle,
    /// joystick, unmapped keys and buttons), a finger's window can't be found, or the renderer can't convert it.
    /// @note Lifecycle events can arrive on another thread. Handle them before translating, and enqueue only on the
    /// main thread.
    auto ToEvent(const SDL_Event& x, SDL_Renderer* renderer = nullptr) -> std::optional<aspire::core::Event>
    {
        if (renderer == nullptr)
        {
            return Translate(x, false);
        }

        // SDL converts in place, so convert a copy.
        auto converted = x;

        if (!SDL_ConvertEventToRenderCoordinates(renderer, &converted))
        {
            return std::nullopt;
        }

        return Translate(converted, true);
    }
}
