export module aspire.graphics:drawstate;

import std;
import :rect;
import :transform;

export namespace aspire::graphics
{
    /// @brief Where a node draws: its transform to screen coordinates, its clip and its layer.
    struct DrawState
    {
        Transform transform;

        /// The index of the clip rectangle in `DrawList::clips()`, or zero for none.
        std::uint32_t clip{};

        /// The clip rectangle in screen coordinates, if any. Children intersect theirs with it.
        std::optional<Rect> clipRect;

        int layer{};
    };
}
