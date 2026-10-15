#include <gtest/gtest.h>

#include <SDL3/SDL.h>

import std;
import aspire.sdl;

namespace
{
    constexpr int WindowWidth{64};
    constexpr int WindowHeight{32};

    // A window on SDL's dummy video driver, which has nothing the GPU renderer can present to.
    class CreateRendererTest : public ::testing::Test
    {
    protected:
        // NOLINTNEXTLINE(readability-identifier-naming)
        auto SetUp() -> void override
        {
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
            ASSERT_TRUE(SDL_Init(SDL_INIT_VIDEO)) << SDL_GetError();
            window_ = SDL_CreateWindow("test", WindowWidth, WindowHeight, 0);
            ASSERT_NE(window_, nullptr) << SDL_GetError();
        }

        // NOLINTNEXTLINE(readability-identifier-naming)
        auto TearDown() -> void override
        {
            if (renderer_ != nullptr)
            {
                SDL_DestroyRenderer(renderer_);
            }

            SDL_DestroyWindow(window_);
            SDL_ResetHint(SDL_HINT_RENDER_DRIVER);
            SDL_Quit();
        }

        // Overrides the renderer the test environment names, if any.
        static auto setRenderDriver(const char* x) -> void
        {
            ASSERT_TRUE(SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, x, SDL_HINT_OVERRIDE)) << SDL_GetError();
        }

        auto create() -> SDL_Renderer*
        {
            renderer_ = aspire::sdl::CreateRenderer(window_);
            return renderer_;
        }

    private:
        SDL_Window* window_{};
        SDL_Renderer* renderer_{};
    };
}

TEST_F(CreateRendererTest, fallsBackWhereTheGpuRendererIsUnavailable)
{
    // An empty hint names no renderer, so the GPU renderer is tried first.
    setRenderDriver("");

    auto* renderer = create();
    ASSERT_NE(renderer, nullptr) << SDL_GetError();
    EXPECT_STRNE(SDL_GetRendererName(renderer), SDL_GPU_RENDERER);
}

TEST_F(CreateRendererTest, usesTheRendererTheHintNames)
{
    setRenderDriver(SDL_SOFTWARE_RENDERER);

    auto* renderer = create();
    ASSERT_NE(renderer, nullptr) << SDL_GetError();
    EXPECT_STREQ(SDL_GetRendererName(renderer), SDL_SOFTWARE_RENDERER);
}
