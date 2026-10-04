export module aspire.graphics:drawsprite;

import std;
import :color;
import :rect;

export namespace aspire::graphics
{
    /// @brief Draws a region of a texture into a rectangle on screen.
    ///
    /// `source` names the texture; the backend loads and caches textures by it. It views the drawing node's own
    /// storage, so it stays valid only until the frame's draw list has been submitted.
    struct DrawSprite
    {
        std::string_view source;
        Rect region;
        Rect bounds;
        Color tint{White};
    };
}
