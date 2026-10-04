export module aspire.graphics:drawrect;

import :color;
import :rect;

export namespace aspire::graphics
{
    /// @brief Fills a rectangle on screen.
    struct DrawRect
    {
        Rect bounds;
        Color color{White};
    };
}
