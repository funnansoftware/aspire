export module aspire.graphics:drawitem;

import std;
import :drawrect;
import :drawsprite;
import :drawtext;

export namespace aspire::graphics
{
    /// @brief One drawing instruction, already in screen coordinates.
    using DrawPrimitive = std::variant<DrawSprite, DrawRect, DrawText>;

    /// @brief A drawing instruction and where it sorts.
    ///
    /// Items sort by `layer`, then `order`, then `sequence`, so tree order breaks every tie.
    struct DrawItem
    {
        /// The group the item belongs to, such as world, UI or debug. Lower layers draw first.
        int layer{};

        /// The order within the layer, such as y for a world sorted by depth. Zero lets tree order decide.
        float order{};

        /// The item's position in tree order, set when it's added.
        std::uint32_t sequence{};

        /// The index of the item's clip rectangle in `DrawList::clips()`, or zero for none.
        std::uint32_t clip{};

        DrawPrimitive primitive;
    };
}
