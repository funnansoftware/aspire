export module scope.world;

import std;
import aspire.core;
import aspire.graphics;

export namespace scope
{
    using aspire::core::Vec2;
    using aspire::graphics::Color;

    /// @brief Who a track is, which sets its color.
    enum class Classification : std::uint8_t
    {
        Ownship,
        Friendly,
        Neutral,
        Hostile,
        Unknown,
    };

    /// @brief How much is known about a track, which sets its symbol.
    enum class Glyph : std::uint8_t
    {
        /// An open circle with a heading tick: a generic track.
        Circle,
        /// An outlined arrowhead: a typed track.
        Outline,
        /// A filled arrowhead: an identified track.
        Filled,
    };

    /// @brief The classification colors of one palette, after briarthorn's.
    struct Palette
    {
        std::string_view name;
        Color ownship;
        Color friendly;
        Color neutral;
        Color hostile;
        Color unknown;

        /// @brief Reports the color a track is drawn in.
        /// @param x The track's classification.
        /// @return The color.
        [[nodiscard]] constexpr auto colorOf(Classification x) const -> Color
        {
            switch (x)
            {
                case Classification::Ownship:
                    return ownship;
                case Classification::Friendly:
                    return friendly;
                case Classification::Neutral:
                    return neutral;
                case Classification::Hostile:
                    return hostile;
                case Classification::Unknown:
                    break;
            }

            return unknown;
        }
    };

    inline constexpr Palette StandardPalette{.name = "STANDARD",
                                             .ownship = {.r = 0x00, .g = 0xE5, .b = 0xFF},
                                             .friendly = {.r = 0x3D, .g = 0xFF, .b = 0x6E},
                                             .neutral = {.r = 0xFF, .g = 0xD2, .b = 0x3F},
                                             .hostile = {.r = 0xFF, .g = 0x3B, .b = 0x3B},
                                             .unknown = {.r = 0x9A, .g = 0xA0, .b = 0xA6}};

    /// Okabe and Ito's colors, which stay apart under the common red-green deficiencies.
    inline constexpr Palette ColorBlindPalette{.name = "COLOR-BLIND",
                                               .ownship = {.r = 0xFF, .g = 0xFF, .b = 0xFF},
                                               .friendly = {.r = 0x56, .g = 0xB4, .b = 0xE9},
                                               .neutral = {.r = 0xF0, .g = 0xE4, .b = 0x42},
                                               .hostile = {.r = 0xD5, .g = 0x5E, .b = 0x00},
                                               .unknown = {.r = 0x99, .g = 0x99, .b = 0x99}};

    inline constexpr Color BackgroundColor{.r = 0x0B, .g = 0x0F, .b = 0x14};

    /// The rings, ticks and readouts: everything that isn't a track.
    inline constexpr Color FurnitureColor{.r = 0x9A, .g = 0xA0, .b = 0xA6};

    /// @brief Flies a circle at a constant speed.
    ///
    /// Positions are in nautical miles east (`x`) and north (`y`) of ownship. Bearings are in degrees clockwise from
    /// north.
    struct Orbit
    {
        Vec2 center;
        float radius{};
        float knots{};
        /// Where on the circle the track is at time zero, as a bearing from the center.
        float startBearing{};
        bool clockwise{true};
    };

    /// @brief Flies a figure of eight, a lemniscate of Gerono, once a period.
    struct FigureEight
    {
        Vec2 center;
        /// From one end of the eight to the other, in nautical miles.
        float length{};
        /// The bearing of the eight's long axis.
        float bearing{};
        float seconds{};
    };

    /// @brief Flies a straight line, starting over once it has flown `span` nautical miles.
    struct Transit
    {
        Vec2 start;
        float heading{};
        float knots{};
        float span{};
    };

    using Route = std::variant<Orbit, FigureEight, Transit>;

    /// @brief Where a track is and where it's going.
    struct TrackState
    {
        Vec2 position;
        /// Degrees clockwise from north, in [0, 360).
        float heading{};
        float knots{};
    };

    struct Track
    {
        std::string callsign;
        Classification classification{};
        Glyph glyph{};
        Route route;
    };

    /// Scope seconds per real second, so the tracks visibly move at airliner speeds.
    inline constexpr float TimeCompression = 12.0F;

    /// Scope seconds between a track's history dots.
    inline constexpr float HistoryInterval = 12.0F;
    inline constexpr std::size_t HistoryCount = 6;

    /// Where the scope's clock starts, so the transits are already in view.
    inline constexpr float StartSeconds = 600.0F;

    /// The ranges the scope steps between, in nautical miles from ownship to the scale ring.
    inline constexpr std::array RangeSteps{10.0F, 20.0F, 40.0F, 80.0F};
    inline constexpr std::size_t InitialRangeStep = 2;

    /// How long the rings take to grow or shrink to a new range.
    inline constexpr float RangeTransitionSeconds = 0.7F;

    /// How long an automatic range change holds before the next.
    inline constexpr float RangeHoldSeconds = 3.0F;

    /// The shortest range shows four rings, and each doubling of range doubles their spacing.
    inline constexpr float RingsPerRange = 4.0F;

    /// The share of the range over which a ring fades in as it comes inside the scale ring.
    inline constexpr float RingEdgeFade = 0.05F;

    /// Seconds between pings, and how long each spreads and fades, so two are out at once.
    inline constexpr float PingPeriod = 1.5F;
    inline constexpr float PingLife = 3.0F;
    inline constexpr float PingOpacity = 0.6F;
}

// Helpers for the routes and eases, kept out of the module's interface.
namespace scope
{
    namespace
    {
        constexpr float SecondsPerHour = 3600.0F;
        constexpr float HalfCircle = 180.0F;
        constexpr float FullCircle = 360.0F;
        constexpr float RightAngle = 90.0F;
        constexpr float Half = 0.5F;
        constexpr float Twice = 2.0F;

        // Generated traffic is placed this close to ownship, and flies no further than its reach from where it's
        // placed, so all of it stays inside the scale ring at the initial range.
        constexpr float TrafficRadius = 34.0F;
        constexpr float TrafficReach = 5.0F;
        constexpr float TrafficSlowest = 180.0F;
        constexpr float TrafficFastest = 420.0F;
        constexpr float TrafficShortestEight = 400.0F;
        constexpr float TrafficLongestEight = 800.0F;
        constexpr std::uint32_t TrafficSeed = 0x5C09E;

        // Each place is this far round from the last, as seeds in a sunflower are, which spreads them evenly.
        constexpr float GoldenAngle = 137.507764F;

        // A track the scope starts with, its callsign a literal so that the list of them can be a constant.
        struct StartingTrack
        {
            std::string_view callsign;
            Classification classification{};
            Glyph glyph{};
            Route route;
        };

        constexpr std::array StartingTracks{
            StartingTrack{
                .callsign = "VIPER 2",
                .classification = Classification::Friendly,
                .glyph = Glyph::Filled,
                .route = Orbit{.center = {.x = 9.0F, .y = 12.0F}, .radius = 6.0F, .knots = 360.0F, .startBearing = 200.0F, .clockwise = true}},
            StartingTrack{.callsign = "BANDIT 1",
                          .classification = Classification::Hostile,
                          .glyph = Glyph::Outline,
                          .route = FigureEight{.center = {.x = -15.0F, .y = 9.0F}, .length = 22.0F, .bearing = 120.0F, .seconds = 640.0F}},
            StartingTrack{.callsign = "HEAVY 31",
                          .classification = Classification::Neutral,
                          .glyph = Glyph::Filled,
                          .route = Transit{.start = {.x = -55.0F, .y = -50.0F}, .heading = 52.0F, .knots = 300.0F, .span = 150.0F}},
            StartingTrack{.callsign = "TRACK 4",
                          .classification = Classification::Unknown,
                          .glyph = Glyph::Circle,
                          .route = Orbit{.center = {}, .radius = 28.0F, .knots = 240.0F, .startBearing = 120.0F, .clockwise = false}},
        };

        auto Radians(float degrees) -> float
        {
            return degrees * std::numbers::pi_v<float> / HalfCircle;
        }

        auto Degrees(float radians) -> float
        {
            return radians * HalfCircle / std::numbers::pi_v<float>;
        }

        // A unit vector toward a bearing.
        auto Toward(float bearing) -> Vec2
        {
            const auto angle = Radians(bearing);
            return {.x = std::sin(angle), .y = std::cos(angle)};
        }

        auto Add(Vec2 a, Vec2 b) -> Vec2
        {
            return {.x = a.x + b.x, .y = a.y + b.y};
        }

        auto Scale(Vec2 a, float b) -> Vec2
        {
            return {.x = a.x * b, .y = a.y * b};
        }

        auto Wrap(float x, float period) -> float
        {
            const auto wrapped = std::fmod(x, period);
            return wrapped < 0.0F ? wrapped + period : wrapped;
        }

        auto SampleOrbit(const Orbit& x, float seconds) -> TrackState
        {
            const auto turned = Degrees(x.knots / SecondsPerHour / x.radius * seconds);
            const auto bearing = x.startBearing + (x.clockwise ? turned : -turned);
            return {.position = Add(x.center, Scale(Toward(bearing), x.radius)),
                    .heading = Wrap(bearing + (x.clockwise ? RightAngle : -RightAngle), FullCircle),
                    .knots = x.knots};
        }

        auto SampleFigureEight(const FigureEight& x, float seconds) -> TrackState
        {
            // Along the axis the track swings out to each end; across it, twice as fast, to make the two loops.
            const auto rate = Twice * std::numbers::pi_v<float> / x.seconds;
            const auto phase = rate * seconds;
            const auto reach = x.length * Half;
            const auto along = Toward(x.bearing);
            const auto across = Toward(x.bearing + RightAngle);
            const auto position = Add(x.center, Add(Scale(along, reach * std::sin(phase)), Scale(across, reach * Half * std::sin(Twice * phase))));
            const auto velocity = Add(Scale(along, reach * rate * std::cos(phase)), Scale(across, reach * rate * std::cos(Twice * phase)));
            return {.position = position,
                    .heading = Wrap(Degrees(std::atan2(velocity.x, velocity.y)), FullCircle),
                    .knots = std::hypot(velocity.x, velocity.y) * SecondsPerHour};
        }

        auto SampleTransit(const Transit& x, float seconds) -> TrackState
        {
            const auto flown = Wrap(x.knots / SecondsPerHour * seconds, x.span);
            return {.position = Add(x.start, Scale(Toward(x.heading), flown)), .heading = Wrap(x.heading, FullCircle), .knots = x.knots};
        }

        // Starts and ends gently, with the fastest change halfway: 4t^3, then mirrored.
        auto EaseInOutCubic(float t) -> float
        {
            constexpr float steepness = 4.0F;
            const auto rest = Twice * (1.0F - t);
            return t < Half ? steepness * t * t * t : 1.0F - (rest * rest * rest * Half);
        }

        auto EaseOutCubic(float t) -> float
        {
            const auto rest = 1.0F - t;
            return 1.0F - (rest * rest * rest);
        }

        // 3t^2 - 2t^3, clamped to [0, 1].
        auto SmoothStep(float t) -> float
        {
            constexpr float rise = 3.0F;
            const auto x = std::clamp(t, 0.0F, 1.0F);
            return x * x * (rise - (Twice * x));
        }
    }
}

export namespace scope
{
    /// @brief Reports where a route has taken its track.
    /// @param route The route.
    /// @param seconds Scope seconds since the route began. Negative times are earlier laps.
    /// @return The track's position, heading and speed.
    [[nodiscard]] auto Sample(const Route& route, float seconds) -> TrackState
    {
        return std::visit(
            aspire::core::Overloaded{
                [seconds](const Orbit& x) { return SampleOrbit(x, seconds); },
                [seconds](const FigureEight& x) { return SampleFigureEight(x, seconds); },
                [seconds](const Transit& x) { return SampleTransit(x, seconds); },
            },
            route);
    }

    /// @brief Reports where a track has been: its history dots, newest first.
    /// @param route The track's route.
    /// @param seconds The scope's time.
    /// @return The positions `HistoryInterval` apart before `seconds`.
    [[nodiscard]] auto History(const Route& route, float seconds) -> std::array<Vec2, HistoryCount>
    {
        std::array<Vec2, HistoryCount> dots{};

        for (std::size_t i = 0; i < HistoryCount; ++i)
        {
            dots.at(i) = Sample(route, seconds - (static_cast<float>(i + 1) * HistoryInterval)).position;
        }

        return dots;
    }

    /// @brief Makes the tracks a scope shows until it's given others.
    /// @return Four tracks, one of each classification but ownship's, flying every kind of route.
    [[nodiscard]] auto DefaultTracks() -> std::vector<Track>
    {
        return StartingTracks
               | std::views::transform(
                   [](const StartingTrack& x)
                   { return Track{.callsign = std::string{x.callsign}, .classification = x.classification, .glyph = x.glyph, .route = x.route}; })
               | std::ranges::to<std::vector>();
    }

    /// @brief Makes a crowd of tracks, to see how a scope copes with many.
    ///
    /// The tracks are spread evenly over a disc inside the scale ring at the initial range, and stay inside it, with
    /// every route, glyph and classification mixed among them. The same count always makes the same tracks.
    ///
    /// @param count How many tracks to make.
    /// @return The tracks, with callsigns numbered from `TRK 001`.
    [[nodiscard]] auto Traffic(std::size_t count) -> std::vector<Track>
    {
        constexpr std::array classifications{Classification::Friendly, Classification::Neutral, Classification::Hostile, Classification::Unknown};
        constexpr std::array glyphs{Glyph::Circle, Glyph::Outline, Glyph::Filled};
        constexpr auto routes = std::variant_size_v<Route>;
        constexpr std::uint32_t fractionBits = 24;

        std::mt19937 random{TrafficSeed};
        // Between two values, from the generator's bits alone, so every standard library makes the same tracks.
        const auto between = [&random](float low, float high)
        {
            constexpr auto shift = static_cast<std::uint32_t>(std::numeric_limits<std::uint32_t>::digits) - fractionBits;
            const auto fraction = static_cast<float>(random() >> shift) / static_cast<float>(1U << fractionBits);
            return low + ((high - low) * fraction);
        };

        std::vector<Track> tracks;
        tracks.reserve(count);

        for (std::size_t i = 0; i < count; ++i)
        {
            const auto out = TrafficRadius * std::sqrt((static_cast<float>(i) + Half) / static_cast<float>(count));
            const auto place = Scale(Toward(static_cast<float>(i) * GoldenAngle), out);
            const auto heading = between(0.0F, FullCircle);
            const auto knots = between(TrafficSlowest, TrafficFastest);
            Route route;

            switch (i % routes)
            {
                case 0:
                    route = Orbit{.center = place,
                                  .radius = between(TrafficReach * Half, TrafficReach),
                                  .knots = knots,
                                  .startBearing = heading,
                                  .clockwise = i % 2 == 0};
                    break;
                case 1:
                    route = FigureEight{.center = place,
                                        .length = between(TrafficReach, TrafficReach * Twice),
                                        .bearing = heading,
                                        .seconds = between(TrafficShortestEight, TrafficLongestEight)};
                    break;
                default:
                    // Back and forth across the place, so the start-over jump stays near it.
                    route = Transit{
                        .start = Add(place, Scale(Toward(heading), -TrafficReach)), .heading = heading, .knots = knots, .span = TrafficReach * Twice};
                    break;
            }

            tracks.push_back({.callsign = std::format("TRK {:03}", i + 1),
                              .classification = classifications.at(i % std::size(classifications)),
                              .glyph = glyphs.at((i / routes) % std::size(glyphs)),
                              .route = route});
        }

        return tracks;
    }

    /// @brief Reports a position's bearing from ownship.
    /// @param x The position.
    /// @return Degrees clockwise from north, in [0, 360).
    [[nodiscard]] auto BearingOf(Vec2 x) -> float
    {
        return Wrap(Degrees(std::atan2(x.x, x.y)), FullCircle);
    }

    /// @brief Reports a position's distance from ownship.
    /// @param x The position.
    /// @return Nautical miles.
    [[nodiscard]] auto RangeOf(Vec2 x) -> float
    {
        return std::hypot(x.x, x.y);
    }

    /// @brief The range at the scale ring, which eases between `RangeSteps` so the rings grow and shrink.
    ///
    /// A new range starts from the range shown, so a change partway through another reverses smoothly. The ease runs
    /// in log space, so doubling and halving the range look alike.
    class RangeScale
    {
    public:
        /// @brief Steps to the next shorter or longer range.
        /// @param direction Negative for shorter, positive for longer.
        /// @return `false` if there's no range that way.
        auto step(int direction) -> bool
        {
            const auto next = static_cast<std::ptrdiff_t>(step_) + std::clamp(direction, -1, 1);

            if (direction == 0 || next < 0 || next >= std::ssize(RangeSteps))
            {
                return false;
            }

            from_ = range();
            step_ = static_cast<std::size_t>(next);
            progress_ = 0.0F;
            held_ = 0.0F;
            return true;
        }

        /// @brief Moves the ease on, and when cycling, steps to the next range once this one has held.
        /// @param seconds Real seconds since the last call.
        auto advance(float seconds) -> void
        {
            progress_ = std::min(1.0F, progress_ + (seconds / RangeTransitionSeconds));

            if (!cycling_ || progress_ < 1.0F)
            {
                return;
            }

            held_ += seconds;

            if (held_ >= RangeHoldSeconds)
            {
                // Out to the longest range and back in to the shortest, over and over.
                if (!step(direction_))
                {
                    direction_ = -direction_;
                    step(direction_);
                }
            }
        }

        /// @brief Reports the range shown at the scale ring, partway through any change.
        /// @return Nautical miles.
        [[nodiscard]] auto range() const -> float
        {
            const auto to = target();
            return std::exp(std::lerp(std::log(from_), std::log(to), EaseInOutCubic(progress_)));
        }

        /// @brief Reports the range being changed to, or shown.
        /// @return Nautical miles.
        [[nodiscard]] auto target() const -> float
        {
            return RangeSteps.at(step_);
        }

        [[nodiscard]] auto settled() const -> bool
        {
            return progress_ >= 1.0F;
        }

        /// @brief Sets whether the range steps out and back in on its own.
        /// @param x `true` to cycle, the default.
        auto setCycling(bool x) -> void
        {
            cycling_ = x;
            held_ = 0.0F;
        }

        [[nodiscard]] auto getCycling() const -> bool
        {
            return cycling_;
        }

    private:
        std::size_t step_{InitialRangeStep};
        float from_{RangeSteps.at(InitialRangeStep)};
        float progress_{1.0F};
        float held_{};
        int direction_{1};
        bool cycling_{true};
    };

    /// @brief A range ring inside the scale ring.
    struct RangeRing
    {
        float nm{};
        float opacity{};
    };

    /// @brief Reports the range rings to draw inside the scale ring at a range.
    ///
    /// The rings stay where they are on the ground, so they grow as the range shortens and shrink as it lengthens.
    /// Between two steps, the rings only the shorter range has fade out, so every step shows the same number of rings.
    ///
    /// @param range The range at the scale ring, in nautical miles.
    /// @return The rings, innermost first, without one at the scale ring itself.
    [[nodiscard]] auto RangeRings(float range) -> std::vector<RangeRing>
    {
        std::vector<RangeRing> rings;

        if (!(range > 0.0F))
        {
            return rings;
        }

        const auto level = std::log2(range / RangeSteps.front());
        const auto whole = std::floor(level);
        const auto spacing = RangeSteps.front() / RingsPerRange * std::exp2(whole);
        const auto fade = SmoothStep(level - whole);

        for (auto i = 1;; ++i)
        {
            const auto nm = static_cast<float>(i) * spacing;

            if (nm >= range)
            {
                break;
            }

            const auto kept = i % 2 == 0 ? 1.0F : 1.0F - fade;
            const auto inside = std::clamp((range - nm) / (range * RingEdgeFade), 0.0F, 1.0F);

            if (const auto opacity = kept * inside; opacity > 0.0F)
            {
                rings.emplace_back(RangeRing{.nm = nm, .opacity = opacity});
            }
        }

        return rings;
    }

    /// @brief A ring spreading out from ownship.
    struct Ping
    {
        /// How far it has spread, as a share of the scale ring's radius.
        float reach{};
        float opacity{};
    };

    /// @brief Reports the pings out at a time.
    /// @param seconds Real seconds the scope has run.
    /// @return The pings, newest first.
    [[nodiscard]] auto Pings(float seconds) -> std::vector<Ping>
    {
        std::vector<Ping> pings;
        const auto newest = Wrap(seconds, PingPeriod);

        for (auto i = 0;; ++i)
        {
            const auto age = newest + (static_cast<float>(i) * PingPeriod);

            if (age >= PingLife)
            {
                break;
            }

            const auto life = age / PingLife;
            const auto fading = 1.0F - life;
            pings.emplace_back(Ping{.reach = EaseOutCubic(life), .opacity = PingOpacity * fading * fading});
        }

        return pings;
    }

    /// @brief The scope's state: its clocks, its range, and how it's shown.
    class Scope
    {
    public:
        /// The longest step taken at once, so a stall doesn't jump the tracks.
        static constexpr float MaxStepSeconds = 0.1F;

        /// @brief Moves the scope on.
        /// @param seconds Real seconds since the last call. Negative steps do nothing.
        auto advance(float seconds) -> void
        {
            const auto step = std::clamp(seconds, 0.0F, MaxStepSeconds);
            range_.advance(step);

            if (!paused_)
            {
                seconds_ += step * TimeCompression;
                pingSeconds_ += step;
            }
        }

        /// @brief Steps the range by hand, which stops it cycling.
        /// @param direction Negative for shorter, positive for longer.
        auto stepRange(int direction) -> void
        {
            range_.setCycling(false);
            range_.step(direction);
        }

        auto toggleCycling() -> void
        {
            range_.setCycling(!range_.getCycling());
        }

        auto togglePause() -> void
        {
            paused_ = !paused_;
        }

        auto togglePalette() -> void
        {
            colorBlind_ = !colorBlind_;
        }

        auto toggleGlow() -> void
        {
            glow_ = !glow_;
        }

        [[nodiscard]] auto getRange() const -> const RangeScale&
        {
            return range_;
        }

        /// @brief Reports the scope's clock, which the tracks fly by.
        /// @return Scope seconds, `TimeCompression` times faster than real ones.
        [[nodiscard]] auto getSeconds() const -> float
        {
            return seconds_;
        }

        /// @brief Reports the clock the pings spread by.
        /// @return Real seconds the scope has run unpaused.
        [[nodiscard]] auto getPingSeconds() const -> float
        {
            return pingSeconds_;
        }

        [[nodiscard]] auto getPaused() const -> bool
        {
            return paused_;
        }

        [[nodiscard]] auto getPalette() const -> const Palette&
        {
            return colorBlind_ ? ColorBlindPalette : StandardPalette;
        }

        [[nodiscard]] auto getGlow() const -> bool
        {
            return glow_;
        }

        /// @brief Replaces the tracks the scope shows.
        /// @param x The tracks, in the order the track table lists them.
        auto setTracks(std::vector<Track> x) -> void
        {
            tracks_ = std::move(x);
        }

        /// @brief Reports the tracks the scope shows: `DefaultTracks()`, until `setTracks()` replaces them.
        /// @return The tracks, in the order the track table lists them.
        [[nodiscard]] auto getTracks() const -> std::span<const Track>
        {
            return tracks_;
        }

    private:
        std::vector<Track> tracks_{DefaultTracks()};
        RangeScale range_;
        float seconds_{StartSeconds};
        float pingSeconds_{};
        bool paused_{false};
        bool colorBlind_{false};
        bool glow_{true};
    };
}
