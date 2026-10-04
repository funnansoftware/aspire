export module aspire.graphics:drawtext;

import std;
import aspire.core;
import :color;

export namespace aspire::graphics
{
    /// @brief Draws a line of text with the backend's built-in font.
    ///
    /// `text` views the drawing node's own storage, so it stays valid only until the frame's draw list has been
    /// submitted.
    struct DrawText
    {
        std::string_view text;
        aspire::core::Vec2 position;
        aspire::core::Vec2 scale{.x = 1.0F, .y = 1.0F};
        Color color{White};
    };
}
