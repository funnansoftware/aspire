export module scope.tessellation;

import std;
import aspire.core;
import scope.mesh;
import scope.picture;
import scope.world;

// A picture's shapes as antialiased triangles, for painters that draw triangles: where they are, how they're clipped,
// and which of their channels a pass draws.
namespace scope
{
    namespace
    {
        constexpr float FullTurn = 360.0F;

        auto Scaled(Vec2 x, float scale) -> Vec2
        {
            return {.x = x.x * scale, .y = x.y * scale};
        }

        // Hands each of a shape's outlines to `add` as a line in pixels, with whether it closes. The line is `add`'s to
        // change. Separate segments are each handed over in the same line, refilled for each.
        auto Outlines(const Geometry& geometry, float scale, const auto& add) -> void
        {
            std::visit(
                aspire::core::Overloaded{
                    [&](const Circle& x)
                    {
                        auto points = FlattenArc(Scaled(x.center, scale), x.radius * scale, {0.0F, FullTurn});
                        points.pop_back();
                        add(points, true);
                    },
                    [&](const Arc& x)
                    {
                        auto points = FlattenArc(Scaled(x.center, scale), x.radius * scale, {x.from, x.sweep});
                        add(points, false);
                    },
                    [&](const Polyline& x)
                    {
                        std::vector<Vec2> points;
                        points.reserve(std::size(x.points));
                        std::ranges::transform(x.points, std::back_inserter(points), [scale](Vec2 p) { return Scaled(p, scale); });
                        add(points, x.closed);
                    },
                    [&](const Segments& x)
                    {
                        std::vector<Vec2> line;

                        for (std::size_t i = 0; i + 1 < std::size(x.ends); i += 2)
                        {
                            line.assign({Scaled(x.ends.at(i), scale), Scaled(x.ends.at(i + 1), scale)});
                            add(line, false);
                        }
                    },
                },
                geometry);
        }
    }
}

export namespace scope
{
    /// @brief Which of a color's channels are drawn.
    ///
    /// The glow adds light in straight color, clamped at full: a channel at full strength only gains the room the
    /// background leaves it, as with a screen blend, while a fainter one gains all its light, as with an add. Painters
    /// draw the glow a pass for each, so each can be blended to match.
    enum class Channels : std::uint8_t
    {
        /// Every channel, as the color is.
        All,
        /// Only the channels at full strength; the others are dropped to 0.
        Full,
        /// Only the channels below full strength; the full ones are dropped to 0.
        Fainter,
    };

    /// @brief A circle in pixels that shapes are kept inside.
    struct Clip
    {
        Vec2 center;
        float radius{};
    };

    /// @brief How a shape is turned into triangles: where it is, how it's widened, how strong it is, and which
    /// channels.
    struct Style
    {
        /// Target pixels per logical unit.
        float scale{};
        std::optional<Clip> clip;
        /// Added to every stroke's width, in pixels.
        float widen{};
        float opacity{1.0F};
        Channels channels{Channels::All};
    };

    /// @brief Converts a color, keeping only some of its channels.
    /// @param color The color.
    /// @param opacity From 0 to 1, scaling its alpha.
    /// @param channels The channels kept.
    /// @return The tint.
    [[nodiscard]] auto Tinted(Color color, float opacity, Channels channels) -> Tint
    {
        const auto keep = [channels](std::uint8_t value) -> std::uint8_t
        {
            const auto full = value == Color::Opaque;
            return channels == Channels::All || (channels == Channels::Full) == full ? value : 0;
        };

        return TintOf({.r = keep(color.r), .g = keep(color.g), .b = keep(color.b), .a = color.a}, opacity);
    }

    /// @brief Scales a group's clip from logical units to pixels.
    /// @param clip The clip, in logical units.
    /// @param scale Target pixels per logical unit.
    /// @return The clip in pixels.
    [[nodiscard]] auto ClipOf(const Circle& clip, float scale) -> Clip
    {
        return {.center = Scaled(clip.center, scale), .radius = clip.radius * scale};
    }
}

namespace scope
{
    namespace
    {
        auto Inside(const Clip& clip, Vec2 x) -> bool
        {
            return std::hypot(x.x - clip.center.x, x.y - clip.center.y) <= clip.radius;
        }

        // Whether a closed shape is kept inside a clip: a mesh can't be clipped to a circle without cutting its
        // triangles, so it's kept or dropped whole by its middle.
        auto Kept(const Style& style, const std::vector<Vec2>& points) -> bool
        {
            if (!style.clip.has_value() || std::empty(points))
            {
                return true;
            }

            Vec2 middle{};
            for (const auto& p : points)
            {
                middle = {.x = middle.x + p.x, .y = middle.y + p.y};
            }

            return Inside(*style.clip, Scaled(middle, 1.0F / static_cast<float>(std::size(points))));
        }

        // Strokes an outline, dashed if the pen says, with open lines cut at the clip's edge. A closed outline that's
        // dashed has its first point added again at its end, so its dashes run on round its last edge.
        auto StrokeOutline(Mesh& mesh, const Pen& pen, std::vector<Vec2>& points, bool closed, const Style& style, float opacity) -> void
        {
            const auto width = (pen.width * style.scale) + style.widen;
            const auto tint = Tinted(pen.color, opacity, style.channels);

            // Strokes a piece of the outline, or if it's open, the parts of it inside the clip.
            const auto strokePiece = [&](const std::vector<Vec2>& piece, bool joined)
            {
                if (!style.clip.has_value() || joined)
                {
                    mesh.stroke(piece, joined, width, tint);
                    return;
                }

                for (const auto& part : ClipToCircle(piece, style.clip->center, style.clip->radius))
                {
                    mesh.stroke(part, false, width, tint);
                }
            };

            if (!pen.dashed)
            {
                strokePiece(points, closed);
                return;
            }

            if (closed)
            {
                points.push_back(points.front());
            }

            for (const auto& dash : Dash(points, {DashPattern.at(0) * style.scale, DashPattern.at(1) * style.scale}))
            {
                strokePiece(dash, false);
            }
        }
    }
}

export namespace scope
{
    /// @brief Adds a shape's triangles to a mesh.
    ///
    /// Closed outlines are kept or dropped whole by whether their middle is inside the style's clip; open ones are cut
    /// at its edge. A shape both filled and stroked has only its inside filled, since the stroke covers its edge.
    ///
    /// @param mesh The mesh to add to.
    /// @param shape The shape, in logical units.
    /// @param style How it's drawn.
    auto Tessellate(Mesh& mesh, const Shape& shape, const Style& style) -> void
    {
        const auto opacity = shape.opacity * style.opacity;

        // A filled circle with no outline, as each track's history dots are, is by far the commonest shape, so the mesh
        // fills it from the circle it keeps rather than flattening it afresh. Its middle is its center.
        if (const auto* circle = std::get_if<Circle>(&shape.geometry);
            circle != nullptr && shape.fill.has_value() && !shape.pen.has_value() && circle->radius * style.scale > 0.0F)
        {
            const auto center = Scaled(circle->center, style.scale);

            if (!style.clip.has_value() || Inside(*style.clip, center))
            {
                mesh.fillCircle(center, circle->radius * style.scale, Tinted(*shape.fill, opacity, style.channels));
            }

            return;
        }

        Outlines(shape.geometry, style.scale,
                 [&](std::vector<Vec2>& points, bool closed)
                 {
                     if (closed && !Kept(style, points))
                     {
                         return;
                     }

                     // A shape stroked over its outline only needs its inside filled; otherwise its edge fades.
                     if (shape.fill.has_value() && closed && shape.pen.has_value())
                     {
                         mesh.fillFan(points, Tinted(*shape.fill, opacity, style.channels));
                     }
                     else if (shape.fill.has_value() && closed)
                     {
                         mesh.fillConvex(points, Tinted(*shape.fill, opacity, style.channels));
                     }

                     if (shape.pen.has_value())
                     {
                         StrokeOutline(mesh, *shape.pen, points, closed, style, opacity);
                     }
                 });
    }

    /// @brief Adds a rule's triangles to a mesh: a quad fading from its color on the left to clear on the right.
    /// @param mesh The mesh to add to.
    /// @param rule The rule, in logical units.
    /// @param scale Target pixels per logical unit.
    auto TessellateRule(Mesh& mesh, const Rule& rule, float scale) -> void
    {
        constexpr float half = 0.5F;
        const auto from = Scaled(rule.from, scale);
        const auto length = rule.length * scale;
        const auto thickness = rule.width * scale * half;
        const auto solid = TintOf(rule.color, 1.0F);
        const auto clear = TintOf(rule.color, 0.0F);
        mesh.fillQuad({{{.position = {.x = from.x, .y = from.y - thickness}, .tint = solid},
                        {.position = {.x = from.x + length, .y = from.y - thickness}, .tint = clear},
                        {.position = {.x = from.x + length, .y = from.y + thickness}, .tint = clear},
                        {.position = {.x = from.x, .y = from.y + thickness}, .tint = solid}}});
    }

    /// @brief Adds a backdrop's triangles to a mesh: its rectangle in the outer color, and a disc shading out to it.
    /// @param mesh The mesh to add to.
    /// @param backdrop The backdrop, in logical units.
    /// @param scale Target pixels per logical unit.
    auto TessellateBackdrop(Mesh& mesh, const Backdrop& backdrop, float scale) -> void
    {
        const auto outer = TintOf(backdrop.outer, 1.0F);
        const auto from = Scaled(backdrop.corner, scale);
        const Vec2 to{.x = from.x + (backdrop.width * scale), .y = from.y + (backdrop.height * scale)};
        mesh.fillQuad({{{.position = from, .tint = outer},
                        {.position = {.x = to.x, .y = from.y}, .tint = outer},
                        {.position = to, .tint = outer},
                        {.position = {.x = from.x, .y = to.y}, .tint = outer}}});
        mesh.fillRadial(Scaled(backdrop.center, scale), backdrop.radius * scale, {TintOf(backdrop.inner, 1.0F), outer});
    }

    /// @brief Finds the shapes of a group's glowing groups, wherever they nest.
    ///
    /// A glowing group's own shapes glow; groups nested inside a glowing group don't. The search recurses, but a
    /// picture's groups nest only a few deep.
    ///
    /// @param group The group to search.
    /// @param shapes Where the shapes are added, in drawing order. They point into `group`, so they last as long as
    /// it does.
    // NOLINTNEXTLINE(misc-no-recursion)
    auto CollectGlow(const Group& group, std::vector<const Shape*>& shapes) -> void
    {
        for (const auto& item : group.items)
        {
            if (const auto* inner = std::get_if<Group>(&item.value); inner != nullptr)
            {
                if (inner->glow)
                {
                    for (const auto& glowing : inner->items)
                    {
                        if (const auto* shape = std::get_if<Shape>(&glowing.value); shape != nullptr)
                        {
                            shapes.push_back(shape);
                        }
                    }
                }
                else
                {
                    CollectGlow(*inner, shapes);
                }
            }
        }
    }
}
