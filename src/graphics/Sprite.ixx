export module aspire.graphics:sprite;

import std;
import :color;
import :node;
import :rect;
import :renderer;

export namespace aspire::graphics
{
    /// @brief Draws a region of a texture at its origin, at the region's size.
    ///
    /// Registers the properties `source` (the texture's path, which the backend resolves), `region` (`[x, y, w, h]`
    /// in texture pixels) and `tint` (`[r, g, b]` or `[r, g, b, a]`).
    class Sprite : public Node
    {
    public:
        Sprite()
        {
            registerProperty("source", source_);
            registerProperty("region", region_);
            registerProperty("tint", tint_);
        }

        /// @brief Sets the texture to draw from.
        /// @param x The texture's path. Relative paths resolve against the backend's asset root.
        auto setSource(std::string x) -> void
        {
            source_ = std::move(x);
        }

        /// @brief Reports the texture this sprite draws from.
        /// @return The texture's path.
        [[nodiscard]] auto getSource() const -> std::string_view
        {
            return source_;
        }

        /// @brief Sets the part of the texture to draw.
        /// @param x The region, in texture pixels. The sprite is drawn at this size.
        auto setRegion(Rect x) -> void
        {
            region_ = x;
        }

        /// @brief Reports the part of the texture this sprite draws.
        /// @return The region, in texture pixels.
        [[nodiscard]] auto getRegion() const -> Rect
        {
            return region_;
        }

        /// @brief Sets the color the texture is multiplied by.
        /// @param x The tint. White, the default, leaves the texture unchanged.
        auto setTint(Color x) -> void
        {
            tint_ = x;
        }

        /// @brief Reports the color the texture is multiplied by.
        /// @return The tint.
        [[nodiscard]] auto getTint() const -> Color
        {
            return tint_;
        }

        auto draw(Renderer& x) const -> void override
        {
            x.sprite(source_, region_, {.x = 0.0F, .y = 0.0F, .w = region_.w, .h = region_.h}, tint_);
        }

    private:
        std::string source_;
        Rect region_;
        Color tint_{White};
    };
}
