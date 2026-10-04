module;

#include <nlohmann/json.hpp>

export module aspire.graphics:rect;

import std;
import aspire.core;

export namespace aspire::graphics
{
    /// @brief An axis-aligned rectangle: its top-left corner and its size.
    struct Rect
    {
        float x{};
        float y{};
        float w{};
        float h{};
    };

    /// @brief Finds the overlap of two rectangles.
    /// @param a One rectangle.
    /// @param b The other rectangle.
    /// @return The overlap. When the rectangles don't overlap, its width or height is zero.
    [[nodiscard]] constexpr auto Intersect(Rect a, Rect b) -> Rect
    {
        const auto left = std::max(a.x, b.x);
        const auto top = std::max(a.y, b.y);
        const auto right = std::min(a.x + a.w, b.x + b.w);
        const auto bottom = std::min(a.y + a.h, b.y + b.h);
        return Rect{.x = left, .y = top, .w = std::max(0.0F, right - left), .h = std::max(0.0F, bottom - top)};
    }

    /// @brief Reports whether a point is inside a rectangle. The left and top edges are inside; the right and
    /// bottom edges aren't, so neighbouring rectangles never both contain a point.
    /// @param a The rectangle.
    /// @param x The point.
    /// @return `true` if `x` is inside `a`.
    [[nodiscard]] constexpr auto Contains(Rect a, aspire::core::Vec2 x) -> bool
    {
        return x.x >= a.x && x.x < a.x + a.w && x.y >= a.y && x.y < a.y + a.h;
    }

    /// @brief Writes a Rect as a JSON array, `[x, y, w, h]`.
    /// @param json The JSON value to write.
    /// @param x The rectangle.
    // nlohmann finds these by name, so they keep the library's spelling.
    // NOLINTNEXTLINE(readability-identifier-naming)
    auto to_json(nlohmann::json& json, const Rect& x) -> void
    {
        json = nlohmann::json::array({x.x, x.y, x.w, x.h});
    }

    /// @brief Reads a Rect from a JSON array, `[x, y, w, h]`.
    /// @param json The JSON value to read.
    /// @param x The rectangle to fill.
    /// @throws nlohmann::json::exception If `json` isn't an array of four numbers.
    // NOLINTNEXTLINE(readability-identifier-naming)
    auto from_json(const nlohmann::json& json, Rect& x) -> void
    {
        json.at(0).get_to(x.x);
        json.at(1).get_to(x.y);
        json.at(2).get_to(x.w);
        json.at(3).get_to(x.h);
    }
}
