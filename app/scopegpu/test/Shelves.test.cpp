#include <gtest/gtest.h>

import std;
import scopegpu.shelves;

namespace
{
    constexpr std::uint32_t Size{100};
    constexpr std::uint32_t Padding{scopegpu::ShelfPacker::Padding};
}

TEST(ShelfPacker, PlacesLeftToRightWithPaddingBetween)
{
    scopegpu::ShelfPacker packer{Size};

    EXPECT_EQ(packer.place(10, 20), (scopegpu::Slot{.x = 0, .y = 0, .width = 10, .height = 20}));
    EXPECT_EQ(packer.place(5, 8), (scopegpu::Slot{.x = 11, .y = 0, .width = 5, .height = 8}));
}

TEST(ShelfPacker, StartsAShelfUnderTheTallestWhenARowIsFull)
{
    scopegpu::ShelfPacker packer{Size};
    ASSERT_TRUE(packer.place(60, 30).has_value());
    ASSERT_TRUE(packer.place(30, 10).has_value());

    // 60 + 1 + 30 + 1 leaves 8 columns, too few for 10 and its padding.
    EXPECT_EQ(packer.place(10, 10), (scopegpu::Slot{.x = 0, .y = 31, .width = 10, .height = 10}));
}

TEST(ShelfPacker, RefusesWhatDoesNotFit)
{
    scopegpu::ShelfPacker packer{Size};

    // With its padding, a rectangle as big as the atlas doesn't fit.
    EXPECT_FALSE(packer.place(Size, 1).has_value());
    EXPECT_FALSE(packer.place(1, Size).has_value());

    ASSERT_TRUE(packer.place(Size - Padding, 60).has_value());
    EXPECT_FALSE(packer.place(10, 40).has_value());
    EXPECT_TRUE(packer.place(10, 38).has_value());
}

TEST(ShelfPacker, ReportsHowMuchIsUsedAndClears)
{
    scopegpu::ShelfPacker packer{Size};
    EXPECT_EQ(packer.used(), 0.0F);

    ASSERT_TRUE(packer.place(10, 49).has_value());
    EXPECT_FLOAT_EQ(packer.used(), 0.5F);

    packer.clear();
    EXPECT_EQ(packer.used(), 0.0F);
    EXPECT_EQ(packer.place(10, 10), (scopegpu::Slot{.x = 0, .y = 0, .width = 10, .height = 10}));
}
