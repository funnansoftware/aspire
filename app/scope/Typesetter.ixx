module;

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

export module scope.typesetter;

import std;
import scope.mesh;
import scope.picture;
import scope.world;

// A picture's text, drawn with SDL's renderer and SDL_ttf.
namespace scope
{
    namespace
    {
        // The picture sizes text in points at 96 dots per inch, as the web does, and SDL_ttf at 72, so SDL_ttf's are
        // scaled to match.
        constexpr float PointsToPixels = 96.0F / 72.0F;

        // Big enough that the font's metrics, which SDL_ttf gives in whole pixels, are measured to a fine fraction.
        constexpr float ProbeSize = 1000.0F;
        constexpr char ProbeGlyph = '0';

        // UTF-8's bytes that continue a character, rather than start one.
        constexpr unsigned char ContinuationMask = 0xC0;
        constexpr unsigned char ContinuationByte = 0x80;
    }
}

export namespace scope
{
    /// @brief Draws a picture's text with SDL's renderer, from glyphs SDL_ttf caches in textures.
    ///
    /// Text is drawn a glyph at a time, each placed from the text's anchor and rounded to a pixel on its own, so text
    /// moves as smoothly as the window when it's resized.
    class Typesetter
    {
    public:
        /// @brief Makes a typesetter that draws with a renderer the caller owns, with no font yet.
        /// @param renderer The renderer. It must outlive the typesetter.
        explicit Typesetter(SDL_Renderer* renderer) : renderer_{renderer}
        {
        }

        ~Typesetter()
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
        }

        Typesetter(const Typesetter&) = delete;
        auto operator=(const Typesetter&) -> Typesetter& = delete;
        Typesetter(Typesetter&&) noexcept = delete;
        auto operator=(Typesetter&&) noexcept -> Typesetter& = delete;

        /// @brief Loads the font text is drawn in. Without one, there's no text.
        /// @param data A TrueType font. The typesetter keeps a copy.
        /// @return `true` if it loaded; otherwise `SDL_GetError()` says why.
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
                // Data that isn't a font is no font at all.
                fontData_.clear();
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

        /// @brief Reports whether there's a font to draw text in.
        /// @return `true` once a font's loaded.
        [[nodiscard]] auto ready() const -> bool
        {
            return engine_ != nullptr && !std::empty(fontData_);
        }

        /// @brief Reports the font's advance, for laying out text.
        /// @return The advance as a share of the font's size.
        [[nodiscard]] auto advance() const -> float
        {
            return advance_;
        }

        /// @brief Lays out the text in a group, and in the groups in it, before any of it's drawn.
        ///
        /// SDL_ttf adds a glyph to its texture the first time it's drawn at a size, so after the window's resized
        /// every glyph's added again. SDL's renderer sends what it's been asked to draw to the GPU before a texture
        /// it's drawing from changes, so text drawn a glyph at a time would be sent a glyph at a time, each in a
        /// render pass of its own. Laid out before the frame's drawn, the glyphs are added while nothing's drawn from
        /// the texture yet.
        ///
        /// @param group The group.
        /// @param scale Target pixels per logical unit, as the text will be drawn at.
        /// @return `false` if SDL_ttf failed to lay out a glyph; `SDL_GetError()` says why.
        // NOLINTNEXTLINE(misc-no-recursion)
        auto prepare(const Group& group, float scale) -> bool
        {
            for (const auto& item : group.items)
            {
                if (const auto* text = std::get_if<Text>(&item.value); text != nullptr && !layOut(*text, scale))
                {
                    return false;
                }

                if (const auto* inner = std::get_if<Group>(&item.value); inner != nullptr && !prepare(*inner, scale))
                {
                    return false;
                }
            }

            return true;
        }

        /// @brief Draws a text a glyph at a time.
        ///
        /// SDL_ttf would round each glyph's place to a pixel from the text's left end, so text anchored on its right
        /// would wobble by a pixel as the window's resized. Placed from the anchor, each glyph's rounded on its own and
        /// moves as smoothly as the window. The font's monospaced, so every glyph advances the same.
        ///
        /// @param x The text, in logical units.
        /// @param scale Target pixels per logical unit.
        /// @return `false` if SDL failed to draw a glyph; `SDL_GetError()` says why. Without a font, or at a size the
        /// font can't be opened or set to, nothing's drawn and the result is `true`.
        auto draw(const Text& x, float scale) -> bool
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

    private:
        // The font's metrics, each a share of its size in pixels.
        struct FontMetrics
        {
            float advance{};
            float ascent{};
            float height{};
            float lineSkip{};
        };

        // Opens the font at a size in pixels.
        auto openFont(float pixels) -> TTF_Font*
        {
            // The stream reads the typesetter's copy of the font, which outlives every font opened from it.
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

        // Lays out a text's glyphs as they'll be drawn, adding any that are new at this size to SDL_ttf's texture.
        auto layOut(const Text& x, float scale) -> bool
        {
            auto* face = font(x, scale);

            if (face == nullptr || engine_ == nullptr)
            {
                return true;
            }

            const auto* next = x.text.c_str();
            auto remaining = std::size(x.text);

            while (remaining > 0)
            {
                const auto* start = next;
                const auto codepoint = SDL_StepUTF8(&next, &remaining);

                if (codepoint == '\n' || codepoint == ' ')
                {
                    continue;
                }

                auto* text = glyph(x, face, {start, static_cast<std::size_t>(next - start)}, codepoint);

                if (text == nullptr || !TTF_UpdateText(text))
                {
                    return false;
                }
            }

            return true;
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

        SDL_Renderer* renderer_;
        bool ttf_{};
        TTF_TextEngine* engine_{};
        std::vector<char> fontData_;
        std::map<float, TTF_Font*> fonts_;
        std::map<std::pair<float, std::uint32_t>, TTF_Text*> glyphs_;
        FontMetrics metrics_;
        float advance_{DefaultAdvance};
    };
}
