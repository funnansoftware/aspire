module;

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

export module scope.painter;

import std;
import aspire.core;
import scope.mesh;
import scope.picture;
import scope.world;

// The scope's picture, drawn with nothing but SDL's renderer: triangles for the shapes and SDL_ttf for the text.
namespace scope
{
    namespace
    {
        constexpr float Half = 0.5F;
        constexpr float FullTurn = 360.0F;

        // The glow is a Gaussian blur of the glowing shapes, added to what's there. SDL's renderers
        // can't blur, but they can draw a texture shifted, weighted and added: the shapes are drawn into a smaller
        // texture, blurred across and then down by adding shifted copies of it, and stretched back over the frame. Only
        // texture draws are used, so it works with every renderer, WebGL too.
        constexpr float GlowSigma = 6.0F;
        // The blur's cut off two sigmas out, so the halo is a little narrower than a whole Gaussian's, and ends
        // sooner.
        constexpr float GlowReach = 2.0F;
        // How many of the smaller texture's pixels the blur's sigma spans: enough that the blurred texture stretches
        // back up smoothly, and few enough that the blur needs only a few copies.
        constexpr float GlowTexelSigma = 3.0F;
        // Drawing into the smaller texture and stretching it back blur a little themselves, by a pixel-wide box and a
        // two-pixel tent: a variance of 1/12 + 1/6 of a pixel squared, which the copies leave out.
        constexpr float StretchVariance = 0.25F;
        constexpr float GlowOpacity = 1.0F;
        // The glow textures grow in steps this many pixels wide, and never shrink, so dragging a window's edge doesn't
        // make new ones every frame.
        constexpr int GlowTextureStep = 128;

        // The picture sizes text in points at 96 dots per inch, as the web does, and SDL_ttf at 72, so SDL_ttf's are
        // scaled to match.
        constexpr float PointsToPixels = 96.0F / 72.0F;

        // Big enough that the font's metrics, which SDL_ttf gives in whole pixels, are measured to a fine fraction.
        constexpr float ProbeSize = 1000.0F;
        constexpr char ProbeGlyph = '0';

        // UTF-8's bytes that continue a character, rather than start one.
        constexpr unsigned char ContinuationMask = 0xC0;
        constexpr unsigned char ContinuationByte = 0x80;

        auto Scaled(Vec2 x, float scale) -> Vec2
        {
            return {.x = x.x * scale, .y = x.y * scale};
        }

        // A shape's outline as lines in pixels, each with whether it closes.
        auto Outlines(const Geometry& geometry, float scale) -> std::vector<std::pair<std::vector<Vec2>, bool>>
        {
            std::vector<std::pair<std::vector<Vec2>, bool>> lines;

            std::visit(
                aspire::core::Overloaded{
                    [&](const Circle& x)
                    {
                        auto points = FlattenArc(Scaled(x.center, scale), x.radius * scale, {0.0F, FullTurn});
                        points.pop_back();
                        lines.emplace_back(std::move(points), true);
                    },
                    [&](const Arc& x) { lines.emplace_back(FlattenArc(Scaled(x.center, scale), x.radius * scale, {x.from, x.sweep}), false); },
                    [&](const Polyline& x)
                    {
                        std::vector<Vec2> points;
                        points.reserve(std::size(x.points));
                        std::ranges::transform(x.points, std::back_inserter(points), [scale](Vec2 p) { return Scaled(p, scale); });
                        lines.emplace_back(std::move(points), x.closed);
                    },
                    [&](const Segments& x)
                    {
                        for (std::size_t i = 0; i + 1 < std::size(x.ends); i += 2)
                        {
                            lines.push_back({{Scaled(x.ends.at(i), scale), Scaled(x.ends.at(i + 1), scale)}, false});
                        }
                    },
                },
                geometry);

            return lines;
        }

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
        explicit Painter(SDL_Renderer* renderer) : renderer_{renderer}
        {
        }

        ~Painter()
        {
            closeFonts();

            if (engine_ != nullptr)
            {
                TTF_DestroyRendererTextEngine(engine_);
            }

            if (ttf_)
            {
                TTF_Quit();
            }

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
            if (!ttf_)
            {
                ttf_ = TTF_Init();
            }

            if (ttf_ && engine_ == nullptr)
            {
                engine_ = TTF_CreateRendererTextEngine(renderer_);
            }

            if (engine_ == nullptr)
            {
                return false;
            }

            // Text and fonts from a font loaded before read its data, so they go first.
            closeFonts();
            fontData_.assign(std::begin(data), std::end(data));
            auto* probe = openFont(ProbeSize);

            if (probe == nullptr)
            {
                return false;
            }

            // Text is laid out from the font's metrics, and the labels' gaps are cut from the rings by how far the
            // monospaced font advances.
            auto advance = 0;

            if (TTF_GetGlyphMetrics(probe, ProbeGlyph, nullptr, nullptr, nullptr, nullptr, &advance) && advance > 0)
            {
                metrics_ = {.advance = static_cast<float>(advance) / ProbeSize,
                            .ascent = static_cast<float>(TTF_GetFontAscent(probe)) / ProbeSize,
                            .height = static_cast<float>(TTF_GetFontHeight(probe)) / ProbeSize,
                            .lineSkip = static_cast<float>(TTF_GetFontLineSkip(probe)) / ProbeSize};
                advance_ = metrics_.advance * PointsToPixels;
            }

            TTF_CloseFont(probe);
            return true;
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
                                                 .text = engine_ != nullptr && !std::empty(fontData_),
                                                 .advance = advance_,
                                                 .caption = caption_,
                                                 .milliseconds = milliseconds_,
                                                 .framesPerSecond = framesPerSecond_});

            if (picture.scale <= 0.0F)
            {
                return clear();
            }

            // The glow's drawn into its own textures before the frame's begun. Some renderers wait for the window's
            // next frame to be free when the frame's first drawn to, and that wait isn't painting.
            const auto drawn = SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND) && drawGlow(picture, {.x = width, .y = height}) && clear()
                               && drawGroup(picture.root, {.scale = picture.scale, .clip = std::nullopt, .widen = 0.0F, .opacity = 1.0F})
                               && flush(mesh_);

            milliseconds_ = std::chrono::duration<double, std::milli>{std::chrono::steady_clock::now() - started}.count();
            return drawn;
        }

    private:
        // A circle in pixels that shapes are kept inside.
        struct Clip
        {
            Vec2 center;
            float radius{};
        };

        // Which of a color's channels are drawn. The glow adds light in straight color, clamped at full: a channel at
        // full strength only gains the room the background leaves it, as with a screen blend, while a fainter one gains
        // all its light, as with an add. The glow is drawn twice, a pass for each, so each can be blended to match.
        enum class Channels : std::uint8_t
        {
            All,
            Full,
            Fainter,
        };

        // How a shape is turned into triangles: where it is, how it's widened, how strong it is, and which channels.
        struct Style
        {
            float scale{};
            std::optional<Clip> clip;
            // Added to every stroke's width, in pixels.
            float widen{};
            float opacity{1.0F};
            Channels channels{Channels::All};
        };

        static auto tinted(Color color, float opacity, Channels channels) -> Tint
        {
            const auto keep = [channels](std::uint8_t value) -> std::uint8_t
            {
                const auto full = value == Color::Opaque;
                return channels == Channels::All || (channels == Channels::Full) == full ? value : 0;
            };

            return TintOf({.r = keep(color.r), .g = keep(color.g), .b = keep(color.b), .a = color.a}, opacity);
        }

        static auto inside(const Clip& clip, Vec2 x) -> bool
        {
            return std::hypot(x.x - clip.center.x, x.y - clip.center.y) <= clip.radius;
        }

        // Whether a closed shape is kept inside a clip: SDL can only clip to rectangles, so it's kept or dropped
        // whole by its middle.
        static auto kept(const Style& style, const std::vector<Vec2>& points) -> bool
        {
            if (!style.clip.has_value() || std::empty(points))
            {
                return true;
            }

            Vec2 middle{};
            for (const auto& p : points)
            {
                middle = {.x = middle.x + p.x, .y = middle.y + p.y};
            }

            return inside(*style.clip, Scaled(middle, 1.0F / static_cast<float>(std::size(points))));
        }

        // Strokes an outline, dashed if the pen says, with open lines cut at the clip's edge.
        static auto strokeOutline(Mesh& mesh, const Pen& pen, std::vector<Vec2> points, bool closed, const Style& style, float opacity) -> void
        {
            const auto width = (pen.width * style.scale) + style.widen;
            const auto tint = tinted(pen.color, opacity, style.channels);
            std::vector<std::vector<Vec2>> pieces;

            if (pen.dashed)
            {
                if (closed)
                {
                    points.push_back(points.front());
                }

                pieces = Dash(points, {DashPattern.at(0) * style.scale, DashPattern.at(1) * style.scale});
                closed = false;
            }
            else
            {
                pieces.push_back(std::move(points));
            }

            for (const auto& piece : pieces)
            {
                if (!style.clip.has_value() || closed)
                {
                    mesh.stroke(piece, closed, width, tint);
                    continue;
                }

                for (const auto& part : ClipToCircle(piece, style.clip->center, style.clip->radius))
                {
                    mesh.stroke(part, false, width, tint);
                }
            }
        }

        // Adds a shape's triangles to a mesh.
        static auto tessellate(Mesh& mesh, const Shape& shape, const Style& style) -> void
        {
            const auto opacity = shape.opacity * style.opacity;

            for (auto& [points, closed] : Outlines(shape.geometry, style.scale))
            {
                if (closed && !kept(style, points))
                {
                    continue;
                }

                // A shape stroked over its outline only needs its inside filled; otherwise its edge fades.
                if (shape.fill.has_value() && closed && shape.pen.has_value())
                {
                    mesh.fillFan(points, tinted(*shape.fill, opacity, style.channels));
                }
                else if (shape.fill.has_value() && closed)
                {
                    mesh.fillConvex(points, tinted(*shape.fill, opacity, style.channels));
                }

                if (shape.pen.has_value())
                {
                    strokeOutline(mesh, *shape.pen, std::move(points), closed, style, opacity);
                }
            }
        }

        // The font's metrics, each a share of its size in pixels.
        struct FontMetrics
        {
            float advance{};
            float ascent{};
            float height{};
            float lineSkip{};
        };

        // Fills the frame with the background. Just after the window's resized, the frame can be drawn into a
        // backbuffer that's new, and not yet the window's size, and what was in it would show at the edges.
        auto clear() -> bool
        {
            const auto background = TintOf(BackgroundColor, 1.0F);
            return SDL_SetRenderDrawColorFloat(renderer_, background.r, background.g, background.b, background.a) && SDL_RenderClear(renderer_);
        }

        // Opens the font at a size in pixels.
        auto openFont(float pixels) -> TTF_Font*
        {
            // The stream reads the painter's copy of the font, which outlives every font opened from it.
            auto* stream = SDL_IOFromConstMem(std::data(fontData_), std::size(fontData_));
            auto* opened = stream != nullptr ? TTF_OpenFontIO(stream, true, pixels) : nullptr;

            if (opened != nullptr)
            {
                // Without hinting, glyphs keep their outlines' shapes at every size, as the rings and tracks do.
                TTF_SetFontHinting(opened, TTF_HINTING_NONE);
            }

            return opened;
        }

        // The font for text, sized in pixels to the frame. There's one for each size of text, opened the first time
        // it's asked for and sized to the frame after that, so resizing the window doesn't open fonts. Its text
        // follows it to the new size. Sizes aren't rounded, and SDL_ttf lays text out with HarfBuzz, which keeps the
        // glyphs' advances to a fraction of a pixel, so text grows and shrinks smoothly with the window.
        auto font(const Text& x, float scale) -> TTF_Font*
        {
            const auto size = x.size * scale * PointsToPixels;
            auto found = fonts_.find(x.size);

            if (found == std::end(fonts_))
            {
                found = fonts_.emplace(x.size, openFont(size)).first;
            }

            auto* face = found->second;
            return face != nullptr && TTF_SetFontSize(face, size) ? face : nullptr;
        }

        // Lets go of the text and the fonts it's drawn in.
        auto closeFonts() -> void
        {
            for (const auto& [key, glyph] : glyphs_)
            {
                TTF_DestroyText(glyph);
            }

            glyphs_.clear();

            for (const auto& [size, face] : fonts_)
            {
                TTF_CloseFont(face);
            }

            fonts_.clear();
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
            collectGlow(picture.root);

            if (std::empty(glowShapes_))
            {
                return true;
            }

            // The smaller textures' resolution, as a share of the frame's, so the blur spans GlowTexelSigma of their pixels.
            const auto sigma = GlowSigma * picture.scale;
            const auto resolution = std::min(1.0F, GlowTexelSigma / sigma);

            if (!makeGlowTextures({.x = static_cast<int>(std::ceil(static_cast<float>(size.x) * resolution)),
                                   .y = static_cast<int>(std::ceil(static_cast<float>(size.y) * resolution))}))
            {
                return false;
            }

            const auto texelSigma = sigma * resolution;
            const auto taps = GaussianTaps(std::sqrt(std::max((texelSigma * texelSigma) - StretchVariance, StretchVariance)), GlowReach);
            const Style style{.scale = picture.scale * resolution, .clip = std::nullopt, .widen = 0.0F, .opacity = GlowOpacity};
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
                tessellate(glowMesh_, *shape, style);
            }

            auto* across = glow_.back();
            return SDL_SetRenderTarget(renderer_, pass.into) && SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0) && SDL_RenderClear(renderer_)
                   && flush(glowMesh_) && blurGlow(pass.into, across, taps, {.x = 1.0F, .y = 0.0F})
                   && blurGlow(across, pass.into, taps, {.x = 0.0F, .y = 1.0F});
        }

        // Makes the glow textures at least a size, all the same size, and keeps the size of them the glow uses.
        auto makeGlowTextures(SDL_Point size) -> bool
        {
            glowSize_ = {.x = std::max(size.x, 1), .y = std::max(size.y, 1)};
            const auto stepped = [](int x) { return ((x + GlowTextureStep - 1) / GlowTextureStep) * GlowTextureStep; };

            for (auto*& texture : glow_)
            {
                if (texture == nullptr || texture->w < glowSize_.x || texture->h < glowSize_.y)
                {
                    const auto width = stepped(std::max(glowSize_.x, texture != nullptr ? texture->w : 0));
                    const auto height = stepped(std::max(glowSize_.y, texture != nullptr ? texture->h : 0));
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

        // Finds the glowing groups' shapes. It walks the picture's groups, which nest only a few deep.
        // NOLINTNEXTLINE(misc-no-recursion)
        auto collectGlow(const Group& group) -> void
        {
            for (const auto& item : group.items)
            {
                if (const auto* inner = std::get_if<Group>(&item.value); inner != nullptr)
                {
                    if (inner->glow)
                    {
                        for (const auto& glowing : inner->items)
                        {
                            if (const auto* shape = std::get_if<Shape>(&glowing.value); shape != nullptr)
                            {
                                glowShapes_.push_back(shape);
                            }
                        }
                    }
                    else
                    {
                        collectGlow(*inner);
                    }
                }
            }
        }

        auto drawGroup(const Group& group, Style style) -> bool
        {
            if (group.clip.has_value())
            {
                style.clip = Clip{.center = Scaled(group.clip->center, style.scale), .radius = group.clip->radius * style.scale};
            }

            for (const auto& item : group.items)
            {
                const auto drawn = std::visit(
                    aspire::core::Overloaded{
                        [&](const Shape& x)
                        {
                            tessellate(mesh_, x, style);
                            return true;
                        },
                        [&](const Text& x) { return flush(mesh_) && drawText(x, style.scale); },
                        [&](const Backdrop& x) { return drawBackdrop(x, style.scale); },
                        [&](const Rule& x)
                        {
                            drawRule(x, style.scale);
                            return true;
                        },
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
            const auto outer = TintOf(x.outer, 1.0F);
            const auto from = Scaled(x.corner, scale);
            const Vec2 to{.x = from.x + (x.width * scale), .y = from.y + (x.height * scale)};
            mesh_.fillQuad({{{.position = from, .tint = outer},
                             {.position = {.x = to.x, .y = from.y}, .tint = outer},
                             {.position = to, .tint = outer},
                             {.position = {.x = from.x, .y = to.y}, .tint = outer}}});
            mesh_.fillRadial(Scaled(x.center, scale), x.radius * scale, {TintOf(x.inner, 1.0F), outer});

            if (!flush(mesh_))
            {
                return false;
            }

            const auto& [full, fainter, across] = glow_;
            const auto used = glowRect();
            return !glowing_ || (SDL_RenderTexture(renderer_, fainter, &used, nullptr) && SDL_RenderTexture(renderer_, full, &used, nullptr));
        }

        auto drawRule(const Rule& x, float scale) -> void
        {
            const auto from = Scaled(x.from, scale);
            const auto length = x.length * scale;
            const auto half = x.width * scale * Half;
            const auto solid = TintOf(x.color, 1.0F);
            const auto clear = TintOf(x.color, 0.0F);
            mesh_.fillQuad({{{.position = {.x = from.x, .y = from.y - half}, .tint = solid},
                             {.position = {.x = from.x + length, .y = from.y - half}, .tint = clear},
                             {.position = {.x = from.x + length, .y = from.y + half}, .tint = clear},
                             {.position = {.x = from.x, .y = from.y + half}, .tint = solid}}});
        }

        // A glyph of text of a size, laid out by SDL_ttf in that size's font the first time it's drawn.
        auto glyph(const Text& x, TTF_Font* face, std::string_view bytes, std::uint32_t codepoint) -> TTF_Text*
        {
            const auto key = std::pair{x.size, codepoint};

            if (const auto found = glyphs_.find(key); found != std::end(glyphs_))
            {
                return found->second;
            }

            auto* created = TTF_CreateText(engine_, face, std::data(bytes), std::size(bytes));
            glyphs_.emplace(key, created);
            return created;
        }

        // Draws text a glyph at a time. SDL_ttf would round each glyph's place to a pixel from the text's left end,
        // so text anchored on its right would wobble by a pixel as the window's resized. Placed from the anchor,
        // each glyph's rounded on its own and moves as smoothly as the window. The font's monospaced, so every glyph
        // advances the same.
        auto drawText(const Text& x, float scale) -> bool
        {
            auto* face = font(x, scale);

            if (face == nullptr || engine_ == nullptr)
            {
                return true;
            }

            const auto pixels = x.size * scale * PointsToPixels;
            const auto advance = metrics_.advance * pixels;
            const auto lineSkip = metrics_.lineSkip * pixels;
            auto lines = 1;
            auto columns = 0;
            auto longest = 0;

            for (const auto c : x.text)
            {
                // Bytes that continue a character don't count.
                if (c == '\n')
                {
                    ++lines;
                    columns = 0;
                }
                else if ((static_cast<unsigned char>(c) & ContinuationMask) != ContinuationByte)
                {
                    longest = std::max(longest, ++columns);
                }
            }

            const auto left = (x.at.x * scale) - (x.anchor.x * advance * static_cast<float>(longest));
            const auto height = (metrics_.height * pixels) + (lineSkip * static_cast<float>(lines - 1));
            const auto baseline = (x.at.y * scale) - (x.anchor.y * height) + (metrics_.ascent * pixels);
            const auto tint = TintOf(x.color, x.opacity);
            const auto ascent = static_cast<float>(TTF_GetFontAscent(face));
            const auto* next = x.text.c_str();
            auto remaining = std::size(x.text);
            auto line = 0;
            auto column = 0;

            while (remaining > 0)
            {
                const auto* start = next;
                const auto codepoint = SDL_StepUTF8(&next, &remaining);
                const auto bytes = std::string_view{start, static_cast<std::size_t>(next - start)};

                if (codepoint == '\n')
                {
                    ++line;
                    column = 0;
                    continue;
                }

                // Glyphs are drawn at whole pixels, where the cached textures land sharply, with their baselines
                // rounded rather than their tops.
                const auto at = SDL_FPoint{.x = std::round(left + (advance * static_cast<float>(column))),
                                           .y = std::round(baseline + (lineSkip * static_cast<float>(line))) - ascent};
                ++column;

                if (codepoint == ' ')
                {
                    continue;
                }

                auto* text = glyph(x, face, bytes, codepoint);

                if (text == nullptr || !TTF_SetTextColorFloat(text, tint.r, tint.g, tint.b, tint.a) || !TTF_DrawRendererText(text, at.x, at.y))
                {
                    return false;
                }
            }

            return true;
        }

        SDL_Renderer* renderer_;
        bool ttf_{};
        TTF_TextEngine* engine_{};
        std::vector<char> fontData_;
        std::map<float, TTF_Font*> fonts_;
        std::map<std::pair<float, std::uint32_t>, TTF_Text*> glyphs_;
        FontMetrics metrics_;
        float advance_{DefaultAdvance};
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
