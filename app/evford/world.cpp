#include "world.hpp"

import std;

namespace evford
{
    namespace
    {
        float nextUnit(std::uint32_t& state)
        {
            // Specified integer arithmetic makes resets reproducible across toolchains,
            // without relying on implementation-specific random distributions.
            state = state * 1664525U + 1013904223U;
            return static_cast<float>(state >> 8U) / 16777216.0F;
        }

        void advanceAxis(std::array<float, particleCount>& positions, std::array<float, particleCount>& velocities, float low, float high,
                         float seconds)
        {
            for (std::size_t i = 0; i < particleCount; ++i)
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

    void reset(World& world, std::uint32_t seed)
    {
        for (std::size_t i = 0; i < particleCount; ++i)
        {
            world.x[i] = fieldLeft + nextUnit(seed) * (fieldRight - fieldLeft - particleSize);
            world.y[i] = fieldTop + nextUnit(seed) * (fieldBottom - fieldTop - particleSize);
            const auto speedX = 30.0F + nextUnit(seed) * 100.0F;
            const auto speedY = 30.0F + nextUnit(seed) * 100.0F;
            world.velocityX[i] = nextUnit(seed) < 0.5F ? -speedX : speedX;
            world.velocityY[i] = nextUnit(seed) < 0.5F ? -speedY : speedY;
        }
    }

    void advance(World& world, float seconds)
    {
        if (!std::isfinite(seconds) || seconds <= 0.0F)
        {
            return;
        }
        seconds = std::min(seconds, maxFrameSeconds);
        advanceAxis(world.x, world.velocityX, fieldLeft, fieldRight - particleSize, seconds);
        advanceAxis(world.y, world.velocityY, fieldTop, fieldBottom - particleSize, seconds);
    }
}
