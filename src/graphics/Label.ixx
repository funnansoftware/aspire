export module aspire.graphics:label;

import std;
import :color;
import :node;
import :renderer;

export namespace aspire::graphics
{
    /// @brief Draws a line of text at its origin, in the backend's built-in font.
    ///
    /// Registers the properties `text` and `color` (`[r, g, b]` or `[r, g, b, a]`).
    class Label : public Node
    {
    public:
        Label()
        {
            registerProperty("text", text_);
            registerProperty("color", color_);
        }

        /// @brief Sets the text to draw.
        /// @param x The text.
        auto setText(std::string x) -> void
        {
            text_ = std::move(x);
        }

        /// @brief Reports the text this label draws.
        /// @return The text.
        [[nodiscard]] auto getText() const -> std::string_view
        {
            return text_;
        }

        /// @brief Sets the text color.
        /// @param x The color. White by default.
        auto setColor(Color x) -> void
        {
            color_ = x;
        }

        /// @brief Reports the text color.
        /// @return The color.
        [[nodiscard]] auto getColor() const -> Color
        {
            return color_;
        }

        auto draw(Renderer& x) const -> void override
        {
            x.text(text_, {}, color_);
        }

    private:
        std::string text_;
        Color color_{White};
    };
}
