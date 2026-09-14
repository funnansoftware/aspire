module;

#include <raylib.h>
#include <filesystem>

export module aspire.raylib.textureloader;

export namespace aspire::raylib
{
    class TextureLoader
    {
    public:
        auto LoadTexture(const std::filesystem::path& path) -> ::Texture
        {
            return ::LoadTexture(path.string().c_str());
        }

        auto UnloadTexture(::Texture texture) -> void
        {
            ::UnloadTexture(texture);
        }

    private:
    };
}