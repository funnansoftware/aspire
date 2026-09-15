export module aspire.string.fromstring;

import std;
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
        const auto [ptr, ec] = std::from_chars(std::to_address(str.begin()), std::to_address(str.end()), value);

        if (ec == std::errc())
        {
            return value;
        }
        if (ec == std::errc::invalid_argument)
        {
            return std::unexpected(ErrorCode::InvalidArgument);
        }
        if (ec == std::errc::result_out_of_range)
        {
            return std::unexpected(ErrorCode::OutOfRange);
        }

        return std::unexpected(ErrorCode::UnknownError);
    }

    // Custom constexpr tolower for ASCII
    constexpr auto AsciiTolower(char c) -> char
    {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
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

        const auto test = std::views::transform(str, [](auto c) { return AsciiTolower(c); });

        if (std::ranges::equal(test, strTrue))
        {
            return true;
        }
        if (std::ranges::equal(test, strFalse))
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

        return str.front();
    }
}
