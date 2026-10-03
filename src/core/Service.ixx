export module aspire.core:service;

import std;
import :event;
import :object;

export namespace aspire::core
{
    /// @brief Interface for objects that take part in the frame loop.
    ///
    /// A Service must be a direct child of Engine. Engine ticks only its direct children, so a nested Service starts
    /// and shuts down but never ticks. Deeper objects tick through the service that owns them.
    ///
    /// Each frame, Engine calls every started service in child order: `event()` for each queued event, then
    /// `update()`, then `updateFixed()` zero or more times, then `render()`.
    class Service : public Object
    {
    public:
        /// @brief Receives a queued event.
        ///
        /// Every started service receives every event, in child order. Engine doesn't check whether an event is
        /// handled; each service decides whether to act on an event that an earlier service marked handled.
        ///
        /// @param x The event. Its `handled` flag is visible to the services after this one.
        virtual auto event(Event& x) -> void = 0;

        /// @brief Advances the service by one frame of variable length.
        /// @param x Seconds since the previous frame, clamped by Engine to a maximum frame length.
        virtual auto update(float x) -> void = 0;

        /// @brief Advances the service by one fixed step.
        ///
        /// Called after `update()`, once for each fixed interval that has elapsed, so possibly not at all in a frame.
        ///
        /// @param x Seconds in one fixed step. The same value on every call.
        virtual auto updateFixed(float x) -> void = 0;

        /// @brief Draws, or queues drawing for, the current frame.
        ///
        /// Called once per frame, after all updates. Non-const, so a graphics service can fill its queue here.
        virtual auto render() -> void = 0;
    };
}
