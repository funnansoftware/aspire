module;

#include <raylib.h>

export module aspire.raylib:texture;

import std;
import aspire.core;
import :node;
import :textureloader;

export namespace aspire::raylib
{
    class Texture : public aspire::raylib::Node
    {
    public:
        Texture()
        {
            registerProperty("source", source_);
            registerProperty("rect", rect_);
        }

    protected:
        auto onRender() const -> void override
        {
            auto texture = getParent<aspire::core::Engine>()->getOrCreateChild<TextureLoader>()->loadTexture(source_);
            const auto& [x, y, width, height] = rect_;
            DrawTextureRec(texture, Rectangle{.x = x, .y = y, .width = width, .height = height}, {}, WHITE);
        }

    private:
        std::filesystem::path source_;
        std::array<float, 4> rect_{};
    };
}
