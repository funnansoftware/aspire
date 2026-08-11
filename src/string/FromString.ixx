module;

#include <charconv>
#include <expected>
#include <stdexcept>
#include <string>

export module stream.string.fromstring;
import stream.string.concepts;
import stream.string.errorcode;

export namespace stream::string
{
    template <typename T>
    auto FromString(std::string_view) -> std::expected<T, ErrorCode>;

    template <StringType T>
    [[nodiscard]] constexpr auto FromString(std::string_view str) -> std::expected<T, ErrorCode>
    {
        try
        {
            return str.data();
        }
        catch (const std::invalid_argument&)
        {
            return std::unexpected(ErrorCode::InvalidArgument);
        }
        catch (const std::out_of_range&)
        {
            return std::unexpected(ErrorCode::OutOfRange);
        }
        catch (const std::exception&)
        {
            return std::unexpected(ErrorCode::UnknownError);
        }
    }

    template <IntType T>
    [[nodiscard]] constexpr auto FromString(std::string_view str) -> std::expected<T, ErrorCode>
    {
        T value{};
        const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);

        if (ec == std::errc())
        {
            return value;
        }
        else if (ec == std::errc::invalid_argument)
        {
            return std::unexpected(ErrorCode::InvalidArgument);
        }
        else if (ec == std::errc::result_out_of_range)
        {
            return std::unexpected(ErrorCode::OutOfRange);
        }
        else
        {
            return std::unexpected(ErrorCode::UnknownError);
        }
    }
}