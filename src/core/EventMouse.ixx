export module aspire.core:eventmouse;

import std;
import :vec2;

export namespace aspire::core
{
    struct EventMouse
    {
        enum class Type : std::uint8_t
        {
            Unknown,
            ButtonPressed,
            ButtonReleased,
            Moved,
            Scrolled
        };

        enum class Button : std::uint8_t
        {
            Unknown,
            Left,
            Right,
            Middle,
            Side,
            Extra,
            Back
        };

        Vec2 position{};
        Vec2 scroll{};
        Vec2 delta{};
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        Type type{Type::Unknown};
        Button button{Button::Unknown};
        bool handled{false};
    };
}
