#include <gtest/gtest.h>

#include <SDL3/SDL.h>

import std;
import aspire.core;
import aspire.sdl;

// SDL_Event is a C union; tests build events by writing the member that matches their type.
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)

namespace
{
    using Key = aspire::core::EventKeyboard::Key;
    using KeyType = aspire::core::EventKeyboard::Type;
    using Button = aspire::core::EventMouse::Button;
    using MouseType = aspire::core::EventMouse::Type;
    using WindowType = aspire::core::EventWindow::Type;

    constexpr float PositionX{12.0F};
    constexpr float PositionY{34.0F};
    constexpr float DeltaX{5.0F};
    constexpr float DeltaY{-6.0F};

    constexpr int WindowWidth{200};
    constexpr int WindowHeight{100};

    // SDL reports finger positions normalized to the window, from 0 to 1.
    constexpr float FingerPosition{0.5F};
    constexpr float FingerDeltaX{0.1F};
    constexpr float FingerDeltaY{0.2F};

    // Render coordinates in the renderer test are this many times the window coordinates.
    constexpr int RenderScale{2};

    auto KeyEvent(SDL_EventType type, SDL_Keycode key, bool repeat = false) -> SDL_Event
    {
        SDL_Event x{};
        x.type = type;
        x.key.key = key;
        x.key.down = type == SDL_EVENT_KEY_DOWN;
        x.key.repeat = repeat;
        return x;
    }

    auto ButtonEvent(SDL_EventType type, Uint8 button) -> SDL_Event
    {
        SDL_Event x{};
        x.type = type;
        x.button.button = button;
        x.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        x.button.x = PositionX;
        x.button.y = PositionY;
        return x;
    }

    auto FingerEvent(SDL_EventType type, SDL_WindowID window) -> SDL_Event
    {
        SDL_Event x{};
        x.type = type;
        x.tfinger.touchID = 1;
        x.tfinger.windowID = window;
        x.tfinger.x = FingerPosition;
        x.tfinger.y = FingerPosition;
        x.tfinger.dx = FingerDeltaX;
        x.tfinger.dy = FingerDeltaY;
        return x;
    }

    auto Keyboard(const SDL_Event& x) -> aspire::core::EventKeyboard
    {
        auto event = aspire::sdl::ToEvent(x);
        EXPECT_TRUE(event.has_value());
        const auto* keyboard = event.has_value() ? std::get_if<aspire::core::EventKeyboard>(&*event) : nullptr;
        EXPECT_NE(keyboard, nullptr);
        return keyboard != nullptr ? *keyboard : aspire::core::EventKeyboard{};
    }

    auto Mouse(const SDL_Event& x, SDL_Renderer* renderer = nullptr) -> aspire::core::EventMouse
    {
        auto event = aspire::sdl::ToEvent(x, renderer);
        EXPECT_TRUE(event.has_value());
        const auto* mouse = event.has_value() ? std::get_if<aspire::core::EventMouse>(&*event) : nullptr;
        EXPECT_NE(mouse, nullptr);
        return mouse != nullptr ? *mouse : aspire::core::EventMouse{};
    }

    auto Window(SDL_EventType type) -> WindowType
    {
        SDL_Event x{};
        x.type = type;
        auto event = aspire::sdl::ToEvent(x);
        EXPECT_TRUE(event.has_value());
        const auto* window = event.has_value() ? std::get_if<aspire::core::EventWindow>(&*event) : nullptr;
        EXPECT_NE(window, nullptr);
        return window != nullptr ? window->type : WindowType::Unknown;
    }
}

TEST(ToEvent, keyDownIsPressed)
{
    const auto event = Keyboard(KeyEvent(SDL_EVENT_KEY_DOWN, SDLK_A));
    EXPECT_EQ(event.type, KeyType::KeyPressed);
    EXPECT_EQ(event.key, Key::A);
}

TEST(ToEvent, keyRepeatIsRepeated)
{
    EXPECT_EQ(Keyboard(KeyEvent(SDL_EVENT_KEY_DOWN, SDLK_A, true)).type, KeyType::KeyRepeated);
}

TEST(ToEvent, keyUpIsReleased)
{
    EXPECT_EQ(Keyboard(KeyEvent(SDL_EVENT_KEY_UP, SDLK_A)).type, KeyType::KeyReleased);
}

TEST(ToEvent, keysMapAcrossTheTable)
{
    // Spans the sorted table: Backspace has the smallest SDL keycode and Android Back the largest.
    const std::array<std::pair<SDL_Keycode, Key>, 12> keys{{
        {SDLK_BACKSPACE, Key::Backspace},
        {SDLK_0, Key::Zero},
        {SDLK_Z, Key::Z},
        {SDLK_AC_BACK, Key::BACK},
        {SDLK_SPACE, Key::Space},
        {SDLK_ESCAPE, Key::Escape},
        {SDLK_RETURN, Key::Enter},
        {SDLK_F12, Key::F12},
        {SDLK_RGUI, Key::RightSuper},
        {SDLK_KP_0, Key::Kp_0},
        {SDLK_KP_EQUALS, Key::KpEqual},
        {SDLK_VOLUMEDOWN, Key::VOLUME_DOWN},
    }};

    for (const auto& [from, to] : keys)
    {
        EXPECT_EQ(Keyboard(KeyEvent(SDL_EVENT_KEY_DOWN, from)).key, to);
    }
}

TEST(ToEvent, unmappedKeyIsDropped)
{
    EXPECT_FALSE(aspire::sdl::ToEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDLK_F24)).has_value());
}

TEST(ToEvent, mouseButtonsMap)
{
    const std::array<std::pair<Uint8, Button>, 5> buttons{{
        {SDL_BUTTON_LEFT, Button::Left},
        {SDL_BUTTON_MIDDLE, Button::Middle},
        {SDL_BUTTON_RIGHT, Button::Right},
        {SDL_BUTTON_X1, Button::Side},
        {SDL_BUTTON_X2, Button::Extra},
    }};

    for (const auto& [from, to] : buttons)
    {
        const auto event = Mouse(ButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN, from));
        EXPECT_EQ(event.type, MouseType::ButtonPressed);
        EXPECT_EQ(event.button, to);
        EXPECT_FLOAT_EQ(event.position.x, PositionX);
        EXPECT_FLOAT_EQ(event.position.y, PositionY);
    }
}

TEST(ToEvent, mouseButtonUpIsReleased)
{
    EXPECT_EQ(Mouse(ButtonEvent(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT)).type, MouseType::ButtonReleased);
}

TEST(ToEvent, mouseMotionCarriesPositionAndDelta)
{
    SDL_Event x{};
    x.type = SDL_EVENT_MOUSE_MOTION;
    x.motion.x = PositionX;
    x.motion.y = PositionY;
    x.motion.xrel = DeltaX;
    x.motion.yrel = DeltaY;

    const auto event = Mouse(x);
    EXPECT_EQ(event.type, MouseType::Moved);
    EXPECT_FLOAT_EQ(event.position.x, PositionX);
    EXPECT_FLOAT_EQ(event.delta.x, DeltaX);
    EXPECT_FLOAT_EQ(event.delta.y, DeltaY);
}

TEST(ToEvent, wheelCarriesScrollAndUnflipsIt)
{
    SDL_Event x{};
    x.type = SDL_EVENT_MOUSE_WHEEL;
    x.wheel.x = DeltaX;
    x.wheel.y = DeltaY;
    x.wheel.mouse_x = PositionX;
    x.wheel.mouse_y = PositionY;

    auto event = Mouse(x);
    EXPECT_EQ(event.type, MouseType::Scrolled);
    EXPECT_FLOAT_EQ(event.scroll.x, DeltaX);
    EXPECT_FLOAT_EQ(event.scroll.y, DeltaY);
    EXPECT_FLOAT_EQ(event.position.y, PositionY);

    x.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    event = Mouse(x);
    EXPECT_FLOAT_EQ(event.scroll.x, -DeltaX);
    EXPECT_FLOAT_EQ(event.scroll.y, -DeltaY);
}

TEST(ToEvent, mouseEventsFromTouchAreDropped)
{
    auto x = ButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT);
    x.button.which = SDL_TOUCH_MOUSEID;
    EXPECT_FALSE(aspire::sdl::ToEvent(x).has_value());
}

TEST(ToEvent, fingerEventsFromMouseAreDropped)
{
    auto x = FingerEvent(SDL_EVENT_FINGER_DOWN, 0);
    x.tfinger.touchID = SDL_MOUSE_TOUCHID;
    EXPECT_FALSE(aspire::sdl::ToEvent(x).has_value());
}

TEST(ToEvent, fingerWithoutWindowIsDropped)
{
    // Without its window, a normalized position can't become window coordinates.
    EXPECT_FALSE(aspire::sdl::ToEvent(FingerEvent(SDL_EVENT_FINGER_DOWN, 0)).has_value());
}

TEST(ToEvent, windowEventsMap)
{
    EXPECT_EQ(Window(SDL_EVENT_QUIT), WindowType::Closed);
    EXPECT_EQ(Window(SDL_EVENT_WINDOW_CLOSE_REQUESTED), WindowType::Closed);
    EXPECT_EQ(Window(SDL_EVENT_WINDOW_RESIZED), WindowType::Resized);
    EXPECT_EQ(Window(SDL_EVENT_WINDOW_MOVED), WindowType::Moved);
    EXPECT_EQ(Window(SDL_EVENT_WINDOW_FOCUS_GAINED), WindowType::FocusGained);
    EXPECT_EQ(Window(SDL_EVENT_WINDOW_FOCUS_LOST), WindowType::FocusLost);
}

TEST(ToEvent, untranslatedEventsAreDropped)
{
    for (const auto type : {SDL_EVENT_DID_ENTER_BACKGROUND, SDL_EVENT_JOYSTICK_AXIS_MOTION, SDL_EVENT_WINDOW_MINIMIZED})
    {
        SDL_Event x{};
        x.type = type;
        EXPECT_FALSE(aspire::sdl::ToEvent(x).has_value());
    }
}

TEST(ToEvent, timestampFollowsSdlEventTime)
{
    // Conversions read the clocks separately, so allow for the time the test itself takes.
    constexpr std::chrono::milliseconds tolerance{100};
    const auto ticks = SDL_GetTicksNS();

    // An event stamped now converts to about now.
    auto current = KeyEvent(SDL_EVENT_KEY_DOWN, SDLK_A);
    current.key.timestamp = ticks;
    const auto converted = Keyboard(current).timestamp;
    EXPECT_LE(std::chrono::steady_clock::now() - converted, tolerance);

    // An event stamped when SDL's clock started converts to that long ago.
    auto first = KeyEvent(SDL_EVENT_KEY_DOWN, SDLK_A);
    first.key.timestamp = 0;
    const auto gap = converted - Keyboard(first).timestamp;
    EXPECT_GE(gap, std::chrono::nanoseconds{static_cast<std::int64_t>(ticks)} - tolerance);
    EXPECT_LE(gap, std::chrono::nanoseconds{static_cast<std::int64_t>(ticks)} + tolerance);

    // A timestamp from the future is clamped to now.
    auto future = KeyEvent(SDL_EVENT_KEY_DOWN, SDLK_A);
    future.key.timestamp = ticks + static_cast<Uint64>(std::chrono::nanoseconds{std::chrono::hours{1}}.count());
    const auto clamped = Keyboard(future).timestamp;
    EXPECT_LE(clamped, std::chrono::steady_clock::now());
}

namespace
{
    // A real window and renderer on SDL's dummy video driver, for the coordinate conversions.
    class ToEventWithWindow : public ::testing::Test
    {
    protected:
        // NOLINTNEXTLINE(readability-identifier-naming)
        auto SetUp() -> void override
        {
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
            ASSERT_TRUE(SDL_Init(SDL_INIT_VIDEO)) << SDL_GetError();
            ASSERT_TRUE(SDL_CreateWindowAndRenderer("test", WindowWidth, WindowHeight, 0, &window_, &renderer_)) << SDL_GetError();
        }

        // NOLINTNEXTLINE(readability-identifier-naming)
        auto TearDown() -> void override
        {
            SDL_DestroyRenderer(renderer_);
            SDL_DestroyWindow(window_);
            SDL_Quit();
        }

        [[nodiscard]] auto windowId() const -> SDL_WindowID
        {
            return SDL_GetWindowID(window_);
        }

        [[nodiscard]] auto renderer() const -> SDL_Renderer*
        {
            return renderer_;
        }

    private:
        SDL_Window* window_{};
        SDL_Renderer* renderer_{};
    };
}

TEST_F(ToEventWithWindow, fingerIsScaledToWindowCoordinates)
{
    const auto event = Mouse(FingerEvent(SDL_EVENT_FINGER_DOWN, windowId()));
    EXPECT_EQ(event.type, MouseType::ButtonPressed);
    EXPECT_EQ(event.button, Button::Left);
    EXPECT_FLOAT_EQ(event.position.x, WindowWidth * FingerPosition);
    EXPECT_FLOAT_EQ(event.position.y, WindowHeight * FingerPosition);
    EXPECT_FLOAT_EQ(event.delta.x, WindowWidth * FingerDeltaX);
    EXPECT_FLOAT_EQ(event.delta.y, WindowHeight * FingerDeltaY);
}

TEST_F(ToEventWithWindow, fingerUpAndMotionMapToReleasedAndMoved)
{
    const auto window = windowId();
    EXPECT_EQ(Mouse(FingerEvent(SDL_EVENT_FINGER_UP, window)).type, MouseType::ButtonReleased);
    EXPECT_EQ(Mouse(FingerEvent(SDL_EVENT_FINGER_MOTION, window)).type, MouseType::Moved);
}

TEST_F(ToEventWithWindow, rendererConvertsToRenderCoordinates)
{
    // A logical size twice the window's makes render coordinates double the window coordinates.
    ASSERT_TRUE(
        SDL_SetRenderLogicalPresentation(renderer(), WindowWidth * RenderScale, WindowHeight * RenderScale, SDL_LOGICAL_PRESENTATION_STRETCH));

    auto click = ButtonEvent(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT);
    click.button.windowID = windowId();
    const auto mouse = Mouse(click, renderer());
    EXPECT_FLOAT_EQ(mouse.position.x, PositionX * RenderScale);
    EXPECT_FLOAT_EQ(mouse.position.y, PositionY * RenderScale);

    const auto finger = Mouse(FingerEvent(SDL_EVENT_FINGER_DOWN, windowId()), renderer());
    EXPECT_FLOAT_EQ(finger.position.x, static_cast<float>(WindowWidth));
    EXPECT_FLOAT_EQ(finger.position.y, static_cast<float>(WindowHeight));
}

// NOLINTEND(cppcoreguidelines-pro-type-union-access)
