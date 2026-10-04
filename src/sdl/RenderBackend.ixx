module;

#include <SDL3/SDL.h>

export module aspire.sdl:renderbackend;

import std;
import aspire.core;
import aspire.graphics;

export namespace aspire::sdl
{
    /// @brief Draws `aspire.graphics` frames with an SDL renderer.
    ///
    /// Textures are loaded from `.png` and `.bmp` files the first time a sprite names them, and kept until the
    /// backend is destroyed. A texture that fails to load is logged once and its sprites draw nothing.
    class RenderBackend : public aspire::graphics::RenderBackend
    {
    public:
        /// @brief Creates a backend that draws with a renderer the caller owns.
        /// @param renderer The renderer. It must outlive this backend.
        explicit RenderBackend(SDL_Renderer* renderer) : renderer_{renderer}
        {
        }

        ~RenderBackend() override
        {
            for (const auto& [source, texture] : textures_)
            {
                if (texture != nullptr)
                {
                    SDL_DestroyTexture(texture);
                }
            }
        }

        RenderBackend(const RenderBackend&) = delete;
        auto operator=(const RenderBackend&) -> RenderBackend& = delete;

        RenderBackend(RenderBackend&&) noexcept = delete;
        auto operator=(RenderBackend&&) noexcept -> RenderBackend& = delete;

        /// @brief Sets the folder relative texture paths resolve against.
        /// @param x The folder. Empty, the default, means the working directory.
        auto setAssetRoot(std::filesystem::path x) -> void
        {
            assetRoot_ = std::move(x);
        }

        /// @brief Sets how textures loaded from now on are sampled when scaled.
        /// @param x The scale mode. `SDL_SCALEMODE_PIXELART` by default, for crisp pixel art.
        auto setScaleMode(SDL_ScaleMode x) -> void
        {
            scaleMode_ = x;
        }

        /// @brief Clears to `clear`, draws every item in list order with its clip, and presents.
        /// @param list The frame's items, already sorted.
        /// @param clear The color to clear to.
        /// @return `false` if the renderer failed. The error is logged. A texture that fails to load doesn't count.
        auto submit(const aspire::graphics::DrawList& list, aspire::graphics::Color clear) -> bool override
        {
            // Blending lets translucent rectangles and tints show what's underneath.
            if (!SDL_SetRenderClipRect(renderer_, nullptr) || !SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND) || !setDrawColor(clear)
                || !SDL_RenderClear(renderer_))
            {
                return fail("Clear frame");
            }

            std::uint32_t current{};

            for (const auto& item : list.items())
            {
                const auto clip = clipOf(list, item);

                // An empty clip covers nothing, so the item can't show.
                if (clip.has_value() && (clip->w <= 0.0F || clip->h <= 0.0F))
                {
                    continue;
                }

                if (item.clip != current)
                {
                    if (!applyClip(clip, {.x = 1.0F, .y = 1.0F}))
                    {
                        return fail("Set clip");
                    }

                    current = item.clip;
                }

                if (!draw(item.primitive, clip))
                {
                    return fail("Draw item");
                }
            }

            if (!SDL_RenderPresent(renderer_))
            {
                return fail("Present frame");
            }

            return true;
        }

    private:
        // Lets textures_ be searched with a string_view, without building a std::string per sprite.
        struct StringHash
        {
            using is_transparent = void;

            auto operator()(std::string_view x) const noexcept -> std::size_t
            {
                return std::hash<std::string_view>{}(x);
            }
        };

        static auto log(std::string_view x) -> void
        {
            // SDL logging is printf-style, so preformatted text goes through a fixed format.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
            SDL_LogMessage(SDL_LOG_CATEGORY_RENDER, SDL_LOG_PRIORITY_ERROR, "%.*s", static_cast<int>(std::size(x)), std::data(x));
        }

        static auto fail(std::string_view operation) -> bool
        {
            log(std::format("{}: {}", operation, SDL_GetError()));
            return false;
        }

        static auto toSdl(aspire::graphics::Rect x) -> SDL_FRect
        {
            return {.x = x.x, .y = x.y, .w = x.w, .h = x.h};
        }

        // SDL clips to whole pixels, so round outward to the smallest rectangle that covers the clip.
        static auto toClip(aspire::graphics::Rect x) -> SDL_Rect
        {
            const auto left = static_cast<int>(std::floor(x.x));
            const auto top = static_cast<int>(std::floor(x.y));
            const auto right = static_cast<int>(std::ceil(x.x + x.w));
            const auto bottom = static_cast<int>(std::ceil(x.y + x.h));
            return {.x = left, .y = top, .w = right - left, .h = bottom - top};
        }

        static auto clipOf(const aspire::graphics::DrawList& list, const aspire::graphics::DrawItem& x) -> std::optional<aspire::graphics::Rect>
        {
            if (x.clip == 0)
            {
                return std::nullopt;
            }

            return *std::next(std::begin(list.clips()), static_cast<std::ptrdiff_t>(x.clip));
        }

        auto setDrawColor(aspire::graphics::Color x) -> bool
        {
            return SDL_SetRenderDrawColor(renderer_, x.r, x.g, x.b, x.a);
        }

        // Sets the clip for drawing at a render scale. SDL scales the clip with everything else, so divide it first.
        auto applyClip(const std::optional<aspire::graphics::Rect>& x, aspire::core::Vec2 scale) -> bool
        {
            if (!x.has_value())
            {
                return SDL_SetRenderClipRect(renderer_, nullptr);
            }

            const auto rect = toClip({.x = x->x / scale.x, .y = x->y / scale.y, .w = x->w / scale.x, .h = x->h / scale.y});
            return SDL_SetRenderClipRect(renderer_, &rect);
        }

        auto draw(const aspire::graphics::DrawPrimitive& x, const std::optional<aspire::graphics::Rect>& clip) -> bool
        {
            return std::visit(
                aspire::core::Overloaded{
                    [this](const aspire::graphics::DrawSprite& sprite) { return drawSprite(sprite); },
                    [this](const aspire::graphics::DrawRect& rect)
                    {
                        const auto bounds = toSdl(rect.bounds);
                        return setDrawColor(rect.color)
                               && (rect.filled ? SDL_RenderFillRect(renderer_, &bounds) : SDL_RenderRect(renderer_, &bounds));
                    },
                    [this, &clip](const aspire::graphics::DrawText& text) { return drawText(text, clip); },
                },
                x);
        }

        auto drawSprite(const aspire::graphics::DrawSprite& x) -> bool
        {
            auto* texture = load(x.source);

            // A texture that failed to load leaves a hole rather than failing the frame.
            if (texture == nullptr)
            {
                return true;
            }

            const auto region = toSdl(x.region);
            const auto bounds = toSdl(x.bounds);
            return SDL_SetTextureColorMod(texture, x.tint.r, x.tint.g, x.tint.b) && SDL_SetTextureAlphaMod(texture, x.tint.a)
                   && SDL_RenderTexture(renderer_, texture, &region, &bounds);
        }

        // SDL's debug font only draws at the render scale, so scaled text sets the scale around the call.
        auto drawText(const aspire::graphics::DrawText& x, const std::optional<aspire::graphics::Rect>& clip) -> bool
        {
            // SDL needs a null-terminated string; text_ keeps its storage between calls.
            text_.assign(x.text);

            if (!setDrawColor(x.color))
            {
                return false;
            }

            if (x.scale.x == 1.0F && x.scale.y == 1.0F)
            {
                return SDL_RenderDebugText(renderer_, x.position.x, x.position.y, text_.c_str());
            }

            const auto drawn = SDL_SetRenderScale(renderer_, x.scale.x, x.scale.y) && applyClip(clip, x.scale)
                               && SDL_RenderDebugText(renderer_, x.position.x / x.scale.x, x.position.y / x.scale.y, text_.c_str());

            // Restore the scale and clip whatever happened, so later items aren't affected.
            const auto restored = SDL_SetRenderScale(renderer_, 1.0F, 1.0F) && applyClip(clip, {.x = 1.0F, .y = 1.0F});
            return drawn && restored;
        }

        auto load(std::string_view source) -> SDL_Texture*
        {
            if (const auto found = textures_.find(source); found != std::end(textures_))
            {
                return found->second;
            }

            auto* texture = loadFile(source);
            textures_.emplace(std::string{source}, texture);
            return texture;
        }

        auto loadFile(std::string_view source) -> SDL_Texture*
        {
            // Joining an absolute path replaces the root, so absolute sources are used as they are.
            const auto path = assetRoot_ / std::filesystem::path{source};
            const auto utf8 = path.u8string();
            const std::string file{std::begin(utf8), std::end(utf8)};

            auto extension = path.extension().string();
            std::ranges::transform(extension, std::begin(extension), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            SDL_Surface* surface{};

            if (extension == ".png")
            {
                surface = SDL_LoadPNG(file.c_str());
            }
            else if (extension == ".bmp")
            {
                surface = SDL_LoadBMP(file.c_str());
            }
            else
            {
                log(std::format("Load texture {}: only .png and .bmp are supported", file));
                return nullptr;
            }

            if (surface == nullptr)
            {
                log(std::format("Load texture {}: {}", file, SDL_GetError()));
                return nullptr;
            }

            auto* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            SDL_DestroySurface(surface);

            if (texture == nullptr)
            {
                log(std::format("Create texture {}: {}", file, SDL_GetError()));
                return nullptr;
            }

            SDL_SetTextureScaleMode(texture, scaleMode_);
            return texture;
        }

        SDL_Renderer* renderer_;
        std::filesystem::path assetRoot_;
        SDL_ScaleMode scaleMode_{SDL_SCALEMODE_PIXELART};

        // Null entries remember textures that failed to load, so they're only tried and logged once.
        std::unordered_map<std::string, SDL_Texture*, StringHash, std::equal_to<>> textures_;
        std::string text_;
    };
}
