module;

#include <SDL3/SDL.h>

export module aspire.sdl:renderer;

import std;

export namespace aspire::sdl
{
    /// @brief Creates a window's renderer, on SDL's GPU renderer where the platform supports it.
    ///
    /// The GPU renderer draws through SDL's GPU API: Direct3D 12 on Windows, Metal on Apple platforms and Vulkan on
    /// Linux and Android. Where it can't be created, such as on the web or with the dummy video driver, the renderer
    /// SDL picks by default is used instead. A renderer named with `SDL_HINT_RENDER_DRIVER` (or the
    /// `SDL_RENDER_DRIVER` environment variable) overrides both.
    ///
    /// @param window The window to draw to.
    /// @return The renderer, which the caller destroys with `SDL_DestroyRenderer()` before the window. Null if none
    /// could be created; `SDL_GetError()` says why.
    auto CreateRenderer(SDL_Window* window) -> SDL_Renderer*
    {
        // SDL ignores the hint when it's asked for a renderer by name, so honor it before asking for the GPU renderer.
        if (const auto* hint = SDL_GetHint(SDL_HINT_RENDER_DRIVER); hint == nullptr || std::string_view{hint}.empty())
        {
            if (auto* renderer = SDL_CreateGPURenderer(nullptr, window); renderer != nullptr)
            {
                return renderer;
            }
        }

        return SDL_CreateRenderer(window, nullptr);
    }
}
