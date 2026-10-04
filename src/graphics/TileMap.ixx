export module aspire.graphics:tilemap;

import std;
import aspire.core;
import :node;
import :rect;
import :renderer;

export namespace aspire::graphics
{
    /// @brief Draws a grid of tiles from a tilesheet.
    ///
    /// The map is `width` by `height` cells, each `tileWidth` by `tileHeight` pixels, with cell (0, 0) at the node's
    /// origin. `data` lists each cell's tile, row by row; a negative tile leaves the cell empty. Tile `n` is the
    /// sheet's tile in column `n % columns` and row `n / columns`, with `spacing` pixels between tiles on the sheet.
    ///
    /// Registers the properties `source`, `tileWidth`, `tileHeight`, `spacing`, `columns`, `width`, `height` and
    /// `data`.
    class TileMap : public Node
    {
    public:
        /// @brief A cell's column and row in the map.
        struct Tile
        {
            int x{};
            int y{};

            auto operator==(const Tile&) const -> bool = default;
        };

        TileMap()
        {
            registerProperty("source", source_);
            registerProperty("tileWidth", tileWidth_);
            registerProperty("tileHeight", tileHeight_);
            registerProperty("spacing", spacing_);
            registerProperty("columns", columns_);
            registerProperty("width", width_);
            registerProperty("height", height_);
            registerProperty("data", data_);
        }

        /// @brief Sets the tilesheet.
        /// @param x The tilesheet's path. Relative paths resolve against the backend's asset root.
        auto setSource(std::string x) -> void
        {
            source_ = std::move(x);
        }

        /// @brief Reports the tilesheet.
        /// @return The tilesheet's path.
        [[nodiscard]] auto getSource() const -> std::string_view
        {
            return source_;
        }

        /// @brief Sets the width of each tile, on the sheet and in the map.
        /// @param x The width in pixels.
        auto setTileWidth(int x) -> void
        {
            tileWidth_ = x;
        }

        /// @brief Reports the width of each tile.
        /// @return The width in pixels.
        [[nodiscard]] auto getTileWidth() const -> int
        {
            return tileWidth_;
        }

        /// @brief Sets the height of each tile, on the sheet and in the map.
        /// @param x The height in pixels.
        auto setTileHeight(int x) -> void
        {
            tileHeight_ = x;
        }

        /// @brief Reports the height of each tile.
        /// @return The height in pixels.
        [[nodiscard]] auto getTileHeight() const -> int
        {
            return tileHeight_;
        }

        /// @brief Sets the gap between tiles on the sheet.
        /// @param x The gap in pixels.
        auto setSpacing(int x) -> void
        {
            spacing_ = x;
        }

        /// @brief Reports the gap between tiles on the sheet.
        /// @return The gap in pixels.
        [[nodiscard]] auto getSpacing() const -> int
        {
            return spacing_;
        }

        /// @brief Sets how many tiles fit across the sheet.
        /// @param x The number of columns.
        auto setColumns(int x) -> void
        {
            columns_ = x;
        }

        /// @brief Reports how many tiles fit across the sheet.
        /// @return The number of columns.
        [[nodiscard]] auto getColumns() const -> int
        {
            return columns_;
        }

        /// @brief Sets the map's width.
        /// @param x The width in cells.
        auto setWidth(int x) -> void
        {
            width_ = x;
        }

        /// @brief Reports the map's width.
        /// @return The width in cells.
        [[nodiscard]] auto getWidth() const -> int
        {
            return width_;
        }

        /// @brief Sets the map's height.
        /// @param x The height in cells.
        auto setHeight(int x) -> void
        {
            height_ = x;
        }

        /// @brief Reports the map's height.
        /// @return The height in cells.
        [[nodiscard]] auto getHeight() const -> int
        {
            return height_;
        }

        /// @brief Sets each cell's tile, row by row.
        /// @param x The tiles. A negative tile leaves its cell empty.
        auto setData(std::vector<int> x) -> void
        {
            data_ = std::move(x);
        }

        /// @brief Reports each cell's tile, row by row.
        /// @return The tiles.
        [[nodiscard]] auto getData() const -> std::span<const int>
        {
            return data_;
        }

        /// @brief Reports the map's size.
        /// @return The size in local coordinates: width and height in cells, times the tile size.
        [[nodiscard]] auto size() const -> aspire::core::Vec2
        {
            return {.x = static_cast<float>(width_ * tileWidth_), .y = static_cast<float>(height_ * tileHeight_)};
        }

        /// @brief Finds the cell under a point.
        /// @param x The point, in local coordinates.
        /// @return The cell, or `std::nullopt` if the point is outside the map.
        [[nodiscard]] auto tileAt(aspire::core::Vec2 x) const -> std::optional<Tile>
        {
            if (tileWidth_ <= 0 || tileHeight_ <= 0 || !std::isfinite(x.x) || !std::isfinite(x.y) || x.x < 0.0F || x.y < 0.0F)
            {
                return std::nullopt;
            }

            const Tile tile{.x = static_cast<int>(x.x / static_cast<float>(tileWidth_)),
                            .y = static_cast<int>(x.y / static_cast<float>(tileHeight_))};

            if (!contains(tile))
            {
                return std::nullopt;
            }

            return tile;
        }

        /// @brief Reports where a cell is.
        /// @param x The cell.
        /// @return The cell's rectangle, in local coordinates.
        [[nodiscard]] auto tileBounds(Tile x) const -> Rect
        {
            return {.x = static_cast<float>(x.x * tileWidth_),
                    .y = static_cast<float>(x.y * tileHeight_),
                    .w = static_cast<float>(tileWidth_),
                    .h = static_cast<float>(tileHeight_)};
        }

        /// @brief Reports a cell's tile.
        /// @param x The cell.
        /// @return The tile from `data`, or `std::nullopt` if the cell is outside the map or has no data.
        [[nodiscard]] auto tileIndexAt(Tile x) const -> std::optional<int>
        {
            if (!contains(x))
            {
                return std::nullopt;
            }

            // contains() has ruled out negative cells, so the index can be computed unsigned.
            const auto index = (static_cast<std::size_t>(x.y) * static_cast<std::size_t>(width_)) + static_cast<std::size_t>(x.x);

            if (index >= std::size(data_))
            {
                return std::nullopt;
            }

            return data_.at(index);
        }

        auto draw(Renderer& x) const -> void override
        {
            if (columns_ <= 0 || width_ <= 0)
            {
                return;
            }

            for (std::size_t i = 0; i < std::size(data_); ++i)
            {
                const auto tile = data_.at(i);
                const auto cell = static_cast<int>(i);

                if (tile < 0 || cell >= width_ * height_)
                {
                    continue;
                }

                // The sheet's column and row of the tile: whole numbers, so integer division is intended.
                const auto column = tile % columns_;
                const auto row = tile / columns_;
                const Rect region{.x = static_cast<float>(column * (tileWidth_ + spacing_)),
                                  .y = static_cast<float>(row * (tileHeight_ + spacing_)),
                                  .w = static_cast<float>(tileWidth_),
                                  .h = static_cast<float>(tileHeight_)};

                x.sprite(source_, region, tileBounds({.x = cell % width_, .y = cell / width_}));
            }
        }

    private:
        [[nodiscard]] auto contains(Tile x) const -> bool
        {
            return x.x >= 0 && x.x < width_ && x.y >= 0 && x.y < height_;
        }

        std::string source_;
        std::vector<int> data_;
        int tileWidth_{};
        int tileHeight_{};
        int spacing_{};
        int columns_{};
        int width_{};
        int height_{};
    };
}
