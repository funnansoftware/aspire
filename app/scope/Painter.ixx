module;

#include <SDL3/SDL.h>

export module scope.painter;

import std;
import aspire.core;
import scope.glow;
import scope.mesh;
import scope.picture;
import scope.tessellation;
import scope.typesetter;
import scope.world;

// The scope's picture, drawn with nothing but SDL's renderer: triangles for the shapes and SDL_ttf for the text.
namespace scope
{
    namespace
    {
        auto ToSdl(const Vertex& x) -> SDL_Vertex
        {
            return {.position = {.x = x.position.x, .y = x.position.y},
                    .color = {.r = x.tint.r, .g = x.tint.g, .b = x.tint.b, .a = x.tint.a},
                    .tex_coord = {.x = 0.0F, .y = 0.0F}};
        }
    }
}

export namespace scope
{
    /// @brief Draws the scope with SDL's renderer.
    ///
    /// Shapes are tessellated into triangles with antialiased edges and drawn with `SDL_RenderGeometry()`; text is
    /// drawn with SDL_ttf from glyphs it caches in textures. Circular clips are approximated, and the glow is a soft
    /// low-resolution pass stretched over the frame rather than a Gaussian blur.
    class Painter
    {
    public:
        /// @brief Creates a painter that draws with a renderer the caller owns.
        /// @param renderer The renderer. It must outlive this painter.
        explicit Painter(SDL_Renderer* renderer) : renderer_{renderer}, typesetter_{renderer}
        {
        }

        ~Painter()
        {
            for (auto* texture : glow_)
            {
                SDL_DestroyTexture(texture);
            }
        }

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

        [[nodiscard]] auto getMilliseconds() const -> double
        {
            return milliseconds_;
        }

        auto setCaption(std::string x) -> void
        {
            caption_ = std::move(x);
        }

        auto setFramesPerSecond(double x) -> void
        {
            framesPerSecond_ = x;
        }

        /// @brief Draws a frame of the scope over the renderer's whole output. The caller presents it.
        /// @param scope The scope to draw.
        /// @return `false` if SDL failed; `SDL_GetError()` says why.
        auto paint(const Scope& scope) -> bool
        {
            const auto started = std::chrono::steady_clock::now();
            auto width = 0;
            auto height = 0;

            if (!SDL_GetCurrentRenderOutputSize(renderer_, &width, &height))
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

            // The text's laid out, and the glow's drawn into its own textures, before the frame's begun. Some
            // renderers wait for the window's next frame to be free when the frame's first drawn to, and that wait
            // isn't painting.
            const auto drawn = typesetter_.prepare(picture.root, picture.scale) && SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND)
                               && drawGlow(picture, {.x = width, .y = height}) && clear()
                               && drawGroup(picture.root, {.scale = picture.scale, .clip = std::nullopt, .widen = 0.0F, .opacity = 1.0F})
                               && flush(mesh_);

            milliseconds_ = std::chrono::duration<double, std::milli>{std::chrono::steady_clock::now() - started}.count();
            return drawn;
        }

    private:
        // Fills the frame with the background. Just after the window's resized, the frame can be drawn into a
        // backbuffer that's new, and not yet the window's size, and what was in it would show at the edges.
        auto clear() -> bool
        {
            const auto background = TintOf(BackgroundColor, 1.0F);
            return SDL_SetRenderDrawColorFloat(renderer_, background.r, background.g, background.b, background.a) && SDL_RenderClear(renderer_);
        }

        // Draws a mesh's triangles and empties it.
        auto flush(Mesh& mesh) -> bool
        {
            if (mesh.empty())
            {
                return true;
            }

            vertices_.clear();
            std::ranges::transform(mesh.vertices(), std::back_inserter(vertices_), ToSdl);
            const auto indices = mesh.indices();
            const auto drawn = SDL_RenderGeometry(renderer_, nullptr, std::data(vertices_), static_cast<int>(std::size(vertices_)),
                                                  std::data(indices), static_cast<int>(std::size(indices)));
            mesh.clear();
            return drawn;
        }

        // Blurs the glowing groups' full-strength channels into one glow texture and their fainter ones into another.
        auto drawGlow(const Picture& picture, SDL_Point size) -> bool
        {
            glowing_ = false;
            glowShapes_.clear();
            CollectGlow(picture.root, glowShapes_);

            if (std::empty(glowShapes_))
            {
                return true;
            }

            // SDL's renderers can't blur, but they can draw a texture shifted, weighted and added: the glow's blurred
            // by adding shifted copies of its texture. Only texture draws are used, so it works with every renderer,
            // WebGL too.
            const auto plan = PlanGlow(picture.scale, size.x, size.y);

            if (!makeGlowTextures({.x = plan.width, .y = plan.height}))
            {
                return false;
            }

            const auto& taps = plan.taps;
            const Style style{.scale = plan.scale, .clip = std::nullopt, .widen = 0.0F, .opacity = GlowOpacity};
            auto& [full, fainter, across] = glow_;
            const auto drawn = blurChannels({.channels = Channels::Full, .into = full}, style, taps)
                               && blurChannels({.channels = Channels::Fainter, .into = fainter}, style, taps);

            // The fainter channels are added to the frame as they are, and the full ones screened, if the renderer can.
            glowing_ = drawn && SDL_SetTextureColorModFloat(full, 1.0F, 1.0F, 1.0F) && SDL_SetTextureColorModFloat(fainter, 1.0F, 1.0F, 1.0F)
                       && SDL_SetTextureBlendMode(fainter, SDL_BLENDMODE_ADD_PREMULTIPLIED)
                       && (SDL_SetTextureBlendMode(full, screen_) || SDL_SetTextureBlendMode(full, SDL_BLENDMODE_ADD_PREMULTIPLIED));
            return SDL_SetRenderTarget(renderer_, nullptr) && drawn;
        }

        struct Pass
        {
            Channels channels{};
            SDL_Texture* into{};
        };

        // Draws the glowing shapes' channels into a texture, which is blurred across into the spare texture and down
        // back into it. Drawn over a transparent target, the textures hold premultiplied color.
        auto blurChannels(const Pass& pass, Style style, const std::vector<Tap>& taps) -> bool
        {
            style.channels = pass.channels;
            glowMesh_.clear();

            for (const auto* shape : glowShapes_)
            {
                Tessellate(glowMesh_, *shape, style);
            }

            auto* across = glow_.back();
            return SDL_SetRenderTarget(renderer_, pass.into) && SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0) && SDL_RenderClear(renderer_)
                   && flush(glowMesh_) && blurGlow(pass.into, across, taps, {.x = 1.0F, .y = 0.0F})
                   && blurGlow(across, pass.into, taps, {.x = 0.0F, .y = 1.0F});
        }

        // Makes the glow textures at least a size, all the same size, and keeps the size of them the glow uses.
        auto makeGlowTextures(SDL_Point size) -> bool
        {
            glowSize_ = size;

            for (auto*& texture : glow_)
            {
                if (texture == nullptr || texture->w < glowSize_.x || texture->h < glowSize_.y)
                {
                    const auto width = GlowTextureSize(glowSize_.x, texture != nullptr ? texture->w : 0);
                    const auto height = GlowTextureSize(glowSize_.y, texture != nullptr ? texture->h : 0);
                    SDL_DestroyTexture(texture);
                    texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, width, height);

                    if (texture == nullptr || !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR))
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        // Blurs one glow texture into the other along an axis: copies of it, shifted and weighted by each tap, added
        // together.
        auto blurGlow(SDL_Texture* from, SDL_Texture* to, const std::vector<Tap>& taps, SDL_FPoint axis) -> bool
        {
            if (!SDL_SetRenderTarget(renderer_, to) || !SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0) || !SDL_RenderClear(renderer_)
                || !SDL_SetTextureBlendMode(from, SDL_BLENDMODE_ADD_PREMULTIPLIED))
            {
                return false;
            }

            const auto used = glowRect();

            for (const auto& tap : taps)
            {
                const SDL_FRect shifted{.x = axis.x * tap.offset, .y = axis.y * tap.offset, .w = used.w, .h = used.h};

                if (!SDL_SetTextureColorModFloat(from, tap.weight, tap.weight, tap.weight) || !SDL_RenderTexture(renderer_, from, &used, &shifted))
                {
                    return false;
                }
            }

            return true;
        }

        // The part of the glow textures the glow uses.
        [[nodiscard]] auto glowRect() const -> SDL_FRect
        {
            return {.x = 0.0F, .y = 0.0F, .w = static_cast<float>(glowSize_.x), .h = static_cast<float>(glowSize_.y)};
        }

        // NOLINTNEXTLINE(misc-no-recursion)
        auto drawGroup(const Group& group, Style style) -> bool
        {
            if (group.clip.has_value())
            {
                style.clip = ClipOf(*group.clip, style.scale);
            }

            for (const auto& item : group.items)
            {
                const auto drawn = std::visit(
                    aspire::core::Overloaded{
                        [&](const Shape& x)
                        {
                            Tessellate(mesh_, x, style);
                            return true;
                        },
                        [&](const Text& x) { return flush(mesh_) && typesetter_.draw(x, style.scale); },
                        [&](const Backdrop& x) { return drawBackdrop(x, style.scale); },
                        [&](const Rule& x)
                        {
                            TessellateRule(mesh_, x, style.scale);
                            return true;
                        },
                        // NOLINTNEXTLINE(misc-no-recursion)
                        [&](const Group& x) { return drawGroup(x, style); },
                    },
                    item.value);

                if (!drawn)
                {
                    return false;
                }
            }

            return true;
        }

        // The background, then the glow over it, under everything else.
        auto drawBackdrop(const Backdrop& x, float scale) -> bool
        {
            TessellateBackdrop(mesh_, x, scale);

            if (!flush(mesh_))
            {
                return false;
            }

            const auto& [full, fainter, across] = glow_;
            const auto used = glowRect();
            return !glowing_ || (SDL_RenderTexture(renderer_, fainter, &used, nullptr) && SDL_RenderTexture(renderer_, full, &used, nullptr));
        }

        SDL_Renderer* renderer_;
        Typesetter typesetter_;
        Mesh mesh_;
        Mesh glowMesh_;
        // The full and fainter channels of the glowing shapes, each blurred in place, and the blur across on the way.
        std::array<SDL_Texture*, 3> glow_{};
        SDL_Point glowSize_{};
        std::vector<const Shape*> glowShapes_;
        // Adds a color to what's there by the room left for it: a screen blend. SDL checks whether the renderer can
        // when a texture is set to it.
        SDL_BlendMode screen_{SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR, SDL_BLENDOPERATION_ADD,
                                                         SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD)};
        bool glowing_{};
        std::vector<SDL_Vertex> vertices_;
        std::string caption_;
        double milliseconds_{};
        double framesPerSecond_{};
    };
}
