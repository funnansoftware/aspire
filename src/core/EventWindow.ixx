export module aspire.core:eventwindow;

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
}
