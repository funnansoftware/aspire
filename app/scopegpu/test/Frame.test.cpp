#include <gtest/gtest.h>

import std;
import scope.glow;
import scope.mesh;
import scope.picture;
import scope.tessellation;
import scope.world;
import scopegpu.frame;

namespace
{
    constexpr std::uint32_t Width{1280};
    constexpr std::uint32_t Height{720};

    auto View(bool text) -> scope::View
    {
        return {.width = Width,
                .height = Height,
                .text = text,
                .advance = scope::DefaultAdvance,
                .caption = "TEST",
                .milliseconds = 0.0,
                .framesPerSecond = 0.0};
    }

    auto Build(scopegpu::Frame& frame, const scope::Picture& picture) -> void
    {
        frame.build(picture, static_cast<int>(Width), static_cast<int>(Height));
    }

    // How many of a group's items are texts, wherever they nest.
    // NOLINTNEXTLINE(misc-no-recursion)
    auto CountTexts(const scope::Group& group) -> std::size_t
    {
        std::size_t count = 0;

        for (const auto& item : group.items)
        {
            if (std::holds_alternative<scope::Text>(item.value))
            {
                ++count;
            }
            else if (const auto* inner = std::get_if<scope::Group>(&item.value); inner != nullptr)
            {
                count += CountTexts(*inner);
            }
        }

        return count;
    }
}

TEST(Frame, IsEmptyForAPictureWithNoPixels)
{
    const scope::Scope scope;
    scopegpu::Frame frame;
    Build(
        frame,
        scope::Compose(
            scope,
            {.width = 0, .height = 0, .text = false, .advance = scope::DefaultAdvance, .caption = {}, .milliseconds = 0.0, .framesPerSecond = 0.0}));

    EXPECT_TRUE(std::empty(frame.runs()));
    EXPECT_TRUE(frame.mesh().empty());
    EXPECT_FALSE(frame.glow().has_value());
}

TEST(Frame, StartsWithTheBackdropAndDrawsEveryShapeOnceInOrder)
{
    const scope::Scope scope;
    const auto picture = scope::Compose(scope, View(true));
    scopegpu::Frame frame;
    Build(frame, picture);

    const auto& runs = frame.runs();
    ASSERT_FALSE(std::empty(runs));
    EXPECT_TRUE(std::holds_alternative<scopegpu::BackdropRun>(runs.front()));

    // The shape runs cover the mesh's indices end to end.
    std::uint32_t next = 0;

    for (const auto& run : runs)
    {
        if (const auto* shapes = std::get_if<scopegpu::ShapeRun>(&run); shapes != nullptr)
        {
            EXPECT_EQ(shapes->firstIndex, next);
            EXPECT_GT(shapes->indexCount, 0U);
            next = shapes->firstIndex + shapes->indexCount;
        }
    }

    EXPECT_EQ(next, std::size(frame.mesh().indices()));
}

TEST(Frame, DrawsEveryTextInTheShapesBetween)
{
    const scope::Scope scope;
    const auto picture = scope::Compose(scope, View(true));
    scopegpu::Frame frame;
    Build(frame, picture);

    const auto texts = std::ranges::count_if(frame.runs(), [](const scopegpu::Run& x) { return std::holds_alternative<scopegpu::TextRun>(x); });
    EXPECT_EQ(static_cast<std::size_t>(texts), CountTexts(picture.root));
    EXPECT_GT(texts, 0);

    // Without a font there's no text, so the shapes between are drawn as one run.
    const auto bare = scope::Compose(scope, View(false));
    Build(frame, bare);
    EXPECT_EQ(std::size(frame.runs()), 2U);
}

TEST(Frame, TessellatesTheShapesAsScopeDoes)
{
    const scope::Scope scope;
    const auto picture = scope::Compose(scope, View(false));
    scopegpu::Frame frame;
    Build(frame, picture);

    // Without text, the frame's mesh is scope's for the whole picture, the backdrop aside.
    scope::Mesh expected;
    const scope::Style style{.scale = picture.scale, .clip = std::nullopt, .widen = 0.0F, .opacity = 1.0F, .channels = scope::Channels::All};

    // NOLINTNEXTLINE(misc-no-recursion)
    const auto add = [&](const auto& self, const scope::Group& group, scope::Style groupStyle) -> void
    {
        if (group.clip.has_value())
        {
            groupStyle.clip = scope::ClipOf(*group.clip, groupStyle.scale);
        }

        for (const auto& item : group.items)
        {
            if (const auto* shape = std::get_if<scope::Shape>(&item.value); shape != nullptr)
            {
                scope::Tessellate(expected, *shape, groupStyle);
            }
            else if (const auto* inner = std::get_if<scope::Group>(&item.value); inner != nullptr)
            {
                self(self, *inner, groupStyle);
            }
            else if (const auto* rule = std::get_if<scope::Rule>(&item.value); rule != nullptr)
            {
                scope::TessellateRule(expected, *rule, groupStyle.scale);
            }
        }
    };
    add(add, picture.root, style);

    ASSERT_EQ(std::size(frame.mesh().vertices()), std::size(expected.vertices()));
    EXPECT_EQ(frame.mesh().indices(), expected.indices());
}

TEST(Frame, PlansTheGlowOnlyWhenSomethingGlows)
{
    scope::Scope scope;
    scopegpu::Frame frame;
    Build(frame, scope::Compose(scope, View(false)));

    ASSERT_TRUE(frame.glow().has_value());
    EXPECT_FALSE(frame.glowMesh().empty());
    EXPECT_EQ(frame.glow().value_or(scope::GlowPlan{}).width,
              scope::PlanGlow(frame.scale(), static_cast<int>(Width), static_cast<int>(Height)).width);

    scope.toggleGlow();
    Build(frame, scope::Compose(scope, View(false)));
    EXPECT_FALSE(frame.glow().has_value());
    EXPECT_TRUE(frame.glowMesh().empty());
}
