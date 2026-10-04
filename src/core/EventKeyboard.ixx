export module aspire.core:eventkeyboard;

import std;

export namespace aspire::core
{
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
            /// Key: Unknown, used for no key pressed
            Unknown,
            /// Key: '
            Apostrophe,
            /// Key: ,
            Comma,
            /// Key: -
            Minus,
            /// Key: .
            Period,
            /// Key: /
            Slash,
            /// Key: 0
            Zero,
            /// Key: 1
            One,
            /// Key: 2
            Two,
            /// Key: 3
            Three,
            /// Key: 4
            Four,
            /// Key: 5
            Five,
            /// Key: 6
            Six,
            /// Key: 7
            Seven,
            /// Key: 8
            Eight,
            /// Key: 9
            Nine,
            /// Key: ;
            Semicolon,
            /// Key: =
            Equal,
            /// Key: A | a
            A,
            /// Key: B | b
            B,
            /// Key: C | c
            C,
            /// Key: D | d
            D,
            /// Key: E | e
            E,
            /// Key: F | f
            F,
            /// Key: G | g
            G,
            /// Key: H | h
            H,
            /// Key: I | i
            I,
            /// Key: J | j
            J,
            /// Key: K | k
            K,
            /// Key: L | l
            L,
            /// Key: M | m
            M,
            /// Key: N | n
            N,
            /// Key: O | o
            O,
            /// Key: P | p
            P,
            /// Key: Q | q
            Q,
            /// Key: R | r
            R,
            /// Key: S | s
            S,
            /// Key: T | t
            T,
            /// Key: U | u
            U,
            /// Key: V | v
            V,
            /// Key: W | w
            W,
            /// Key: X | x
            X,
            /// Key: Y | y
            Y,
            /// Key: Z | z
            Z,
            /// Key: [
            Left_Bracket,
            /// Key: '\'
            Backslash,
            /// Key: ]
            Right_Bracket,
            /// Key: `
            Grave,
            /// Key: Space
            Space,
            /// Key: Esc
            Escape,
            /// Key: Enter
            Enter,
            /// Key: Tab
            Tab,
            /// Key: Backspace
            Backspace,
            /// Key: Ins
            Insert,
            /// Key: Del
            Delete,
            /// Key: Cursor right
            Right,
            /// Key: Cursor left
            Left,
            /// Key: Cursor down
            Down,
            /// Key: Cursor up
            Up,
            /// Key: Page up
            Page_Up,
            /// Key: Page down
            Page_Down,
            /// Key: Home
            Home,
            /// Key: End
            End,
            /// Key: Caps lock
            Caps_Lock,
            /// Key: Scroll down
            Scroll_Lock,
            /// Key: Num lock
            Num_Lock,
            /// Key: Print screen
            Print_Screen,
            /// Key: Pause
            Pause,
            /// Key: F1
            F1,
            /// Key: F2
            F2,
            /// Key: F3
            F3,
            /// Key: F4
            F4,
            /// Key: F5
            F5,
            /// Key: F6
            F6,
            /// Key: F7
            F7,
            /// Key: F8
            F8,
            /// Key: F9
            F9,
            /// Key: F10
            F10,
            /// Key: F11
            F11,
            /// Key: F12
            F12,
            /// Key: Shift left
            LeftShift,
            /// Key: Control left
            LeftControl,
            /// Key: Alt left
            LeftAlt,
            /// Key: Super left
            LeftSuper,
            /// Key: Shift right
            RightShift,
            /// Key: Control right
            RightControl,
            /// Key: Alt right
            RightAlt,
            /// Key: Super right
            RightSuper,
            /// Key: KB menu
            Kb_Menu,
            /// Key: Keypad 0
            Kp_0,
            /// Key: Keypad 1
            Kp_1,
            /// Key: Keypad 2
            Kp_2,
            /// Key: Keypad 3
            Kp_3,
            /// Key: Keypad 4
            Kp_4,
            /// Key: Keypad 5
            Kp_5,
            /// Key: Keypad 6
            Kp_6,
            /// Key: Keypad 7
            Kp_7,
            /// Key: Keypad 8
            Kp_8,
            /// Key: Keypad 9
            Kp_9,
            /// Key: Keypad .
            KpDecimal,
            /// Key: Keypad /
            KpDivide,
            /// Key: Keypad *
            KpMultiply,
            /// Key: Keypad -
            KpSubtract,
            /// Key: Keypad +
            KpAdd,
            /// Key: Keypad Enter
            KpEnter,
            /// Key: Keypad =
            KpEqual,
            /// Key: Android back button
            BACK,
            /// Key: Android menu button
            MENU,
            /// Key: Android volume up button
            VOLUME_UP,
            /// Key: Android volume down button
            VOLUME_DOWN,
        };

        std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};
        Type type{Type::Unknown};
        Key key{Key::Unknown};
        bool handled{false};
    };
}
