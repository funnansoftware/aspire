#include "world.hpp"

import std;

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    bool equal(const evford::World& a, const evford::World& b)
    {
        return a.x == b.x && a.y == b.y && a.velocityX == b.velocityX && a.velocityY == b.velocityY;
    }

    bool near(float a, float b)
    {
        return std::abs(a - b) < 0.0001F;
    }

    void requireBounds(const evford::World& world)
    {
        for (std::size_t i = 0; i < evford::particleCount; ++i)
        {
            require(world.x[i] >= evford::fieldLeft && world.x[i] <= evford::fieldRight - evford::particleSize,
                    "particle remains inside horizontal bounds");
            require(world.y[i] >= evford::fieldTop && world.y[i] <= evford::fieldBottom - evford::particleSize,
                    "particle remains inside vertical bounds");
        }
    }
}

int main()
{
    evford::World world;
    evford::World same;
    evford::reset(world);
    evford::reset(same);
    require(equal(world, same), "identical seeds produce identical worlds");
    requireBounds(world);
    evford::reset(same, evford::initialSeed + 1U);
    require(!equal(world, same), "different seeds produce different worlds");

    // Check expected integration and reflected overshoot at both pairs of walls.
    world.x[0] = 200.0F;
    world.y[0] = 200.0F;
    world.velocityX[0] = 40.0F;
    world.velocityY[0] = -60.0F;
    world.x[1] = evford::fieldLeft + 1.0F;
    world.y[1] = evford::fieldTop + 1.0F;
    world.velocityX[1] = -100.0F;
    world.velocityY[1] = -100.0F;
    world.x[2] = evford::fieldRight - evford::particleSize - 1.0F;
    world.y[2] = evford::fieldBottom - evford::particleSize - 1.0F;
    world.velocityX[2] = 100.0F;
    world.velocityY[2] = 100.0F;
    evford::advance(world, 0.025F);
    require(near(world.x[0], 201.0F) && near(world.y[0], 198.5F), "position advances by velocity times elapsed seconds");
    require(near(world.x[1], evford::fieldLeft + 1.5F) && near(world.y[1], evford::fieldTop + 1.5F), "low wall collision retains overshoot");
    require(world.velocityX[1] == 100.0F && world.velocityY[1] == 100.0F, "low walls reflect velocity inward");
    require(near(world.x[2], evford::fieldRight - evford::particleSize - 1.5F) && near(world.y[2], evford::fieldBottom - evford::particleSize - 1.5F),
            "high wall collision retains overshoot");
    require(world.velocityX[2] == -100.0F && world.velocityY[2] == -100.0F, "high walls reflect velocity inward");

    same = world;
    evford::advance(world, 0.0F);
    evford::advance(world, -1.0F);
    evford::advance(world, std::numeric_limits<float>::quiet_NaN());
    evford::advance(world, std::numeric_limits<float>::infinity());
    require(equal(world, same), "zero, negative and non-finite intervals do nothing");
    evford::advance(world, 100.0F);
    evford::advance(same, evford::maxFrameSeconds);
    require(equal(world, same), "long stalls are clamped");

    evford::reset(world);
    evford::reset(same);
    for (int step = 0; step < 12000; ++step)
    {
        evford::advance(world, 1.0F / 120.0F);
        requireBounds(world);
    }
    for (std::size_t i = 0; i < evford::particleCount; ++i)
    {
        require(std::abs(world.velocityX[i]) == std::abs(same.velocityX[i]) && std::abs(world.velocityY[i]) == std::abs(same.velocityY[i]),
                "wall collisions preserve speed over a long simulation");
    }
    evford::reset(world);
    require(equal(world, same), "reset restores the initial scene after simulation");
    std::cout << "Evford world tests passed\n";
    return 0;
}
