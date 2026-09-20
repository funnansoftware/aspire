#pragma once

import std;

namespace evford
{
    inline constexpr std::size_t particleCount = 1024;
    inline constexpr int canvasWidth = 960;
    inline constexpr int canvasHeight = 540;
    inline constexpr float particleSize = 5.0F;
    inline constexpr float fieldLeft = 32.0F;
    inline constexpr float fieldTop = 100.0F;
    inline constexpr float fieldRight = 928.0F;
    inline constexpr float fieldBottom = 472.0F;
    inline constexpr float maxFrameSeconds = 0.05F;
    inline constexpr std::uint32_t initialSeed = 0xE7F04DU;

    // Each column is contiguous; simulation systems traverse only the data they need.
    // The world owns no SDL types, entity objects, virtual calls, or per-frame allocations.
    struct World
    {
        std::array<float, particleCount> x{};
        std::array<float, particleCount> y{};
        std::array<float, particleCount> velocityX{};
        std::array<float, particleCount> velocityY{};
    };

    void reset(World& world, std::uint32_t seed = initialSeed);

    // Positions are top-left corners in logical pixels. Velocities are pixels/second.
    // Invalid/nonpositive intervals do nothing; stalls are clamped to 50 ms.
    void advance(World& world, float seconds);
}
