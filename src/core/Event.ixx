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
        enum class Type : std::uint8_t
        {
            Unknown,
            KeyPressed,
            KeyReleased,
            KeyRepeated
        };

        enum class Key : std::uint8_t
        {
            Unknown,       // Key: Unknown, used for no key pressed
            Apostrophe,    // Key: '
            Comma,         // Key: ,
            Minus,         // Key: -
            Period,        // Key: .
            Slash,         // Key: /
            Zero,          // Key: 0
            One,           // Key: 1
            Two,           // Key: 2
            Three,         // Key: 3
            Four,          // Key: 4
            Five,          // Key: 5
            Six,           // Key: 6
            Seven,         // Key: 7
            Eight,         // Key: 8
            Nine,          // Key: 9
            Semicolon,     // Key: ;
            Equal,         // Key: =
            A,             // Key: A | a
            B,             // Key: B | b
            C,             // Key: C | c
            D,             // Key: D | d
            E,             // Key: E | e
            F,             // Key: F | f
            G,             // Key: G | g
            H,             // Key: H | h
            I,             // Key: I | i
            J,             // Key: J | j
            K,             // Key: K | k
            L,             // Key: L | l
            M,             // Key: M | m
            N,             // Key: N | n
            O,             // Key: O | o
            P,             // Key: P | p
            Q,             // Key: Q | q
            R,             // Key: R | r
            S,             // Key: S | s
            T,             // Key: T | t
            U,             // Key: U | u
            V,             // Key: V | v
            W,             // Key: W | w
            X,             // Key: X | x
            Y,             // Key: Y | y
            Z,             // Key: Z | z
            Left_Bracket,  // Key: [
            Backslash,     // Key: '\'
            Right_Bracket, // Key: ]
            Grave,         // Key: `
            Space,         // Key: Space
            Escape,        // Key: Esc
            Enter,         // Key: Enter
            Tab,           // Key: Tab
            Backspace,     // Key: Backspace
            Insert,        // Key: Ins
            Delete,        // Key: Del
            Right,         // Key: Cursor right
            Left,          // Key: Cursor left
            Down,          // Key: Cursor down
            Up,            // Key: Cursor up
            Page_Up,       // Key: Page up
            Page_Down,     // Key: Page down
            Home,          // Key: Home
            End,           // Key: End
            Caps_Lock,     // Key: Caps lock
            Scroll_Lock,   // Key: Scroll down
            Num_Lock,      // Key: Num lock
            Print_Screen,  // Key: Print screen
            Pause,         // Key: Pause
            F1,            // Key: F1
            F2,            // Key: F2
            F3,            // Key: F3
            F4,            // Key: F4
            F5,            // Key: F5
            F6,            // Key: F6
            F7,            // Key: F7
            F8,            // Key: F8
            F9,            // Key: F9
            F10,           // Key: F10
            F11,           // Key: F11
            F12,           // Key: F12
            LeftShift,     // Key: Shift left
            LeftControl,   // Key: Control left
            LeftAlt,       // Key: Alt left
            LeftSuper,     // Key: Super left
            RightShift,    // Key: Shift right
            RightControl,  // Key: Control right
            RightAlt,      // Key: Alt right
            RightSuper,    // Key: Super right
            Kb_Menu,       // Key: KB menu
            Kp_0,          // Key: Keypad 0
            Kp_1,          // Key: Keypad 1
            Kp_2,          // Key: Keypad 2
            Kp_3,          // Key: Keypad 3
            Kp_4,          // Key: Keypad 4
            Kp_5,          // Key: Keypad 5
            Kp_6,          // Key: Keypad 6
            Kp_7,          // Key: Keypad 7
            Kp_8,          // Key: Keypad 8
            Kp_9,          // Key: Keypad 9
            KpDecimal,     // Key: Keypad .
            KpDivide,      // Key: Keypad /
            KpMultiply,    // Key: Keypad *
            KpSubtract,    // Key: Keypad -
            KpAdd,         // Key: Keypad +
            KpEnter,       // Key: Keypad Enter
            KpEqual,       // Key: Keypad =
            BACK,          // Key: Android back button
            MENU,          // Key: Android menu button
            VOLUME_UP,     // Key: Android volume up button
            VOLUME_DOWN,   // Key: Android volume down button
        };

        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        Type type{Type::Unknown};
        Key key{Key::Unknown};
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