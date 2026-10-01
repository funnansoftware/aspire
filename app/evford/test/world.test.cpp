#include "world.hpp"

import std;

namespace
{
    constexpr float Tolerance = 0.0001F;

    auto Require(bool condition, const char* message) -> void
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    auto Equal(const evford::World& a, const evford::World& b) -> bool
    {
        return a.x == b.x && a.y == b.y && a.velocityX == b.velocityX && a.velocityY == b.velocityY;
    }

    auto Near(float a, float b) -> bool
    {
        return std::abs(a - b) < Tolerance;
    }

    auto RequireBounds(const evford::World& world) -> void
    {
        for (std::size_t i = 0; i < evford::ParticleCount; ++i)
        {
            Require(world.x.at(i) >= evford::FieldLeft && world.x.at(i) <= evford::FieldRight - evford::ParticleSize,
                    "particle remains inside horizontal bounds");
            Require(world.y.at(i) >= evford::FieldTop && world.y.at(i) <= evford::FieldBottom - evford::ParticleSize,
                    "particle remains inside vertical bounds");
        }
    }
}

// Standard streams throw only when an exception mask is set, which these tests never do.
// NOLINTNEXTLINE(bugprone-exception-escape)
auto main() -> int
{
    evford::World world;
    evford::World same;
    evford::Reset(world);
    evford::Reset(same);
    Require(Equal(world, same), "identical seeds produce identical worlds");
    RequireBounds(world);
    evford::Reset(same, evford::InitialSeed + 1U);
    Require(!Equal(world, same), "different seeds produce different worlds");

    // Check expected integration and reflected overshoot at both pairs of walls.
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
    Require(Near(world.x.at(0), expectedX) && Near(world.y.at(0), expectedY), "position advances by velocity times elapsed seconds");
    Require(Near(world.x.at(1), evford::FieldLeft + overshoot) && Near(world.y.at(1), evford::FieldTop + overshoot),
            "low wall collision retains overshoot");
    Require(world.velocityX.at(1) == wallSpeed && world.velocityY.at(1) == wallSpeed, "low walls reflect velocity inward");
    Require(Near(world.x.at(2), evford::FieldRight - evford::ParticleSize - overshoot)
                && Near(world.y.at(2), evford::FieldBottom - evford::ParticleSize - overshoot),
            "high wall collision retains overshoot");
    Require(world.velocityX.at(2) == -wallSpeed && world.velocityY.at(2) == -wallSpeed, "high walls reflect velocity inward");

    same = world;
    evford::Advance(world, 0.0F);
    evford::Advance(world, -1.0F);
    evford::Advance(world, std::numeric_limits<float>::quiet_NaN());
    evford::Advance(world, std::numeric_limits<float>::infinity());
    Require(Equal(world, same), "zero, negative and non-finite intervals do nothing");
    evford::Advance(world, 100.0F);
    evford::Advance(same, evford::MaxFrameSeconds);
    Require(Equal(world, same), "long stalls are clamped");

    evford::Reset(world);
    evford::Reset(same);
    // Simulate 100 seconds at the app's fixed step.
    constexpr int steps = 12000;
    constexpr float fixedStepSeconds = 1.0F / 120.0F;
    for (int step = 0; step < steps; ++step)
    {
        evford::Advance(world, fixedStepSeconds);
        RequireBounds(world);
    }
    for (std::size_t i = 0; i < evford::ParticleCount; ++i)
    {
        Require(
            std::abs(world.velocityX.at(i)) == std::abs(same.velocityX.at(i)) && std::abs(world.velocityY.at(i)) == std::abs(same.velocityY.at(i)),
            "wall collisions preserve speed over a long simulation");
    }
    evford::Reset(world);
    Require(Equal(world, same), "reset restores the initial scene after simulation");
    std::cout << "Evford world tests passed\n";
    return 0;
}
