module;

#include <raylib.h>
#include <filesystem>
#include <unordered_map>

export module aspire.raylib.textureloader;
import aspire.core.object;

export namespace aspire::raylib
{
    class TextureLoader : public aspire::core::Object
    {
    public:
        TextureLoader() = default;

        ~TextureLoader() override
        {
            for (const auto& [path, texture] : textures_)
            {
                ::UnloadTexture(texture);
            }
        }

        TextureLoader(const TextureLoader&) = delete;
        auto operator=(const TextureLoader&) -> TextureLoader& = delete;

        TextureLoader(TextureLoader&&) noexcept = delete;
        auto operator=(TextureLoader&&) noexcept -> TextureLoader& = delete;

        auto LoadTexture(const std::filesystem::path& path) -> ::Texture
        {
            auto it = textures_.find(path.string());

            if (it != std::end(textures_))
            {
                return it->second;
            }

            auto texture = ::LoadTexture(path.string().c_str());
            textures_.emplace(path.string(), texture);
            return texture;
        }

        auto UnloadTexture(::Texture texture) -> void
        {
            std::erase_if(textures_, [texture](const auto& pair) { return pair.second.id == texture.id; });
            ::UnloadTexture(texture);
        }

    private:
        std::unordered_map<std::string, ::Texture> textures_;
    };
}