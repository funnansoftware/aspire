module;
#include <chrono>
export module aspire.core.event;

import std;

export namespace aspire::core
{
    struct EventWindow
    {
        enum class Type : std::uint8_t
        {
            Unknown,
            Resized,
            Closed,
            FocusGained,
            FocusLost,
            Moved
        };

        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        Type type{Type::Unknown};
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

    struct EventStartup
    {
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        bool handled{false};
    };

    struct EventUpdate
    {
        std::chrono::steady_clock::duration elapsed{};
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        bool handled{false};
    };

    struct EventUpdateFixed
    {
        std::chrono::steady_clock::duration elapsed{};
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        bool handled{false};
    };

    struct EventRender
    {
        std::chrono::steady_clock::duration elapsed{};
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        float alpha{1.0F};
        bool handled{false};
    };

    struct EventUser
    {
        EventUser() = default;
        virtual ~EventUser() = default;

        EventUser(const EventUser&) = delete;
        auto operator=(const EventUser&) -> EventUser& = delete;

        EventUser(EventUser&&) noexcept = delete;
        auto operator=(EventUser&&) noexcept -> EventUser& = delete;

        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        bool handled{false};
    };

    using Event = std::variant<EventWindow, EventKeyboard, EventMouse, EventJoystick, EventStartup, EventUpdate, EventUpdateFixed, EventRender,
                               std::unique_ptr<EventUser>>;
}