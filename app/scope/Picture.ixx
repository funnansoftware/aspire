export module scope.picture;

import std;
import scope.world;

// A frame of the scope as a picture of shapes and text, in logical units, for a renderer to draw however it can.
export namespace scope
{
    /// Everything is laid out on a 1280 x 720 design, scaled uniformly to fit the target.
    inline constexpr float DesignWidth = 1280.0F;
    inline constexpr float DesignHeight = 720.0F;

    /// A dashed outline's dashes and gaps, in logical units.
    inline constexpr std::array DashPattern{6.0F, 6.0F};

    /// B612 Mono's advance as a share of its size, until the loaded font is measured.
    inline constexpr float DefaultAdvance = 0.6F;

    /// @brief How a shape's outline is drawn.
    struct Pen
    {
        Color color;
        float width{};
        bool dashed{};
        /// Mitered corners, for the symbols' points. Otherwise corners are beveled.
        bool sharp{};
    };

    struct Circle
    {
        Vec2 center;
        float radius{};
    };

    /// @brief Part of a circle, clockwise from a bearing.
    struct Arc
    {
        Vec2 center;
        float radius{};
        /// Degrees clockwise from north.
        float from{};
        float sweep{};
    };

    /// @brief Points joined in order, and back to the first if closed.
    struct Polyline
    {
        std::vector<Vec2> points;
        bool closed{};
    };

    /// @brief Separate lines, each between a pair of points.
    struct Segments
    {
        std::vector<Vec2> ends;
    };

    using Geometry = std::variant<Circle, Arc, Polyline, Segments>;

    struct Shape
    {
        Geometry geometry;
        std::optional<Pen> pen;
        std::optional<Color> fill;
        /// Applies to the shape as a whole.
        float opacity{1.0F};
    };

    struct Text
    {
        std::string text;
        Vec2 at;
        float size{};
        Color color;
        /// The point of the text's box pinned to `at`, from (0, 0) at its top left to (1, 1) at its bottom right.
        Vec2 anchor;
        float opacity{1.0F};
    };

    /// @brief The background: shading from `inner` at `center` to `outer` at `radius` and beyond.
    struct Backdrop
    {
        Vec2 corner;
        float width{};
        float height{};
        Vec2 center;
        float radius{};
        Color inner;
        Color outer;
    };

    /// @brief A horizontal line fading out to the right.
    struct Rule
    {
        Vec2 from;
        float length{};
        float width{};
        Color color;
    };

    struct Item;

    /// @brief Items drawn together: clipped to a circle, or glowing like a phosphor display.
    struct Group
    {
        std::vector<Item> items;
        std::optional<Circle> clip;
        bool glow{};
    };

    struct Item
    {
        std::variant<Shape, Text, Backdrop, Rule, Group> value;
    };

    /// @brief A frame of the scope.
    struct Picture
    {
        /// Target pixels per logical unit.
        float scale{};
        Group root;
    };

    /// @brief What a frame is drawn for: the target, the font, and the readouts on how it's drawn.
    struct View
    {
        std::uint32_t width{};
        std::uint32_t height{};
        /// Whether there's a font. Without one, the picture has no text.
        bool text{};
        /// The font's advance as a share of its size.
        float advance{DefaultAdvance};
        std::string_view caption;
        double milliseconds{};
        double framesPerSecond{};
    };
}

namespace scope
{
    namespace
    {
        constexpr float Half = 0.5F;
        constexpr float HalfTurn = 180.0F;
        constexpr float FullTurn = 360.0F;
        constexpr float Tenths = 10.0F;

        // The outer ring is fixed; the scale ring inside it marks the range, after briarthorn's decision 0168.
        constexpr float OuterRadius = 300.0F;
        constexpr float ScaleRadius = OuterRadius * 0.9F;
        constexpr float LineWidth = 2.0F;
        constexpr float ThinLineWidth = 1.5F;
        constexpr float HairlineWidth = 1.0F;

        constexpr int MinorTicks = 36;
        constexpr float MinorTickLength = 6.0F;
        constexpr float MinorTickOpacity = 0.45F;
        constexpr int HeadingTicks = 12;
        constexpr float HeadingTickLength = 14.0F;
        constexpr float NorthTickLength = 24.0F;
        constexpr float TickLabelPadding = 6.0F;
        constexpr float HeadingLabelSize = 13.0F;
        constexpr float NorthLabelSize = 18.0F;

        // Each ring's label sits 20 degrees right of north, in a gap cut from the ring.
        constexpr float RingLabelBearing = 20.0F;
        constexpr float RingLabelSize = 13.0F;
        constexpr float ScaleLabelSize = 18.0F;
        constexpr float LabelGapPadding = 6.0F;
        constexpr float RangeRingOpacity = 0.55F;
        constexpr float PingWidth = 1.5F;

        // The placeholder symbols of briarthorn's decision 0117: an arrowhead, or a circle with a heading tick.
        constexpr float SymbolSize = 10.0F;
        constexpr float OwnshipSymbolSize = 15.0F;
        constexpr float GenericCircleShare = 0.55F;
        constexpr float ArrowWidthShare = 0.7F;
        constexpr float ArrowNotchShare = 0.5F;
        constexpr float LeaderPerKnot = 0.06F;
        constexpr float HistoryDotRadius = 1.8F;
        constexpr float HistoryOpacity = 0.8F;
        constexpr float DataBlockSize = 12.0F;
        constexpr float DataBlockOffset = 12.0F;

        // A track past the scale ring shows as a caret in the band outside it, at its bearing.
        constexpr float CaretGap = 6.0F;
        constexpr float CaretSize = 10.0F;
        constexpr float CaretWidthShare = 0.6F;

        constexpr float Margin = 32.0F;
        constexpr float TitleSize = 28.0F;
        constexpr float ReadoutSize = 13.0F;
        constexpr float HintSize = 12.0F;
        constexpr float RangeSize = 28.0F;
        constexpr float SubtitleTop = 40.0F;
        constexpr float RuleTop = 64.0F;
        constexpr float RuleLength = 300.0F;
        constexpr float RangeTop = 18.0F;
        constexpr float ModeTop = 54.0F;
        constexpr float TableTop = 104.0F;
        constexpr float RowHeight = 22.0F;
        constexpr float SectionGap = 18.0F;
        constexpr float GlassReach = 1.6F;
        // The backdrop bleeds past the edges, so no edge pixel is antialiased against the target's old contents.
        constexpr float Bleed = 1.0F;

        constexpr Color TitleColor{.r = 0xE4, .g = 0xED, .b = 0xF7};
        constexpr Color GlassColor{.r = 0x15, .g = 0x1E, .b = 0x29};

        // The point of a label's box pinned to its position.
        constexpr Vec2 TopLeft{.x = 0.0F, .y = 0.0F};
        constexpr Vec2 TopRight{.x = 1.0F, .y = 0.0F};
        constexpr Vec2 Middle{.x = 0.5F, .y = 0.5F};
        constexpr Vec2 BottomLeft{.x = 0.0F, .y = 1.0F};
        constexpr Vec2 BottomRight{.x = 1.0F, .y = 1.0F};

        // What a frame is composed from.
        struct Frame
        {
            // The target's size in logical units.
            float width{};
            float height{};
            Vec2 center;
            // Logical units per nautical mile at the current range.
            float unitsPerNm{};
            Palette palette;
            // The range at the scale ring, in nautical miles.
            float range{};
            // The tracks' and the pings' clocks.
            float seconds{};
            float pingSeconds{};
            bool text{};
            float advance{};
            bool glow{};
        };

        // A point a distance from a center, at a bearing.
        struct Polar
        {
            float radius{};
            float bearing{};
        };

        struct RingLabel
        {
            std::string_view text;
            float size{};
            float opacity{};
        };

        struct Symbol
        {
            Glyph glyph{};
            Vec2 at;
            float heading{};
            // From the symbol's center to its nose.
            float size{};
            Color color;
        };

        auto Radians(float degrees) -> float
        {
            return degrees * std::numbers::pi_v<float> / HalfTurn;
        }

        auto Degrees(float radians) -> float
        {
            return radians * HalfTurn / std::numbers::pi_v<float>;
        }

        // On screen, where y grows downward.
        auto At(Vec2 center, Polar x) -> Vec2
        {
            const auto angle = Radians(x.bearing);
            return {.x = center.x + (x.radius * std::sin(angle)), .y = center.y - (x.radius * std::cos(angle))};
        }

        auto ToView(const Frame& frame, Vec2 nm) -> Vec2
        {
            return {.x = frame.center.x + (nm.x * frame.unitsPerNm), .y = frame.center.y - (nm.y * frame.unitsPerNm)};
        }

        auto FormatNm(float nm) -> std::string
        {
            return std::format("{:g}", std::round(nm * Tenths) / Tenths);
        }

        auto AddShape(Group& group, Shape x) -> void
        {
            group.items.emplace_back(Item{std::move(x)});
        }

        auto AddText(Group& group, const Frame& frame, Text x) -> void
        {
            if (frame.text)
            {
                group.items.emplace_back(Item{std::move(x)});
            }
        }

        auto AddGroup(Group& group, Group x) -> void
        {
            group.items.emplace_back(Item{std::move(x)});
        }

        auto Line(Vec2 from, Vec2 to, Pen pen) -> Shape
        {
            return {.geometry = Polyline{.points = {from, to}, .closed = false}, .pen = pen, .fill = std::nullopt, .opacity = 1.0F};
        }

        // A ring in the furniture color with a gap at `RingLabelBearing`, and its label in the gap.
        auto AddLabelledRing(Group& group, const Frame& frame, const RingLabel& label, float radius, Pen pen) -> void
        {
            const auto width = static_cast<float>(std::size(label.text)) * frame.advance * label.size;
            const auto gap = frame.text ? Degrees(((width * Half) + LabelGapPadding) / radius) : 0.0F;
            AddShape(group, {.geometry = Arc{.center = frame.center, .radius = radius, .from = RingLabelBearing + gap, .sweep = FullTurn - gap - gap},
                             .pen = pen,
                             .fill = std::nullopt,
                             .opacity = label.opacity});
            AddText(group, frame,
                    {.text = std::string{label.text},
                     .at = At(frame.center, {.radius = radius, .bearing = RingLabelBearing}),
                     .size = label.size,
                     .color = FurnitureColor,
                     .anchor = Middle,
                     .opacity = label.opacity});
        }

        // The fixed outer ring and the scale ring, with its heading ticks and range.
        auto AddCompass(Group& group, const Frame& frame) -> void
        {
            const auto center = frame.center;
            AddShape(group, {.geometry = Circle{.center = center, .radius = OuterRadius},
                             .pen = Pen{.color = FurnitureColor, .width = LineWidth},
                             .fill = std::nullopt,
                             .opacity = 1.0F});

            Segments minor;
            for (auto i = 0; i < MinorTicks; ++i)
            {
                const auto bearing = FullTurn * static_cast<float>(i) / static_cast<float>(MinorTicks);
                minor.ends.push_back(At(center, {.radius = OuterRadius, .bearing = bearing}));
                minor.ends.push_back(At(center, {.radius = OuterRadius - MinorTickLength, .bearing = bearing}));
            }
            AddShape(group, {.geometry = std::move(minor),
                             .pen = Pen{.color = FurnitureColor, .width = HairlineWidth},
                             .fill = std::nullopt,
                             .opacity = MinorTickOpacity});

            AddLabelledRing(group, frame, {.text = std::format("{:.0f}", frame.range), .size = ScaleLabelSize, .opacity = 1.0F}, ScaleRadius,
                            {.color = FurnitureColor, .width = LineWidth});

            Segments ticks;
            std::vector<Text> labels;
            for (auto i = 0; i < HeadingTicks; ++i)
            {
                const auto north = i == 0;
                const auto bearing = FullTurn * static_cast<float>(i) / static_cast<float>(HeadingTicks);
                const auto length = north ? NorthTickLength : HeadingTickLength;
                const auto size = north ? NorthLabelSize : HeadingLabelSize;
                ticks.ends.push_back(At(center, {.radius = ScaleRadius, .bearing = bearing}));
                ticks.ends.push_back(At(center, {.radius = ScaleRadius - length, .bearing = bearing}));
                labels.push_back({.text = north ? std::string{"N"} : std::format("{:.0f}", bearing),
                                  .at = At(center, {.radius = ScaleRadius - length - TickLabelPadding - (size * Half), .bearing = bearing}),
                                  .size = size,
                                  .color = FurnitureColor,
                                  .anchor = Middle,
                                  .opacity = 1.0F});
            }

            AddShape(group,
                     {.geometry = std::move(ticks), .pen = Pen{.color = FurnitureColor, .width = LineWidth}, .fill = std::nullopt, .opacity = 1.0F});

            for (auto& label : labels)
            {
                AddText(group, frame, std::move(label));
            }
        }

        // The dashed range rings, which grow and shrink as the range changes.
        auto AddRangeRings(Group& group, const Frame& frame) -> void
        {
            for (const auto& ring : RangeRings(frame.range))
            {
                AddLabelledRing(group, frame, {.text = FormatNm(ring.nm), .size = RingLabelSize, .opacity = ring.opacity * RangeRingOpacity},
                                ring.nm * frame.unitsPerNm, {.color = FurnitureColor, .width = ThinLineWidth, .dashed = true});
            }
        }

        // The rings spreading out from ownship.
        auto AddPings(Group& group, const Frame& frame) -> void
        {
            for (const auto& ping : Pings(frame.pingSeconds))
            {
                AddShape(group, {.geometry = Circle{.center = frame.center, .radius = ping.reach * ScaleRadius},
                                 .pen = Pen{.color = frame.palette.ownship, .width = PingWidth},
                                 .fill = std::nullopt,
                                 .opacity = ping.opacity});
            }
        }

        // A track's symbol, pointing along its heading.
        auto AddSymbol(Group& group, const Symbol& x) -> void
        {
            const auto angle = Radians(x.heading);
            const auto cosine = std::cos(angle);
            const auto sine = std::sin(angle);
            const Pen pen{.color = x.color, .width = LineWidth, .sharp = true};

            // Turns a point of the symbol drawn pointing up, about its center, then moves it to the track.
            const auto place = [&](Vec2 p) -> Vec2
            { return {.x = x.at.x + (p.x * cosine) - (p.y * sine), .y = x.at.y + (p.x * sine) + (p.y * cosine)}; };

            if (x.glyph == Glyph::Circle)
            {
                const auto radius = x.size * GenericCircleShare;
                AddShape(group, {.geometry = Circle{.center = x.at, .radius = radius}, .pen = pen, .fill = std::nullopt, .opacity = 1.0F});
                AddShape(group, Line(place({.x = 0.0F, .y = -radius}), place({.x = 0.0F, .y = -x.size}), pen));
                return;
            }

            AddShape(group, {.geometry = Polyline{.points = {place({.x = 0.0F, .y = -x.size}), place({.x = x.size * ArrowWidthShare, .y = x.size}),
                                                             place({.x = 0.0F, .y = x.size * ArrowNotchShare}),
                                                             place({.x = -x.size * ArrowWidthShare, .y = x.size})},
                                                  .closed = true},
                             .pen = pen,
                             .fill = x.glyph == Glyph::Filled ? std::optional{x.color} : std::nullopt,
                             .opacity = 1.0F});
        }

        // The tracks inside the scale ring: their history dots, leader lines showing their speed, and symbols, each
        // in a glowing group of its own.
        auto AddTracks(Group& group, const Frame& frame) -> void
        {
            for (const auto& track : Tracks)
            {
                const auto state = Sample(track.route, frame.seconds);

                if (RangeOf(state.position) > frame.range)
                {
                    continue;
                }

                const auto color = frame.palette.colorOf(track.classification);
                Group drawn{.items = {}, .clip = std::nullopt, .glow = frame.glow};
                const auto dots = History(track.route, frame.seconds);

                for (std::size_t i = 0; i < std::size(dots); ++i)
                {
                    AddShape(drawn, {.geometry = Circle{.center = ToView(frame, dots.at(i)), .radius = HistoryDotRadius},
                                     .pen = std::nullopt,
                                     .fill = color,
                                     .opacity = HistoryOpacity * (1.0F - (static_cast<float>(i) / static_cast<float>(std::size(dots))))});
                }

                const auto at = ToView(frame, state.position);
                const auto nose = At(at, {.radius = SymbolSize, .bearing = state.heading});
                AddShape(drawn, Line(nose, At(nose, {.radius = state.knots * LeaderPerKnot, .bearing = state.heading}),
                                     {.color = color, .width = ThinLineWidth}));
                AddSymbol(drawn, {.glyph = track.glyph, .at = at, .heading = state.heading, .size = SymbolSize, .color = color});
                AddGroup(group, std::move(drawn));
            }
        }

        // Over the tracks, unclipped: their data blocks, or a caret for each past the scale ring.
        auto AddTrackLabels(Group& group, const Frame& frame) -> void
        {
            const auto base = ScaleRadius + CaretGap;
            const auto spread = Degrees(CaretSize * CaretWidthShare / base);

            for (const auto& track : Tracks)
            {
                const auto state = Sample(track.route, frame.seconds);
                const auto color = frame.palette.colorOf(track.classification);

                if (RangeOf(state.position) <= frame.range)
                {
                    const auto at = ToView(frame, state.position);
                    AddText(group, frame,
                            {.text = std::format("{}\n{:.0f}", track.callsign, state.knots),
                             .at = {.x = at.x + DataBlockOffset, .y = at.y - DataBlockOffset},
                             .size = DataBlockSize,
                             .color = color,
                             .anchor = BottomLeft,
                             .opacity = 1.0F});
                    continue;
                }

                const auto bearing = BearingOf(state.position);
                AddShape(group, {.geometry = Polyline{.points = {At(frame.center, {.radius = base + CaretSize, .bearing = bearing}),
                                                                 At(frame.center, {.radius = base, .bearing = bearing + spread}),
                                                                 At(frame.center, {.radius = base, .bearing = bearing - spread})},
                                                      .closed = true},
                                 .pen = std::nullopt,
                                 .fill = color,
                                 .opacity = 1.0F});
            }
        }

        auto Readout(std::string text, Vec2 at, Color color, Vec2 anchor) -> Text
        {
            return {.text = std::move(text), .at = at, .size = ReadoutSize, .color = color, .anchor = anchor, .opacity = 1.0F};
        }

        // The title, the range readout, the track table, and the controls.
        auto AddReadouts(Group& group, const Frame& frame, const Scope& scope, const View& view) -> void
        {
            const auto right = frame.width - Margin;
            const auto bottom = frame.height - Margin;
            const auto& range = scope.getRange();

            AddText(group, frame,
                    {.text = "SCOPE", .at = {.x = Margin, .y = Margin}, .size = TitleSize, .color = TitleColor, .anchor = TopLeft, .opacity = 1.0F});
            AddText(group, frame, Readout(std::string{view.caption}, {.x = Margin, .y = Margin + SubtitleTop}, FurnitureColor, TopLeft));
            group.items.emplace_back(
                Item{Rule{.from = {.x = Margin, .y = Margin + RuleTop}, .length = RuleLength, .width = LineWidth, .color = FurnitureColor}});

            AddText(group, frame, Readout("RANGE", {.x = right, .y = Margin}, FurnitureColor, TopRight));
            AddText(group, frame,
                    {.text = std::format("{:.0f} NM", range.target()),
                     .at = {.x = right, .y = Margin + RangeTop},
                     .size = RangeSize,
                     .color = TitleColor,
                     .anchor = TopRight,
                     .opacity = 1.0F});
            AddText(group, frame,
                    Readout(range.getCycling() ? "AUTO" : "MANUAL", {.x = right, .y = Margin + ModeTop},
                            range.getCycling() ? frame.palette.ownship : FurnitureColor, TopRight));

            auto row = Margin + TableTop;
            AddText(group, frame,
                    Readout(std::format("{:<9} {:>7} {:>4}", "TRACK", "BRG/RNG", "KT"), {.x = right, .y = row}, FurnitureColor, TopRight));

            for (const auto& track : Tracks)
            {
                row += RowHeight;
                const auto state = Sample(track.route, frame.seconds);
                const auto bearing = std::lround(BearingOf(state.position)) % static_cast<long>(FullTurn);
                AddText(group, frame,
                        Readout(std::format("{:<9} {:03}/{:<3.0f} {:>4.0f}", track.callsign, bearing, RangeOf(state.position), state.knots),
                                {.x = right, .y = row}, frame.palette.colorOf(track.classification), TopRight));
            }

            // How it's drawn, in a block under the tracks, padded to the same width.
            row += SectionGap;
            const std::array<std::pair<std::string_view, std::string>, 5> details{{
                {"PALETTE", std::string{frame.palette.name}},
                {"GLOW", scope.getGlow() ? "ON" : "OFF"},
                {"PIXELS", std::format("{}x{}", view.width, view.height)},
                {"PAINT", std::format("{:.1f} MS", view.milliseconds)},
                {"RATE", std::format("{:.0f} FPS", view.framesPerSecond)},
            }};

            for (const auto& [name, value] : details)
            {
                row += RowHeight;
                AddText(group, frame, Readout(std::format("{:<8}{:>14}", name, value), {.x = right, .y = row}, FurnitureColor, TopRight));
            }

            AddText(group, frame,
                    {.text = "UP/DOWN WHEEL  RANGE    A  AUTO    P  PALETTE    G  GLOW    SPACE  PAUSE    ESC  QUIT",
                     .at = {.x = Margin, .y = bottom},
                     .size = HintSize,
                     .color = FurnitureColor,
                     .anchor = BottomLeft,
                     .opacity = 1.0F});

            if (scope.getPaused())
            {
                AddText(group, frame, Readout("PAUSED", {.x = right, .y = bottom}, frame.palette.ownship, BottomRight));
            }
        }
    }
}

export namespace scope
{
    /// @brief Lays out a frame of the scope.
    /// @param scope The scope to draw.
    /// @param view What it's drawn for.
    /// @return The picture, empty if the view has no pixels.
    [[nodiscard]] auto Compose(const Scope& scope, const View& view) -> Picture
    {
        if (view.width == 0 || view.height == 0)
        {
            return {};
        }

        const auto scale = std::min(static_cast<float>(view.width) / DesignWidth, static_cast<float>(view.height) / DesignHeight);
        const auto range = scope.getRange().range();
        const auto width = static_cast<float>(view.width) / scale;
        const auto height = static_cast<float>(view.height) / scale;
        const Frame frame{.width = width,
                          .height = height,
                          .center = {.x = width * Half, .y = height * Half},
                          .unitsPerNm = ScaleRadius / range,
                          .palette = scope.getPalette(),
                          .range = range,
                          .seconds = scope.getSeconds(),
                          .pingSeconds = scope.getPingSeconds(),
                          .text = view.text,
                          .advance = view.advance,
                          .glow = scope.getGlow()};

        Picture picture{.scale = scale, .root = {}};
        auto& root = picture.root;
        root.items.emplace_back(Item{Backdrop{.corner = {.x = -Bleed, .y = -Bleed},
                                              .width = width + Bleed + Bleed,
                                              .height = height + Bleed + Bleed,
                                              .center = frame.center,
                                              .radius = OuterRadius * GlassReach,
                                              .inner = GlassColor,
                                              .outer = BackgroundColor}});

        Group display;
        AddCompass(display, frame);

        // Rings and tracks inside the scale ring are clipped to it, so they slide out of view at its edge.
        Group inside{.items = {}, .clip = Circle{.center = frame.center, .radius = ScaleRadius}, .glow = false};
        AddRangeRings(inside, frame);
        AddPings(inside, frame);
        AddTracks(inside, frame);
        AddGroup(display, std::move(inside));
        AddTrackLabels(display, frame);

        Group ownship{.items = {}, .clip = std::nullopt, .glow = frame.glow};
        AddSymbol(ownship, {.glyph = Glyph::Filled, .at = frame.center, .heading = 0.0F, .size = OwnshipSymbolSize, .color = frame.palette.ownship});
        AddGroup(display, std::move(ownship));
        AddGroup(root, std::move(display));

        AddReadouts(root, frame, scope, view);
        return picture;
    }
}
