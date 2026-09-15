export module aspire.string.concepts;

import std;

export namespace aspire::string
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

    template <typename T>
    concept PointerType = std::is_pointer_v<T> || std::same_as<std::decay_t<T>, std::nullptr_t> || std::same_as<std::decay_t<T>, std::unique_ptr<T>>
                          || std::same_as<std::decay_t<T>, std::shared_ptr<T>> || std::same_as<std::decay_t<T>, std::weak_ptr<T>>;
}
