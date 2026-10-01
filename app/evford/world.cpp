#include "world.hpp"

import std;

namespace evford
{
    namespace
    {
        auto NextUnit(std::uint32_t& state) -> float
        {
            // Specified integer arithmetic makes resets reproducible across toolchains,
            // without relying on implementation-specific random distributions.
            state = state * 1664525U + 1013904223U;
            return static_cast<float>(state >> 8U) / 16777216.0F;
        }

        auto AdvanceAxis(std::array<float, ParticleCount>& positions, std::array<float, ParticleCount>& velocities, float low, float high,
                         float seconds) -> void
        {
            for (std::size_t i = 0; i < ParticleCount; ++i)
            {
                auto position = positions[i] + velocities[i] * seconds;
                // Retain the overshoot when bouncing, so particles do not stick to walls.
                // The initialized speed and clamped timestep cannot cross two walls.
                if (position < low)
                {
                    position = low + (low - position);
                    velocities[i] = std::abs(velocities[i]);
                }
                else if (position > high)
                {
                    position = high - (position - high);
                    velocities[i] = -std::abs(velocities[i]);
                }
                positions[i] = position;
            }
        }
    }

    auto Reset(World& world, std::uint32_t seed) -> void
    {
        for (std::size_t i = 0; i < ParticleCount; ++i)
        {
            world.x[i] = FieldLeft + NextUnit(seed) * (FieldRight - FieldLeft - ParticleSize);
            world.y[i] = FieldTop + NextUnit(seed) * (FieldBottom - FieldTop - ParticleSize);
            const auto speedX = 30.0F + NextUnit(seed) * 100.0F;
            const auto speedY = 30.0F + NextUnit(seed) * 100.0F;
            world.velocityX[i] = NextUnit(seed) < 0.5F ? -speedX : speedX;
            world.velocityY[i] = NextUnit(seed) < 0.5F ? -speedY : speedY;
        }
    }

    auto Advance(World& world, float seconds) -> void
    {
        if (!std::isfinite(seconds) || seconds <= 0.0F)
        {
            return;
        }
        seconds = std::min(seconds, MaxFrameSeconds);
        AdvanceAxis(world.x, world.velocityX, FieldLeft, FieldRight - ParticleSize, seconds);
        AdvanceAxis(world.y, world.velocityY, FieldTop, FieldBottom - ParticleSize, seconds);
    }
}
