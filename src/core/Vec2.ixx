module;

#include <nlohmann/json.hpp>

export module aspire.core:vec2;

export namespace aspire::core
{
    struct Vec2
    {
        float x{0.0F};
        float y{0.0F};
    };

    /// @brief Writes a Vec2 as a JSON array, `[x, y]`.
    /// @param json The JSON value to write.
    /// @param x The vector.
    // nlohmann finds this by name, so it keeps the library's spelling.
    // NOLINTNEXTLINE(readability-identifier-naming)
    auto to_json(nlohmann::json& json, const Vec2& x) -> void
    {
        json = nlohmann::json::array({x.x, x.y});
    }

    /// @brief Reads a Vec2 from a JSON array, `[x, y]`.
    /// @param json The JSON value to read.
    /// @param x The vector to fill.
    /// @throws nlohmann::json::exception If `json` isn't an array of two numbers.
    // NOLINTNEXTLINE(readability-identifier-naming)
    auto from_json(const nlohmann::json& json, Vec2& x) -> void
    {
        json.at(0).get_to(x.x);
        json.at(1).get_to(x.y);
    }
}
