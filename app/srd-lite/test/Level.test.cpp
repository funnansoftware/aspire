#include <gtest/gtest.h>

import std;
import sl.level;

namespace
{
    using sl::Tile;

    constexpr int Floor{0};
    constexpr int Wall{1};
    constexpr int Size{5};
    constexpr int TileSize{16};
    constexpr auto Cells{static_cast<std::size_t>(Size) * static_cast<std::size_t>(Size)};

    // A 5 x 5 level of floor. Walls are added by each test.
    auto MakeLevel(const std::vector<Tile>& walls = {}) -> std::shared_ptr<sl::Level>
    {
        auto level = std::make_shared<sl::Level>();
        level->setTileWidth(TileSize);
        level->setTileHeight(TileSize);
        level->setColumns(Size);
        level->setWidth(Size);
        level->setHeight(Size);
        level->setAllowed({Floor});

        std::vector<int> data(Cells, Floor);

        for (const auto wall : walls)
        {
            data.at((static_cast<std::size_t>(wall.y) * static_cast<std::size_t>(Size)) + static_cast<std::size_t>(wall.x)) = Wall;
        }

        level->setData(std::move(data));
        return level;
    }

    constexpr Tile Center{.x = 2, .y = 2};
}

TEST(Level, isAllowedFollowsTheTileAndTheMapEdge)
{
    const auto level = MakeLevel({{.x = 1, .y = 1}});

    EXPECT_TRUE(level->isAllowed({.x = 0, .y = 0}));
    EXPECT_FALSE(level->isAllowed({.x = 1, .y = 1}));
    EXPECT_FALSE(level->isAllowed({.x = -1, .y = 0}));
    EXPECT_FALSE(level->isAllowed({.x = Size, .y = 0}));
}

TEST(Level, rangeOneReachesTheEightNeighbours)
{
    const auto range = MakeLevel()->movementFrom(Center, 1);

    EXPECT_EQ(std::size(range.reachableTiles()), 8);
    EXPECT_TRUE(range.canMoveTo({.x = 1, .y = 1}));
    EXPECT_FALSE(range.canMoveTo({.x = 0, .y = 0}));
}

TEST(Level, theStartIsNotReachable)
{
    const auto range = MakeLevel()->movementFrom(Center, 2);

    EXPECT_EQ(range.distanceTo(Center), 0);
    EXPECT_FALSE(range.canMoveTo(Center));
}

TEST(Level, wallsBlockMovement)
{
    // A wall around the start, except for the cell to its left.
    const auto level =
        MakeLevel({{.x = 1, .y = 1}, {.x = 2, .y = 1}, {.x = 3, .y = 1}, {.x = 3, .y = 2}, {.x = 1, .y = 3}, {.x = 2, .y = 3}, {.x = 3, .y = 3}});
    const auto range = level->movementFrom(Center, 1);

    EXPECT_EQ(range.reachableTiles(), (std::vector<Tile>{{.x = 1, .y = 2}}));
}

TEST(Level, diagonalsCannotCutCorners)
{
    // Walls left of and above the start block the diagonal between them.
    const auto level = MakeLevel({{.x = 1, .y = 2}, {.x = 2, .y = 1}});
    const auto range = level->movementFrom(Center, 1);

    EXPECT_FALSE(range.canMoveTo({.x = 1, .y = 1}));
    EXPECT_TRUE(range.canMoveTo({.x = 3, .y = 3}));
}

TEST(Level, pathToIsShortestAndStartsAtTheStart)
{
    const auto range = MakeLevel()->movementFrom({.x = 0, .y = 0}, 4);

    // Diagonals make (3, 3) three moves away.
    const auto path = range.pathTo({.x = 3, .y = 3});
    EXPECT_EQ(range.distanceTo({.x = 3, .y = 3}), 3);
    ASSERT_EQ(std::size(path), 4);
    EXPECT_EQ(path.front(), (Tile{.x = 0, .y = 0}));
    EXPECT_EQ(path.back(), (Tile{.x = 3, .y = 3}));
}

TEST(Level, unreachableTileHasNoPath)
{
    const auto range = MakeLevel()->movementFrom({.x = 0, .y = 0}, 1);

    EXPECT_TRUE(std::empty(range.pathTo({.x = 4, .y = 4})));
    EXPECT_EQ(range.distanceTo({.x = 4, .y = 4}), -1);
}

TEST(Level, noMovementFromAWallOrWithANegativeRange)
{
    const auto level = MakeLevel({Center});

    EXPECT_TRUE(std::empty(level->movementFrom(Center, 3).reachableTiles()));
    EXPECT_TRUE(std::empty(MakeLevel()->movementFrom(Center, -1).reachableTiles()));
}
