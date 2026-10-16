#include <gtest/gtest.h>

import std;
import scope.world;

namespace
{
    using scope::Vec2;

    constexpr float Tolerance{1e-3F};
    constexpr float SecondsPerHour{3600.0F};
    constexpr int FramesPerSecond{60};
    constexpr float FrameSeconds{1.0F / FramesPerSecond};
    constexpr float Half{0.5F};
    constexpr float Twice{2.0F};
    constexpr float Tau{2.0F * std::numbers::pi_v<float>};

    // The range one step longer than the one the scope starts at.
    constexpr float LongerRange{scope::RangeSteps.at(scope::InitialRangeStep + 1)};

    auto Distance(Vec2 a, Vec2 b) -> float
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    // The heading a track flies between two moments, from where it was at each.
    auto HeadingBetween(const scope::Route& route, float seconds) -> float
    {
        constexpr float step{0.01F};
        const auto before = scope::Sample(route, seconds - step).position;
        const auto after = scope::Sample(route, seconds + step).position;
        return scope::BearingOf({.x = after.x - before.x, .y = after.y - before.y});
    }

    // How far apart two headings are, the short way round.
    auto TurnBetween(float a, float b) -> float
    {
        constexpr float fullCircle{360.0F};
        const auto turn = std::fmod(std::abs(a - b), fullCircle);
        return std::min(turn, fullCircle - turn);
    }

    // Runs a range for a while in frame-sized steps.
    auto RunFor(scope::RangeScale& range, float seconds) -> void
    {
        const auto frames = static_cast<int>(std::ceil(seconds * FramesPerSecond));

        for (auto i = 0; i < frames; ++i)
        {
            range.advance(FrameSeconds);
        }
    }

    auto Find(const std::vector<scope::RangeRing>& rings, float nm) -> std::optional<float>
    {
        const auto found = std::ranges::find_if(rings, [nm](const auto& x) { return std::abs(x.nm - nm) < Tolerance; });
        return found == std::end(rings) ? std::nullopt : std::optional{found->opacity};
    }

    // Everything about some tracks, exactly, and where they are at the start, so two sets can be compared whole.
    auto Describe(const std::vector<scope::Track>& tracks) -> std::vector<std::string>
    {
        std::vector<std::string> described;

        for (const auto& track : tracks)
        {
            const auto state = scope::Sample(track.route, scope::StartSeconds);
            described.push_back(std::format("{} {} {} {} {:a} {:a} {:a}", track.callsign, std::to_underlying(track.classification),
                                            std::to_underlying(track.glyph), track.route.index(), state.position.x, state.position.y, state.heading));
        }

        return described;
    }
}

TEST(Bearing, RunsClockwiseFromNorth)
{
    EXPECT_NEAR(scope::BearingOf({.x = 0.0F, .y = 1.0F}), 0.0F, Tolerance);
    EXPECT_NEAR(scope::BearingOf({.x = 1.0F, .y = 0.0F}), 90.0F, Tolerance);
    EXPECT_NEAR(scope::BearingOf({.x = 0.0F, .y = -1.0F}), 180.0F, Tolerance);
    EXPECT_NEAR(scope::BearingOf({.x = -1.0F, .y = 0.0F}), 270.0F, Tolerance);
    EXPECT_NEAR(scope::RangeOf({.x = 3.0F, .y = 4.0F}), 5.0F, Tolerance);
}

TEST(Orbit, StaysOnItsCircleAtItsSpeed)
{
    const scope::Orbit orbit{.center = {.x = 5.0F, .y = -3.0F}, .radius = 6.0F, .knots = 360.0F, .startBearing = 0.0F, .clockwise = true};

    for (const auto seconds : {0.0F, 17.0F, 123.0F, -40.0F})
    {
        const auto state = scope::Sample(orbit, seconds);
        EXPECT_NEAR(Distance(state.position, orbit.center), orbit.radius, Tolerance);
        EXPECT_FLOAT_EQ(state.knots, orbit.knots);
        EXPECT_LT(TurnBetween(state.heading, HeadingBetween(orbit, seconds)), 0.1F);
    }
}

TEST(Orbit, StartsAtItsBearingAndLapsAtItsSpeed)
{
    // A clockwise orbit starting north of its center flies east, and covers its speed's distance along the circle.
    const scope::Orbit orbit{.center = {.x = 5.0F, .y = -3.0F}, .radius = 6.0F, .knots = 360.0F, .startBearing = 0.0F, .clockwise = true};
    const auto start = scope::Sample(orbit, 0.0F);
    EXPECT_NEAR(start.position.y, orbit.center.y + orbit.radius, Tolerance);
    EXPECT_NEAR(start.heading, 90.0F, Tolerance);
    const auto lap = Tau * orbit.radius / (orbit.knots / SecondsPerHour);
    EXPECT_NEAR(scope::Sample(orbit, lap).position.x, start.position.x, 1e-2F);
}

TEST(Orbit, TurnsTheOtherWayAnticlockwise)
{
    const scope::Orbit orbit{.center = {}, .radius = 10.0F, .knots = 240.0F, .startBearing = 90.0F, .clockwise = false};
    const auto state = scope::Sample(orbit, 0.0F);
    EXPECT_NEAR(state.position.x, 10.0F, Tolerance);
    EXPECT_NEAR(state.heading, 0.0F, Tolerance);
}

TEST(FigureEight, CrossesItsCenterAndFliesAlongItsPath)
{
    const scope::FigureEight eight{.center = {.x = -15.0F, .y = 9.0F}, .length = 22.0F, .bearing = 120.0F, .seconds = 400.0F};

    const auto start = scope::Sample(eight, 0.0F);
    EXPECT_NEAR(Distance(start.position, eight.center), 0.0F, Tolerance);

    // A quarter of the way round, it's at one end of the eight, on its long axis.
    const auto end = scope::Sample(eight, eight.seconds / 4.0F);
    EXPECT_NEAR(Distance(end.position, eight.center), eight.length * Half, Tolerance);
    EXPECT_NEAR(scope::BearingOf({.x = end.position.x - eight.center.x, .y = end.position.y - eight.center.y}), eight.bearing, 0.01F);

    for (const auto seconds : {13.0F, 77.0F, 250.0F})
    {
        const auto state = scope::Sample(eight, seconds);
        EXPECT_LT(TurnBetween(state.heading, HeadingBetween(eight, seconds)), 0.1F);
        EXPECT_GT(state.knots, 0.0F);
    }
}

TEST(Transit, FliesStraightAndStartsOver)
{
    const scope::Transit transit{.start = {.x = -10.0F, .y = 0.0F}, .heading = 90.0F, .knots = 360.0F, .span = 30.0F};

    const auto later = scope::Sample(transit, 100.0F);
    EXPECT_NEAR(later.position.x, 0.0F, Tolerance);
    EXPECT_NEAR(later.position.y, 0.0F, Tolerance);
    EXPECT_FLOAT_EQ(later.heading, 90.0F);

    // 30 NM at 360 kt is 300 seconds, after which it starts over.
    EXPECT_NEAR(scope::Sample(transit, 400.0F).position.x, 0.0F, Tolerance);
    EXPECT_NEAR(scope::Sample(transit, -200.0F).position.x, 0.0F, Tolerance);
}

TEST(History, TrailsBehindTheTrack)
{
    const scope::Transit transit{.start = {}, .heading = 0.0F, .knots = 360.0F, .span = 1000.0F};
    constexpr float now{600.0F};
    const auto dots = scope::History(transit, now);
    auto previous = scope::Sample(transit, now).position.y;

    for (const auto& dot : dots)
    {
        EXPECT_NEAR(previous - dot.y, 360.0F / SecondsPerHour * scope::HistoryInterval, Tolerance);
        previous = dot.y;
    }
}

TEST(RangeScale, EasesToEachStepAndStops)
{
    scope::RangeScale range;
    range.setCycling(false);
    const auto start = range.target();
    EXPECT_FLOAT_EQ(range.range(), start);

    ASSERT_TRUE(range.step(1));
    EXPECT_FLOAT_EQ(range.target(), LongerRange);
    EXPECT_FLOAT_EQ(range.range(), start);

    // The rings grow and shrink without jumping: every frame moves the range a little, one way.
    auto previous = range.range();
    for (auto i = 0; i < static_cast<int>(scope::RangeTransitionSeconds * FramesPerSecond); ++i)
    {
        range.advance(FrameSeconds);
        EXPECT_GE(range.range(), previous);
        EXPECT_LT(range.range() / previous, 1.1F);
        previous = range.range();
    }

    range.advance(FrameSeconds);
    EXPECT_TRUE(range.settled());
    EXPECT_FLOAT_EQ(range.range(), LongerRange);

    // Without cycling, it stays put.
    RunFor(range, scope::RangeHoldSeconds + scope::RangeHoldSeconds);
    EXPECT_FLOAT_EQ(range.range(), LongerRange);
}

TEST(RangeScale, ReversesFromWhereItIs)
{
    scope::RangeScale range;
    range.setCycling(false);
    const auto start = range.target();
    range.step(1);
    range.advance(scope::RangeTransitionSeconds * Half);
    const auto midway = range.range();
    EXPECT_GT(midway, start);
    EXPECT_LT(midway, LongerRange);

    range.step(-1);
    EXPECT_FLOAT_EQ(range.range(), midway);
    RunFor(range, scope::RangeTransitionSeconds);
    EXPECT_FLOAT_EQ(range.range(), start);
}

TEST(RangeScale, StopsAtTheShortestAndLongest)
{
    scope::RangeScale range;
    range.setCycling(false);

    while (range.step(-1))
    {
    }
    EXPECT_FLOAT_EQ(range.target(), scope::RangeSteps.front());
    EXPECT_FALSE(range.step(-1));

    while (range.step(1))
    {
    }
    EXPECT_FLOAT_EQ(range.target(), scope::RangeSteps.back());
    EXPECT_FALSE(range.step(1));
    EXPECT_FALSE(range.step(0));
}

TEST(RangeScale, CyclesOutAndBackIn)
{
    scope::RangeScale range;
    std::vector<float> visited{range.target()};
    const auto dwell = scope::RangeHoldSeconds + scope::RangeTransitionSeconds;

    for (std::size_t i = 0; i < 2 * std::size(scope::RangeSteps); ++i)
    {
        RunFor(range, dwell);
        if (range.target() != visited.back())
        {
            visited.push_back(range.target());
        }
    }

    // From the initial step out to the longest range, then back in past where it started.
    const auto longest = std::ranges::find(visited, scope::RangeSteps.back());
    ASSERT_NE(longest, std::end(visited));
    ASSERT_NE(std::next(longest), std::end(visited));
    EXPECT_LT(*std::next(longest), scope::RangeSteps.back());

    for (const auto [a, b] : visited | std::views::adjacent<2>)
    {
        EXPECT_TRUE(b == a * Twice || b == a * Half) << a << " to " << b;
    }
}

TEST(RangeRings, QuarterEachStep)
{
    for (const auto step : scope::RangeSteps)
    {
        const auto rings = scope::RangeRings(step);
        ASSERT_EQ(std::size(rings), 3U) << step;

        for (std::size_t i = 0; i < std::size(rings); ++i)
        {
            EXPECT_NEAR(rings.at(i).nm, step * static_cast<float>(i + 1) / scope::RingsPerRange, Tolerance);
            EXPECT_FLOAT_EQ(rings.at(i).opacity, 1.0F);
        }
    }
}

TEST(RangeRings, FadeTheRingsOnlyTheShorterRangeHas)
{
    // Halfway from 20 NM to 40 NM, the 10 and 20 NM rings both ranges have stay, and the 5 and 15 NM rings fade.
    const auto rings = scope::RangeRings(std::sqrt(20.0F * 40.0F));
    EXPECT_EQ(Find(rings, 10.0F), 1.0F);
    EXPECT_EQ(Find(rings, 20.0F), 1.0F);
    EXPECT_GT(Find(rings, 5.0F).value_or(0.0F), 0.0F);
    EXPECT_LT(Find(rings, 5.0F).value_or(1.0F), 1.0F);
    EXPECT_EQ(Find(rings, 5.0F), Find(rings, 15.0F));
}

TEST(RangeRings, ChangeSmoothlyAcrossAStep)
{
    // Just either side of 40 NM, every ring has nearly the same opacity, so nothing pops.
    constexpr float step{40.0F};
    constexpr float nudge{1e-3F};
    const auto below = scope::RangeRings(step - nudge);
    const auto above = scope::RangeRings(step + nudge);

    for (const auto& ring : below)
    {
        EXPECT_NEAR(ring.opacity, Find(above, ring.nm).value_or(0.0F), 0.01F) << ring.nm;
    }

    for (const auto& ring : above)
    {
        EXPECT_NEAR(ring.opacity, Find(below, ring.nm).value_or(0.0F), 0.01F) << ring.nm;
    }
}

TEST(RangeRings, FadeInAtTheScaleRing)
{
    // A ring just inside the scale ring is faint, and none is drawn on or past it.
    constexpr float range{20.05F};
    const auto rings = scope::RangeRings(range);
    ASSERT_TRUE(Find(rings, 20.0F).has_value());
    EXPECT_LT(Find(rings, 20.0F).value_or(1.0F), 0.1F);
    EXPECT_TRUE(std::ranges::all_of(rings, [](const auto& x) { return x.nm < range; }));
    EXPECT_TRUE(std::empty(scope::RangeRings(0.0F)));
}

TEST(Pings, SpreadAndFade)
{
    const auto fresh = scope::Pings(0.0F);
    ASSERT_FALSE(std::empty(fresh));
    EXPECT_FLOAT_EQ(fresh.front().reach, 0.0F);
    EXPECT_FLOAT_EQ(fresh.front().opacity, scope::PingOpacity);

    // Two are out at once, the older wider and fainter.
    const auto later = scope::Pings(scope::PingPeriod * Half);
    ASSERT_EQ(std::size(later), 2U);
    EXPECT_GT(later.at(1).reach, later.at(0).reach);
    EXPECT_LT(later.at(1).opacity, later.at(0).opacity);
    EXPECT_TRUE(std::ranges::all_of(later, [](const auto& x) { return x.reach <= 1.0F && x.opacity >= 0.0F; }));
}

TEST(Palette, KeepsTheFiveClassificationsApart)
{
    using enum scope::Classification;

    for (const auto& palette : {scope::StandardPalette, scope::ColorBlindPalette})
    {
        const std::array colors{palette.colorOf(Ownship), palette.colorOf(Friendly), palette.colorOf(Neutral), palette.colorOf(Hostile),
                                palette.colorOf(Unknown)};

        for (std::size_t i = 0; i < std::size(colors); ++i)
        {
            for (std::size_t j = i + 1; j < std::size(colors); ++j)
            {
                const auto& a = colors.at(i);
                const auto& b = colors.at(j);
                EXPECT_FALSE(a.r == b.r && a.g == b.g && a.b == b.b) << palette.name << " " << i << " " << j;
            }
        }
    }
}

TEST(Scope, PausesTheTracksButNotTheRange)
{
    scope::Scope scope;
    scope.togglePause();
    scope.stepRange(1);
    const auto seconds = scope.getSeconds();
    scope.advance(scope::RangeTransitionSeconds);
    EXPECT_FLOAT_EQ(scope.getSeconds(), seconds);
    EXPECT_FALSE(scope.getRange().getCycling());

    scope.togglePause();
    scope.advance(FrameSeconds);
    EXPECT_NEAR(scope.getSeconds(), seconds + (FrameSeconds * scope::TimeCompression), Tolerance);

    // A stall is clamped rather than jumping the tracks, and time never runs backward.
    constexpr float stall{10.0F};
    const auto before = scope.getSeconds();
    scope.advance(stall);
    EXPECT_NEAR(scope.getSeconds(), before + (scope::Scope::MaxStepSeconds * scope::TimeCompression), Tolerance);
    scope.advance(-1.0F);
    EXPECT_NEAR(scope.getSeconds(), before + (scope::Scope::MaxStepSeconds * scope::TimeCompression), Tolerance);
}

TEST(Scope, ShowsTheTracksItIsGiven)
{
    constexpr std::size_t count{400};
    scope::Scope scope;
    EXPECT_EQ(std::size(scope.getTracks()), std::size(scope::DefaultTracks()));

    scope.setTracks(scope::Traffic(count));
    EXPECT_EQ(std::size(scope.getTracks()), count);
}

TEST(Traffic, MakesTheSameTracksForTheSameCount)
{
    constexpr std::size_t count{400};
    const auto tracks = Describe(scope::Traffic(count));
    EXPECT_EQ(std::size(tracks), count);
    EXPECT_EQ(tracks, Describe(scope::Traffic(count)));
}

TEST(Traffic, MixesEveryRouteGlyphAndClassification)
{
    constexpr std::size_t count{36};
    std::set<std::size_t> routes;
    std::set<scope::Glyph> glyphs;
    std::set<scope::Classification> classifications;
    std::set<std::string> callsigns;

    for (const auto& track : scope::Traffic(count))
    {
        routes.insert(track.route.index());
        glyphs.insert(track.glyph);
        classifications.insert(track.classification);
        callsigns.insert(track.callsign);
    }

    EXPECT_EQ(std::size(routes), std::variant_size_v<scope::Route>);
    EXPECT_EQ(std::size(glyphs), 3U);
    // Ownship is the scope's own symbol, never a track.
    EXPECT_EQ(std::size(classifications), 4U);
    EXPECT_FALSE(classifications.contains(scope::Classification::Ownship));
    EXPECT_EQ(std::size(callsigns), count);
}

TEST(Traffic, StaysInsideTheScaleRingAtTheInitialRange)
{
    constexpr std::size_t count{400};
    constexpr int steps{720};
    constexpr float step{5.0F};
    const auto range = scope::RangeSteps.at(scope::InitialRangeStep);
    // From the oldest history dot at the start, through an hour of flying.
    const auto from = scope::StartSeconds - (static_cast<float>(scope::HistoryCount) * scope::HistoryInterval);

    for (const auto& track : scope::Traffic(count))
    {
        for (auto i = 0; i < steps; ++i)
        {
            const auto seconds = from + (static_cast<float>(i) * step);
            ASSERT_LT(scope::RangeOf(scope::Sample(track.route, seconds).position), range) << track.callsign << " at " << seconds;
        }
    }
}
