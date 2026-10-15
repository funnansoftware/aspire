export module scopegpu.shelves;

import std;

export namespace scopegpu
{
    /// @brief A rectangle's place in an atlas, in pixels from its top left.
    struct Slot
    {
        std::uint32_t x{};
        std::uint32_t y{};
        std::uint32_t width{};
        std::uint32_t height{};

        auto operator==(const Slot&) const -> bool = default;
    };

    /// @brief Packs rectangles into a square atlas on shelves: rows filled left to right, each as tall as its tallest
    /// rectangle.
    ///
    /// Nothing is ever taken out; the whole atlas is cleared at once. That suits glyphs, which come in a few heights
    /// and are all thrown away when the text's resized. A pixel's left clear to the right of and below each rectangle,
    /// so nothing sampled at a rectangle's edge picks up its neighbor.
    class ShelfPacker
    {
    public:
        /// Pixels left clear to the right of and below each rectangle.
        static constexpr std::uint32_t Padding = 1;

        /// @brief Makes an empty packer.
        /// @param size The atlas's width and height, in pixels.
        explicit ShelfPacker(std::uint32_t size) : size_{size}
        {
        }

        /// @brief Finds a place for a rectangle.
        /// @param width The rectangle's width, in pixels.
        /// @param height Its height.
        /// @return Its place, or empty if there's no room left for it.
        [[nodiscard]] auto place(std::uint32_t width, std::uint32_t height) -> std::optional<Slot>
        {
            const auto paddedWidth = width + Padding;
            const auto paddedHeight = height + Padding;

            if (paddedWidth > size_ || paddedHeight > size_)
            {
                return std::nullopt;
            }

            // Too wide for what's left of the shelf: start another under it.
            if (x_ + paddedWidth > size_)
            {
                top_ += height_;
                x_ = 0;
                height_ = 0;
            }

            if (top_ + paddedHeight > size_)
            {
                return std::nullopt;
            }

            const Slot slot{.x = x_, .y = top_, .width = width, .height = height};
            x_ += paddedWidth;
            height_ = std::max(height_, paddedHeight);
            return slot;
        }

        /// @brief Empties the atlas.
        auto clear() -> void
        {
            x_ = 0;
            top_ = 0;
            height_ = 0;
        }

        /// @brief Reports how much of the atlas's height the shelves take.
        /// @return From 0, empty, to 1, full.
        [[nodiscard]] auto used() const -> float
        {
            return static_cast<float>(top_ + height_) / static_cast<float>(size_);
        }

    private:
        std::uint32_t size_;
        // The left of the free part of the current shelf, the shelf's top, and its height so far.
        std::uint32_t x_{};
        std::uint32_t top_{};
        std::uint32_t height_{};
    };
}
