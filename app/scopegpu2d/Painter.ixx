module;

#include <SDL3/SDL.h>

export module scopegpu2d.painter;

import std;
import aspire.core;
import scope.glow;
import scope.mesh;
import scope.picture;
import scope.tessellation;
import scope.typesetter;
import scope.world;
import scopegpu.gpu;
import scopegpu.shadercode;
import scopegpu.shaders;

// The scope's picture, drawn with SDL's renderer as scope draws it, but with scopegpu's shaders for the glow and the
// backdrop, set on the renderer's draws as custom GPU render state.
namespace scopegpu2d
{
    namespace
    {
        auto ToSdl(const scope::Vertex& x) -> SDL_Vertex
        {
            return {.position = {.x = x.position.x, .y = x.position.y},
                    .color = {.r = x.tint.r, .g = x.tint.g, .b = x.tint.b, .a = x.tint.a},
                    .tex_coord = {.x = 0.0F, .y = 0.0F}};
        }

        auto Rgba(scope::Color x) -> std::array<float, 4>
        {
            const auto tint = scope::TintOf(x, 1.0F);
            return {tint.r, tint.g, tint.b, tint.a};
        }

        // The GPU texture behind one of the renderer's, to bind to a render state.
        auto GpuTextureOf(SDL_Texture* x) -> SDL_GPUTexture*
        {
            return static_cast<SDL_GPUTexture*>(SDL_GetPointerProperty(SDL_GetTextureProperties(x), SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
        }
    }
}

export namespace scopegpu2d
{
    /// @brief Draws the scope with SDL's GPU renderer and scopegpu's shaders.
    ///
    /// Shapes and text are drawn as scope draws them. The glow and the backdrop use the same shader code as scopegpu,
    /// through the renderer's custom render states: the glowing shapes are tessellated once and split into their full
    /// and fainter channels by a shader, each set is blurred across and down by a shader that takes every tap in one
    /// draw, and one shader shades the backdrop and composites the glow over it. The renderer draws into one target at
    /// a time, so the two channel sets are drawn and blurred apart, where scopegpu draws them together.
    class Painter
    {
    public:
        /// @brief Makes a painter that draws with SDL's GPU renderer, which the caller owns.
        /// @param renderer The renderer: SDL's GPU renderer, whose device takes scopegpu's shaders. It must outlive the
        /// painter.
        /// @param frame The texture each frame is drawn into, or null for the renderer's window. It must outlive the
        /// painter.
        /// @return The painter, or null if the renderer isn't SDL's GPU renderer or SDL failed; `SDL_GetError()` says
        /// why.
        [[nodiscard]] static auto create(SDL_Renderer* renderer, SDL_Texture* frame) -> std::unique_ptr<Painter>
        {
            auto* device = SDL_GetGPURendererDevice(renderer);

            if (device == nullptr)
            {
                return nullptr;
            }

            auto painter = std::unique_ptr<Painter>(new (std::nothrow) Painter(renderer, device, frame));
            return painter != nullptr && painter->initialize() ? std::move(painter) : nullptr;
        }

        ~Painter() = default;
        Painter(const Painter&) = delete;
        auto operator=(const Painter&) -> Painter& = delete;
        Painter(Painter&&) noexcept = delete;
        auto operator=(Painter&&) noexcept -> Painter& = delete;

        /// @brief Loads the font the labels are drawn in. Without one, they're left out.
        /// @param data A TrueType font. The painter keeps a copy.
        /// @return `true` if it loaded.
        auto loadFont(std::span<const char> data) -> bool
        {
            return typesetter_.loadFont(data);
        }

        /// @brief Reports how long the last frame took to paint, before the caller presents it.
        /// @return Milliseconds.
        [[nodiscard]] auto getMilliseconds() const -> double
        {
            return milliseconds_;
        }

        /// @brief Sets the line under the title, saying what draws the scope.
        /// @param x The caption.
        auto setCaption(std::string x) -> void
        {
            caption_ = std::move(x);
        }

        /// @brief Sets the frame rate the readouts show.
        /// @param x Frames per second.
        auto setFramesPerSecond(double x) -> void
        {
            framesPerSecond_ = x;
        }

        /// @brief Draws a frame of the scope over the whole frame. The caller presents it.
        /// @param scope The scope to draw.
        /// @return `false` if SDL failed; `SDL_GetError()` says why.
        auto paint(const scope::Scope& scope) -> bool
        {
            const auto started = std::chrono::steady_clock::now();
            auto width = 0;
            auto height = 0;

            if (!SDL_SetRenderTarget(renderer_, frame_) || !SDL_GetCurrentRenderOutputSize(renderer_, &width, &height))
            {
                return false;
            }

            const auto picture = Compose(scope, {.width = static_cast<std::uint32_t>(std::max(width, 0)),
                                                 .height = static_cast<std::uint32_t>(std::max(height, 0)),
                                                 .text = typesetter_.ready(),
                                                 .advance = typesetter_.advance(),
                                                 .caption = caption_,
                                                 .milliseconds = milliseconds_,
                                                 .framesPerSecond = framesPerSecond_});

            if (picture.scale <= 0.0F)
            {
                return clear();
            }

            // The text's laid out, and the glow's drawn into its own textures, before the frame's begun, as scope
            // draws it.
            const auto drawn = typesetter_.prepare(picture.root, picture.scale) && SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND)
                               && drawGlow(picture, width, height) && clear()
                               && drawGroup(picture.root, {.scale = picture.scale, .clip = std::nullopt, .widen = 0.0F, .opacity = 1.0F})
                               && flush(mesh_);

            milliseconds_ = std::chrono::duration<double, std::milli>{std::chrono::steady_clock::now() - started}.count();
            return drawn;
        }

    private:
        // As blur.glsl was compiled with.
        static constexpr std::size_t MaxTaps = scopegpu::shadercode::MaxTaps;

        struct TextureDeleter
        {
            auto operator()(SDL_Texture* x) const -> void
            {
                SDL_DestroyTexture(x);
            }
        };

        struct StateDeleter
        {
            auto operator()(SDL_GPURenderState* x) const -> void
            {
                SDL_DestroyGPURenderState(x);
            }
        };

        using Texture = std::unique_ptr<SDL_Texture, TextureDeleter>;
        using State = std::unique_ptr<SDL_GPURenderState, StateDeleter>;

        // The shaders' uniform blocks, laid out as std140 lays them out: glow2d.frag's, blur2d.frag's and
        // composite2d.frag's, the last two as scopegpu's blur and composite lay theirs out.
        struct GlowUniforms
        {
            std::array<float, 4> channels{};
        };

        struct BlurUniforms
        {
            std::array<float, 4> axis{};
            std::array<float, 4> used{};
            std::array<std::array<float, 4>, MaxTaps> taps{};
        };

        struct BackdropUniforms
        {
            std::array<float, 4> inner{};
            std::array<float, 4> outer{};
            std::array<float, 4> disc{};
            std::array<float, 4> glow{};
            std::array<float, 4> rect{};
            std::array<float, 4> background{};
        };

        Painter(SDL_Renderer* renderer, SDL_GPUDevice* device, SDL_Texture* frame)
            : renderer_{renderer}, device_{device}, frame_{frame}, typesetter_{renderer}
        {
        }

        // Makes the shaders, and the render states that don't change with the glow's textures.
        auto initialize() -> bool
        {
            glowShader_ = scopegpu::CreateShader(device_, scopegpu::ShaderName::Glow2dFragment);
            blurShader_ = scopegpu::CreateShader(device_, scopegpu::ShaderName::Blur2dFragment);
            compositeShader_ = scopegpu::CreateShader(device_, scopegpu::ShaderName::Composite2dFragment);
            sampler_ = scopegpu::CreateSampler(device_, SDL_GPU_FILTER_LINEAR);
            // Drawn in the glow's place when nothing glows, since the composite always takes two textures.
            placeholder_ = createTexture(1, 1);

            if (!glowShader_ || !blurShader_ || !compositeShader_ || !sampler_ || !placeholder_)
            {
                return false;
            }

            fullChannels_ = createState(glowShader_.get(), {});
            fainterChannels_ = createState(glowShader_.get(), {});
            blurAcross_ = createState(blurShader_.get(), {});
            blurDown_ = createState(blurShader_.get(), {});
            compositeBare_ = createState(compositeShader_.get(), GpuTextureOf(placeholder_.get()));

            const GlowUniforms full{.channels = {1.0F, 0.0F, 0.0F, 0.0F}};
            const GlowUniforms fainter{.channels = {0.0F, 0.0F, 0.0F, 0.0F}};
            return fullChannels_ && fainterChannels_ && blurAcross_ && blurDown_ && compositeBare_
                   && SDL_SetGPURenderStateFragmentUniforms(fullChannels_.get(), 0, &full, sizeof(full))
                   && SDL_SetGPURenderStateFragmentUniforms(fainterChannels_.get(), 0, &fainter, sizeof(fainter));
        }

        // A render state for a fragment shader, with a second texture bound after the one each draw binds, if any.
        auto createState(SDL_GPUShader* shader, SDL_GPUTexture* second) -> State
        {
            const SDL_GPUTextureSamplerBinding binding{.texture = second, .sampler = sampler_.get()};
            const SDL_GPURenderStateCreateInfo info{.fragment_shader = shader,
                                                    .num_sampler_bindings = second != nullptr ? 1 : 0,
                                                    .sampler_bindings = second != nullptr ? &binding : nullptr,
                                                    .num_storage_textures = 0,
                                                    .storage_textures = nullptr,
                                                    .num_storage_buffers = 0,
                                                    .storage_buffers = nullptr,
                                                    .props = 0};
            return State{SDL_CreateGPURenderState(renderer_, &info)};
        }

        // A target texture the glow's drawn into: 8-bit channels, as scope's are, sampled linearly, and drawn over
        // what's there without blending, since the shaders work out every pixel.
        auto createTexture(int width, int height) -> Texture
        {
            Texture texture{SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, width, height)};

            if (!texture || !SDL_SetTextureScaleMode(texture.get(), SDL_SCALEMODE_LINEAR)
                || !SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_NONE))
            {
                return {};
            }

            return texture;
        }

        // Fills the frame with the background. Just after the window's resized, the frame can be drawn into a
        // backbuffer that's new, and not yet the window's size, and what was in it would show at the edges.
        auto clear() -> bool
        {
            const auto background = scope::TintOf(scope::BackgroundColor, 1.0F);
            return SDL_SetRenderDrawColorFloat(renderer_, background.r, background.g, background.b, background.a) && SDL_RenderClear(renderer_);
        }

        // Draws a mesh's triangles.
        auto draw(const scope::Mesh& mesh) -> bool
        {
            vertices_.clear();
            std::ranges::transform(mesh.vertices(), std::back_inserter(vertices_), ToSdl);
            const auto& indices = mesh.indices();
            return SDL_RenderGeometry(renderer_, nullptr, std::data(vertices_), static_cast<int>(std::size(vertices_)), std::data(indices),
                                      static_cast<int>(std::size(indices)));
        }

        // Draws a mesh's triangles and empties it.
        auto flush(scope::Mesh& mesh) -> bool
        {
            if (mesh.empty())
            {
                return true;
            }

            const auto drawn = draw(mesh);
            mesh.clear();
            return drawn;
        }

        // Makes the glow textures at least a size, all the same size, as scope does, and the composite's render state
        // with the fainter texture they now hold.
        auto makeGlowTextures(const scope::GlowPlan& plan) -> bool
        {
            glowUsed_ = {.x = plan.width, .y = plan.height};

            if (full_ && glowSize_.x >= plan.width && glowSize_.y >= plan.height)
            {
                return true;
            }

            glowSize_ = {.x = scope::GlowTextureSize(plan.width, glowSize_.x), .y = scope::GlowTextureSize(plan.height, glowSize_.y)};
            // The state binds the fainter texture, so it goes before the texture does.
            compositeGlow_.reset();
            full_ = createTexture(glowSize_.x, glowSize_.y);
            fainter_ = createTexture(glowSize_.x, glowSize_.y);
            across_ = createTexture(glowSize_.x, glowSize_.y);

            if (!full_ || !fainter_ || !across_)
            {
                full_.reset();
                return false;
            }

            compositeGlow_ = createState(compositeShader_.get(), GpuTextureOf(fainter_.get()));
            return static_cast<bool>(compositeGlow_);
        }

        // Draws the glowing shapes' full channels into one glow texture and their fainter ones into another, and
        // blurs each across and back down.
        auto drawGlow(const scope::Picture& picture, int width, int height) -> bool
        {
            glowing_ = false;
            glowShapes_.clear();
            scope::CollectGlow(picture.root, glowShapes_);

            if (std::empty(glowShapes_))
            {
                return true;
            }

            const auto plan = scope::PlanGlow(picture.scale, width, height);

            if (std::size(plan.taps) > MaxTaps)
            {
                // SDL's errors are printf-style, so the count goes through as an argument.
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
                SDL_SetError("The glow's blur has %zu taps, more than the %zu its shader takes", std::size(plan.taps), MaxTaps);
                return false;
            }

            if (!makeGlowTextures(plan))
            {
                return false;
            }

            // Tessellated once, with every channel, for the shader to split.
            glowMesh_.clear();
            const scope::Style style{.scale = plan.scale, .clip = std::nullopt, .widen = 0.0F, .opacity = scope::GlowOpacity};

            for (const auto* shape : glowShapes_)
            {
                scope::Tessellate(glowMesh_, *shape, style);
            }

            BlurUniforms blur;
            const auto& taps = plan.taps;

            for (std::size_t i = 0; i < std::size(taps); ++i)
            {
                blur.taps.at(i) = {taps.at(i).offset, taps.at(i).weight, 0.0F, 0.0F};
            }

            const auto texel = std::array{1.0F / static_cast<float>(glowSize_.x), 1.0F / static_cast<float>(glowSize_.y)};
            blur.used = {static_cast<float>(plan.width), static_cast<float>(plan.height), static_cast<float>(std::size(taps)), 0.0F};
            blur.axis = {1.0F, 0.0F, texel.at(0), texel.at(1)};
            auto set = SDL_SetGPURenderStateFragmentUniforms(blurAcross_.get(), 0, &blur, sizeof(blur));
            blur.axis = {0.0F, 1.0F, texel.at(0), texel.at(1)};
            set = set && SDL_SetGPURenderStateFragmentUniforms(blurDown_.get(), 0, &blur, sizeof(blur));

            // A tap's copy reaches no further than its shift past the used part, so the blur shades only that far, and
            // the clear leaves the rest of the texture empty, as scope's copies do.
            const auto reach = std::ranges::max(taps | std::views::transform([](const scope::Tap& x) { return std::abs(x.offset); }));
            const auto beyond = [reach](int used, int size)
            { return static_cast<int>(std::min(static_cast<float>(size), std::ceil(static_cast<float>(used) + reach))); };
            const SDL_Rect acrossArea{.x = 0, .y = 0, .w = beyond(plan.width, glowSize_.x), .h = plan.height};
            const SDL_Rect downArea{.x = 0, .y = 0, .w = plan.width, .h = beyond(plan.height, glowSize_.y)};

            const auto drawn =
                set && drawChannels(full_.get(), fullChannels_.get()) && blurInto(full_.get(), across_.get(), blurAcross_.get(), acrossArea)
                && blurInto(across_.get(), full_.get(), blurDown_.get(), downArea) && drawChannels(fainter_.get(), fainterChannels_.get())
                && blurInto(fainter_.get(), across_.get(), blurAcross_.get(), acrossArea)
                && blurInto(across_.get(), fainter_.get(), blurDown_.get(), downArea);

            glowing_ = drawn;
            return SDL_SetRenderTarget(renderer_, frame_) && drawn;
        }

        // Draws the glowing shapes into a cleared texture, keeping the channels a render state says. Drawn over a
        // transparent target, the texture holds premultiplied color.
        auto drawChannels(SDL_Texture* into, SDL_GPURenderState* channels) -> bool
        {
            return SDL_SetRenderTarget(renderer_, into) && SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0) && SDL_RenderClear(renderer_)
                   && SDL_SetGPURenderState(renderer_, channels) && draw(glowMesh_) && SDL_SetGPURenderState(renderer_, nullptr);
        }

        // Blurs one glow texture into another along the axis a render state says, drawing the source over the target's
        // reach with the blur's shader.
        auto blurInto(SDL_Texture* from, SDL_Texture* to, SDL_GPURenderState* axis, const SDL_Rect& area) -> bool
        {
            return SDL_SetRenderTarget(renderer_, to) && SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0) && SDL_RenderClear(renderer_)
                   && SDL_SetRenderClipRect(renderer_, &area) && SDL_SetGPURenderState(renderer_, axis)
                   && SDL_RenderTexture(renderer_, from, nullptr, nullptr) && SDL_SetGPURenderState(renderer_, nullptr)
                   && SDL_SetRenderClipRect(renderer_, nullptr);
        }

        // NOLINTNEXTLINE(misc-no-recursion)
        auto drawGroup(const scope::Group& group, scope::Style style) -> bool
        {
            if (group.clip.has_value())
            {
                style.clip = scope::ClipOf(*group.clip, style.scale);
            }

            for (const auto& item : group.items)
            {
                const auto drawn = std::visit(
                    aspire::core::Overloaded{
                        [&](const scope::Shape& x)
                        {
                            scope::Tessellate(mesh_, x, style);
                            return true;
                        },
                        [&](const scope::Text& x) { return flush(mesh_) && typesetter_.draw(x, style.scale); },
                        [&](const scope::Backdrop& x) { return flush(mesh_) && drawBackdrop(x, style.scale); },
                        [&](const scope::Rule& x)
                        {
                            scope::TessellateRule(mesh_, x, style.scale);
                            return true;
                        },
                        // NOLINTNEXTLINE(misc-no-recursion)
                        [&](const scope::Group& x) { return drawGroup(x, style); },
                    },
                    item.value);

                if (!drawn)
                {
                    return false;
                }
            }

            return true;
        }

        // Shades the backdrop over the whole frame, with the glow over it if anything glows: the full glow texture
        // drawn with the composite's shader, which samples the fainter one too.
        auto drawBackdrop(const scope::Backdrop& x, float scale) -> bool
        {
            auto width = 0;
            auto height = 0;

            if (!SDL_GetCurrentRenderOutputSize(renderer_, &width, &height))
            {
                return false;
            }

            // The glow's stretched from the part of its textures it uses over the whole frame, as scope draws it.
            const auto stretch = [](int used, int length, int texture)
            { return static_cast<float>(used) / static_cast<float>(length) / static_cast<float>(texture); };

            BackdropUniforms uniforms;
            uniforms.inner = Rgba(x.inner);
            uniforms.outer = Rgba(x.outer);
            uniforms.disc = {x.center.x * scale, x.center.y * scale, x.radius * scale, glowing_ ? 1.0F : 0.0F};
            uniforms.glow = glowing_ ? std::array{stretch(glowUsed_.x, width, glowSize_.x), stretch(glowUsed_.y, height, glowSize_.y), 0.0F, 0.0F}
                                     : std::array{0.0F, 0.0F, 0.0F, 0.0F};
            uniforms.rect = {x.corner.x * scale, x.corner.y * scale, (x.corner.x + x.width) * scale, (x.corner.y + x.height) * scale};
            uniforms.background = Rgba(scope::BackgroundColor);

            auto* state = glowing_ ? compositeGlow_.get() : compositeBare_.get();
            auto* texture = glowing_ ? full_.get() : placeholder_.get();
            return SDL_SetGPURenderStateFragmentUniforms(state, 0, &uniforms, sizeof(uniforms)) && SDL_SetGPURenderState(renderer_, state)
                   && SDL_RenderTexture(renderer_, texture, nullptr, nullptr) && SDL_SetGPURenderState(renderer_, nullptr);
        }

        SDL_Renderer* renderer_;
        SDL_GPUDevice* device_;
        SDL_Texture* frame_;
        scope::Typesetter typesetter_;
        // The shaders and sampler go after the render states that use them, and the textures after the states that
        // bind them, so they're declared first.
        scopegpu::Shader glowShader_;
        scopegpu::Shader blurShader_;
        scopegpu::Shader compositeShader_;
        scopegpu::Sampler sampler_;
        Texture placeholder_;
        // The glow's full and fainter channels, each blurred in place, and the blur across on the way.
        Texture full_;
        Texture fainter_;
        Texture across_;
        SDL_Point glowSize_{};
        SDL_Point glowUsed_{};
        State fullChannels_;
        State fainterChannels_;
        State blurAcross_;
        State blurDown_;
        State compositeBare_;
        State compositeGlow_;
        scope::Mesh mesh_;
        scope::Mesh glowMesh_;
        std::vector<const scope::Shape*> glowShapes_;
        bool glowing_{};
        std::vector<SDL_Vertex> vertices_;
        std::string caption_;
        double milliseconds_{};
        double framesPerSecond_{};
    };
}
