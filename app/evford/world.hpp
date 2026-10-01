#pragma once

import std;

namespace evford
{
    inline constexpr std::size_t ParticleCount = 1024;
    inline constexpr int CanvasWidth = 960;
    inline constexpr int CanvasHeight = 540;
    inline constexpr float ParticleSize = 5.0F;
    inline constexpr float FieldLeft = 32.0F;
    inline constexpr float FieldTop = 100.0F;
    inline constexpr float FieldRight = 928.0F;
    inline constexpr float FieldBottom = 472.0F;
    inline constexpr float MaxFrameSeconds = 0.05F;
    inline constexpr std::uint32_t InitialSeed = 0xE7F04DU;

    // Each column is contiguous; simulation systems traverse only the data they need.
    // The world owns no SDL types, entity objects, virtual calls, or per-frame allocations.
    struct World
    {
        std::array<float, ParticleCount> x{};
        std::array<float, ParticleCount> y{};
        std::array<float, ParticleCount> velocityX{};
        std::array<float, ParticleCount> velocityY{};
    };

    auto Reset(World& world, std::uint32_t seed = InitialSeed) -> void;

    // Positions are top-left corners in logical pixels. Velocities are pixels/second.
    // Invalid/nonpositive intervals do nothing; stalls are clamped to 50 ms.
    auto Advance(World& world, float seconds) -> void;
}
