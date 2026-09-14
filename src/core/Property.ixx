module;
#include <nlohmann/json.hpp>
#include <typeinfo>

export module aspire.core.property;

import std;

export namespace aspire::core
{
    template <typename T>
    concept JsonSerializable = requires(T a) {
        { nlohmann::json(a) } -> std::convertible_to<nlohmann::json>;
    };

    class Property
    {
    public:
        Property(std::string_view x) : name_{x}
        {
        }

        virtual ~Property() = default;

        Property(const Property&) = delete;
        auto operator=(const Property&) -> Property& = delete;

        Property(Property&&) noexcept = delete;
        auto operator=(Property&&) noexcept -> Property& = delete;

        auto name() const -> std::string_view
        {
            return name_;
        }

        virtual auto typeInfo() const -> const std::type_info& = 0;

        template <JsonSerializable T>
        auto isType() const -> bool
        {
            return typeid(T) == typeInfo();
        }

        virtual auto setValueAny(std::any x) -> void = 0;
        virtual auto getValueAny() const -> std::any = 0;

        template <JsonSerializable T>
        auto getValueAs() const -> T
        {
            return std::any_cast<T>(getValueAny());
        }

        virtual auto setValueString(std::string_view value) -> void = 0;
        virtual auto getValueString() const -> std::string = 0;

    private:
        std::string name_;
    };

    template <JsonSerializable T>
    class TemplateProperty : public Property
    {
    public:
        TemplateProperty(std::string_view x, T& value) : Property(x), value_{value}
        {
        }

        auto typeInfo() const -> const std::type_info& override
        {
            return typeid(T);
        }

        auto setValueAny(std::any x) -> void
        {
            value_ = std::any_cast<T>(x);
        }

        auto getValueAny() const -> std::any
        {
            return value_;
        }

        auto setValueString(std::string_view value) -> void
        {
            value_ = nlohmann::json::parse(value).get<T>();
        }

        auto getValueString() const -> std::string
        {
            return nlohmann::json(value_).dump();
        }

    private:
        T& value_;
    };
}