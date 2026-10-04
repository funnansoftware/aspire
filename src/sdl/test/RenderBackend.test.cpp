#include <gtest/gtest.h>

#include <SDL3/SDL.h>

import std;
import aspire.core;
import aspire.graphics;
import aspire.sdl;

namespace
{
    using aspire::graphics::Color;
    using aspire::graphics::DrawItem;
    using aspire::graphics::DrawList;
    using aspire::graphics::DrawPrimitive;
    using aspire::graphics::Rect;

    // The target is Size x Size pixels.
    constexpr int Size{32};
    constexpr float Half{16.0F};
    constexpr float Cell{8.0F};

    constexpr Color Clear{.r = 10, .g = 20, .b = 30, .a = Color::Opaque};
    constexpr Color Red{.r = 255, .g = 0, .b = 0, .a = Color::Opaque};
    constexpr Color Green{.r = 0, .g = 255, .b = 0, .a = Color::Opaque};
    constexpr Color Blue{.r = 0, .g = 0, .b = 255, .a = Color::Opaque};
    constexpr Color HalfWhite{.r = 255, .g = 255, .b = 255, .a = 128};
    constexpr Color HalfRedTint{.r = 128, .g = 255, .b = 255, .a = Color::Opaque};
    constexpr std::uint8_t HalfChannel{128};
    constexpr int Tolerance{2};

    // Pixels inside the top-left cell, the center cell, and just outside a 4.5-pixel clip.
    constexpr int InTopLeft{4};
    constexpr int InCenter{20};
    constexpr int PastPartialClip{5};

    // A small square inside the top-left cell, drawn after scaled text with the same clip.
    constexpr Rect InnerSquare{.x = 6.0F, .y = 6.0F, .w = 2.0F, .h = 2.0F};
    constexpr int InInnerSquare{7};

    constexpr aspire::core::Vec2 Unscaled{.x = 1.0F, .y = 1.0F};
    constexpr aspire::core::Vec2 Doubled{.x = 2.0F, .y = 2.0F};

    constexpr SDL_Rect WholeTarget{.x = 0, .y = 0, .w = Size, .h = Size};

    constexpr Rect Everything{.x = 0.0F, .y = 0.0F, .w = Size, .h = Size};
    constexpr Rect TopLeftCell{.x = 0.0F, .y = 0.0F, .w = Cell, .h = Cell};
    constexpr Rect CenterCell{.x = Half, .y = Half, .w = Cell, .h = Cell};

    // The test tilesheet is two pixels wide: red on the left, green on the right.
    constexpr Rect RedTile{.x = 0.0F, .y = 0.0F, .w = 1.0F, .h = 1.0F};
    constexpr Rect GreenTile{.x = 1.0F, .y = 0.0F, .w = 1.0F, .h = 1.0F};

    auto Item(DrawPrimitive x, std::uint32_t clip = 0) -> DrawItem
    {
        return DrawItem{.layer = 0, .order = 0.0F, .sequence = 0, .clip = clip, .primitive = x};
    }

    auto FillRect(Rect bounds, Color color, std::uint32_t clip = 0) -> DrawItem
    {
        return Item(aspire::graphics::DrawRect{.bounds = bounds, .color = color, .filled = true}, clip);
    }

    auto Text(std::string_view text, aspire::core::Vec2 position, aspire::core::Vec2 scale, std::uint32_t clip = 0) -> DrawItem
    {
        return Item(aspire::graphics::DrawText{.text = text, .position = position, .scale = scale, .color = aspire::graphics::White}, clip);
    }

    auto Sprite(std::string_view source, Rect region, Rect bounds, Color tint = aspire::graphics::White) -> DrawItem
    {
        return Item(aspire::graphics::DrawSprite{.source = source, .region = region, .bounds = bounds, .tint = tint});
    }

    // Draws on a software renderer targeting a surface, so pixels can be read back without a window.
    class RenderBackendTest : public ::testing::Test
    {
    protected:
        // NOLINTNEXTLINE(readability-identifier-naming)
        auto SetUp() -> void override
        {
            surface_ = SDL_CreateSurface(Size, Size, SDL_PIXELFORMAT_RGBA32);
            ASSERT_NE(surface_, nullptr) << SDL_GetError();
            renderer_ = SDL_CreateSoftwareRenderer(surface_);
            ASSERT_NE(renderer_, nullptr) << SDL_GetError();
            backend_ = std::make_unique<aspire::sdl::RenderBackend>(renderer_);

            directory_ = std::filesystem::temp_directory_path() / "aspire-sdl-render-backend-test";
            std::filesystem::create_directories(directory_);
        }

        // NOLINTNEXTLINE(readability-identifier-naming)
        auto TearDown() -> void override
        {
            backend_.reset();
            SDL_DestroyRenderer(renderer_);
            SDL_DestroySurface(surface_);
            EXPECT_GT(std::filesystem::remove_all(directory_), 0);
        }

        [[nodiscard]] auto backend() const -> aspire::sdl::RenderBackend&
        {
            return *backend_;
        }

        [[nodiscard]] auto directory() const -> const std::filesystem::path&
        {
            return directory_;
        }

        [[nodiscard]] auto pixel(int x, int y) const -> Color
        {
            Color color;
            EXPECT_TRUE(SDL_ReadSurfacePixel(surface_, x, y, &color.r, &color.g, &color.b, &color.a));
            return color;
        }

        // Whether any pixel in the area differs from the clear color.
        [[nodiscard]] auto drawnIn(SDL_Rect area) const -> bool
        {
            for (auto row = area.y; row < area.y + area.h; ++row)
            {
                for (auto column = area.x; column < area.x + area.w; ++column)
                {
                    const auto color = pixel(column, row);

                    if (color.r != Clear.r || color.g != Clear.g || color.b != Clear.b)
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        // Writes the two-pixel test tilesheet, red then green, and returns its path.
        [[nodiscard]] auto writeTilesheet(std::string_view name) const -> std::filesystem::path
        {
            auto* sheet = SDL_CreateSurface(2, 1, SDL_PIXELFORMAT_RGBA32);
            EXPECT_TRUE(SDL_WriteSurfacePixel(sheet, 0, 0, Red.r, Red.g, Red.b, Red.a));
            EXPECT_TRUE(SDL_WriteSurfacePixel(sheet, 1, 0, Green.r, Green.g, Green.b, Green.a));

            const auto path = directory_ / name;
            EXPECT_TRUE(SDL_SavePNG(sheet, path.string().c_str())) << SDL_GetError();
            SDL_DestroySurface(sheet);
            return path;
        }

    private:
        SDL_Surface* surface_{};
        SDL_Renderer* renderer_{};
        std::unique_ptr<aspire::sdl::RenderBackend> backend_;
        std::filesystem::path directory_;
    };

    auto ExpectColor(Color actual, Color expected) -> void
    {
        EXPECT_EQ(actual.r, expected.r);
        EXPECT_EQ(actual.g, expected.g);
        EXPECT_EQ(actual.b, expected.b);
    }
}

TEST_F(RenderBackendTest, clearFillsTheTarget)
{
    const DrawList list;
    ASSERT_TRUE(backend().submit(list, Clear));

    ExpectColor(pixel(0, 0), Clear);
    ExpectColor(pixel(Size - 1, Size - 1), Clear);
}

TEST_F(RenderBackendTest, rectFillsItsPixels)
{
    DrawList list;
    list.add(FillRect(CenterCell, Red));
    ASSERT_TRUE(backend().submit(list, Clear));

    ExpectColor(pixel(InCenter, InCenter), Red);
    ExpectColor(pixel(InTopLeft, InTopLeft), Clear);
}

TEST_F(RenderBackendTest, outlineDrawsOnlyTheEdge)
{
    DrawList list;
    list.add(Item(aspire::graphics::DrawRect{.bounds = CenterCell, .color = Red, .filled = false}));
    ASSERT_TRUE(backend().submit(list, Clear));

    ExpectColor(pixel(InCenter - 4, InCenter - 4), Red);
    ExpectColor(pixel(InCenter, InCenter), Clear);
}

TEST_F(RenderBackendTest, translucentRectBlends)
{
    DrawList list;
    list.add(FillRect(Everything, HalfWhite));
    ASSERT_TRUE(backend().submit(list, aspire::graphics::Black));

    EXPECT_NEAR(pixel(0, 0).r, HalfChannel, Tolerance);
}

TEST_F(RenderBackendTest, clipLimitsDrawing)
{
    DrawList list;
    const auto clip = list.addClip(TopLeftCell);
    list.add(FillRect(Everything, Red, clip));
    ASSERT_TRUE(backend().submit(list, Clear));

    ExpectColor(pixel(InTopLeft, InTopLeft), Red);
    ExpectColor(pixel(InCenter, InCenter), Clear);
}

TEST_F(RenderBackendTest, clipCoversPartialPixels)
{
    // A clip ending halfway through a pixel still covers that pixel.
    DrawList list;
    const auto clip = list.addClip({.x = 0.0F, .y = 0.0F, .w = 4.5F, .h = 4.5F});
    list.add(FillRect(Everything, Red, clip));
    ASSERT_TRUE(backend().submit(list, Clear));

    ExpectColor(pixel(InTopLeft, InTopLeft), Red);
    ExpectColor(pixel(PastPartialClip, PastPartialClip), Clear);
}

TEST_F(RenderBackendTest, emptyClipDrawsNothing)
{
    DrawList list;
    const auto clip = list.addClip({.x = 0.0F, .y = 0.0F, .w = 0.0F, .h = Cell});
    list.add(FillRect(Everything, Red, clip));
    ASSERT_TRUE(backend().submit(list, Clear));

    EXPECT_FALSE(drawnIn(WholeTarget));
}

TEST_F(RenderBackendTest, itemAfterAClippedItemIsUnclipped)
{
    DrawList list;
    const auto clip = list.addClip(TopLeftCell);
    list.add(FillRect(Everything, Blue, clip));
    list.add(FillRect(CenterCell, Red));
    ASSERT_TRUE(backend().submit(list, Clear));

    ExpectColor(pixel(InTopLeft, InTopLeft), Blue);
    ExpectColor(pixel(InCenter, InCenter), Red);
}

TEST_F(RenderBackendTest, spriteDrawsItsRegionWithTint)
{
    const auto path = writeTilesheet("tiles.png").string();

    DrawList list;
    list.add(Sprite(path, GreenTile, TopLeftCell));
    list.add(Sprite(path, RedTile, CenterCell, HalfRedTint));
    ASSERT_TRUE(backend().submit(list, Clear));

    ExpectColor(pixel(InTopLeft, InTopLeft), Green);
    EXPECT_NEAR(pixel(InCenter, InCenter).r, HalfChannel, Tolerance);
}

TEST_F(RenderBackendTest, relativeSourceResolvesAgainstAssetRoot)
{
    static_cast<void>(writeTilesheet("tiles.png"));
    backend().setAssetRoot(directory());

    DrawList list;
    list.add(Sprite("tiles.png", GreenTile, TopLeftCell));
    ASSERT_TRUE(backend().submit(list, Clear));

    ExpectColor(pixel(InTopLeft, InTopLeft), Green);
}

TEST_F(RenderBackendTest, textureIsLoadedOnce)
{
    const auto path = writeTilesheet("tiles.png");
    const auto source = path.string();

    DrawList list;
    list.add(Sprite(source, GreenTile, TopLeftCell));
    ASSERT_TRUE(backend().submit(list, Clear));

    // Still drawn from the cache once the file is gone.
    EXPECT_TRUE(std::filesystem::remove(path));
    ASSERT_TRUE(backend().submit(list, aspire::graphics::Black));
    ExpectColor(pixel(InTopLeft, InTopLeft), Green);
}

TEST_F(RenderBackendTest, missingTextureDrawsNothingAndSucceeds)
{
    DrawList list;
    list.add(Sprite("missing.png", GreenTile, Everything));

    EXPECT_TRUE(backend().submit(list, Clear));
    EXPECT_TRUE(backend().submit(list, Clear));
    EXPECT_FALSE(drawnIn(WholeTarget));
}

TEST_F(RenderBackendTest, textDrawsInsideItsCharacterCell)
{
    DrawList list;
    list.add(Text("M", {.x = Cell, .y = Cell}, Unscaled));
    ASSERT_TRUE(backend().submit(list, Clear));

    EXPECT_TRUE(drawnIn({.x = 8, .y = 8, .w = 8, .h = 8}));
    EXPECT_FALSE(drawnIn({.x = 16, .y = 0, .w = 16, .h = Size}));
}

TEST_F(RenderBackendTest, scaledTextCoversTheScaledCell)
{
    DrawList list;
    list.add(Text("M", {.x = 0.0F, .y = 0.0F}, Doubled));
    ASSERT_TRUE(backend().submit(list, Clear));

    // An 8x8 glyph at twice the size reaches past x = 8, and stops before x = 16.
    EXPECT_TRUE(drawnIn({.x = 8, .y = 0, .w = 8, .h = 16}));
    EXPECT_FALSE(drawnIn({.x = 16, .y = 0, .w = 16, .h = Size}));
}

TEST_F(RenderBackendTest, scaledTextIsClippedAndRestoresScaleAndClip)
{
    DrawList list;
    const auto clip = list.addClip(TopLeftCell);
    list.add(Text("M", {.x = 0.0F, .y = 0.0F}, Doubled, clip));

    // Drawn after the text with the same clip, then with none: both must be back at scale 1.
    list.add(FillRect(InnerSquare, Red, clip));
    list.add(FillRect(CenterCell, Green));
    ASSERT_TRUE(backend().submit(list, Clear));

    EXPECT_TRUE(drawnIn({.x = 0, .y = 0, .w = 6, .h = 6}));
    EXPECT_FALSE(drawnIn({.x = 8, .y = 0, .w = 8, .h = 8}));
    EXPECT_FALSE(drawnIn({.x = 0, .y = 8, .w = 8, .h = 8}));
    ExpectColor(pixel(InInnerSquare, InInnerSquare), Red);
    ExpectColor(pixel(InCenter, InCenter), Green);
}
