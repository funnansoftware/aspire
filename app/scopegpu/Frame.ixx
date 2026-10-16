export module scopegpu.frame;

import std;
import aspire.core;
import scope.glow;
import scope.mesh;
import scope.picture;
import scope.tessellation;
import scope.world;

// A frame of the scope laid out for the GPU: the same triangles scope draws, and what's drawn in what order.
export namespace scopegpu
{
    using scope::Vec2;

    /// @brief Which part of a mark a corner of the marks' mesh belongs to, as `mark.vert` numbers them.
    enum class MarkPart : std::uint8_t
    {
        /// The history dots, newest first, are the first six.
        Dot = 0,
        LeaderNear = 6,
        LeaderFar = 7,
        /// A generic track's circle.
        Ring = 8,
        /// A generic track's heading tick.
        TickNear = 9,
        TickFar = 10,
        /// A typed track's arrowhead.
        Outline = 11,
        /// An identified track's arrowhead.
        Filled = 12,
    };

    /// @brief A corner of the marks' mesh: a mark at the origin pointing north, in target pixels.
    struct MarkVertex
    {
        Vec2 position;
        /// The corner's alpha, which the track's color is drawn at.
        float alpha{};
        /// For a leader's or tick's end, how far that end is from the mark's center in logical units, which the
        /// track moves it from.
        float along{};
        std::uint32_t part{};
    };

    /// @brief A mark's mesh at one scale, with every glyph's parts: a track draws only its own.
    struct MarkMesh
    {
        std::vector<MarkVertex> vertices;
        std::vector<std::uint32_t> indices;
    };

    /// @brief A track's mark as the GPU draws it: an instance of the marks' mesh, in logical units, with the route its
    /// dots are sampled from.
    struct MarkInstance
    {
        /// Where the track is, its heading in degrees, and how far along its route it is, within one lap: an orbit's
        /// bearing from its center, an eight's phase in radians, or a transit's miles flown.
        std::array<float, 4> place{};
        /// Its straight color, and in the fourth, how far its leader reaches without the scale ring cutting it.
        std::array<float, 4> color{};
        /// How far its leader and then its tick start and end along its heading, as the scale ring cuts them.
        std::array<float, 4> segments{};
        /// Its route, in nautical miles and degrees: an orbit's center and radius; a figure of eight's center, axis
        /// bearing and half its length; or a transit's start, heading and span.
        std::array<float, 4> route{};
        /// How fast it goes along its route: an orbit's degrees, an eight's radians, or a transit's miles, a second.
        float pace{};
        /// Whether the scale ring keeps its symbol (bit 6), its leader (bit 7) and its tick (bit 8), and from bit 9
        /// its glyph, and from bit 11 its route.
        std::uint32_t flags{};
    };

    /// @brief How the marks' dots are placed: from nautical miles to logical units, how far apart in time, and the
    /// scale ring that keeps or drops them, if it clips them.
    struct MarkView
    {
        /// Where ownship is, in logical units.
        Vec2 center;
        float unitsPerNm{};
        /// Scope seconds from one dot to the next.
        float interval{};
        /// The scale ring, in target pixels.
        std::optional<scope::Clip> clip;
    };

    /// @brief The tracks' marks, drawn as instances of the marks' mesh in the main pass.
    struct MarksRun
    {
        std::uint32_t firstInstance{};
        std::uint32_t instanceCount{};
    };

    /// @brief Triangles from the frame's mesh, drawn in the main pass.
    struct ShapeRun
    {
        /// The first of the mesh's indices drawn.
        std::uint32_t firstIndex{};
        std::uint32_t indexCount{};
    };

    /// @brief A text, drawn in the main pass at the picture's scale.
    struct TextRun
    {
        const scope::Text* text{};
    };

    /// @brief The backdrop with the glow over it, drawn over the whole target.
    struct BackdropRun
    {
        const scope::Backdrop* backdrop{};
    };

    /// @brief Data blocks, drawn in the main pass at the picture's scale as instances of a glyph's quad.
    struct BlocksRun
    {
        const scope::DataBlocks* blocks{};
    };

    /// @brief Something drawn in the main pass.
    using Run = std::variant<ShapeRun, TextRun, BackdropRun, MarksRun, BlocksRun>;
}

namespace scopegpu
{
    // How far a mark's leader and tick start and end along its heading, and where its arrowheads' middles are, as
    // `scope::AddMark()` draws them at the origin pointing north. Logical units. Unlike the rest below, this isn't
    // private to the file: `Frame` keeps one, and GCC won't let an exported class name what only this file can see.
    // NOLINTNEXTLINE(misc-use-internal-linkage)
    struct MarkShape
    {
        float leaderNear{};
        float tickNear{};
        float tickFar{};
        Vec2 outlineMiddle;
        Vec2 filledMiddle;
    };

    namespace
    {
        constexpr std::uint32_t KeptSymbol = 1U << 6U;
        constexpr std::uint32_t KeptLeader = 1U << 7U;
        constexpr std::uint32_t KeptTick = 1U << 8U;
        constexpr std::uint32_t KeptAll = KeptSymbol | KeptLeader | KeptTick;
        constexpr std::uint32_t GlyphShift = 9U;
        constexpr std::uint32_t RouteShift = 11U;
        constexpr float HalfTurn = 180.0F;
        constexpr float FullTurn = 360.0F;
        constexpr double FullTurnRadians = 2.0 * std::numbers::pi;
        constexpr float SecondsPerHour = 3600.0F;

        // A mark at the origin pointing north: the one the mesh is made from.
        constexpr scope::Mark Canonical{.glyph = scope::Glyph::Circle,
                                        .color = {.r = scope::Color::Opaque, .g = scope::Color::Opaque, .b = scope::Color::Opaque},
                                        .at = {},
                                        .heading = 0.0F,
                                        // Long enough that the leader's ends are told apart at any scale.
                                        .leader = 100.0F,
                                        .dots = {},
                                        .route = {}};

        // A canonical mark's shapes: its dots, its leader, and its symbol's parts.
        auto ShapesOf(scope::Glyph glyph) -> std::vector<scope::Shape>
        {
            scope::Group group;
            auto mark = Canonical;
            mark.glyph = glyph;
            scope::AddMark(group, mark);
            std::vector<scope::Shape> shapes;

            for (const auto& item : group.items)
            {
                shapes.push_back(std::get<scope::Shape>(item.value));
            }

            return shapes;
        }

        // A line's points, or none if the shape isn't one.
        auto PointsOf(const scope::Shape& x) -> std::span<const Vec2>
        {
            const auto* line = std::get_if<scope::Polyline>(&x.geometry);
            return line != nullptr ? std::span{line->points} : std::span<const Vec2>{};
        }

        auto MiddleOf(std::span<const Vec2> points) -> Vec2
        {
            Vec2 sum{};

            for (const auto& p : points)
            {
                sum = {.x = sum.x + p.x, .y = sum.y + p.y};
            }

            const auto count = static_cast<float>(std::max<std::size_t>(std::size(points), 1));
            return {.x = sum.x / count, .y = sum.y / count};
        }

        auto MarkShapeOf() -> MarkShape
        {
            const auto circle = ShapesOf(scope::Glyph::Circle);
            // Past the dots: the leader, then the symbol's parts.
            constexpr auto leader = scope::HistoryCount;
            const auto tick = PointsOf(circle.at(leader + 2));
            return {.leaderNear = -PointsOf(circle.at(leader)).front().y,
                    .tickNear = -tick.front().y,
                    .tickFar = -tick.back().y,
                    .outlineMiddle = MiddleOf(PointsOf(ShapesOf(scope::Glyph::Outline).at(leader + 1))),
                    .filledMiddle = MiddleOf(PointsOf(ShapesOf(scope::Glyph::Filled).at(leader + 1)))};
        }

        // Adds a shape's triangles to the marks' mesh as a part, or for a leader or tick, as its two ends' parts.
        auto AddPart(MarkMesh& mesh, const scope::Shape& shape, const scope::Style& style, MarkPart part) -> void
        {
            scope::Mesh piece;
            scope::Tessellate(piece, shape, style);
            const auto first = static_cast<std::uint32_t>(std::size(mesh.vertices));
            const auto points = PointsOf(shape);
            const auto segment = part == MarkPart::LeaderNear || part == MarkPart::TickNear;

            for (const auto& vertex : piece.vertices())
            {
                MarkVertex corner{.position = vertex.position, .alpha = vertex.tint.a, .along = 0.0F, .part = static_cast<std::uint32_t>(part)};

                // A line's corners are at its ends, past them by no more than its width: each goes with the end it's
                // nearer, north being up the target.
                if (segment)
                {
                    const auto near = -points.front().y;
                    const auto far = -points.back().y;
                    const auto reach = -vertex.position.y / style.scale;
                    const auto atNear = std::abs(reach - near) < std::abs(reach - far);
                    corner.along = atNear ? near : far;
                    corner.part = static_cast<std::uint32_t>(part) + (atNear ? 0U : 1U);
                }

                mesh.vertices.push_back(corner);
            }

            for (const auto index : piece.indices())
            {
                mesh.indices.push_back(first + static_cast<std::uint32_t>(index));
            }
        }

        // Makes the marks' mesh at a style's scale: the dots and leader every track has, then each glyph's parts, in the
        // order `scope::AddMark()` draws them.
        auto MakeMarkMesh(MarkMesh& mesh, const scope::Style& style) -> void
        {
            mesh.vertices.clear();
            mesh.indices.clear();
            const auto circle = ShapesOf(scope::Glyph::Circle);
            constexpr auto leader = scope::HistoryCount;

            for (std::size_t i = 0; i < leader; ++i)
            {
                AddPart(mesh, circle.at(i), style, static_cast<MarkPart>(i));
            }

            AddPart(mesh, circle.at(leader), style, MarkPart::LeaderNear);
            AddPart(mesh, circle.at(leader + 1), style, MarkPart::Ring);
            AddPart(mesh, circle.at(leader + 2), style, MarkPart::TickNear);
            AddPart(mesh, ShapesOf(scope::Glyph::Outline).at(leader + 1), style, MarkPart::Outline);
            AddPart(mesh, ShapesOf(scope::Glyph::Filled).at(leader + 1), style, MarkPart::Filled);
        }

        auto Inside(const scope::Clip& clip, Vec2 x) -> bool
        {
            return std::hypot(x.x - clip.center.x, x.y - clip.center.y) <= clip.radius;
        }

        auto Scaled(Vec2 x, float scale) -> Vec2
        {
            return {.x = x.x * scale, .y = x.y * scale};
        }

        // Where a line from one point to another runs inside a clip, as shares of the way along it, as
        // `scope::ClipToCircle()` cuts it. Empty if it's all outside.
        auto Cut(Vec2 from, Vec2 to, const scope::Clip& clip) -> std::optional<std::pair<float, float>>
        {
            const Vec2 d{.x = to.x - from.x, .y = to.y - from.y};
            const Vec2 f{.x = from.x - clip.center.x, .y = from.y - clip.center.y};
            const auto a = (d.x * d.x) + (d.y * d.y);
            const auto b = (f.x * d.x) + (f.y * d.y);
            const auto c = (f.x * f.x) + (f.y * f.y) - (clip.radius * clip.radius);
            const auto discriminant = (b * b) - (a * c);

            if (a == 0.0F || discriminant <= 0.0F)
            {
                return std::nullopt;
            }

            const auto root = std::sqrt(discriminant);
            const auto enter = std::max(0.0F, (-b - root) / a);
            const auto leave = std::min(1.0F, (-b + root) / a);
            return enter < leave ? std::optional{std::pair{enter, leave}} : std::nullopt;
        }

        // A point a distance from another along a heading, given as its sine and cosine, as the picture places it.
        auto Along(Vec2 from, float distance, float sine, float cosine) -> Vec2
        {
            return {.x = from.x + (distance * sine), .y = from.y - (distance * cosine)};
        }

        // A route as mark.vert samples it: four of its parameters, how fast it goes, and how far along it the track
        // is at a time, as a bearing, a phase or miles flown, within one lap. See `MarkInstance`.
        //
        // How far along is worked out in double precision and rounded once, and the GPU steps back from it to each dot,
        // so the dots are as exact on a scope that's run for days as on one just started: a float clock times a speed
        // loses more of the position with every lap.
        struct RouteRecord
        {
            std::array<float, 4> route{};
            float pace{};
            float progress{};
        };

        // Wraps a value into [0, period), as scope's routes wrap theirs.
        auto Lapped(double x, double period) -> float
        {
            const auto wrapped = std::fmod(x, period);
            const auto lapped = static_cast<float>(wrapped < 0.0 ? wrapped + period : wrapped);
            return std::isfinite(lapped) ? lapped : 0.0F;
        }

        auto RouteOf(const scope::Route& x, float seconds) -> RouteRecord
        {
            constexpr double secondsPerHour = SecondsPerHour;
            const auto clock = static_cast<double>(seconds);

            return std::visit(
                aspire::core::Overloaded{
                    [clock](const scope::Orbit& orbit)
                    {
                        const auto turn = static_cast<double>(orbit.clockwise ? 1.0F : -1.0F);
                        const auto degrees = static_cast<double>(orbit.knots) / secondsPerHour / static_cast<double>(orbit.radius) * HalfTurn
                                             / std::numbers::pi * turn;
                        return RouteRecord{.route = {orbit.center.x, orbit.center.y, orbit.radius, 0.0F},
                                           .pace = static_cast<float>(degrees),
                                           .progress = Lapped(static_cast<double>(orbit.startBearing) + (degrees * clock), FullTurn)};
                    },
                    [clock](const scope::FigureEight& eight)
                    {
                        constexpr float half = 0.5F;
                        const auto rate = FullTurnRadians / static_cast<double>(eight.seconds);
                        return RouteRecord{.route = {eight.center.x, eight.center.y, eight.bearing, eight.length * half},
                                           .pace = static_cast<float>(rate),
                                           .progress = Lapped(rate * clock, FullTurnRadians)};
                    },
                    [clock](const scope::Transit& transit)
                    {
                        const auto speed = static_cast<double>(transit.knots) / secondsPerHour;
                        return RouteRecord{.route = {transit.start.x, transit.start.y, transit.heading, transit.span},
                                           .pace = static_cast<float>(speed),
                                           .progress = Lapped(speed * clock, transit.span)};
                    },
                },
                x);
        }

        // A mark as an instance of the marks' mesh, with the parts a style's clip keeps, and its lines cut where they
        // leave it, as `scope::Tessellate()` keeps and cuts them. Its dots, the GPU samples from its route at `seconds`,
        // and keeps or drops itself.
        auto InstanceOf(const scope::Mark& x, const MarkShape& shape, const scope::Style& style, float seconds) -> MarkInstance
        {
            const auto tint = scope::TintOf(x.color, 1.0F);
            const auto leaderFar = shape.leaderNear + x.leader;
            const auto route = RouteOf(x.route, seconds);
            MarkInstance instance{
                .place = {x.at.x, x.at.y, x.heading, route.progress},
                .color = {tint.r, tint.g, tint.b, leaderFar},
                .segments = {shape.leaderNear, leaderFar, shape.tickNear, shape.tickFar},
                .route = route.route,
                .pace = route.pace,
                .flags = (static_cast<std::uint32_t>(x.glyph) << GlyphShift) | (static_cast<std::uint32_t>(x.route.index()) << RouteShift)};

            if (!style.clip.has_value())
            {
                instance.flags |= KeptAll;
                return instance;
            }

            // A mark well inside the clip keeps every part whole: its leader and tick reach furthest from the track,
            // and its arrowheads' middles less far.
            const auto& clip = *style.clip;
            const auto at = Scaled(x.at, style.scale);

            if (std::hypot(at.x - clip.center.x, at.y - clip.center.y) + (std::max(leaderFar, shape.tickFar) * style.scale) + 1.0F < clip.radius)
            {
                instance.flags |= KeptAll;
                return instance;
            }

            const auto angle = x.heading * std::numbers::pi_v<float> / HalfTurn;
            const auto cosine = std::cos(angle);
            const auto sine = std::sin(angle);

            // A closed shape's kept or dropped whole by its middle: a circle's is its center, and an arrowhead's is
            // turned with it.
            auto middle = Vec2{};

            if (x.glyph == scope::Glyph::Outline)
            {
                middle = shape.outlineMiddle;
            }
            else if (x.glyph == scope::Glyph::Filled)
            {
                middle = shape.filledMiddle;
            }

            const Vec2 turned{.x = x.at.x + ((middle.x * cosine) - (middle.y * sine)), .y = x.at.y + ((middle.x * sine) + (middle.y * cosine))};
            instance.flags |= Inside(clip, Scaled(turned, style.scale)) ? KeptSymbol : 0U;

            // Lines are cut where they leave the clip: how far along the heading a line's ends reach once it's cut,
            // or none if it's all outside. Their ends are placed as the picture places them: the leader's from the
            // symbol's nose, and the tick's from the track.
            const auto cut = [&](std::pair<Vec2, Vec2> ends, std::pair<float, float> reach) -> std::optional<std::pair<float, float>>
            {
                const auto inside = Cut(Scaled(ends.first, style.scale), Scaled(ends.second, style.scale), clip);

                if (!inside.has_value())
                {
                    return std::nullopt;
                }

                const auto [near, far] = reach;
                return std::pair{near + (inside->first * (far - near)), near + (inside->second * (far - near))};
            };

            const auto nose = Along(x.at, shape.leaderNear, sine, cosine);

            if (const auto leader = cut({nose, Along(nose, x.leader, sine, cosine)}, {shape.leaderNear, leaderFar}); leader.has_value())
            {
                instance.flags |= KeptLeader;
                std::tie(instance.segments.at(0), instance.segments.at(1)) = *leader;
            }

            if (const auto tick =
                    cut({Along(x.at, shape.tickNear, sine, cosine), Along(x.at, shape.tickFar, sine, cosine)}, {shape.tickNear, shape.tickFar});
                tick.has_value())
            {
                instance.flags |= KeptTick;
                std::tie(instance.segments.at(2), instance.segments.at(3)) = *tick;
            }

            return instance;
        }

        // The tracks' marks, wherever they are in a group.
        // NOLINTNEXTLINE(misc-no-recursion)
        auto FindMarks(const scope::Group& group) -> const scope::Marks*
        {
            for (const auto& item : group.items)
            {
                if (const auto* marks = std::get_if<scope::Marks>(&item.value); marks != nullptr)
                {
                    return marks;
                }

                if (const auto* inner = std::get_if<scope::Group>(&item.value); inner != nullptr)
                {
                    if (const auto* marks = FindMarks(*inner); marks != nullptr)
                    {
                        return marks;
                    }
                }
            }

            return nullptr;
        }
    }
}

export namespace scopegpu
{

    /// @brief A frame of the scope, laid out for the GPU.
    ///
    /// The shapes are tessellated as scope tessellates them, into one mesh for the frame and one for the glow. The
    /// main pass draws the frame's runs in the picture's order, so shapes and text overlap as they do in scope. The
    /// glow is tessellated once with all its channels, which the GPU splits into full and fainter as it draws them,
    /// and on a thread of its own while the frame's mesh is tessellated, since every glowing shape is tessellated twice.
    ///
    /// A picture that holds its tracks' marks, rather than their shapes, has them drawn as instances of one mesh that
    /// stays the same from frame to frame: a mark at the origin pointing north, with every glyph's parts, tessellated
    /// as scope tessellates them. Each frame only says where each mark is, which of its parts the scale ring keeps,
    /// and the route the GPU samples its dots from.
    ///
    /// A frame is reused from one to the next, so its meshes keep their memory.
    class Frame
    {
    public:
        /// @brief Lays out a picture.
        /// @param picture The picture. The frame's runs point into it, so it must outlive their use.
        /// @param width The target's width in pixels.
        /// @param height The target's height in pixels.
        auto build(const scope::Picture& picture, int width, int height) -> void
        {
            mesh_.clear();
            glowMesh_.clear();
            runs_.clear();
            glowShapes_.clear();
            glow_.reset();
            markInstances_.clear();
            marksGlow_ = false;
            runStart_ = 0;
            scale_ = picture.scale;

            if (!(picture.scale > 0.0F))
            {
                return;
            }

            scope::CollectGlow(picture.root, glowShapes_);
            const auto* marks = FindMarks(picture.root);
            const auto glowingMarks = marks != nullptr && marks->glow && !std::empty(marks->marks);

            // The glow's mesh is tessellated alongside the frame's. The two only read the picture, and each fills a
            // mesh of its own.
            std::future<void> glowing;

            if (!std::empty(glowShapes_) || glowingMarks)
            {
                glow_ = scope::PlanGlow(picture.scale, width, height);
                const scope::Style style{.scale = glow_->scale, .clip = std::nullopt, .widen = 0.0F, .opacity = scope::GlowOpacity};
                glowing = std::async(std::launch::async, [this, style] { tessellateGlow(style); });
            }

            addGroup(picture.root, {.scale = picture.scale, .clip = std::nullopt, .widen = 0.0F, .opacity = 1.0F});
            closeShapes();

            // Rethrows anything the glow's thread threw, once it's done.
            if (glowing.valid())
            {
                glowing.get();
            }
        }

        /// @brief The main pass's triangles, in target pixels.
        /// @return The mesh, until the next `build()`.
        [[nodiscard]] auto mesh() const -> const scope::Mesh&
        {
            return mesh_;
        }

        /// @brief The glowing shapes' triangles, in the glow textures' pixels, with all their channels.
        /// @return The mesh, until the next `build()`.
        [[nodiscard]] auto glowMesh() const -> const scope::Mesh&
        {
            return glowMesh_;
        }

        /// @brief What the main pass draws, in order.
        /// @return The runs, until the next `build()`. They point into the picture.
        [[nodiscard]] auto runs() const -> const std::vector<Run>&
        {
            return runs_;
        }

        /// @brief How the glow is drawn.
        /// @return The plan, empty if nothing glows.
        [[nodiscard]] auto glow() const -> const std::optional<scope::GlowPlan>&
        {
            return glow_;
        }

        /// @brief Reports the picture's scale.
        /// @return Target pixels per logical unit.
        [[nodiscard]] auto scale() const -> float
        {
            return scale_;
        }

        /// @brief The tracks' marks, as instances of the marks' mesh, for the main pass and the glow alike.
        /// @return The instances, until the next `build()`: none unless the picture holds its marks.
        [[nodiscard]] auto markInstances() const -> const std::vector<MarkInstance>&
        {
            return markInstances_;
        }

        /// @brief Reports whether the tracks' marks glow.
        /// @return `true` if they're drawn into the glow too.
        [[nodiscard]] auto marksGlow() const -> bool
        {
            return marksGlow_;
        }

        /// @brief Reports how the marks' dots are placed from their routes, and kept.
        /// @return The view, until the next `build()`.
        [[nodiscard]] auto markView() const -> const MarkView&
        {
            return markView_;
        }

        /// @brief The marks' mesh at the picture's scale.
        /// @return The mesh, which only changes when `markGeneration()` does.
        [[nodiscard]] auto markMesh() const -> const MarkMesh&
        {
            return markMesh_;
        }

        /// @brief The marks' mesh at the glow's scale.
        /// @return The mesh, which only changes when `markGeneration()` does; empty if nothing's glowed since.
        [[nodiscard]] auto markGlowMesh() const -> const MarkMesh&
        {
            return markGlowMesh_;
        }

        /// @brief Reports which make of the marks' meshes this is, so a painter knows when to upload them again.
        /// @return A count of the times they've been made, 0 until they first are.
        [[nodiscard]] auto markGeneration() const -> std::uint32_t
        {
            return markGeneration_;
        }

    private:
        // Makes the marks' meshes again if the picture's or the glow's scale changed since they were last made.
        auto prepareMarkMeshes() -> void
        {
            const auto glowScale = glow_.has_value() ? glow_->scale : markGlowScale_;

            if (markGeneration_ > 0 && markScale_ == scale_ && markGlowScale_ == glowScale)
            {
                return;
            }

            markShape_ = MarkShapeOf();
            markScale_ = scale_;
            markGlowScale_ = glowScale;
            MakeMarkMesh(markMesh_, {.scale = scale_, .clip = std::nullopt, .widen = 0.0F, .opacity = 1.0F});
            markGlowMesh_ = {};

            if (glowScale > 0.0F)
            {
                MakeMarkMesh(markGlowMesh_, {.scale = glowScale, .clip = std::nullopt, .widen = 0.0F, .opacity = scope::GlowOpacity});
            }

            ++markGeneration_;
        }

        // Adds the tracks' marks as a run of instances, kept and cut by the style's clip.
        auto addMarks(const scope::Marks& x, const scope::Style& style) -> void
        {
            closeShapes();
            prepareMarkMeshes();
            marksGlow_ = x.glow;
            markView_ = {.center = x.center, .unitsPerNm = x.unitsPerNm, .interval = scope::HistoryInterval, .clip = style.clip};
            const auto first = static_cast<std::uint32_t>(std::size(markInstances_));

            for (const auto& mark : x.marks)
            {
                markInstances_.push_back(InstanceOf(mark, markShape_, style, x.seconds));
            }

            runs_.emplace_back(MarksRun{.firstInstance = first, .instanceCount = static_cast<std::uint32_t>(std::size(markInstances_)) - first});
        }

        // Adds the glowing shapes to the glow's mesh, in the order they're drawn.
        auto tessellateGlow(const scope::Style& style) -> void
        {
            for (const auto* shape : glowShapes_)
            {
                scope::Tessellate(glowMesh_, *shape, style);
            }
        }

        // Adds a group's items in order, as scope's painter draws them.
        // NOLINTNEXTLINE(misc-no-recursion)
        auto addGroup(const scope::Group& group, scope::Style style) -> void
        {
            if (group.clip.has_value())
            {
                style.clip = scope::ClipOf(*group.clip, style.scale);
            }

            for (const auto& item : group.items)
            {
                std::visit(
                    aspire::core::Overloaded{
                        [&](const scope::Shape& x) { scope::Tessellate(mesh_, x, style); },
                        [&](const scope::Text& x)
                        {
                            closeShapes();
                            runs_.emplace_back(TextRun{.text = &x});
                        },
                        [&](const scope::Backdrop& x)
                        {
                            closeShapes();
                            runs_.emplace_back(BackdropRun{.backdrop = &x});
                        },
                        [&](const scope::Rule& x) { scope::TessellateRule(mesh_, x, style.scale); },
                        // NOLINTNEXTLINE(misc-no-recursion)
                        [&](const scope::Group& x) { addGroup(x, style); },
                        [&](const scope::Marks& x) { addMarks(x, style); },
                        [&](const scope::DataBlocks& x)
                        {
                            closeShapes();
                            runs_.emplace_back(BlocksRun{.blocks = &x});
                        },
                    },
                    item.value);
            }
        }

        // Ends the run of shapes added since the last text or backdrop, if there are any.
        auto closeShapes() -> void
        {
            const auto end = std::size(mesh_.indices());

            if (end > runStart_)
            {
                runs_.emplace_back(
                    ShapeRun{.firstIndex = static_cast<std::uint32_t>(runStart_), .indexCount = static_cast<std::uint32_t>(end - runStart_)});
            }

            runStart_ = end;
        }

        scope::Mesh mesh_;
        scope::Mesh glowMesh_;
        std::vector<Run> runs_;
        std::vector<const scope::Shape*> glowShapes_;
        std::optional<scope::GlowPlan> glow_;
        std::vector<MarkInstance> markInstances_;
        bool marksGlow_{};
        MarkView markView_;
        MarkMesh markMesh_;
        MarkMesh markGlowMesh_;
        MarkShape markShape_;
        // The scales the marks' meshes were made at, and how many times they've been made.
        float markScale_{};
        float markGlowScale_{};
        std::uint32_t markGeneration_{};
        float scale_{};
        // Where the run of shapes being added starts, in the mesh's indices.
        std::size_t runStart_{};
    };
}
