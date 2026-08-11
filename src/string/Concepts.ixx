module;

#include <concepts>
#include <string>

export module stream.string.concepts;

export namespace stream::string
{
    template <typename T>
    concept IntType = std::integral<T> && !std::same_as<T, bool>;

    template <typename T>
    concept FloatType = std::floating_point<T>;

    template <typename T>
    concept BoolType = std::same_as<std::decay_t<T>, bool>;

    template <typename T>
    concept StringType =
        std::same_as<std::decay_t<T>, std::string> || std::same_as<std::decay_t<T>, std::string_view> || std::same_as<T, const char*>;
}