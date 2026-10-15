#include <gtest/gtest.h>

import std;
import scope.mesh;
import scope.world;

namespace
{
    using scope::Vec2;

    constexpr float Tolerance{1e-3F};
    constexpr float Half{0.5F};
    constexpr float Tau{2.0F * std::numbers::pi_v<float>};
    constexpr float Side{10.0F};
    constexpr scope::Tint White{.r = 1.0F, .g = 1.0F, .b = 1.0F, .a = 1.0F};

    auto Distance(Vec2 a, Vec2 b) -> float
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    auto Length(const std::vector<Vec2>& points) -> float
    {
        auto length = 0.0F;

        for (std::size_t i = 1; i < std::size(points); ++i)
        {
            length += Distance(points.at(i - 1), points.at(i));
        }

        return length;
    }

    // A 10 by 10 square, wound clockwise on screen.
    auto Square() -> std::vector<Vec2>
    {
        return {{.x = 0.0F, .y = 0.0F}, {.x = Side, .y = 0.0F}, {.x = Side, .y = Side}, {.x = 0.0F, .y = Side}};
    }

    // Whether each corner's transparent vertex lies outside its solid one, away from the middle.
    auto FringeOutside(const scope::Mesh& mesh, Vec2 middle) -> bool
    {
        const auto& vertices = mesh.vertices();

        for (std::size_t i = 0; i + 1 < std::size(vertices); i += 2)
        {
            if (vertices.at(i + 1).tint.a != 0.0F || Distance(vertices.at(i + 1).position, middle) <= Distance(vertices.at(i).position, middle))
            {
                return false;
            }
        }

        return true;
    }
}

TEST(CircleSegments, KeepEachChordWithinTheTolerance)
{
    for (const auto radius : {2.0F, 20.0F, 600.0F})
    {
        // How far each chord strays from the circle.
        const auto segments = scope::CircleSegments(radius);
        const auto sag = radius * (1.0F - std::cos(Tau / static_cast<float>(segments) * Half));
        EXPECT_LE(sag, scope::CurveTolerance + Tolerance) << radius;
        EXPECT_GE(segments, 12) << radius;
    }

    EXPECT_LT(scope::CircleSegments(20.0F), scope::CircleSegments(600.0F));
}

TEST(FlattenArc, RunsClockwiseFromItsBearing)
{
    const Vec2 center{.x = 100.0F, .y = 50.0F};
    constexpr float radius{40.0F};
    const auto points = scope::FlattenArc(center, radius, {0.0F, 90.0F});

    // North is up the screen, and a quarter turn clockwise is east.
    EXPECT_NEAR(points.front().x, center.x, Tolerance);
    EXPECT_NEAR(points.front().y, center.y - radius, Tolerance);
    EXPECT_NEAR(points.back().x, center.x + radius, Tolerance);
    EXPECT_NEAR(points.back().y, center.y, Tolerance);
}

TEST(FlattenArc, StaysOnTheCircle)
{
    const Vec2 center{.x = 100.0F, .y = 50.0F};
    constexpr float radius{40.0F};
    const auto points = scope::FlattenArc(center, radius, {30.0F, 200.0F});
    EXPECT_TRUE(std::ranges::all_of(points, [&](Vec2 p) { return std::abs(Distance(p, center) - radius) < Tolerance; }));
}

TEST(Dash, AlternatesDashesAndGapsAcrossCorners)
{
    // An L of 10 then 10, dashed 3 on and 2 off: dashes at 0-3, 5-8, 10-13 (round the corner), and 15-18.
    const std::vector<Vec2> points{{.x = 0.0F, .y = 0.0F}, {.x = 10.0F, .y = 0.0F}, {.x = 10.0F, .y = 10.0F}};
    constexpr float dash{3.0F};
    const auto dashes = scope::Dash(points, {dash, 2.0F});

    ASSERT_EQ(std::size(dashes), 4U);
    EXPECT_TRUE(std::ranges::all_of(dashes, [](const auto& x) { return std::abs(Length(x) - dash) < Tolerance; }));
    EXPECT_NEAR(dashes.at(1).front().x, 5.0F, Tolerance);
    EXPECT_NEAR(dashes.at(2).front().x, 10.0F, Tolerance);
    EXPECT_NEAR(dashes.at(2).front().y, 0.0F, Tolerance);
    EXPECT_NEAR(dashes.at(3).back().y, 8.0F, Tolerance);
    EXPECT_TRUE(std::empty(scope::Dash(points, {0.0F, 0.0F})));
}

TEST(ClipToCircle, CutsALineWhereItCrosses)
{
    constexpr float radius{5.0F};
    const std::vector<Vec2> through{{.x = -10.0F, .y = 0.0F}, {.x = 10.0F, .y = 0.0F}};
    const auto cut = scope::ClipToCircle(through, {}, radius);
    ASSERT_EQ(std::size(cut), 1U);
    EXPECT_NEAR(cut.front().front().x, -radius, Tolerance);
    EXPECT_NEAR(cut.front().back().x, radius, Tolerance);
}

TEST(ClipToCircle, KeepsTheInsideAndDropsTheOutside)
{
    constexpr float radius{5.0F};
    const std::vector<Vec2> inside{{.x = -1.0F, .y = 0.0F}, {.x = 1.0F, .y = 1.0F}, {.x = 2.0F, .y = 0.0F}};
    const auto kept = scope::ClipToCircle(inside, {}, radius);
    ASSERT_EQ(std::size(kept), 1U);
    EXPECT_EQ(std::size(kept.front()), 3U);

    const std::vector<Vec2> outside{{.x = 6.0F, .y = 6.0F}, {.x = 9.0F, .y = 6.0F}};
    EXPECT_TRUE(std::empty(scope::ClipToCircle(outside, {}, radius)));

    // Out, in, out again, and back: two separate parts.
    const std::vector<Vec2> twice{{.x = -10.0F, .y = 1.0F}, {.x = 10.0F, .y = 1.0F}, {.x = 10.0F, .y = -1.0F}, {.x = -10.0F, .y = -1.0F}};
    EXPECT_EQ(std::size(scope::ClipToCircle(twice, {}, radius)), 2U);
}

namespace
{
    // Undoes the linear filtering of shifted copies: the kernel's weight at each whole pixel.
    auto Expand(const std::vector<scope::Tap>& taps) -> std::map<int, float>
    {
        std::map<int, float> kernel;

        for (const auto& tap : taps)
        {
            const auto below = std::floor(tap.offset);
            const auto share = tap.offset - below;
            kernel.try_emplace(static_cast<int>(below), 0.0F).first->second += tap.weight * (1.0F - share);
            kernel.try_emplace(static_cast<int>(below) + 1, 0.0F).first->second += tap.weight * share;
        }

        return kernel;
    }
}

TEST(GaussianTaps, KeepTheBrightnessAndAreSymmetric)
{
    for (const auto [sigma, reach] : {std::pair{0.8F, 3.0F}, std::pair{3.0F, 2.0F}, std::pair{7.5F, 3.0F}})
    {
        const auto taps = scope::GaussianTaps(sigma, reach);
        EXPECT_NEAR(std::ranges::fold_left(taps | std::views::transform(&scope::Tap::weight), 0.0F, std::plus{}), 1.0F, Tolerance) << sigma;

        for (std::size_t i = 0; i < std::size(taps); ++i)
        {
            EXPECT_NEAR(taps.at(i).offset, -taps.at(std::size(taps) - 1 - i).offset, Tolerance) << sigma;
        }
    }
}

TEST(GaussianTaps, SpreadAsWideAsTheirSigma)
{
    for (const auto sigma : {1.5F, 3.0F, 5.0F})
    {
        auto variance = 0.0F;

        for (const auto& [pixel, weight] : Expand(scope::GaussianTaps(sigma, 3.0F)))
        {
            variance += weight * static_cast<float>(pixel * pixel);
        }

        // Cutting the kernel off at three sigmas trims a little of its spread.
        EXPECT_NEAR(std::sqrt(variance), sigma, sigma * 0.02F) << sigma;
    }
}

TEST(GaussianTaps, NeedAboutHalfAsManyCopiesAsPixels)
{
    // Three sigmas of 3 pixels is 9 either way: 19 pixels, as the center and 5 copies each side. Two sigmas is 6
    // either way: the center and 3 copies each side.
    EXPECT_EQ(std::size(scope::GaussianTaps(3.0F, 3.0F)), 11U);
    EXPECT_EQ(std::size(scope::GaussianTaps(3.0F, 2.0F)), 7U);
    EXPECT_EQ(std::size(scope::GaussianTaps(0.0F, 3.0F)), 1U);
}

TEST(GaussianTaps, EndWhereTheyAreCutOff)
{
    // Cut off two sigmas out, a kernel of 3 pixels reaches no farther than 6, and its outermost copy is past 5.
    constexpr float end{6.0F};
    constexpr float nearEnd{5.0F};
    const auto taps = scope::GaussianTaps(3.0F, 2.0F);
    EXPECT_TRUE(std::ranges::all_of(taps, [](const auto& x) { return std::abs(x.offset) <= end; }));
    EXPECT_FALSE(std::ranges::all_of(taps, [](const auto& x) { return std::abs(x.offset) < nearEnd; }));
}

TEST(Mesh, StrokesWithAFadingFringe)
{
    constexpr float width{4.0F};
    scope::Mesh mesh;
    const std::vector<Vec2> line{{.x = 0.0F, .y = 0.0F}, {.x = 10.0F, .y = 0.0F}};
    mesh.stroke(line, false, width, White);

    // Four vertices per point, from the outer fringe to the inner: transparent, solid, solid, transparent.
    const auto& vertices = mesh.vertices();
    ASSERT_EQ(std::size(vertices), 8U);
    EXPECT_EQ(std::size(mesh.indices()), 18U);
    EXPECT_FLOAT_EQ(vertices.at(0).tint.a, 0.0F);
    EXPECT_FLOAT_EQ(vertices.at(1).tint.a, 1.0F);
    EXPECT_FLOAT_EQ(vertices.at(3).tint.a, 0.0F);
}

TEST(Mesh, StrokesAtItsWidthWithSquareEnds)
{
    scope::Mesh mesh;
    constexpr float width{4.0F};
    constexpr float end{10.0F};
    const std::vector<Vec2> line{{.x = 0.0F, .y = 0.0F}, {.x = end, .y = 0.0F}};
    mesh.stroke(line, false, width, White);
    const auto& vertices = mesh.vertices();
    ASSERT_EQ(std::size(vertices), 8U);

    // Halfway through the fringe, where coverage is half, the stroke is its width across.
    const auto halfway = (std::abs(vertices.at(0).position.y) + std::abs(vertices.at(1).position.y)) * Half;
    EXPECT_NEAR(halfway * 2.0F, width, Tolerance);

    // The ends reach half the width past the points.
    EXPECT_NEAR(vertices.at(0).position.x, -width * Half, Tolerance);
    EXPECT_NEAR(vertices.at(4).position.x, end + (width * Half), Tolerance);
}

TEST(Mesh, FadesStrokesThinnerThanItsFringe)
{
    scope::Mesh mesh;
    const std::vector<Vec2> line{{.x = 0.0F, .y = 0.0F}, {.x = 10.0F, .y = 0.0F}};
    mesh.stroke(line, false, Half, White);
    ASSERT_EQ(std::size(mesh.vertices()), 8U);
    EXPECT_NEAR(mesh.vertices().at(1).tint.a, Half, Tolerance);
}

TEST(Mesh, FillsWithTheFringeOutsideWoundClockwise)
{
    scope::Mesh mesh;
    mesh.fillConvex(Square(), White);
    ASSERT_EQ(std::size(mesh.vertices()), 8U);
    EXPECT_TRUE(FringeOutside(mesh, {.x = 5.0F, .y = 5.0F}));

    // Two triangles inside, and a quad of fringe along each of the four edges.
    EXPECT_EQ(std::size(mesh.indices()), (2U + 8U) * 3U);
}

TEST(Mesh, FillsWithTheFringeOutsideWoundAnticlockwise)
{
    auto square = Square();
    std::ranges::reverse(square);
    scope::Mesh mesh;
    mesh.fillConvex(square, White);
    ASSERT_EQ(std::size(mesh.vertices()), 8U);
    EXPECT_TRUE(FringeOutside(mesh, {.x = 5.0F, .y = 5.0F}));
}

TEST(Mesh, ClearsAndReportsEmpty)
{
    constexpr float radius{10.0F};
    scope::Mesh mesh;
    EXPECT_TRUE(mesh.empty());
    mesh.fillRadial({.x = 0.0F, .y = 0.0F}, radius, {White, White});
    EXPECT_FALSE(mesh.empty());
    mesh.clear();
    EXPECT_TRUE(mesh.empty());
    EXPECT_TRUE(std::empty(mesh.vertices()));
}
