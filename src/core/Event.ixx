export module aspire.core:event;

import std;
export import :eventjoystick;
export import :eventkeyboard;
export import :eventmouse;
export import :eventuser;
export import :eventwindow;

export namespace aspire::core
{
    /// @brief Any input or window event, as queued on Engine and delivered to services.
    using Event = std::variant<EventWindow, EventKeyboard, EventMouse, EventJoystick, std::unique_ptr<EventUser>>;
}