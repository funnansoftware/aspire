module;

#include <raylib.h>

#include <array>
#include <filesystem>
#include <string>
#include <typeinfo>

export module aspire.raylib.texture;

import aspire.core.object;
import aspire.core.engine;
import aspire.core.property;
import aspire.raylib.drawable;
import aspire.raylib.textureloader;

export namespace aspire::raylib
{
    class Texture : public aspire::raylib::Drawable
    {
    public:
        Texture()
        {
            registerProperty("source", source_);
            registerProperty("rect", rect_);
        }

        auto draw() const -> void override
        {
            auto texture = getParent<aspire::core::Engine>()->getOrCreateChild<TextureLoader>()->LoadTexture(source_);
            DrawTextureRec(texture, Rectangle{rect_[0], rect_[1], rect_[2], rect_[3]}, position_, WHITE);
        }

        auto setPosition(Vector2 x) noexcept
        {
            position_ = x;
        }

        auto getPosition() const noexcept
        {
            return position_;
        }

    private:
        std::filesystem::path source_;
        std::array<float, 4> rect_{};
        Vector2 position_{};
    };
}