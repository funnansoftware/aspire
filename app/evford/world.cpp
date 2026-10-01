#include "world.hpp"

import std;

namespace evford
{
    namespace
    {
        // Numerical Recipes linear congruential generator.
        constexpr std::uint32_t LcgMultiplier = 1664525U;
        constexpr std::uint32_t LcgIncrement = 1013904223U;
        // The top 24 bits fit a float mantissa exactly, giving uniform values in [0, 1).
        constexpr std::uint32_t UnitShift = 8U;
        constexpr float UnitScale = 16777216.0F;
        constexpr float NegativeChance = 0.5F;

        struct Bounds
        {
            float low;
            float high;
        };

        auto NextUnit(std::uint32_t& state) -> float
        {
            // Specified integer arithmetic makes resets reproducible across toolchains,
            // without relying on implementation-specific random distributions.
            state = (state * LcgMultiplier) + LcgIncrement;
            return static_cast<float>(state >> UnitShift) / UnitScale;
        }

        auto AdvanceAxis(std::array<float, ParticleCount>& positions, std::array<float, ParticleCount>& velocities, Bounds bounds, float seconds)
            -> void
        {
            for (auto&& [position, velocity] : std::views::zip(positions, velocities))
            {
                position += velocity * seconds;
                // Retain the overshoot when bouncing, so particles do not stick to walls.
                // The initialized speed and clamped timestep cannot cross two walls.
                if (position < bounds.low)
                {
                    position = bounds.low + (bounds.low - position);
                    velocity = std::abs(velocity);
                }
                else if (position > bounds.high)
                {
                    position = bounds.high - (position - bounds.high);
                    velocity = -std::abs(velocity);
                }
            }
        }
    }

    auto Reset(World& world, std::uint32_t seed) -> void
    {
        for (auto&& [x, y, velocityX, velocityY] : std::views::zip(world.x, world.y, world.velocityX, world.velocityY))
        {
            x = FieldLeft + (NextUnit(seed) * (FieldRight - FieldLeft - ParticleSize));
            y = FieldTop + (NextUnit(seed) * (FieldBottom - FieldTop - ParticleSize));
            const auto speedX = 30.0F + (NextUnit(seed) * 100.0F);
            const auto speedY = 30.0F + (NextUnit(seed) * 100.0F);
            velocityX = NextUnit(seed) < NegativeChance ? -speedX : speedX;
            velocityY = NextUnit(seed) < NegativeChance ? -speedY : speedY;
        }
    }

    auto Advance(World& world, float seconds) -> void
    {
        if (!std::isfinite(seconds) || seconds <= 0.0F)
        {
            return;
        }
        seconds = std::min(seconds, MaxFrameSeconds);
        AdvanceAxis(world.x, world.velocityX, {.low = FieldLeft, .high = FieldRight - ParticleSize}, seconds);
        AdvanceAxis(world.y, world.velocityY, {.low = FieldTop, .high = FieldBottom - ParticleSize}, seconds);
    }
}
