module;

#include <concepts>
#include <string>

export module stream.string.concepts;

export namespace stream::string
{
    template <typename T>
    concept IntType = std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, char> && !std::same_as<T, wchar_t> && !std::same_as<T, char16_t>
                      && !std::same_as<T, char32_t>;

    template <typename T>
    concept FloatType = std::floating_point<T>;

    template <typename T>
    concept ArithmeticType = IntType<T> || FloatType<T>;

    template <typename T>
    concept BoolType = std::same_as<std::decay_t<T>, bool>;

    template <typename T>
    concept CharType = std::same_as<std::decay_t<T>, char> || std::same_as<std::decay_t<T>, wchar_t> || std::same_as<std::decay_t<T>, char16_t>
                       || std::same_as<std::decay_t<T>, char32_t>;

    template <typename T>
    concept StringType =
        std::same_as<std::decay_t<T>, std::string> || std::same_as<std::decay_t<T>, std::string_view> || std::same_as<T, const char*>;
}