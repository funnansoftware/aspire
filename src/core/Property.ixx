module;

#include <nlohmann/json.hpp>

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

        [[nodiscard]] auto name() const -> std::string_view
        {
            return name_;
        }

        [[nodiscard]] virtual auto typeInfo() const -> const std::type_info& = 0;

        template <JsonSerializable T>
        [[nodiscard]] auto isType() const -> bool
        {
            return typeid(T) == typeInfo();
        }

        virtual auto setValueAny(std::any x) -> void = 0;
        [[nodiscard]] virtual auto getValueAny() const -> std::any = 0;

        template <JsonSerializable T>
        [[nodiscard]] auto getValueAs() const -> T
        {
            return std::any_cast<T>(getValueAny());
        }

        virtual auto setValueJson(const nlohmann::json& value) -> void = 0;
        [[nodiscard]] virtual auto getValueJson() const -> nlohmann::json = 0;
        virtual auto setValueString(std::string_view value) -> void = 0;
        [[nodiscard]] virtual auto getValueString() const -> std::string = 0;

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

        [[nodiscard]] auto typeInfo() const -> const std::type_info& override
        {
            return typeid(T);
        }

        auto setValueAny(std::any x) -> void override
        {
            value_ = std::any_cast<T>(x);
        }

        [[nodiscard]] auto getValueAny() const -> std::any override
        {
            return value_;
        }

        auto setValueJson(const nlohmann::json& value) -> void override
        {
            value_ = value.get<T>();
        }

        [[nodiscard]] auto getValueJson() const -> nlohmann::json override
        {
            return nlohmann::json(value_);
        }

        auto setValueString(std::string_view value) -> void override
        {
            value_ = nlohmann::json::parse(value).get<T>();
        }

        [[nodiscard]] auto getValueString() const -> std::string override
        {
            return nlohmann::json(value_).dump();
        }

    private:
        T& value_;
    };
}
