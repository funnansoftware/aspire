module;

#include <nlohmann/json.hpp>

export module aspire.graphics:color;

import std;

export namespace aspire::graphics
{
    /// @brief An RGBA color, 8 bits per channel.
    struct Color
    {
        static constexpr std::uint8_t Opaque{255};

        std::uint8_t r{};
        std::uint8_t g{};
        std::uint8_t b{};
        std::uint8_t a{Opaque};
    };

    /// @brief Opaque black, the default clear color.
    constexpr Color Black{.r = 0, .g = 0, .b = 0, .a = Color::Opaque};

    /// @brief Opaque white, the default tint and text color.
    constexpr Color White{.r = Color::Opaque, .g = Color::Opaque, .b = Color::Opaque, .a = Color::Opaque};

    /// @brief Writes a Color as a JSON array, `[r, g, b, a]`.
    /// @param json The JSON value to write.
    /// @param x The color.
    // nlohmann finds these by name, so they keep the library's spelling.
    // NOLINTNEXTLINE(readability-identifier-naming)
    auto to_json(nlohmann::json& json, const Color& x) -> void
    {
        json = nlohmann::json::array({x.r, x.g, x.b, x.a});
    }

    /// @brief Reads a Color from a JSON array, `[r, g, b]` or `[r, g, b, a]`. Alpha defaults to opaque.
    /// @param json The JSON value to read.
    /// @param x The color to fill.
    /// @throws nlohmann::json::exception If `json` isn't an array of three or four numbers.
    // NOLINTNEXTLINE(readability-identifier-naming)
    auto from_json(const nlohmann::json& json, Color& x) -> void
    {
        json.at(0).get_to(x.r);
        json.at(1).get_to(x.g);
        json.at(2).get_to(x.b);
        x.a = std::size(json) > 3 ? json.at(3).get<std::uint8_t>() : Color::Opaque;
    }
}
