module;

#include <expected>
#include <string>

export module stream.string.tostring;
import stream.string.errorcode;

export namespace stream::string
{
    [[nodiscard]] auto ToString(std::string_view str) -> std::expected<std::string, std::string>
    {
        return std::string{str};
    }
}