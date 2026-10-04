export module sl.level;

import std;
import aspire.graphics;

export namespace sl
{
    using Tile = aspire::graphics::TileMap::Tile;

    /// @brief How far each tile is from a start tile, and how to get there.
    class MovementRange
    {
    public:
        MovementRange() = default;

        /// @brief Creates a range over a map, with no tile reached yet.
        /// @param width The map's width in tiles.
        /// @param height The map's height in tiles.
        MovementRange(int width, int height)
            : width_{width}, height_{height}, steps_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
        {
        }

        /// @brief Reports the tiles that can be moved to: reached, and not the start.
        /// @return The tiles, row by row.
        [[nodiscard]] auto reachableTiles() const -> std::vector<Tile>
        {
            std::vector<Tile> tiles;

            for (auto index = 0; std::cmp_less(index, std::size(steps_)); ++index)
            {
                if (at(index).distance > 0)
                {
                    tiles.emplace_back(Tile{.x = index % width_, .y = index / width_});
                }
            }

            return tiles;
        }

        /// @brief Reports how many moves a tile is from the start.
        /// @param x The tile.
        /// @return The number of moves, 0 for the start, or -1 if the tile can't be reached.
        [[nodiscard]] auto distanceTo(Tile x) const -> int
        {
            const auto index = indexOf(x);
            return index < 0 ? -1 : at(index).distance;
        }

        /// @brief Reports whether a tile can be moved to.
        /// @param x The tile.
        /// @return `true` if it's reached and isn't the start.
        [[nodiscard]] auto canMoveTo(Tile x) const -> bool
        {
            return distanceTo(x) > 0;
        }

        /// @brief Finds a shortest path from the start to a tile.
        /// @param x The tile.
        /// @return The tiles from the start to `x`, both included, or nothing if `x` can't be reached.
        [[nodiscard]] auto pathTo(Tile x) const -> std::vector<Tile>
        {
            if (distanceTo(x) < 0)
            {
                return {};
            }

            std::vector<Tile> path;

            for (auto index = indexOf(x); index >= 0; index = at(index).previous)
            {
                path.emplace_back(Tile{.x = index % width_, .y = index / width_});
            }

            std::ranges::reverse(path);
            return path;
        }

        /// @brief Reports a tile's index in the range.
        /// @param x The tile.
        /// @return The index, or -1 if the tile is outside the map.
        [[nodiscard]] auto indexOf(Tile x) const -> int
        {
            if (x.x < 0 || x.x >= width_ || x.y < 0 || x.y >= height_)
            {
                return -1;
            }

            return (x.y * width_) + x.x;
        }

        /// @brief Records how a tile was reached.
        /// @param index The tile's index, from `indexOf()`. Must be in range.
        /// @param distance Its number of moves from the start.
        /// @param previous The index of the tile it was reached from, or -1 for the start.
        auto reach(int index, int distance, int previous) -> void
        {
            steps_.at(static_cast<std::size_t>(index)) = Step{.distance = distance, .previous = previous};
        }

    private:
        // A tile's moves from the start and the tile it was reached from. Unreached tiles have distance -1.
        struct Step
        {
            int distance{-1};
            int previous{-1};
        };

        [[nodiscard]] auto at(int index) const -> const Step&
        {
            return steps_.at(static_cast<std::size_t>(index));
        }

        int width_{};
        int height_{};
        std::vector<Step> steps_;
    };

    /// @brief A tile map with walkable tiles, and the rules for moving around it.
    ///
    /// Registers the property `allowed`: the tile indices that can be walked on.
    class Level : public aspire::graphics::TileMap
    {
    public:
        Level()
        {
            registerProperty("allowed", allowed_);
        }

        /// @brief Sets which tiles can be walked on.
        /// @param x The walkable tile indices.
        auto setAllowed(std::unordered_set<int> x) -> void
        {
            allowed_ = std::move(x);
        }

        /// @brief Reports whether a cell can be walked on.
        /// @param x The cell.
        /// @return `true` if it's on the map and its tile is walkable.
        [[nodiscard]] auto isAllowed(Tile x) const -> bool
        {
            const auto tile = tileIndexAt(x);
            return tile.has_value() && allowed_.contains(*tile);
        }

        /// @brief Finds every cell reachable from a start within a number of moves.
        ///
        /// A move goes to any of the eight neighbouring cells. A diagonal move also needs both cells beside it to be
        /// walkable, so it can't cut a blocked corner.
        ///
        /// @param start The cell to move from.
        /// @param range The most moves allowed.
        /// @return The reachable cells. Empty if the start isn't walkable or the range is negative.
        [[nodiscard]] auto movementFrom(Tile start, int range) const -> MovementRange
        {
            if (range < 0 || !isAllowed(start))
            {
                return {};
            }

            constexpr std::array directions{Tile{.x = 0, .y = -1},  Tile{.x = 1, .y = 0},  Tile{.x = 0, .y = 1}, Tile{.x = -1, .y = 0},
                                            Tile{.x = -1, .y = -1}, Tile{.x = 1, .y = -1}, Tile{.x = 1, .y = 1}, Tile{.x = -1, .y = 1}};

            MovementRange result{getWidth(), getHeight()};

            // Mark the start as reached, so a neighbour can't reach it back and form a cycle.
            result.reach(result.indexOf(start), 0, -1);

            std::queue<Tile> pending;
            pending.push(start);

            while (!std::empty(pending))
            {
                const auto current = pending.front();
                pending.pop();

                const auto distance = result.distanceTo(current);

                if (distance >= range)
                {
                    continue;
                }

                for (const auto& direction : directions)
                {
                    const Tile next{.x = current.x + direction.x, .y = current.y + direction.y};

                    if (!isAllowed(next) || result.distanceTo(next) >= 0)
                    {
                        continue;
                    }

                    // A diagonal needs both bordering cells clear, so it can't cut a blocked corner.
                    const auto diagonal = direction.x != 0 && direction.y != 0;

                    if (diagonal && (!isAllowed({.x = next.x, .y = current.y}) || !isAllowed({.x = current.x, .y = next.y})))
                    {
                        continue;
                    }

                    result.reach(result.indexOf(next), distance + 1, result.indexOf(current));
                    pending.push(next);
                }
            }

            return result;
        }

    private:
        std::unordered_set<int> allowed_;
    };
}
