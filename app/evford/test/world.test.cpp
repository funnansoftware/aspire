#include "world.hpp"

import std;

namespace
{
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
        return std::abs(a - b) < 0.0001F;
    }

    auto RequireBounds(const evford::World& world) -> void
    {
        for (std::size_t i = 0; i < evford::ParticleCount; ++i)
        {
            Require(world.x[i] >= evford::FieldLeft && world.x[i] <= evford::FieldRight - evford::ParticleSize,
                    "particle remains inside horizontal bounds");
            Require(world.y[i] >= evford::FieldTop && world.y[i] <= evford::FieldBottom - evford::ParticleSize,
                    "particle remains inside vertical bounds");
        }
    }
}

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
    world.x[0] = 200.0F;
    world.y[0] = 200.0F;
    world.velocityX[0] = 40.0F;
    world.velocityY[0] = -60.0F;
    world.x[1] = evford::FieldLeft + 1.0F;
    world.y[1] = evford::FieldTop + 1.0F;
    world.velocityX[1] = -100.0F;
    world.velocityY[1] = -100.0F;
    world.x[2] = evford::FieldRight - evford::ParticleSize - 1.0F;
    world.y[2] = evford::FieldBottom - evford::ParticleSize - 1.0F;
    world.velocityX[2] = 100.0F;
    world.velocityY[2] = 100.0F;
    evford::Advance(world, 0.025F);
    Require(Near(world.x[0], 201.0F) && Near(world.y[0], 198.5F), "position advances by velocity times elapsed seconds");
    Require(Near(world.x[1], evford::FieldLeft + 1.5F) && Near(world.y[1], evford::FieldTop + 1.5F), "low wall collision retains overshoot");
    Require(world.velocityX[1] == 100.0F && world.velocityY[1] == 100.0F, "low walls reflect velocity inward");
    Require(Near(world.x[2], evford::FieldRight - evford::ParticleSize - 1.5F) && Near(world.y[2], evford::FieldBottom - evford::ParticleSize - 1.5F),
            "high wall collision retains overshoot");
    Require(world.velocityX[2] == -100.0F && world.velocityY[2] == -100.0F, "high walls reflect velocity inward");

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
    for (int step = 0; step < 12000; ++step)
    {
        evford::Advance(world, 1.0F / 120.0F);
        RequireBounds(world);
    }
    for (std::size_t i = 0; i < evford::ParticleCount; ++i)
    {
        Require(std::abs(world.velocityX[i]) == std::abs(same.velocityX[i]) && std::abs(world.velocityY[i]) == std::abs(same.velocityY[i]),
                "wall collisions preserve speed over a long simulation");
    }
    evford::Reset(world);
    Require(Equal(world, same), "reset restores the initial scene after simulation");
    std::cout << "Evford world tests passed\n";
    return 0;
}
