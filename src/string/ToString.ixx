export module aspire.string.tostring;

import std;
import aspire.string.errorcode;

export namespace aspire::string
{
    [[nodiscard]] auto ToString(std::string_view str) -> std::expected<std::string, std::string>
    {
        return std::string{str};
    }
}
