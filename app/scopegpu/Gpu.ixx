module;

#include <SDL3/SDL.h>

export module scopegpu.gpu;

import std;

// SDL GPU objects owned by RAII handles, and the few ways scopegpu makes them.
export namespace scopegpu
{
    /// @brief Releases a buffer.
    /// @param device The device that made it.
    /// @param x The buffer.
    auto Release(SDL_GPUDevice* device, SDL_GPUBuffer* x) -> void
    {
        SDL_ReleaseGPUBuffer(device, x);
    }

    /// @brief Releases a transfer buffer.
    /// @param device The device that made it.
    /// @param x The transfer buffer.
    auto Release(SDL_GPUDevice* device, SDL_GPUTransferBuffer* x) -> void
    {
        SDL_ReleaseGPUTransferBuffer(device, x);
    }

    /// @brief Releases a texture.
    /// @param device The device that made it.
    /// @param x The texture.
    auto Release(SDL_GPUDevice* device, SDL_GPUTexture* x) -> void
    {
        SDL_ReleaseGPUTexture(device, x);
    }

    /// @brief Releases a sampler.
    /// @param device The device that made it.
    /// @param x The sampler.
    auto Release(SDL_GPUDevice* device, SDL_GPUSampler* x) -> void
    {
        SDL_ReleaseGPUSampler(device, x);
    }

    /// @brief Releases a shader.
    /// @param device The device that made it.
    /// @param x The shader.
    auto Release(SDL_GPUDevice* device, SDL_GPUShader* x) -> void
    {
        SDL_ReleaseGPUShader(device, x);
    }

    /// @brief Releases a graphics pipeline.
    /// @param device The device that made it.
    /// @param x The pipeline.
    auto Release(SDL_GPUDevice* device, SDL_GPUGraphicsPipeline* x) -> void
    {
        SDL_ReleaseGPUGraphicsPipeline(device, x);
    }

    /// @brief An SDL GPU object, released with the device that made it when the handle goes.
    /// @tparam T The object's type, such as `SDL_GPUTexture`, which a `Release()` overload takes.
    template <typename T>
    class Owned
    {
    public:
        Owned() = default;

        /// @brief Takes ownership of an object.
        /// @param device The device that made it. It must outlive the handle.
        /// @param x The object, or null for an empty handle.
        Owned(SDL_GPUDevice* device, T* x) : device_{device}, x_{x}
        {
        }

        ~Owned()
        {
            reset();
        }

        Owned(const Owned&) = delete;
        auto operator=(const Owned&) -> Owned& = delete;

        Owned(Owned&& other) noexcept : device_{std::exchange(other.device_, nullptr)}, x_{std::exchange(other.x_, nullptr)}
        {
        }

        auto operator=(Owned&& other) noexcept -> Owned&
        {
            if (this != &other)
            {
                reset();
                device_ = std::exchange(other.device_, nullptr);
                x_ = std::exchange(other.x_, nullptr);
            }

            return *this;
        }

        /// @brief Releases the object, if there is one, and empties the handle.
        auto reset() -> void
        {
            if (x_ != nullptr)
            {
                Release(device_, x_);
                x_ = nullptr;
            }
        }

        /// @brief Reports the object, which the handle still owns.
        /// @return The object, or null if the handle's empty.
        [[nodiscard]] auto get() const -> T*
        {
            return x_;
        }

        /// @brief Reports whether the handle holds an object.
        /// @return `true` if it does.
        [[nodiscard]] explicit operator bool() const
        {
            return x_ != nullptr;
        }

    private:
        SDL_GPUDevice* device_{};
        T* x_{};
    };

    using Buffer = Owned<SDL_GPUBuffer>;
    using TransferBuffer = Owned<SDL_GPUTransferBuffer>;
    using Texture = Owned<SDL_GPUTexture>;
    using Sampler = Owned<SDL_GPUSampler>;
    using Shader = Owned<SDL_GPUShader>;
    using Pipeline = Owned<SDL_GPUGraphicsPipeline>;

    /// @brief A size in pixels.
    struct Extent
    {
        std::uint32_t width{};
        std::uint32_t height{};

        auto operator==(const Extent&) const -> bool = default;
    };

    /// @brief Makes a 2D texture with one level and one sample.
    /// @param device The device.
    /// @param format The texture's format.
    /// @param size Its size, at least 1 by 1.
    /// @param usage How it's used, such as `SDL_GPU_TEXTUREUSAGE_SAMPLER`.
    /// @return The texture, empty if SDL failed; `SDL_GetError()` says why.
    [[nodiscard]] auto CreateTexture(SDL_GPUDevice* device, SDL_GPUTextureFormat format, Extent size, SDL_GPUTextureUsageFlags usage) -> Texture
    {
        const SDL_GPUTextureCreateInfo info{.type = SDL_GPU_TEXTURETYPE_2D,
                                            .format = format,
                                            .usage = usage,
                                            .width = std::max(size.width, 1U),
                                            .height = std::max(size.height, 1U),
                                            .layer_count_or_depth = 1,
                                            .num_levels = 1,
                                            .sample_count = SDL_GPU_SAMPLECOUNT_1,
                                            .props = 0};
        return {device, SDL_CreateGPUTexture(device, &info)};
    }

    /// @brief Makes a buffer.
    /// @param device The device.
    /// @param usage How it's used, such as `SDL_GPU_BUFFERUSAGE_VERTEX`.
    /// @param size Its size in bytes.
    /// @return The buffer, empty if SDL failed.
    [[nodiscard]] auto CreateBuffer(SDL_GPUDevice* device, SDL_GPUBufferUsageFlags usage, std::uint32_t size) -> Buffer
    {
        const SDL_GPUBufferCreateInfo info{.usage = usage, .size = size, .props = 0};
        return {device, SDL_CreateGPUBuffer(device, &info)};
    }

    /// @brief Makes a transfer buffer.
    /// @param device The device.
    /// @param usage Whether it uploads or downloads.
    /// @param size Its size in bytes.
    /// @return The buffer, empty if SDL failed.
    [[nodiscard]] auto CreateTransferBuffer(SDL_GPUDevice* device, SDL_GPUTransferBufferUsage usage, std::uint32_t size) -> TransferBuffer
    {
        const SDL_GPUTransferBufferCreateInfo info{.usage = usage, .size = size, .props = 0};
        return {device, SDL_CreateGPUTransferBuffer(device, &info)};
    }

    /// @brief Makes a sampler that clamps to the texture's edge, as SDL's renderer samples.
    /// @param device The device.
    /// @param filter Nearest or linear, for minifying, magnifying and between levels alike.
    /// @return The sampler, empty if SDL failed.
    [[nodiscard]] auto CreateSampler(SDL_GPUDevice* device, SDL_GPUFilter filter) -> Sampler
    {
        const SDL_GPUSamplerCreateInfo info{
            .min_filter = filter,
            .mag_filter = filter,
            .mipmap_mode = filter == SDL_GPU_FILTER_LINEAR ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
            .mip_lod_bias = 0.0F,
            .max_anisotropy = 0.0F,
            .compare_op = SDL_GPU_COMPAREOP_INVALID,
            .min_lod = 0.0F,
            .max_lod = 0.0F,
            .enable_anisotropy = false,
            .enable_compare = false,
            .padding1 = 0,
            .padding2 = 0,
            .props = 0};
        return {device, SDL_CreateGPUSampler(device, &info)};
    }

    /// @brief Rounds a byte count up to a multiple of an alignment.
    /// @param x The byte count.
    /// @param alignment A power of two.
    /// @return The rounded count.
    [[nodiscard]] constexpr auto AlignUp(std::uint32_t x, std::uint32_t alignment) -> std::uint32_t
    {
        return (x + alignment - 1) & ~(alignment - 1);
    }
}
