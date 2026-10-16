module;

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_textengine.h>
#include <SDL3_ttf/SDL_ttf.h>

export module scopegpu.glyphs;

import std;
import scope.mesh;
import scope.picture;
import scope.world;
import scopegpu.gpu;
import scopegpu.shelves;

namespace scopegpu
{
    // The atlas starts as big as SDL_ttf's, and doubles whenever a frame's glyphs don't fit. Unlike the constants
    // below, this one isn't private to the file: `Glyphs` starts its atlas at it, and GCC won't let an exported class
    // name what only this file can see.
    inline constexpr std::uint32_t SmallestAtlas = 1024;

    namespace
    {
        // The picture sizes text in points at 96 dots per inch, as the web does, and SDL_ttf at 72.
        constexpr float PointsToPixels = 96.0F / 72.0F;

        // Big enough that the font's metrics, which SDL_ttf gives in whole pixels, are measured to a fine fraction.
        constexpr float ProbeSize = 1000.0F;
        constexpr char ProbeGlyph = '0';

        // UTF-8's bytes that continue a character, rather than start one.
        constexpr unsigned char ContinuationMask = 0xC0;
        constexpr unsigned char ContinuationByte = 0x80;

        // The atlas doubles up to this size, and no further.
        constexpr std::uint32_t LargestAtlas = 8192;
        // Glyphs are thrown away whenever the text's resized, so the atlas is cleared once it's half full, between
        // frames, rather than run out of room partway through one.
        constexpr float AtlasClearShare = 0.5F;
        constexpr std::uint32_t BytesPerPixel = 4;

        // Direct3D 12 copies texture rows from 256-byte pitches at 512-byte offsets; SDL stages anything else
        // through a copy of its own.
        constexpr std::uint32_t RowAlignment = 256;
        constexpr std::uint32_t RegionAlignment = 512;

        // The text engine's hooks. Glyphs are packed into the atlas when they're drawn, so a text needs nothing of its
        // own, but SDL_ttf wants something to hold.
        auto SDLCALL CreateEngineText(void* userdata, TTF_Text* text) -> bool
        {
            text->internal->engine_text = userdata;
            return true;
        }

        auto SDLCALL DestroyEngineText(void* /*userdata*/, TTF_Text* text) -> void
        {
            text->internal->engine_text = nullptr;
        }
    }
}

export namespace scopegpu
{
    using scope::Vec2;

    /// @brief A corner of a glyph's quad.
    struct GlyphVertex
    {
        /// In target pixels.
        Vec2 position;
        /// Straight alpha.
        scope::Tint tint;
        /// Where the corner is in the atlas, from 0 to 1.
        Vec2 uv;
    };

    /// @brief A glyph of a data block as the GPU draws it: an instance of a glyph's quad, which the GPU places at a
    /// whole pixel as `Glyphs::add()` places glyphs.
    struct BlockGlyph
    {
        /// Where the block's first line starts and its baseline is, in target pixels, and the glyph's column and line.
        std::array<float, 4> place{};
        /// The glyph's quad from its pen position: left, top, width and height, in pixels.
        std::array<float, 4> quad{};
        /// Where it is in the atlas, from 0 to 1: left, top, right and bottom.
        std::array<float, 4> uv{};
        /// Straight alpha.
        std::array<float, 4> color{};
    };

    /// @brief How a run of data blocks' glyphs step from where their blocks start: by a column, by a line, and up by
    /// the font's ascent, in pixels.
    struct BlockMetrics
    {
        float advance{};
        float lineSkip{};
        float ascent{};
    };

    /// @brief A glyph's pixels to copy into the atlas: where they are in the staged bytes, and where they go.
    struct GlyphUpload
    {
        /// From the start of the staged bytes; a multiple of 512.
        std::uint32_t offset{};
        /// Bytes from one row to the next; a multiple of 256.
        std::uint32_t pitch{};
        Slot slot;
    };

    /// @brief Text laid out by SDL_ttf as quads of glyphs from an atlas the GPU samples.
    ///
    /// SDL_ttf lays out and rasterizes the glyphs as it does for its renderer, so they're the same pixels in the same
    /// places as scope's: B8G8R8A8 texels, white with coverage in alpha, drawn a pixel to a texel. SDL_ttf's own GPU
    /// engine would upload each new glyph in a command buffer of its own; this one stages them for the frame's copy
    /// pass instead.
    ///
    /// Each frame, `begin()`, `add()` the texts and `addBlocks()` the data blocks in drawing order, copy `staged()` into
    /// `atlas()` as `uploads()` say, and draw `indices()` of `vertices()` and an instance of a glyph's quad for each of
    /// `blockGlyphs()`.
    class Glyphs
    {
    public:
        /// @brief Makes glyphs for a device, with no font yet.
        /// @param device The device. It must outlive these glyphs.
        explicit Glyphs(SDL_GPUDevice* device) : device_{device}, packer_{SmallestAtlas}
        {
            engine_.version = sizeof(engine_);
            engine_.userdata = this;
            engine_.CreateText = CreateEngineText;
            engine_.DestroyText = DestroyEngineText;
        }

        ~Glyphs()
        {
            closeFonts();

            if (ttf_)
            {
                TTF_Quit();
            }
        }

        Glyphs(const Glyphs&) = delete;
        auto operator=(const Glyphs&) -> Glyphs& = delete;
        Glyphs(Glyphs&&) noexcept = delete;
        auto operator=(Glyphs&&) noexcept -> Glyphs& = delete;

        /// @brief Loads the font text is drawn in. Without one, there's no text.
        /// @param data A TrueType font. The glyphs keep a copy.
        /// @return `true` if it loaded; otherwise `SDL_GetError()` says why.
        auto loadFont(std::span<const char> data) -> bool
        {
            if (!ttf_)
            {
                ttf_ = TTF_Init();
            }

            if (!atlas_)
            {
                atlas_ = createAtlas(device_, atlasSize_);
            }

            if (!ttf_ || !atlas_)
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

        /// @brief Reports whether there's a font to draw text in.
        /// @return `true` once a font's loaded.
        [[nodiscard]] auto ready() const -> bool
        {
            return ttf_ && atlas_ && !std::empty(fontData_);
        }

        /// @brief Reports the font's advance, for laying out text.
        /// @return The advance as a share of the font's size.
        [[nodiscard]] auto advance() const -> float
        {
            return advance_;
        }

        /// @brief Starts a frame's text: clears the atlas if it's filling up, and grows it if the glyphs added since
        /// the last `begin()` didn't fit.
        /// @return `false` if the atlas couldn't grow; `SDL_GetError()` says why. Without a font there's no text, and
        /// nothing to fail.
        auto begin() -> bool
        {
            vertices_.clear();
            indices_.clear();
            uploads_.clear();
            staged_.clear();
            blockGlyphs_.clear();

            if (!ready())
            {
                return true;
            }

            if (full_ && atlasSize_ < LargestAtlas)
            {
                atlasSize_ *= 2;
                atlas_ = createAtlas(device_, atlasSize_);
                packer_ = ShelfPacker{atlasSize_};
            }

            if (full_ || packer_.used() > AtlasClearShare)
            {
                clearAtlas();
                full_ = false;
            }

            return static_cast<bool>(atlas_);
        }

        /// @brief Adds a text's glyphs, a quad each.
        ///
        /// Glyphs are placed as scope places them: a glyph at a time from the text's anchor, each at a whole pixel,
        /// with its baseline rounded rather than its top. A glyph that won't fit in the atlas is left out, and
        /// `overflowed()` says so: the next `begin()` grows the atlas, so the frame's text can be laid out again.
        ///
        /// @param x The text, in logical units.
        /// @param scale Target pixels per logical unit.
        /// @return `false` if SDL_ttf failed; `SDL_GetError()` says why.
        auto add(const scope::Text& x, float scale) -> bool
        {
            auto* face = font(x.size, scale);

            if (face == nullptr)
            {
                return true;
            }

            const auto metrics = metricsOf(x.size, scale, face);
            const auto start = place(x.at, x.anchor, extentOf(x.text), metrics, scale);
            const auto tint = scope::TintOf(x.color, x.opacity);

            return forEachGlyph(x, face,
                                [&](int column, int line, const TTF_CopyOperation& operation)
                                {
                                    const Vec2 at{.x = std::round(start.x + (metrics.advance * static_cast<float>(column))),
                                                  .y = std::round(start.y + (metrics.lineSkip * static_cast<float>(line))) - metrics.ascent};
                                    addGlyph(operation, at, tint);
                                });
        }

        /// @brief Adds data blocks' glyphs, each as an instance of a glyph's quad, laid out as `add()` lays out their
        /// texts.
        ///
        /// A block's glyphs are laid out once and kept, by the track it belongs to, until its text changes, or the
        /// atlas or the scale does: after that, each frame only places them where the block is.
        ///
        /// @param x The blocks, in logical units.
        /// @param scale Target pixels per logical unit.
        /// @return How the blocks' glyphs step from where each starts, which they're drawn with; empty if SDL_ttf
        /// failed, and `SDL_GetError()` says why. Without a font there's nothing to add, and nothing fails.
        auto addBlocks(const scope::DataBlocks& x, float scale) -> std::optional<BlockMetrics>
        {
            auto* face = font(x.size, scale);

            if (face == nullptr || std::empty(x.blocks))
            {
                return BlockMetrics{};
            }

            const auto metrics = metricsOf(x.size, scale, face);

            for (const auto& block : x.blocks)
            {
                const auto* laidOut = layOut(block, scale, face);

                if (laidOut == nullptr)
                {
                    return std::nullopt;
                }

                const auto start = place(block.at, x.anchor, laidOut->extent, metrics, scale);
                const auto tint = scope::TintOf(block.color, 1.0F);
                const std::array<float, 4> colored{1.0F, 1.0F, 1.0F, tint.a};

                for (const auto& cell : laidOut->cells)
                {
                    blockGlyphs_.push_back({.place = {start.x, start.y, cell.column, cell.line},
                                            .quad = cell.quad,
                                            .uv = cell.uv,
                                            .color = cell.colored ? colored : std::array{tint.r, tint.g, tint.b, tint.a}});
                }
            }

            return BlockMetrics{.advance = metrics.advance, .lineSkip = metrics.lineSkip, .ascent = metrics.ascent};
        }

        /// @brief The frame's data blocks' glyphs, so far.
        /// @return The instances, one for each glyph.
        [[nodiscard]] auto blockGlyphs() const -> const std::vector<BlockGlyph>&
        {
            return blockGlyphs_;
        }

        /// @brief Reports whether a glyph didn't fit in the atlas since the last `begin()`, and was left out.
        /// @return `true` if one didn't.
        [[nodiscard]] auto overflowed() const -> bool
        {
            return full_;
        }

        /// @brief The frame's glyphs' corners, so far.
        /// @return The corners, four to a glyph.
        [[nodiscard]] auto vertices() const -> const std::vector<GlyphVertex>&
        {
            return vertices_;
        }

        /// @brief The frame's glyphs' triangles, so far.
        /// @return Indices into `vertices()`, six to a glyph.
        [[nodiscard]] auto indices() const -> const std::vector<std::uint32_t>&
        {
            return indices_;
        }

        /// @brief The new glyphs' pixels, staged for copying into the atlas.
        /// @return The bytes, laid out as `uploads()` says.
        [[nodiscard]] auto staged() const -> std::span<const std::byte>
        {
            return staged_;
        }

        /// @brief Where the new glyphs' pixels are in `staged()`, and where they go in the atlas.
        /// @return One upload for each new glyph.
        [[nodiscard]] auto uploads() const -> const std::vector<GlyphUpload>&
        {
            return uploads_;
        }

        /// @brief The atlas the glyphs are drawn from.
        /// @return B8G8R8A8 texels, to be sampled at their nearest, or null without a font.
        [[nodiscard]] auto atlas() const -> SDL_GPUTexture*
        {
            return atlas_.get();
        }

    private:
        // A glyph rasterized at one of a font's sizes: the font's generation changes with its size.
        struct GlyphKey
        {
            TTF_Font* font{};
            std::uint32_t generation{};
            std::uint32_t index{};

            auto operator==(const GlyphKey&) const -> bool = default;
        };

        struct GlyphKeyHash
        {
            auto operator()(const GlyphKey& x) const noexcept -> std::size_t
            {
                // An odd multiplier spreads each part's bits over the others'.
                constexpr std::size_t mix = 0x9E3779B97F4A7C15ULL;
                auto hash = std::hash<TTF_Font*>{}(x.font);
                hash = (hash * mix) + x.generation;
                hash = (hash * mix) + x.index;
                return hash;
            }
        };

        // Where a glyph is in the atlas, and whether its pixels are coverage, to be tinted, or color of their own.
        struct PackedGlyph
        {
            Slot slot;
            TTF_ImageType type{};
        };

        // The font's metrics, each a share of its size in pixels.
        struct FontMetrics
        {
            float advance{};
            float ascent{};
            float height{};
            float lineSkip{};
        };

        // A size of text's metrics in pixels at a scale: its size, how far a column and a line step, and the ascent of
        // its font, sized to it, which glyphs are placed from.
        struct TextMetrics
        {
            float pixels{};
            float advance{};
            float lineSkip{};
            float ascent{};
        };

        // How many lines a text has, and how many characters its longest has.
        struct TextExtent
        {
            int lines{};
            int longest{};
        };

        // A glyph of a laid-out data block: its column and line, and its quad and where it is in the atlas.
        struct GlyphCell
        {
            float column{};
            float line{};
            std::array<float, 4> quad{};
            std::array<float, 4> uv{};
            // A color glyph, drawn white at the text's alpha rather than tinted.
            bool colored{};
        };

        // A track's data block as it was last laid out, and what for: its text, the atlas's contents, and the scale.
        struct LaidOutBlock
        {
            std::string callsign;
            float knots{};
            float scale{};
            std::uint32_t atlas{std::numeric_limits<std::uint32_t>::max()};
            TextExtent extent;
            std::vector<GlyphCell> cells;
        };

        static auto createAtlas(SDL_GPUDevice* device, std::uint32_t size) -> Texture
        {
            return CreateTexture(device, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, {.width = size, .height = size}, SDL_GPU_TEXTUREUSAGE_SAMPLER);
        }

        // Opens the font at a size in pixels.
        auto openFont(float pixels) -> TTF_Font*
        {
            // The stream reads the glyphs' copy of the font, which outlives every font opened from it.
            auto* stream = SDL_IOFromConstMem(std::data(fontData_), std::size(fontData_));
            auto* opened = stream != nullptr ? TTF_OpenFontIO(stream, true, pixels) : nullptr;

            if (opened != nullptr)
            {
                // Without hinting, glyphs keep their outlines' shapes at every size, as the rings and tracks do.
                TTF_SetFontHinting(opened, TTF_HINTING_NONE);
            }

            return opened;
        }

        // The font for a size of text, sized in pixels to the frame: one for each size, opened the first time it's
        // asked for and resized after that, as scope's are.
        auto font(float size, float scale) -> TTF_Font*
        {
            if (!ready())
            {
                return nullptr;
            }

            const auto pixels = size * scale * PointsToPixels;
            auto found = fonts_.find(size);

            if (found == std::end(fonts_))
            {
                found = fonts_.emplace(size, openFont(pixels)).first;
            }

            auto* face = found->second;
            return face != nullptr && TTF_SetFontSize(face, pixels) ? face : nullptr;
        }

        // A size of text's metrics in pixels at a scale, with the ascent of its font, sized to it.
        [[nodiscard]] auto metricsOf(float size, float scale, TTF_Font* face) const -> TextMetrics
        {
            const auto pixels = size * scale * PointsToPixels;
            return {.pixels = pixels,
                    .advance = metrics_.advance * pixels,
                    .lineSkip = metrics_.lineSkip * pixels,
                    .ascent = static_cast<float>(TTF_GetFontAscent(face))};
        }

        // How many lines a text has, and how many characters its longest has. Bytes that continue a character don't
        // count.
        static auto extentOf(std::string_view text) -> TextExtent
        {
            TextExtent x{.lines = 1, .longest = 0};
            auto columns = 0;

            for (const auto c : text)
            {
                if (c == '\n')
                {
                    ++x.lines;
                    columns = 0;
                }
                else if ((static_cast<unsigned char>(c) & ContinuationMask) != ContinuationByte)
                {
                    x.longest = std::max(x.longest, ++columns);
                }
            }

            return x;
        }

        // Where a text's first line starts and its baseline is, in pixels, from the point of it that's pinned.
        [[nodiscard]] auto place(Vec2 at, Vec2 anchor, TextExtent extent, const TextMetrics& metrics, float scale) const -> Vec2
        {
            const auto left = (at.x * scale) - (anchor.x * metrics.advance * static_cast<float>(extent.longest));
            const auto height = (metrics_.height * metrics.pixels) + (metrics.lineSkip * static_cast<float>(extent.lines - 1));
            const auto baseline = (at.y * scale) - (anchor.y * height) + (metrics_.ascent * metrics.pixels);
            return {.x = left, .y = baseline};
        }

        // Hands `glyph` each of a text's glyphs from the atlas, in order, with its column and line: SDL_ttf lays out
        // each character in the text's font, a glyph at a time, as scope places them. False if SDL_ttf failed.
        auto forEachGlyph(const scope::Text& x, TTF_Font* face, const auto& glyph) -> bool
        {
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

                const auto drawnAt = column++;

                if (codepoint == ' ')
                {
                    continue;
                }

                auto* text = glyphText(x, face, bytes, codepoint);

                if (text == nullptr || !TTF_UpdateText(text))
                {
                    return false;
                }

                const auto& internal = *text->internal;

                // SDL_ttf hands a text's drawing over as a union of operations, tagged by their first member.
                for (const auto& operation : std::span{internal.ops, static_cast<std::size_t>(std::max(internal.num_ops, 0))})
                {
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                    if (operation.cmd == TTF_DRAW_COMMAND_COPY)
                    {
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
                        glyph(drawnAt, line, operation.copy);
                    }
                }
            }

            return true;
        }

        // A data block's glyphs, laid out again only if its text, the atlas or the scale changed since they last were.
        // Null if SDL_ttf failed.
        auto layOut(const scope::DataBlock& x, float scale, TTF_Font* face) -> const LaidOutBlock*
        {
            if (std::size(blocks_) <= x.track)
            {
                blocks_.resize(x.track + 1);
            }

            auto& laidOut = blocks_.at(x.track);
            // The block's speed, as `scope::TextOf()` rounds it, says whether its text changed.
            const auto knots = std::nearbyint(x.knots);

            if (laidOut.atlas == atlasGeneration_ && laidOut.scale == scale && laidOut.knots == knots && laidOut.callsign == x.callsign)
            {
                return &laidOut;
            }

            const auto text = scope::TextOf(x);
            laidOut.callsign = x.callsign;
            laidOut.knots = knots;
            laidOut.scale = scale;
            laidOut.atlas = atlasGeneration_;
            laidOut.extent = extentOf(text.text);
            laidOut.cells.clear();
            const auto atlas = static_cast<float>(atlasSize_);

            const auto added = forEachGlyph(
                text, face,
                [&](int column, int line, const TTF_CopyOperation& operation)
                {
                    const auto* glyph = pack(operation);

                    // A glyph that doesn't fit leaves the block to be laid out again, once
                    // the atlas has grown.
                    if (glyph == nullptr)
                    {
                        laidOut.atlas = atlasGeneration_ - 1;
                        return;
                    }

                    const auto& [slot, type] = *glyph;
                    const auto& dst = operation.dst;
                    laidOut.cells.push_back(
                        {.column = static_cast<float>(column),
                         .line = static_cast<float>(line),
                         .quad = {static_cast<float>(dst.x), static_cast<float>(dst.y), static_cast<float>(dst.w), static_cast<float>(dst.h)},
                         .uv = {static_cast<float>(slot.x) / atlas, static_cast<float>(slot.y) / atlas,
                                static_cast<float>(slot.x + slot.width) / atlas, static_cast<float>(slot.y + slot.height) / atlas},
                         .colored = type != TTF_IMAGE_ALPHA});
                });

            return added ? &laidOut : nullptr;
        }

        // A glyph of text of a size, laid out by SDL_ttf in that size's font the first time it's drawn.
        auto glyphText(const scope::Text& x, TTF_Font* face, std::string_view bytes, std::uint32_t codepoint) -> TTF_Text*
        {
            const auto key = std::pair{x.size, codepoint};

            if (const auto found = glyphTexts_.find(key); found != std::end(glyphTexts_))
            {
                return found->second;
            }

            auto* created = TTF_CreateText(&engine_, face, std::data(bytes), std::size(bytes));
            glyphTexts_.emplace(key, created);
            return created;
        }

        // Lets go of the text and the fonts it's drawn in.
        auto closeFonts() -> void
        {
            for (const auto& [key, text] : glyphTexts_)
            {
                TTF_DestroyText(text);
            }

            glyphTexts_.clear();

            for (const auto& [size, face] : fonts_)
            {
                TTF_CloseFont(face);
            }

            fonts_.clear();
            clearAtlas();
        }

        // Forgets every glyph in the atlas, and so the data blocks laid out from them.
        auto clearAtlas() -> void
        {
            packer_.clear();
            packed_.clear();
            ++atlasGeneration_;
        }

        // Adds a glyph's quad, at the glyph text's top left: the quad is where SDL_ttf laid the glyph out, and the
        // atlas holds it a texel to a pixel.
        auto addGlyph(const TTF_CopyOperation& operation, Vec2 at, scope::Tint tint) -> void
        {
            const auto* glyph = pack(operation);

            if (glyph == nullptr)
            {
                return;
            }

            const auto& [slot, type] = *glyph;
            const auto atlas = static_cast<float>(atlasSize_);
            const auto& dst = operation.dst;
            const auto left = at.x + static_cast<float>(dst.x);
            const auto top = at.y + static_cast<float>(dst.y);
            const auto right = left + static_cast<float>(dst.w);
            const auto bottom = top + static_cast<float>(dst.h);
            const auto u0 = static_cast<float>(slot.x) / atlas;
            const auto v0 = static_cast<float>(slot.y) / atlas;
            const auto u1 = static_cast<float>(slot.x + slot.width) / atlas;
            const auto v1 = static_cast<float>(slot.y + slot.height) / atlas;
            // As SDL_ttf's engines do, coverage is tinted and color glyphs keep their own, at the text's opacity.
            const auto color = type == TTF_IMAGE_ALPHA ? tint : scope::Tint{.r = 1.0F, .g = 1.0F, .b = 1.0F, .a = tint.a};
            const auto first = static_cast<std::uint32_t>(std::size(vertices_));

            vertices_.push_back({.position = {.x = left, .y = top}, .tint = color, .uv = {.x = u0, .y = v0}});
            vertices_.push_back({.position = {.x = right, .y = top}, .tint = color, .uv = {.x = u1, .y = v0}});
            vertices_.push_back({.position = {.x = right, .y = bottom}, .tint = color, .uv = {.x = u1, .y = v1}});
            vertices_.push_back({.position = {.x = left, .y = bottom}, .tint = color, .uv = {.x = u0, .y = v1}});
            indices_.insert(std::end(indices_), {first, first + 1, first + 2, first, first + 2, first + 3});
        }

        // Finds a glyph in the atlas, rasterizing and staging it the first time it's drawn at its size.
        auto pack(const TTF_CopyOperation& operation) -> const PackedGlyph*
        {
            const GlyphKey key{
                .font = operation.glyph_font, .generation = TTF_GetFontGeneration(operation.glyph_font), .index = operation.glyph_index};

            if (const auto found = packed_.find(key); found != std::end(packed_))
            {
                return &found->second;
            }

            auto type = TTF_IMAGE_INVALID;
            auto* image = TTF_GetGlyphImageForIndex(operation.glyph_font, operation.glyph_index, &type);

            // The atlas holds B, G, R and A bytes, as SDL_ttf's renderer engine copies them.
            if (image != nullptr && image->format != SDL_PIXELFORMAT_BGRA32)
            {
                auto* converted = SDL_ConvertSurface(image, SDL_PIXELFORMAT_BGRA32);
                SDL_DestroySurface(image);
                image = converted;
            }

            if (image == nullptr)
            {
                return nullptr;
            }

            const auto slot = packer_.place(static_cast<std::uint32_t>(image->w), static_cast<std::uint32_t>(image->h));

            if (!slot.has_value())
            {
                full_ = true;
                SDL_DestroySurface(image);
                return nullptr;
            }

            stage(*image, *slot);
            SDL_DestroySurface(image);
            return &packed_.emplace(key, PackedGlyph{.slot = *slot, .type = type}).first->second;
        }

        // Copies a glyph's rows into the staged bytes, laid out as Direct3D 12 copies from.
        auto stage(const SDL_Surface& image, const Slot& slot) -> void
        {
            const auto rowBytes = slot.width * BytesPerPixel;
            const auto pitch = AlignUp(rowBytes, RowAlignment);
            const auto offset = AlignUp(static_cast<std::uint32_t>(std::size(staged_)), RegionAlignment);
            staged_.resize(offset + (pitch * slot.height));

            const std::span pixels{static_cast<const std::byte*>(image.pixels), static_cast<std::size_t>(image.pitch) * slot.height};

            for (std::uint32_t row = 0; row < slot.height; ++row)
            {
                const auto from = pixels.subspan(static_cast<std::size_t>(row) * static_cast<std::size_t>(image.pitch), rowBytes);
                const auto at = static_cast<std::ptrdiff_t>(offset) + (static_cast<std::ptrdiff_t>(row) * static_cast<std::ptrdiff_t>(pitch));
                std::ranges::copy(from, std::next(std::begin(staged_), at));
            }

            uploads_.push_back({.offset = offset, .pitch = pitch, .slot = slot});
        }

        SDL_GPUDevice* device_;
        TTF_TextEngine engine_{};
        bool ttf_{};
        std::vector<char> fontData_;
        FontMetrics metrics_;
        float advance_{scope::DefaultAdvance};
        std::map<float, TTF_Font*> fonts_;
        std::map<std::pair<float, std::uint32_t>, TTF_Text*> glyphTexts_;
        std::uint32_t atlasSize_{SmallestAtlas};
        Texture atlas_;
        ShelfPacker packer_;
        std::unordered_map<GlyphKey, PackedGlyph, GlyphKeyHash> packed_;
        // A glyph didn't fit, so the atlas grows and is cleared before the next frame.
        bool full_{};
        std::vector<GlyphVertex> vertices_;
        std::vector<std::uint32_t> indices_;
        std::vector<std::byte> staged_;
        std::vector<GlyphUpload> uploads_;
        // How many times the atlas has been cleared, so data blocks laid out from glyphs no longer in it are laid out
        // again.
        std::uint32_t atlasGeneration_{};
        // Each track's data block, by its place in the scope's list, as it was last laid out.
        std::vector<LaidOutBlock> blocks_;
        std::vector<BlockGlyph> blockGlyphs_;
    };
}
