export module aspire.graphics:drawrect;

import :color;
import :rect;

export namespace aspire::graphics
{
    /// @brief Fills a rectangle on screen, or draws its outline.
    struct DrawRect
    {
        Rect bounds;
        Color color{White};

        /// `false` draws only the outline: a line one screen pixel wide, whatever the node's scale.
        bool filled{true};
    };
}
