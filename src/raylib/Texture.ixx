module;

#include <raylib.h>

#include <array>
#include <filesystem>
#include <string>
#include <typeinfo>

export module aspire.raylib.texture;

import aspire.core.object;
import aspire.core.property;
import aspire.raylib.drawable;

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
            // DrawTextureRec(texture, Rectangle{18, 18, 16, 16}, Vector2{30, 30}, WHITE);
        }

    private:
        std::filesystem::path source_;
        std::array<int, 4> rect_;
    };
}