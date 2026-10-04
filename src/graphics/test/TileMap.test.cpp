#include <gtest/gtest.h>

#include <nameof.hpp>
#include <nlohmann/json.hpp>

import std;
import aspire.core;
import aspire.graphics;
import aspire.parser;

namespace
{
    using aspire::graphics::Rect;
    using aspire::graphics::TileMap;
    using Tile = TileMap::Tile;

    // A 3 x 2 map of 16-pixel tiles from a sheet 4 tiles across, with 1 pixel between tiles.
    constexpr int TileSize{16};
    constexpr int Spacing{1};
    constexpr int Columns{4};
    constexpr int Width{3};
    constexpr int Height{2};
    constexpr int Stride{TileSize + Spacing};

    // Tile 5 is in column 1, row 1 of the sheet. Cell (1, 0) is empty.
    constexpr int SecondRowTile{5};
    constexpr int LastTile{7};

    auto Map() -> std::shared_ptr<TileMap>
    {
        auto map = std::make_shared<TileMap>();
        map->setSource("sheet.png");
        map->setTileWidth(TileSize);
        map->setTileHeight(TileSize);
        map->setSpacing(Spacing);
        map->setColumns(Columns);
        map->setWidth(Width);
        map->setHeight(Height);
        map->setData({0, -1, 2, 3, SecondRowTile, LastTile});
        return map;
    }

    auto ExpectRect(Rect actual, Rect expected) -> void
    {
        EXPECT_FLOAT_EQ(actual.x, expected.x);
        EXPECT_FLOAT_EQ(actual.y, expected.y);
        EXPECT_FLOAT_EQ(actual.w, expected.w);
        EXPECT_FLOAT_EQ(actual.h, expected.h);
    }

    auto Sprites(const aspire::graphics::DrawList& list) -> std::vector<aspire::graphics::DrawSprite>
    {
        std::vector<aspire::graphics::DrawSprite> sprites;

        for (const auto& item : list.items())
        {
            sprites.emplace_back(std::get<aspire::graphics::DrawSprite>(item.primitive));
        }

        return sprites;
    }
}

TEST(TileMap, drawsEveryNonEmptyCell)
{
    const auto map = Map();
    map->startup();

    aspire::graphics::DrawList list;
    aspire::graphics::Collect(*map, list);
    const auto sprites = Sprites(list);

    // Six cells, one empty.
    ASSERT_EQ(std::size(sprites), 5);
    EXPECT_EQ(sprites.front().source, "sheet.png");

    // Cell (0, 1) holds tile 5: sheet column 1, row 1, including the spacing.
    const auto& secondRow = *std::next(std::begin(sprites), 3);
    ExpectRect(secondRow.region, {.x = Stride, .y = Stride, .w = TileSize, .h = TileSize});
    ExpectRect(secondRow.bounds, {.x = TileSize, .y = TileSize, .w = TileSize, .h = TileSize});

    // The first cell is at the origin, the last at (2, 1).
    ExpectRect(sprites.front().bounds, {.x = 0.0F, .y = 0.0F, .w = TileSize, .h = TileSize});
    ExpectRect(sprites.back().bounds, {.x = 2 * TileSize, .y = TileSize, .w = TileSize, .h = TileSize});
}

TEST(TileMap, drawsNothingWithoutColumns)
{
    const auto map = Map();
    map->setColumns(0);
    map->startup();

    aspire::graphics::DrawList list;
    aspire::graphics::Collect(*map, list);

    EXPECT_TRUE(std::empty(list.items()));
}

TEST(TileMap, sizeIsCellsTimesTileSize)
{
    const auto size = Map()->size();
    EXPECT_FLOAT_EQ(size.x, Width * TileSize);
    EXPECT_FLOAT_EQ(size.y, Height * TileSize);
}

TEST(TileMap, tileAtFindsTheCellUnderAPoint)
{
    const auto map = Map();

    EXPECT_EQ(map->tileAt({.x = 0.0F, .y = 0.0F}), (Tile{.x = 0, .y = 0}));
    EXPECT_EQ(map->tileAt({.x = TileSize + 1.0F, .y = TileSize - 1.0F}), (Tile{.x = 1, .y = 0}));
    EXPECT_EQ(map->tileAt({.x = (Width * TileSize) - 1.0F, .y = (Height * TileSize) - 1.0F}), (Tile{.x = Width - 1, .y = Height - 1}));
}

TEST(TileMap, tileAtIsEmptyOutsideTheMap)
{
    const auto map = Map();

    EXPECT_FALSE(map->tileAt({.x = -1.0F, .y = 0.0F}).has_value());
    EXPECT_FALSE(map->tileAt({.x = Width * TileSize, .y = 0.0F}).has_value());
    EXPECT_FALSE(map->tileAt({.x = 0.0F, .y = Height * TileSize}).has_value());
    EXPECT_FALSE(map->tileAt({.x = std::numeric_limits<float>::infinity(), .y = 0.0F}).has_value());

    map->setTileWidth(0);
    EXPECT_FALSE(map->tileAt({.x = 0.0F, .y = 0.0F}).has_value());
}

TEST(TileMap, tileBoundsAndIndex)
{
    const auto map = Map();

    ExpectRect(map->tileBounds({.x = 2, .y = 1}), {.x = 2 * TileSize, .y = TileSize, .w = TileSize, .h = TileSize});
    EXPECT_EQ(map->tileIndexAt({.x = 1, .y = 1}), SecondRowTile);
    EXPECT_EQ(map->tileIndexAt({.x = 1, .y = 0}), -1);
    EXPECT_FALSE(map->tileIndexAt({.x = Width, .y = 0}).has_value());
}

TEST(TileMap, loadsFromJson)
{
    aspire::core::ObjectFactory factory;
    factory.registerObject<TileMap>();

    auto json = nlohmann::json::parse(R"({
        "type": "TileMap",
        "source": "sheet.png",
        "tileWidth": 16,
        "tileHeight": 16,
        "spacing": 1,
        "columns": 4,
        "width": 3,
        "height": 2,
        "data": [0, -1, 2, 3, 5, 7]
    })");

    const auto map = std::dynamic_pointer_cast<TileMap>(aspire::parser::ReadJson(factory, json));
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map->getSource(), "sheet.png");
    EXPECT_EQ(map->getSpacing(), Spacing);
    EXPECT_EQ(map->getColumns(), Columns);
    EXPECT_EQ(std::size(map->getData()), Width * Height);
    EXPECT_EQ(map->tileIndexAt({.x = 2, .y = 1}), LastTile);
}
