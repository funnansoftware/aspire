export module aspire.graphics:transform;

import aspire.core;
import :rect;

export namespace aspire::graphics
{
    /// @brief Maps a node's local coordinates to its parent's: scales, then offsets.
    ///
    /// There's no rotation, so a transformed rectangle stays axis-aligned.
    struct Transform
    {
        aspire::core::Vec2 offset{};
        aspire::core::Vec2 scale{.x = 1.0F, .y = 1.0F};

        /// @brief Maps a point.
        /// @param x The point in local coordinates.
        /// @return The point in the outer coordinates.
        [[nodiscard]] constexpr auto apply(aspire::core::Vec2 x) const -> aspire::core::Vec2
        {
            return {.x = offset.x + (x.x * scale.x), .y = offset.y + (x.y * scale.y)};
        }

        /// @brief Maps a rectangle.
        /// @param x The rectangle in local coordinates.
        /// @return The rectangle in the outer coordinates.
        /// @pre `scale` is positive, so the result has a non-negative size.
        [[nodiscard]] constexpr auto apply(Rect x) const -> Rect
        {
            const auto corner = apply(aspire::core::Vec2{.x = x.x, .y = x.y});
            return {.x = corner.x, .y = corner.y, .w = x.w * scale.x, .h = x.h * scale.y};
        }

        /// @brief Composes this transform with a child's.
        /// @param x The child's transform, relative to this one.
        /// @return A transform that maps the child's local coordinates to this transform's outer ones.
        [[nodiscard]] constexpr auto then(Transform x) const -> Transform
        {
            return {.offset = apply(x.offset), .scale = {.x = scale.x * x.scale.x, .y = scale.y * x.scale.y}};
        }

        /// @brief Finds the transform that undoes this one, for mapping screen positions back to local ones.
        /// @return The inverse transform.
        /// @pre Neither scale component is zero.
        [[nodiscard]] constexpr auto inverse() const -> Transform
        {
            return {.offset = {.x = -offset.x / scale.x, .y = -offset.y / scale.y}, .scale = {.x = 1.0F / scale.x, .y = 1.0F / scale.y}};
        }
    };
}
