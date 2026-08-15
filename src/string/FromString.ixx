module;

#include <algorithm>
#include <cctype>
#include <charconv>
#include <expected>
#include <iterator>
#include <ranges>
#include <stdexcept>

export module aspire.string.fromstring;
import aspire.string.concepts;
import aspire.string.errorcode;

export namespace aspire::string
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

    template <ArithmeticType T>
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

    // Custom constexpr tolower for ASCII
    constexpr char ascii_tolower(char c)
    {
        return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
    }

    template <BoolType T>
    [[nodiscard]] constexpr auto FromString(std::string_view str) -> std::expected<T, ErrorCode>
    {
        constexpr std::string_view strTrue{"true"};
        constexpr std::string_view strFalse{"false"};

        if (std::empty(str) or (std::size(str) != std::size(strTrue) and std::size(str) != std::size(strFalse)))
        {
            return std::unexpected(ErrorCode::InvalidArgument);
        }

        const auto test = std::views::transform(str, [](auto c) { return ascii_tolower(c); });

        if (std::ranges::equal(test, strTrue))
        {
            return true;
        }
        else if (std::ranges::equal(test, strFalse))
        {
            return false;
        }

        return std::unexpected(ErrorCode::InvalidArgument);
    }

    template <CharType T>
    [[nodiscard]] constexpr auto FromString(std::string_view str) -> std::expected<T, ErrorCode>
    {
        if (std::size(str) != 1)
        {
            return std::unexpected(ErrorCode::InvalidArgument);
        }

        return str[0];
    }
}