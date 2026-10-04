#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

import std;
import aspire.graphics;

namespace
{
    using aspire::graphics::Rect;

    // B overlaps the right part of A; Far overlaps nothing.
    constexpr Rect A{.x = 0.0F, .y = 0.0F, .w = 10.0F, .h = 10.0F};
    constexpr Rect B{.x = 5.0F, .y = 2.0F, .w = 10.0F, .h = 4.0F};
    constexpr Rect Far{.x = 20.0F, .y = 20.0F, .w = 1.0F, .h = 1.0F};

    auto ExpectRect(Rect actual, Rect expected) -> void
    {
        EXPECT_FLOAT_EQ(actual.x, expected.x);
        EXPECT_FLOAT_EQ(actual.y, expected.y);
        EXPECT_FLOAT_EQ(actual.w, expected.w);
        EXPECT_FLOAT_EQ(actual.h, expected.h);
    }
}

TEST(Intersect, findsTheOverlap)
{
    ExpectRect(aspire::graphics::Intersect(A, B), {.x = B.x, .y = B.y, .w = A.w - B.x, .h = B.h});
}

TEST(Intersect, disjointRectsGiveAnEmptyRect)
{
    const auto overlap = aspire::graphics::Intersect(A, Far);
    EXPECT_FLOAT_EQ(overlap.w, 0.0F);
    EXPECT_FLOAT_EQ(overlap.h, 0.0F);
}

TEST(Rect, roundTripsThroughJson)
{
    const nlohmann::json json = B;
    EXPECT_EQ(json, nlohmann::json::array({B.x, B.y, B.w, B.h}));
    ExpectRect(json.get<Rect>(), B);
}
