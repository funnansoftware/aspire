export module aspire.graphics:drawlist;

import std;
import :drawitem;
import :rect;

export namespace aspire::graphics
{
    /// @brief A frame's draw items and clip rectangles. Keep one between frames so its storage is reused.
    class DrawList
    {
    public:
        DrawList()
        {
            // Index zero means "no clip", so real clips start at one.
            clips_.emplace_back();
        }

        /// @brief Empties the list for the next frame. Keeps its storage.
        auto clear() -> void
        {
            items_.clear();
            clips_.resize(1);
        }

        /// @brief Appends an item, numbering it in the order items are added.
        /// @param x The item. Its `sequence` is overwritten.
        auto add(DrawItem x) -> void
        {
            x.sequence = static_cast<std::uint32_t>(std::size(items_));
            items_.emplace_back(std::move(x));
        }

        /// @brief Appends a clip rectangle for items to refer to.
        /// @param x The rectangle, in screen coordinates.
        /// @return Its index, for `DrawItem::clip`. Never zero.
        auto addClip(Rect x) -> std::uint32_t
        {
            clips_.emplace_back(x);
            return static_cast<std::uint32_t>(std::size(clips_) - 1);
        }

        /// @brief Sorts the items by layer, then order, then sequence.
        auto sort() -> void
        {
            std::ranges::sort(items_, {}, [](const DrawItem& x) { return std::tuple{x.layer, x.order, x.sequence}; });
        }

        /// @brief Reports the recorded items.
        /// @return The items, in the order they were added or, after `sort()`, in draw order.
        [[nodiscard]] auto items() const -> std::span<const DrawItem>
        {
            return items_;
        }

        /// @brief Reports the clip rectangles items refer to by index, in screen coordinates.
        /// @return The rectangles. Index zero is a placeholder for "no clip".
        [[nodiscard]] auto clips() const -> std::span<const Rect>
        {
            return clips_;
        }

    private:
        std::vector<DrawItem> items_;
        std::vector<Rect> clips_;
    };
}
