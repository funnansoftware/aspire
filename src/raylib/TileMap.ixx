module;

#include <raylib.h>

export module aspire.raylib.tilemap;

import std;
import aspire.core.engine;
import aspire.core.object;
import aspire.raylib.node;
import aspire.raylib.textureloader;

export namespace aspire::raylib
{
    struct Tile
    {
        int x{};
        int y{};

        auto operator==(const Tile& other) const -> bool = default;
    };

    struct Movement
    {
        int distance{-1};
        int previous{-1};
    };

    class MovementRange
    {
    public:
        MovementRange() = default;

        MovementRange(int width, int height) : width_(width), height_(height), movements_{static_cast<std::size_t>(width) * height}
        {
        }

        [[nodiscard]] auto reachableTiles() const -> std::vector<Tile>
        {
            std::vector<Tile> tiles;

            for (const auto& [index, move] : std::views::enumerate(movements_))
            {
                if (move.distance > 0)
                {
                    const auto i = static_cast<int>(index);
                    tiles.emplace_back(Tile{.x = i % width_, .y = i / width_});
                }
            }

            return tiles;
        }

        [[nodiscard]] auto indexOf(Tile x) const -> int
        {
            if (x.x < 0 || x.x >= width_ || x.y < 0 || x.y >= height_)
            {
                return -1;
            }

            return (x.y * width_) + x.x;
        }

        [[nodiscard]] auto distanceTo(Tile x) const -> int
        {
            const auto index = indexOf(x);
            return index < 0 ? -1 : movements_.at(index).distance;
        }

        [[nodiscard]] auto canMoveTo(Tile x) const -> bool
        {
            return distanceTo(x) > 0;
        }

        [[nodiscard]] auto pathTo(Tile x) const -> std::vector<Tile>
        {
            if (distanceTo(x) < 0)
            {
                return {};
            }

            std::vector<Tile> path;
            for (auto index = indexOf(x); index >= 0; index = movements_.at(index).previous)
            {
                path.emplace_back(Tile{.x = index % width_, .y = index / width_});
            }

            std::ranges::reverse(path);

            return path;
        }

        [[nodiscard]] auto at(std::size_t x) -> Movement&
        {
            return movements_.at(x);
        }

        [[nodiscard]] auto at(Tile x) -> Movement&
        {
            return movements_.at(indexOf(x));
        }

    private:
        std::vector<Movement> movements_;
        int width_{};
        int height_{};
    };

    class TileMap : public aspire::raylib::Node
    {
    public:
        TileMap()
        {
            registerProperty("data", data_);
            registerProperty("allowed", allowed_);
            registerProperty("source", source_);
            registerProperty("spacing", spacing_);
            registerProperty("tileWidth", tileWidth_);
            registerProperty("tileHeight", tileHeight_);
            registerProperty("tiles", tiles_);
            registerProperty("columns", columns_);
            registerProperty("width", width_);
            registerProperty("height", height_);
        }

        [[nodiscard]] auto width() const -> int
        {
            return width_ * tileWidth_;
        }

        [[nodiscard]] auto height() const -> int
        {
            return height_ * tileHeight_;
        }

        [[nodiscard]] auto tileWidth() const -> int
        {
            return tileWidth_;
        }

        [[nodiscard]] auto tileHeight() const -> int
        {
            return tileHeight_;
        }

        [[nodiscard]] auto tileAtPosition(Vector2 x) const -> std::optional<Tile>
        {
            if (tileWidth_ <= 0 || tileHeight_ <= 0 || width_ <= 0 || height_ <= 0 || !std::isfinite(x.x) || !std::isfinite(x.y) || x.x < 0
                || x.y < 0)
            {
                return std::nullopt;
            }

            const auto tileX = x.x / static_cast<float>(tileWidth_);
            const auto tileY = x.y / static_cast<float>(tileHeight_);
            return Tile{.x = static_cast<int>(tileX), .y = static_cast<int>(tileY)};
        }

        [[nodiscard]] auto tileToRectangle(Tile x) const -> Rectangle
        {
            return Rectangle{
                .x = static_cast<float>(x.x * tileWidth_),
                .y = static_cast<float>(x.y * tileHeight_),
                .width = static_cast<float>(tileWidth_),
                .height = static_cast<float>(tileHeight_),
            };
        }

        auto positionToRectangle(Vector2 position) const -> Rectangle
        {
            const auto x = static_cast<int>(position.x) / tileWidth_;
            const auto y = static_cast<int>(position.y) / tileHeight_;
            return Rectangle{
                .x = static_cast<float>(x * tileWidth_),
                .y = static_cast<float>(y * tileHeight_),
                .width = static_cast<float>(tileWidth_),
                .height = static_cast<float>(tileHeight_),
            };
        }

        [[nodiscard]] auto isAllowed(Tile x) const -> bool
        {
            if (x.x < 0 || x.x >= width_ || x.y < 0 || x.y >= height_)
            {
                return false;
            }

            const auto index = (x.y * width_) + x.x;
            return allowed_.contains(data_.at(index));
        }

        [[nodiscard]] auto movementFrom(Tile start, int range) const -> MovementRange
        {
            if (range < 0 || !isAllowed(start) || width_ > std::numeric_limits<int>::max() / height_)
            {
                return {};
            }

            constexpr auto directions = std::array{Tile{.x = 0, .y = -1},  Tile{.x = 1, .y = 0},  Tile{.x = 0, .y = 1}, Tile{.x = -1, .y = 0},
                                                   Tile{.x = -1, .y = -1}, Tile{.x = 1, .y = -1}, Tile{.x = 1, .y = 1}, Tile{.x = -1, .y = 1}};

            MovementRange result{width_, height_};
            // Mark the start tile as visited so a neighbor's back-edge can't overwrite its `previous` and form a cycle.
            result.at(start).distance = 0;

            std::queue<Tile> toVisit;
            toVisit.push(start);

            while (!toVisit.empty())
            {
                const auto current = toVisit.front();
                toVisit.pop();

                const auto currentIndex = result.indexOf(current);
                const auto distance = result.at(currentIndex).distance;

                if (distance >= range)
                {
                    continue;
                }

                for (const auto& direction : directions)
                {
                    const Tile next{.x = current.x + direction.x, .y = current.y + direction.y};

                    if (!isAllowed(next))
                    {
                        continue;
                    }

                    // A diagonal needs both bordering tiles clear so it cannot cut a blocked corner.
                    if (direction.x != 0 && direction.y != 0
                        && (!isAllowed(Tile{.x = current.x + direction.x, .y = current.y})
                            || !isAllowed(Tile{.x = current.x, .y = current.y + direction.y})))
                    {
                        continue;
                    }

                    auto& move = result.at(next);

                    if (move.distance >= 0)
                    {
                        continue;
                    }

                    move.distance = distance + 1;
                    move.previous = currentIndex;
                    toVisit.push(next);
                }
            }

            return result;
        }

    protected:
        auto onRender() const -> void override
        {
            if (width_ == 0 || height_ == 0)
            {
                return;
            }

            auto texture = getParent<aspire::core::Engine>()->getOrCreateChild<TextureLoader>()->loadTexture(source_);

            for (const auto& [index, tileIndex] : std::views::enumerate(data_))
            {
                const auto rect = tileIndexToRectangle(tileIndex);
                auto position = dataIndexToPosition(index);
                DrawTextureRec(texture, rect, position, WHITE);
            }
        }

    private:
        auto tileIndexToRectangle(int index) const -> Rectangle
        {
            const auto x = (index % columns_) * (tileWidth_ + spacing_);
            const auto y = (index / columns_) * (tileHeight_ + spacing_);
            return Rectangle{
                .x = static_cast<float>(x),
                .y = static_cast<float>(y),
                .width = static_cast<float>(tileWidth_),
                .height = static_cast<float>(tileHeight_),
            };
        }

        // Calculates the texture position within the map based on the index of the tile data.
        auto dataIndexToPosition(std::size_t index) const -> Vector2
        {
            return Vector2{
                .x = static_cast<float>(index % width_) * static_cast<float>(tileWidth_),

                // Loss of floating precision is intention to keep tile positions aligned correctly.
                // NOLINTNEXTLINE(bugprone-integer-division)
                .y = static_cast<float>(index / width_) * static_cast<float>(tileHeight_),
            };
        }

        std::vector<int> data_;
        std::unordered_set<int> allowed_;
        std::filesystem::path source_;
        int spacing_{};
        int tileWidth_{};
        int tileHeight_{};
        int tiles_{};
        int columns_{};
        int width_{};
        int height_{};
    };
}
