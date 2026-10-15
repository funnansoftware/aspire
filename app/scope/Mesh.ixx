export module scope.mesh;

import std;
import scope.world;

// Triangles for strokes and fills, with antialiased edges, and blurs made of shifted copies, for renderers that only
// draw triangles and textures.
export namespace scope
{
    /// @brief A color with straight alpha, each channel from 0 to 1.
    struct Tint
    {
        float r{};
        float g{};
        float b{};
        float a{};
    };

    /// @brief A corner of a triangle: a position in pixels and its color.
    struct Vertex
    {
        Vec2 position;
        Tint tint;
    };

    /// @brief How far a flattened curve may stray from the true one, in pixels.
    inline constexpr float CurveTolerance = 0.25F;

    /// @brief Converts a color, scaling its alpha by an opacity.
    /// @param color The color.
    /// @param opacity From 0 to 1.
    /// @return The tint.
    [[nodiscard]] constexpr auto TintOf(Color color, float opacity) -> Tint
    {
        constexpr float channelMax = 255.0F;
        return {.r = static_cast<float>(color.r) / channelMax,
                .g = static_cast<float>(color.g) / channelMax,
                .b = static_cast<float>(color.b) / channelMax,
                .a = static_cast<float>(color.a) / channelMax * opacity};
    }

    /// @brief Reports how many straight segments a whole circle needs to stay within `CurveTolerance` of it.
    /// @param radius The radius in pixels.
    /// @return At least 12.
    [[nodiscard]] auto CircleSegments(float radius) -> int
    {
        constexpr int fewest = 12;
        constexpr int most = 1024;
        constexpr float twice = 2.0F;

        if (!(radius > CurveTolerance))
        {
            return fewest;
        }

        // A chord of angle a strays r (1 - cos(a / 2)) from its arc.
        const auto angle = twice * std::acos(1.0F - (CurveTolerance / radius));
        return std::clamp(static_cast<int>(std::ceil(twice * std::numbers::pi_v<float> / angle)), fewest, most);
    }

    /// @brief Flattens part of a circle into points, clockwise from a bearing.
    /// @param center The center, on screen, where y grows downward.
    /// @param radius The radius in pixels.
    /// @param bearings The first bearing and the sweep, in degrees clockwise from north.
    /// @return The points, both ends included.
    [[nodiscard]] auto FlattenArc(Vec2 center, float radius, std::pair<float, float> bearings) -> std::vector<Vec2>
    {
        constexpr float fullTurn = 360.0F;
        const auto [from, sweep] = bearings;
        const auto segments = std::max(1, static_cast<int>(std::ceil(static_cast<float>(CircleSegments(radius)) * std::abs(sweep) / fullTurn)));
        std::vector<Vec2> points;
        points.reserve(static_cast<std::size_t>(segments) + 1);

        for (auto i = 0; i <= segments; ++i)
        {
            const auto bearing = from + (sweep * static_cast<float>(i) / static_cast<float>(segments));
            const auto angle = bearing * std::numbers::pi_v<float> / (fullTurn / 2.0F);
            points.push_back({.x = center.x + (radius * std::sin(angle)), .y = center.y - (radius * std::cos(angle))});
        }

        return points;
    }

    /// @brief Splits a line into dashes.
    /// @param points The line's points.
    /// @param pattern Alternating dash and gap lengths, in the points' units.
    /// @return The dashes, each a line of its own.
    [[nodiscard]] auto Dash(const std::vector<Vec2>& points, const std::array<float, 2>& pattern) -> std::vector<std::vector<Vec2>>
    {
        std::vector<std::vector<Vec2>> dashes;

        if (std::size(points) < 2 || std::empty(pattern) || !std::ranges::all_of(pattern, [](float x) { return x > 0.0F; }))
        {
            return dashes;
        }

        std::size_t step = 0;
        auto left = pattern.front();
        auto on = true;
        std::vector<Vec2> dash{points.front()};

        for (std::size_t i = 1; i < std::size(points); ++i)
        {
            auto from = points.at(i - 1);
            const auto to = points.at(i);
            auto length = std::hypot(to.x - from.x, to.y - from.y);

            // Cut the segment wherever the pattern turns a dash on or off.
            while (length > left)
            {
                const auto share = left / length;
                const Vec2 cut{.x = from.x + ((to.x - from.x) * share), .y = from.y + ((to.y - from.y) * share)};

                if (on)
                {
                    dash.push_back(cut);
                    dashes.push_back(std::move(dash));
                    dash.clear();
                }
                else
                {
                    dash = {cut};
                }

                length -= left;
                from = cut;
                on = !on;
                step = (step + 1) % std::size(pattern);
                left = pattern.at(step);
            }

            left -= length;

            if (on)
            {
                dash.push_back(to);
            }
        }

        if (on && std::size(dash) > 1)
        {
            dashes.push_back(std::move(dash));
        }

        return dashes;
    }

    /// @brief Keeps the parts of a line inside a circle.
    /// @param points The line's points.
    /// @param center The circle's center.
    /// @param radius The circle's radius.
    /// @return The parts inside, each a line of its own.
    [[nodiscard]] auto ClipToCircle(const std::vector<Vec2>& points, Vec2 center, float radius) -> std::vector<std::vector<Vec2>>
    {
        std::vector<std::vector<Vec2>> parts;
        std::vector<Vec2> part;
        const auto radiusSquared = radius * radius;

        for (std::size_t i = 1; i < std::size(points); ++i)
        {
            const auto from = points.at(i - 1);
            const auto to = points.at(i);

            // Where the segment from + t (to - from) meets the circle, by solving a t^2 + 2 b t + c = 0.
            const Vec2 d{.x = to.x - from.x, .y = to.y - from.y};
            const Vec2 f{.x = from.x - center.x, .y = from.y - center.y};
            const auto a = (d.x * d.x) + (d.y * d.y);
            const auto b = (f.x * d.x) + (f.y * d.y);
            const auto c = (f.x * f.x) + (f.y * f.y) - radiusSquared;
            const auto discriminant = (b * b) - (a * c);

            if (a == 0.0F || discriminant <= 0.0F)
            {
                continue;
            }

            const auto root = std::sqrt(discriminant);
            const auto enter = std::max(0.0F, (-b - root) / a);
            const auto leave = std::min(1.0F, (-b + root) / a);

            if (enter >= leave)
            {
                continue;
            }

            const auto point = [&](float t) -> Vec2 { return {.x = from.x + (d.x * t), .y = from.y + (d.y * t)}; };

            if (enter > 0.0F || std::empty(part))
            {
                if (std::size(part) > 1)
                {
                    parts.push_back(std::move(part));
                }

                part = {point(enter)};
            }

            part.push_back(point(leave));

            if (leave < 1.0F)
            {
                parts.push_back(std::move(part));
                part.clear();
            }
        }

        if (std::size(part) > 1)
        {
            parts.push_back(std::move(part));
        }

        return parts;
    }

    /// @brief One shifted, weighted copy of an image, in a blur made by adding such copies together.
    struct Tap
    {
        /// How far the copy is shifted, in pixels. It may fall between pixels.
        float offset{};
        float weight{};
    };

    /// @brief Works out a Gaussian blur along one axis as shifted copies of an image, to be added together.
    ///
    /// A copy shifted to between two pixels is filtered linearly, so it stands exactly for two neighboring taps of
    /// the kernel, and the blur needs about half as many copies. The kernel is cut off a number of sigmas either way,
    /// and its weights add up to 1, so the blur keeps the image's brightness.
    ///
    /// @param sigma The blur's standard deviation in pixels, before the kernel is cut off.
    /// @param reach How many sigmas the kernel reaches either way. Three keeps nearly all of a Gaussian; fewer narrow
    /// it, and end it sooner.
    /// @return The copies, from the most negative shift to the most positive.
    [[nodiscard]] auto GaussianTaps(float sigma, float reach) -> std::vector<Tap>
    {
        constexpr float half = 0.5F;
        constexpr float both = 2.0F;

        if (!(sigma > 0.0F) || !(reach > 0.0F))
        {
            return {{.offset = 0.0F, .weight = 1.0F}};
        }

        const auto radius = static_cast<std::size_t>(std::ceil(sigma * reach));
        std::vector<float> weights;
        weights.reserve(radius + 1);
        auto total = 0.0F;

        for (std::size_t k = 0; k <= radius; ++k)
        {
            const auto x = static_cast<float>(k) / sigma;
            weights.push_back(std::exp(-half * x * x));
            total += k == 0 ? weights.back() : both * weights.back();
        }

        // Each side's taps in pairs, each pair one copy between its two pixels, weighted toward the heavier.
        std::vector<Tap> side;

        for (std::size_t k = 1; k <= radius; k += 2)
        {
            const auto near = weights.at(k);
            const auto far = k + 1 <= radius ? weights.at(k + 1) : 0.0F;
            const auto weight = near + far;
            side.push_back({.offset = ((static_cast<float>(k) * near) + (static_cast<float>(k + 1) * far)) / weight, .weight = weight / total});
        }

        std::vector<Tap> taps;
        taps.reserve((std::size(side) * 2) + 1);
        std::ranges::transform(side | std::views::reverse, std::back_inserter(taps),
                               [](Tap x) { return Tap{.offset = -x.offset, .weight = x.weight}; });
        taps.push_back({.offset = 0.0F, .weight = weights.front() / total});
        taps.insert(std::end(taps), std::begin(side), std::end(side));
        return taps;
    }

    /// @brief Triangles with antialiased edges: each shape fades to transparent over its outermost pixel, its fringe.
    class Mesh
    {
    public:
        auto clear() -> void
        {
            vertices_.clear();
            indices_.clear();
        }

        [[nodiscard]] auto vertices() const -> const std::vector<Vertex>&
        {
            return vertices_;
        }

        [[nodiscard]] auto indices() const -> const std::vector<int>&
        {
            return indices_;
        }

        [[nodiscard]] auto empty() const -> bool
        {
            return std::empty(indices_);
        }

        /// @brief Strokes a line, its ends squared off half its width past its points.
        /// @param points The line's points, in pixels.
        /// @param closed Whether it joins its last point back to its first.
        /// @param width The stroke's width in pixels. Narrower than a pixel, it's drawn a pixel wide and fainter.
        /// @param tint The stroke's color.
        auto stroke(const std::vector<Vec2>& points, bool closed, float width, Tint tint) -> void
        {
            const auto count = std::size(points);

            if (count < 2 || (closed && count < 3))
            {
                return;
            }

            // The solid core is the width less the fringe; a stroke thinner than its fringe fades to match.
            const auto core = std::max(0.0F, (width - Fringe) * Half);
            const auto edge = core + Fringe;
            const Tint solid{.r = tint.r, .g = tint.g, .b = tint.b, .a = tint.a * std::min(1.0F, width / Fringe)};
            const Tint clear{.r = tint.r, .g = tint.g, .b = tint.b, .a = 0.0F};
            const auto first = static_cast<int>(std::size(vertices_));

            for (std::size_t i = 0; i < count; ++i)
            {
                const auto normal = joinNormal(points, i, closed);
                auto point = points.at(i);

                // Open ends are squared off half the width past the end.
                if (!closed && (i == 0 || i == count - 1))
                {
                    const auto along = i == 0 ? direction(points.at(1), points.at(0)) : direction(points.at(count - 2), points.at(count - 1));
                    point = {.x = point.x + (along.x * width * Half), .y = point.y + (along.y * width * Half)};
                }

                vertices_.push_back({.position = offset(point, normal, edge), .tint = clear});
                vertices_.push_back({.position = offset(point, normal, core), .tint = solid});
                vertices_.push_back({.position = offset(point, normal, -core), .tint = solid});
                vertices_.push_back({.position = offset(point, normal, -edge), .tint = clear});
            }

            const auto segments = closed ? count : count - 1;

            for (std::size_t i = 0; i < segments; ++i)
            {
                const auto a = first + static_cast<int>(i * StrokeVertices);
                const auto b = first + static_cast<int>(((i + 1) % count) * StrokeVertices);

                // Three strips between the two points' rows: the outer fringe, the core, and the inner fringe.
                for (auto strip = 0; strip < 3; ++strip)
                {
                    quad(a + strip, b + strip, b + strip + 1, a + strip + 1);
                }
            }
        }

        /// @brief Fills a convex polygon, its edges fading over the fringe.
        /// @param points The polygon's corners, in pixels, in either order.
        /// @param tint The fill's color.
        auto fillConvex(const std::vector<Vec2>& points, Tint tint) -> void
        {
            const auto count = std::size(points);

            if (count < 3)
            {
                return;
            }

            // Corners' normals point out of the polygon whichever way round it's wound.
            const auto outward = area(points) > 0.0F ? -1.0F : 1.0F;
            const Tint clear{.r = tint.r, .g = tint.g, .b = tint.b, .a = 0.0F};
            const auto first = static_cast<int>(std::size(vertices_));

            for (std::size_t i = 0; i < count; ++i)
            {
                const auto normal = joinNormal(points, i, true);
                vertices_.push_back({.position = offset(points.at(i), normal, -Fringe * Half * outward), .tint = tint});
                vertices_.push_back({.position = offset(points.at(i), normal, Fringe * Half * outward), .tint = clear});
            }

            for (std::size_t i = 2; i < count; ++i)
            {
                triangle(first, first + static_cast<int>((i - 1) * 2), first + static_cast<int>(i * 2));
            }

            for (std::size_t i = 0; i < count; ++i)
            {
                const auto a = first + static_cast<int>(i * 2);
                const auto b = first + static_cast<int>(((i + 1) % count) * 2);
                quad(a, b, b + 1, a + 1);
            }
        }

        /// @brief Fills a polygon by fanning from its first point, which must see every other, without fading its
        /// edges, as for a shape stroked over its outline.
        /// @param points The polygon's corners, in pixels.
        /// @param tint The fill's color.
        auto fillFan(const std::vector<Vec2>& points, Tint tint) -> void
        {
            const auto first = static_cast<int>(std::size(vertices_));

            for (const auto& point : points)
            {
                vertices_.push_back({.position = point, .tint = tint});
            }

            for (std::size_t i = 2; i < std::size(points); ++i)
            {
                triangle(first, first + static_cast<int>(i - 1), first + static_cast<int>(i));
            }
        }

        /// @brief Fills a quadrilateral, shading between its corners' colors.
        /// @param corners The corners in order around it, in pixels, with their colors.
        auto fillQuad(const std::array<Vertex, 4>& corners) -> void
        {
            const auto first = static_cast<int>(std::size(vertices_));
            vertices_.insert(std::end(vertices_), std::begin(corners), std::end(corners));
            quad(first, first + 1, first + 2, first + 3);
        }

        /// @brief Fills a disc, shading from one color at its center to another at its edge.
        /// @param center The center, in pixels.
        /// @param radius The radius in pixels.
        /// @param tints The center's color and the edge's.
        auto fillRadial(Vec2 center, float radius, std::pair<Tint, Tint> tints) -> void
        {
            const auto rim = FlattenArc(center, radius, {0.0F, FullTurn});
            const auto first = static_cast<int>(std::size(vertices_));
            vertices_.push_back({.position = center, .tint = tints.first});

            for (const auto& point : rim)
            {
                vertices_.push_back({.position = point, .tint = tints.second});
            }

            for (std::size_t i = 1; i < std::size(rim); ++i)
            {
                triangle(first, first + static_cast<int>(i), first + static_cast<int>(i + 1));
            }
        }

    private:
        static constexpr float Half = 0.5F;
        static constexpr float FullTurn = 360.0F;
        // Each shape fades out over its outermost pixel.
        static constexpr float Fringe = 1.0F;
        static constexpr std::size_t StrokeVertices = 4;
        // The longest a sharp corner's miter may reach, as a multiple of the stroke's half width.
        static constexpr float MiterLimit = 4.0F;

        static auto direction(Vec2 from, Vec2 to) -> Vec2
        {
            const auto length = std::hypot(to.x - from.x, to.y - from.y);
            return length > 0.0F ? Vec2{.x = (to.x - from.x) / length, .y = (to.y - from.y) / length} : Vec2{};
        }

        static auto offset(Vec2 point, Vec2 normal, float distance) -> Vec2
        {
            return {.x = point.x + (normal.x * distance), .y = point.y + (normal.y * distance)};
        }

        // Twice the polygon's signed area: positive when wound clockwise on screen, where y grows downward.
        static auto area(const std::vector<Vec2>& points) -> float
        {
            auto area = 0.0F;

            for (std::size_t i = 0; i < std::size(points); ++i)
            {
                const auto& a = points.at(i);
                const auto& b = points.at((i + 1) % std::size(points));
                area += (a.x * b.y) - (b.x * a.y);
            }

            return area;
        }

        // The normal at a point, lengthened at a corner so both edges keep their width: a miter, within the limit.
        static auto joinNormal(const std::vector<Vec2>& points, std::size_t i, bool closed) -> Vec2
        {
            const auto count = std::size(points);
            // At an open line's ends, the missing neighbor is the point itself.
            auto previous = i;
            auto next = i;

            if (i > 0)
            {
                previous = i - 1;
            }
            else if (closed)
            {
                previous = count - 1;
            }

            if (i + 1 < count)
            {
                next = i + 1;
            }
            else if (closed)
            {
                next = 0;
            }
            const auto in = previous != i ? direction(points.at(previous), points.at(i)) : direction(points.at(i), points.at(next));
            const auto out = next != i ? direction(points.at(i), points.at(next)) : in;
            const Vec2 normal{.x = -(in.y + out.y) * Half, .y = (in.x + out.x) * Half};
            const auto lengthSquared = (normal.x * normal.x) + (normal.y * normal.y);

            if (lengthSquared <= 0.0F)
            {
                return {.x = -in.y, .y = in.x};
            }

            const auto scale = std::min(1.0F / lengthSquared, MiterLimit * MiterLimit);
            return {.x = normal.x * scale, .y = normal.y * scale};
        }

        auto triangle(int a, int b, int c) -> void
        {
            indices_.insert(std::end(indices_), {a, b, c});
        }

        auto quad(int a, int b, int c, int d) -> void
        {
            triangle(a, b, c);
            triangle(a, c, d);
        }

        std::vector<Vertex> vertices_;
        std::vector<int> indices_;
    };
}
