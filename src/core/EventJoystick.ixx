export module aspire.core:eventjoystick;

import std;

export namespace aspire::core
{
    struct EventJoystick
    {
        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        bool handled{false};
    };
}
