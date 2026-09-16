module;

#include <raylib.h>

export module aspire.raylib.tilemap;

import std;
import aspire.core.engine;
import aspire.core.object;
import aspire.raylib.node;
import aspire.raylib.textureloader;

export namespace aspire::raylib
{
    class TileMap : public aspire::raylib::Node
    {
    public:
        TileMap()
        {
            registerProperty("data", data_);
            registerProperty("source", source_);
            registerProperty("spacing", spacing_);
            registerProperty("tileWidth", tileWidth_);
            registerProperty("tileHeight", tileHeight_);
            registerProperty("tiles", tiles_);
            registerProperty("columns", columns_);
            registerProperty("width", width_);
            registerProperty("height", height_);
        }

    protected:
        auto onRender() const -> void override
        {
            if (width_ == 0 || height_ == 0)
            {
                return;
            }

            auto texture = getParent<aspire::core::Engine>()->getOrCreateChild<TextureLoader>()->loadTexture(source_);

            for (const auto& [index, tileIndex] : std::views::enumerate(data_))
            {
                const auto rect = tileIndexToRectangle(tileIndex);
                auto position = dataIndexToPosition(index);
                DrawTextureRec(texture, rect, position, WHITE);
            }
        }

    private:
        auto tileIndexToRectangle(int index) const -> Rectangle
        {
            const auto x = (index % columns_) * (tileWidth_ + spacing_);
            const auto y = (index / columns_) * (tileHeight_ + spacing_);
            return Rectangle{
                .x = static_cast<float>(x),
                .y = static_cast<float>(y),
                .width = static_cast<float>(tileWidth_),
                .height = static_cast<float>(tileHeight_),
            };
        }

        // Calculates the texture position within the map based on the index of the tile data.
        auto dataIndexToPosition(std::size_t index) const -> Vector2
        {
            return Vector2{
                .x = static_cast<float>(index % width_) * static_cast<float>(tileWidth_),

                // Loss of floating precision is intention to keep tile positions aligned correctly.
                // NOLINTNEXTLINE(bugprone-integer-division)
                .y = static_cast<float>(index / width_) * static_cast<float>(tileHeight_),
            };
        }

        std::vector<int> data_;
        std::filesystem::path source_;
        int spacing_{};
        int tileWidth_{};
        int tileHeight_{};
        int tiles_{};
        int columns_{};
        int width_{};
        int height_{};
    };
}