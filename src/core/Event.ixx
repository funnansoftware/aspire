module;
#include <chrono>
export module aspire.core.event;

import std;

export namespace aspire::core
{
    struct EventWindow
    {
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        bool handled{false};
    };

    struct EventKeyboard
    {
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        bool handled{false};
    };

    struct EventMouse
    {
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        bool handled{false};
    };

    struct EventJoystick
    {
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        bool handled{false};
    };

    struct EventUpdate
    {
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        bool handled{false};
    };

    struct EventUpdateFixed
    {
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        bool handled{false};
    };

    struct EventRender
    {
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        bool handled{false};
    };

    struct EventUser
    {
        EventUser() = default;
        virtual ~EventUser() = default;

        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        bool handled{false};
    };

    using Event =
        std::variant<EventWindow, EventKeyboard, EventMouse, EventJoystick, EventUpdate, EventUpdateFixed, EventRender, std::unique_ptr<EventUser>>;
}