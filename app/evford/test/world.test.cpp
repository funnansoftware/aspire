#include <gtest/gtest.h>

#include "world.hpp"

import std;

namespace
{
    constexpr float Tolerance = 0.0001F;

    auto Equal(const evford::World& a, const evford::World& b) -> bool
    {
        return a.x == b.x && a.y == b.y && a.velocityX == b.velocityX && a.velocityY == b.velocityY;
    }

    auto ExpectBounds(const evford::World& world) -> void
    {
        for (std::size_t i = 0; i < evford::ParticleCount; ++i)
        {
            EXPECT_GE(world.x.at(i), evford::FieldLeft);
            EXPECT_LE(world.x.at(i), evford::FieldRight - evford::ParticleSize);
            EXPECT_GE(world.y.at(i), evford::FieldTop);
            EXPECT_LE(world.y.at(i), evford::FieldBottom - evford::ParticleSize);
        }
    }
}

TEST(World, ResetIsDeterministicPerSeed)
{
    evford::World world;
    evford::World same;
    evford::Reset(world);
    evford::Reset(same);
    EXPECT_TRUE(Equal(world, same));
    ExpectBounds(world);
    evford::Reset(same, evford::InitialSeed + 1U);
    EXPECT_FALSE(Equal(world, same));
}

TEST(World, AdvanceIntegratesAndReflectsAtWalls)
{
    evford::World world;
    evford::Reset(world);

    // Particles 1 and 2 start one pixel from a wall and travel 2.5 pixels toward it.
    constexpr float start = 200.0F;
    constexpr float velocityX = 40.0F;
    constexpr float velocityY = -60.0F;
    constexpr float wallSpeed = 100.0F;
    constexpr float stepSeconds = 0.025F;
    constexpr float expectedX = 201.0F;
    constexpr float expectedY = 198.5F;
    constexpr float overshoot = 1.5F;
    world.x.at(0) = start;
    world.y.at(0) = start;
    world.velocityX.at(0) = velocityX;
    world.velocityY.at(0) = velocityY;
    world.x.at(1) = evford::FieldLeft + 1.0F;
    world.y.at(1) = evford::FieldTop + 1.0F;
    world.velocityX.at(1) = -wallSpeed;
    world.velocityY.at(1) = -wallSpeed;
    world.x.at(2) = evford::FieldRight - evford::ParticleSize - 1.0F;
    world.y.at(2) = evford::FieldBottom - evford::ParticleSize - 1.0F;
    world.velocityX.at(2) = wallSpeed;
    world.velocityY.at(2) = wallSpeed;
    evford::Advance(world, stepSeconds);

    EXPECT_NEAR(world.x.at(0), expectedX, Tolerance);
    EXPECT_NEAR(world.y.at(0), expectedY, Tolerance);
    EXPECT_NEAR(world.x.at(1), evford::FieldLeft + overshoot, Tolerance);
    EXPECT_NEAR(world.y.at(1), evford::FieldTop + overshoot, Tolerance);
    EXPECT_EQ(world.velocityX.at(1), wallSpeed);
    EXPECT_EQ(world.velocityY.at(1), wallSpeed);
    EXPECT_NEAR(world.x.at(2), evford::FieldRight - evford::ParticleSize - overshoot, Tolerance);
    EXPECT_NEAR(world.y.at(2), evford::FieldBottom - evford::ParticleSize - overshoot, Tolerance);
    EXPECT_EQ(world.velocityX.at(2), -wallSpeed);
    EXPECT_EQ(world.velocityY.at(2), -wallSpeed);
}

TEST(World, AdvanceIgnoresInvalidIntervalsAndClampsStalls)
{
    evford::World world;
    evford::World same;
    evford::Reset(world);
    same = world;
    evford::Advance(world, 0.0F);
    evford::Advance(world, -1.0F);
    evford::Advance(world, std::numeric_limits<float>::quiet_NaN());
    evford::Advance(world, std::numeric_limits<float>::infinity());
    EXPECT_TRUE(Equal(world, same));

    evford::Advance(world, 100.0F);
    evford::Advance(same, evford::MaxFrameSeconds);
    EXPECT_TRUE(Equal(world, same));
}

TEST(World, LongSimulationStaysInBoundsAndPreservesSpeed)
{
    evford::World world;
    evford::World initial;
    evford::Reset(world);
    evford::Reset(initial);

    // Simulate 100 seconds at the app's fixed step.
    constexpr int steps = 12000;
    constexpr float fixedStepSeconds = 1.0F / 120.0F;
    for (int step = 0; step < steps; ++step)
    {
        evford::Advance(world, fixedStepSeconds);
        ExpectBounds(world);
        if (::testing::Test::HasFailure())
        {
            return;
        }
    }
    for (std::size_t i = 0; i < evford::ParticleCount; ++i)
    {
        EXPECT_EQ(std::abs(world.velocityX.at(i)), std::abs(initial.velocityX.at(i)));
        EXPECT_EQ(std::abs(world.velocityY.at(i)), std::abs(initial.velocityY.at(i)));
    }
    evford::Reset(world);
    EXPECT_TRUE(Equal(world, initial));
}
