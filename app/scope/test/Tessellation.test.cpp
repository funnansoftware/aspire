#include <gtest/gtest.h>

import std;
import scope.mesh;
import scope.picture;
import scope.tessellation;
import scope.world;

namespace
{
    using scope::Vec2;

    constexpr float Tolerance{1e-3F};
    constexpr float ChannelMax{255.0F};
    constexpr float Half{0.5F};
    constexpr scope::Color Lime{.r = 0x3D, .g = 0xFF, .b = 0x6E};
    constexpr scope::Clip Unit{.center = {.x = 0.0F, .y = 0.0F}, .radius = 100.0F};
    constexpr float DotRadius{5.0F};
    // Twice the clip's radius, so a line out to it crosses the clip's edge.
    constexpr float Far{200.0F};
    constexpr float DiscRadius{10.0F};

    auto Distance(Vec2 a, Vec2 b) -> float
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    auto Dot(Vec2 center, float radius) -> scope::Shape
    {
        return {.geometry = scope::Circle{.center = center, .radius = radius}, .pen = std::nullopt, .fill = Lime, .opacity = 1.0F};
    }

    auto Line(Vec2 from, Vec2 to, float width) -> scope::Shape
    {
        return {.geometry = scope::Polyline{.points = {from, to}, .closed = false},
                .pen = scope::Pen{.color = Lime, .width = width},
                .fill = std::nullopt,
                .opacity = 1.0F};
    }

    auto Style(float scale, std::optional<scope::Clip> clip) -> scope::Style
    {
        return {.scale = scale, .clip = clip, .widen = 0.0F, .opacity = 1.0F, .channels = scope::Channels::All};
    }
}

TEST(Tinted, KeepsTheChannelsAPassDraws)
{
    const auto all = scope::Tinted(Lime, Half, scope::Channels::All);
    EXPECT_NEAR(all.r, 0x3D / ChannelMax, Tolerance);
    EXPECT_NEAR(all.g, 1.0F, Tolerance);
    EXPECT_NEAR(all.b, 0x6E / ChannelMax, Tolerance);
    EXPECT_NEAR(all.a, Half, Tolerance);

    // Green is at full strength, so it's the full pass's alone, and the others are the fainter pass's.
    const auto full = scope::Tinted(Lime, 1.0F, scope::Channels::Full);
    EXPECT_EQ(full.r, 0.0F);
    EXPECT_NEAR(full.g, 1.0F, Tolerance);
    EXPECT_EQ(full.b, 0.0F);

    const auto fainter = scope::Tinted(Lime, 1.0F, scope::Channels::Fainter);
    EXPECT_NEAR(fainter.r, 0x3D / ChannelMax, Tolerance);
    EXPECT_EQ(fainter.g, 0.0F);
    EXPECT_NEAR(fainter.b, 0x6E / ChannelMax, Tolerance);
    EXPECT_NEAR(fainter.a, 1.0F, Tolerance);
}

TEST(Tessellate, KeepsOrDropsAClosedShapeWholeByItsMiddle)
{
    // Well inside the clip, across its edge with the middle inside, and across it with the middle outside.
    constexpr Vec2 inner{.x = Unit.radius * Half, .y = 0.0F};
    constexpr Vec2 straddling{.x = Unit.radius - (DotRadius * Half), .y = 0.0F};
    constexpr Vec2 outer{.x = Unit.radius + (DotRadius * Half), .y = 0.0F};
    const auto style = Style(1.0F, Unit);
    scope::Mesh kept;
    scope::Mesh crossing;
    scope::Mesh dropped;

    scope::Tessellate(kept, Dot(inner, DotRadius), style);
    scope::Tessellate(crossing, Dot(straddling, DotRadius), style);
    scope::Tessellate(dropped, Dot(outer, DotRadius), style);

    EXPECT_FALSE(kept.empty());
    EXPECT_FALSE(crossing.empty());
    EXPECT_TRUE(dropped.empty());
    // Its middle is inside, so all of it is kept, even the part past the clip's edge.
    EXPECT_TRUE(std::ranges::any_of(crossing.vertices(), [](const scope::Vertex& x) { return Distance(x.position, {}) > Unit.radius; }));
}

TEST(Tessellate, CutsAnOpenLineAtTheClip)
{
    constexpr float width{2.0F};
    const auto line = Line({.x = 0.0F, .y = 0.0F}, {.x = Far, .y = 0.0F}, width);
    scope::Mesh clipped;
    scope::Mesh whole;

    scope::Tessellate(clipped, line, Style(1.0F, Unit));
    scope::Tessellate(whole, line, Style(1.0F, std::nullopt));

    // The cut end is squared off half the width past the edge, plus the fringe.
    const auto reach = [](const scope::Mesh& mesh)
    { return std::ranges::max(mesh.vertices() | std::views::transform([](const scope::Vertex& x) { return x.position.x; })); };
    EXPECT_LE(reach(clipped), Unit.radius + width);
    EXPECT_GT(reach(whole), Far);
}

TEST(Tessellate, ScalesAndWidensStrokes)
{
    constexpr float scale{2.0F};
    constexpr float width{3.0F};
    constexpr float widen{1.0F};
    constexpr float height{10.0F};
    auto style = Style(scale, std::nullopt);
    style.widen = widen;
    scope::Mesh mesh;

    scope::Tessellate(mesh, Line({.x = 0.0F, .y = height}, {.x = DiscRadius, .y = height}, width), style);

    // A stroke's outermost vertices are half its width, and half the fringe's pixel, from its middle.
    const auto top = std::ranges::min(mesh.vertices() | std::views::transform([](const scope::Vertex& x) { return x.position.y; }));
    EXPECT_NEAR(top, (height * scale) - (((width * scale) + widen) * Half) - Half, Tolerance);
}

TEST(Tessellate, FillsAStrokedShapeWithoutFadingItsEdge)
{
    const scope::Shape both{.geometry = scope::Circle{.center = {.x = 0.0F, .y = 0.0F}, .radius = DiscRadius},
                            .pen = scope::Pen{.color = Lime, .width = 1.0F},
                            .fill = Lime,
                            .opacity = 1.0F};
    scope::Mesh stroked;
    scope::Mesh filled;

    scope::Tessellate(stroked, both, Style(1.0F, std::nullopt));
    scope::Tessellate(filled, Dot({}, DiscRadius), Style(1.0F, std::nullopt));

    // Under a stroke, every fill vertex is solid; alone, each corner has a clear one outside it.
    const auto points = static_cast<std::size_t>(scope::CircleSegments(DiscRadius));
    EXPECT_TRUE(std::ranges::all_of(stroked.vertices() | std::views::take(points), [](const scope::Vertex& x) { return x.tint.a == 1.0F; }));
    EXPECT_EQ(std::ranges::count_if(filled.vertices(), [](const scope::Vertex& x) { return x.tint.a == 0.0F; }), std::ssize(filled.vertices()) / 2);
}

TEST(TessellateRule, FadesFromItsColorOnTheLeft)
{
    constexpr float scale{2.0F};
    constexpr scope::Rule rule{.from = {.x = 10.0F, .y = 20.0F}, .length = 100.0F, .width = 2.0F, .color = Lime};
    scope::Mesh mesh;
    scope::TessellateRule(mesh, rule, scale);

    ASSERT_EQ(std::size(mesh.vertices()), 4U);
    for (const auto& vertex : mesh.vertices())
    {
        EXPECT_EQ(vertex.tint.a, vertex.position.x == rule.from.x * scale ? 1.0F : 0.0F);
        EXPECT_NEAR(std::abs(vertex.position.y - (rule.from.y * scale)), rule.width * scale * Half, Tolerance);
    }

    EXPECT_EQ(std::size(mesh.indices()), 6U);
}

TEST(TessellateBackdrop, ShadesFromItsCenterToItsRim)
{
    constexpr scope::Color inner{.r = 0x15, .g = 0x1E, .b = 0x29};
    const scope::Backdrop backdrop{.corner = {.x = -1.0F, .y = -1.0F},
                                   .width = 102.0F,
                                   .height = 52.0F,
                                   .center = {.x = 50.0F, .y = 25.0F},
                                   .radius = 20.0F,
                                   .inner = inner,
                                   .outer = scope::BackgroundColor};
    scope::Mesh mesh;
    scope::TessellateBackdrop(mesh, backdrop, 1.0F);

    const auto& vertices = mesh.vertices();
    ASSERT_GT(std::size(vertices), 5U);
    // The rectangle's corners, then the disc's center in the inner color, then its rim in the outer one.
    EXPECT_EQ(vertices.at(0).position.x, backdrop.corner.x);
    EXPECT_EQ(vertices.at(2).position.x, backdrop.corner.x + backdrop.width);
    EXPECT_NEAR(vertices.at(4).tint.b, scope::TintOf(inner, 1.0F).b, Tolerance);
    EXPECT_NEAR(Distance(vertices.back().position, backdrop.center), backdrop.radius, Tolerance);
    EXPECT_NEAR(vertices.back().tint.b, scope::TintOf(scope::BackgroundColor, 1.0F).b, Tolerance);
}

TEST(CollectGlow, FindsTheGlowingGroupsOwnShapesInOrder)
{
    constexpr Vec2 first{.x = 1.0F, .y = 0.0F};
    constexpr Vec2 second{.x = 2.0F, .y = 0.0F};
    constexpr Vec2 elsewhere{.x = 3.0F, .y = 0.0F};
    scope::Group glowing{.items = {}, .clip = std::nullopt, .glow = true};
    glowing.items.emplace_back(Dot(first, 1.0F));
    glowing.items.emplace_back(scope::Group{.items = {}, .clip = std::nullopt, .glow = false});
    glowing.items.emplace_back(Dot(second, 1.0F));

    scope::Group inside{.items = {}, .clip = scope::Circle{.center = {}, .radius = DiscRadius}, .glow = false};
    inside.items.emplace_back(Dot(elsewhere, 1.0F));
    inside.items.emplace_back(std::move(glowing));

    scope::Group root;
    root.items.emplace_back(std::move(inside));
    root.items.emplace_back(scope::Group{.items = {}, .clip = std::nullopt, .glow = true});

    std::vector<const scope::Shape*> shapes;
    scope::CollectGlow(root, shapes);

    ASSERT_EQ(std::size(shapes), 2U);
    EXPECT_EQ(std::get<scope::Circle>(shapes.at(0)->geometry).center.x, first.x);
    EXPECT_EQ(std::get<scope::Circle>(shapes.at(1)->geometry).center.x, second.x);
}
