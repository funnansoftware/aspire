export module aspire.core:eventuser;

import std;

export namespace aspire::core
{
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
}
