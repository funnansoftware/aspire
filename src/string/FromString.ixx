module;

#include <expected>
#include <string>

export module stream.string.fromstring;

export namespace stream::string
{
    [[nodiscard]] auto FromString(std::string_view str) -> std::expected<std::string, std::string>
    {
        return std::string{str};
    }
}