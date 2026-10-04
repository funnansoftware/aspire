#include <gtest/gtest.h>

import std;
import aspire.core;
import aspire.graphics;

namespace
{
    using aspire::graphics::Rect;
    using aspire::graphics::Transform;

    constexpr Transform Parent{.offset = {.x = 10.0F, .y = 20.0F}, .scale = {.x = 2.0F, .y = 4.0F}};
    constexpr Transform Child{.offset = {.x = 1.0F, .y = 2.0F}, .scale = {.x = 3.0F, .y = 0.5F}};
    constexpr aspire::core::Vec2 Point{.x = 5.0F, .y = 6.0F};
    constexpr Rect Local{.x = 1.0F, .y = 1.0F, .w = 3.0F, .h = 2.0F};

    auto ExpectPoint(aspire::core::Vec2 actual, aspire::core::Vec2 expected) -> void
    {
        EXPECT_FLOAT_EQ(actual.x, expected.x);
        EXPECT_FLOAT_EQ(actual.y, expected.y);
    }
}

TEST(Transform, appliesScaleThenOffsetToPoints)
{
    ExpectPoint(Parent.apply(Point), {.x = Parent.offset.x + (Point.x * Parent.scale.x), .y = Parent.offset.y + (Point.y * Parent.scale.y)});
}

TEST(Transform, appliesScaleThenOffsetToRects)
{
    const auto rect = Parent.apply(Local);
    EXPECT_FLOAT_EQ(rect.x, Parent.offset.x + (Local.x * Parent.scale.x));
    EXPECT_FLOAT_EQ(rect.y, Parent.offset.y + (Local.y * Parent.scale.y));
    EXPECT_FLOAT_EQ(rect.w, Local.w * Parent.scale.x);
    EXPECT_FLOAT_EQ(rect.h, Local.h * Parent.scale.y);
}

TEST(Transform, thenComposesParentFirst)
{
    // Applying the composed transform matches applying the child's, then the parent's.
    ExpectPoint(Parent.then(Child).apply(Point), Parent.apply(Child.apply(Point)));
}

TEST(Transform, inverseUndoesTheTransform)
{
    ExpectPoint(Parent.inverse().apply(Parent.apply(Point)), Point);
}
