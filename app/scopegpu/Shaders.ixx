module;

#include <SDL3/SDL.h>

export module scopegpu.shaders;

import std;
import scopegpu.gpu;
import scopegpu.shadercode;

export namespace scopegpu
{
    /// @brief One of scopegpu's shaders, compiled from `shaders/` into every format it was built with.
    enum class ShaderName : std::uint8_t
    {
        /// Shapes' and glowing shapes' triangles, from target pixels to clip space.
        ShapeVertex,
        /// Shapes in their color.
        ShapeFragment,
        /// Glowing shapes, split into their full and fainter channels.
        GlowFragment,
        /// Glyphs' quads, with where they are in the atlas.
        TextVertex,
        /// Glyphs from the atlas.
        TextFragment,
        /// A triangle over the whole target.
        FullscreenVertex,
        /// One axis of the glow's blur.
        BlurFragment,
        /// The backdrop with the glow over it.
        CompositeFragment,
        /// Glowing shapes' full or fainter channels, drawn through SDL's renderer.
        Glow2dFragment,
        /// One axis of the glow's blur for one texture, drawn through SDL's renderer.
        Blur2dFragment,
        /// The backdrop with the glow over it, drawn through SDL's renderer.
        Composite2dFragment,
    };

    /// @brief Reports the shader formats scopegpu has its shaders in, to ask a device for.
    /// @return The formats, as SDL flags.
    [[nodiscard]] auto ShaderFormats() -> SDL_GPUShaderFormat
    {
        const auto& blobs = shadercode::ShapeVertex;
        SDL_GPUShaderFormat formats = 0;
        formats |= std::empty(blobs.spirv) ? 0 : SDL_GPU_SHADERFORMAT_SPIRV;
        formats |= std::empty(blobs.msl) ? 0 : SDL_GPU_SHADERFORMAT_MSL;
        formats |= std::empty(blobs.dxil) ? 0 : SDL_GPU_SHADERFORMAT_DXIL;
        formats |= std::empty(blobs.dxbc) ? 0 : SDL_GPU_SHADERFORMAT_DXBC;
        return formats;
    }
}

namespace scopegpu
{
    namespace
    {
        // What a shader is, as SDL needs to know when it's made.
        struct ShaderInfo
        {
            const shadercode::Blobs* blobs{};
            SDL_GPUShaderStage stage{};
            std::uint32_t samplers{};
            std::uint32_t uniformBuffers{};
        };

        auto InfoOf(ShaderName x) -> ShaderInfo
        {
            switch (x)
            {
                case ShaderName::ShapeVertex:
                    return {.blobs = &shadercode::ShapeVertex, .stage = SDL_GPU_SHADERSTAGE_VERTEX, .samplers = 0, .uniformBuffers = 1};
                case ShaderName::ShapeFragment:
                    return {.blobs = &shadercode::ShapeFragment, .stage = SDL_GPU_SHADERSTAGE_FRAGMENT, .samplers = 0, .uniformBuffers = 0};
                case ShaderName::GlowFragment:
                    return {.blobs = &shadercode::GlowFragment, .stage = SDL_GPU_SHADERSTAGE_FRAGMENT, .samplers = 0, .uniformBuffers = 0};
                case ShaderName::TextVertex:
                    return {.blobs = &shadercode::TextVertex, .stage = SDL_GPU_SHADERSTAGE_VERTEX, .samplers = 0, .uniformBuffers = 1};
                case ShaderName::TextFragment:
                    return {.blobs = &shadercode::TextFragment, .stage = SDL_GPU_SHADERSTAGE_FRAGMENT, .samplers = 1, .uniformBuffers = 0};
                case ShaderName::FullscreenVertex:
                    return {.blobs = &shadercode::FullscreenVertex, .stage = SDL_GPU_SHADERSTAGE_VERTEX, .samplers = 0, .uniformBuffers = 0};
                case ShaderName::BlurFragment:
                    return {.blobs = &shadercode::BlurFragment, .stage = SDL_GPU_SHADERSTAGE_FRAGMENT, .samplers = 2, .uniformBuffers = 1};
                case ShaderName::CompositeFragment:
                    return {.blobs = &shadercode::CompositeFragment, .stage = SDL_GPU_SHADERSTAGE_FRAGMENT, .samplers = 2, .uniformBuffers = 1};
                case ShaderName::Glow2dFragment:
                    return {.blobs = &shadercode::Glow2dFragment, .stage = SDL_GPU_SHADERSTAGE_FRAGMENT, .samplers = 0, .uniformBuffers = 1};
                case ShaderName::Blur2dFragment:
                    return {.blobs = &shadercode::Blur2dFragment, .stage = SDL_GPU_SHADERSTAGE_FRAGMENT, .samplers = 1, .uniformBuffers = 1};
                case ShaderName::Composite2dFragment:
                    break;
            }

            return {.blobs = &shadercode::Composite2dFragment, .stage = SDL_GPU_SHADERSTAGE_FRAGMENT, .samplers = 2, .uniformBuffers = 1};
        }
    }
}

export namespace scopegpu
{
    /// @brief Makes one of the shaders for a device, in the format it takes best.
    ///
    /// DXIL is preferred to DXBC where the device takes both, as SDL's renderer gives Direct3D 12 DXIL.
    ///
    /// @param device The device.
    /// @param name The shader.
    /// @return The shader, empty if SDL failed or there's no code the device takes; `SDL_GetError()` says why.
    [[nodiscard]] auto CreateShader(SDL_GPUDevice* device, ShaderName name) -> Shader
    {
        const auto info = InfoOf(name);
        const auto taken = SDL_GetGPUShaderFormats(device);
        // Every entry point is `main`, which SPIRV-Cross renames `main0` in MSL; Direct3D 12 ignores it.
        const std::array<std::tuple<SDL_GPUShaderFormat, std::span<const std::uint8_t>, const char*>, 4> choices{{
            {SDL_GPU_SHADERFORMAT_DXIL, info.blobs->dxil, "main"},
            {SDL_GPU_SHADERFORMAT_DXBC, info.blobs->dxbc, "main"},
            {SDL_GPU_SHADERFORMAT_SPIRV, info.blobs->spirv, "main"},
            {SDL_GPU_SHADERFORMAT_MSL, info.blobs->msl, "main0"},
        }};

        for (const auto& [format, code, entrypoint] : choices)
        {
            if ((taken & format) == 0 || std::empty(code))
            {
                continue;
            }

            const SDL_GPUShaderCreateInfo create{.code_size = std::size(code),
                                                 .code = std::data(code),
                                                 .entrypoint = entrypoint,
                                                 .format = format,
                                                 .stage = info.stage,
                                                 .num_samplers = info.samplers,
                                                 .num_storage_textures = 0,
                                                 .num_storage_buffers = 0,
                                                 .num_uniform_buffers = info.uniformBuffers,
                                                 .props = 0};
            return {device, SDL_CreateGPUShader(device, &create)};
        }

        // SDL's errors are printf-style, so a fixed message goes through as one.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        SDL_SetError("No shader code in a format the device takes");
        return {};
    }
}
