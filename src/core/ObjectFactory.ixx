module;

#include <nameof.hpp>

export module aspire.core.objectfactory;

import std;
import aspire.core.object;

namespace aspire::core::detail
{
    constexpr auto shortTypeName(std::string_view name) noexcept -> std::string_view
    {
        if (const auto bracketPos = name.find('['); bracketPos != std::string_view::npos)
        {
            name = name.substr(0, bracketPos);
        }

        if (const auto nsPos = name.rfind("::"); nsPos != std::string_view::npos)
        {
            name = name.substr(nsPos + 2);
        }

        if (name.starts_with("class "))
        {
            name.remove_prefix(6);
        }
        else if (name.starts_with("struct "))
        {
            name.remove_prefix(7);
        }
        else if (name.starts_with("enum "))
        {
            name.remove_prefix(5);
        }

        return name;
    }

    template <typename T>
    constexpr auto typeName() noexcept -> std::string_view
    {
        return shortTypeName(nameof::nameof_type<T>());
    }
}

export namespace aspire::core
{

    class Creator
    {
    public:
        Creator() = default;
        virtual ~Creator() = default;

        Creator(const Creator&) = delete;
        auto operator=(const Creator&) -> Creator& = delete;

        Creator(Creator&&) noexcept = delete;
        auto operator=(Creator&&) noexcept -> Creator& = delete;

        virtual auto create() -> std::shared_ptr<Object> = 0;
    };

    template <ObjectType T>
    class TemplateCreator : public Creator
    {
    public:
        TemplateCreator() = default;
        ~TemplateCreator() override = default;
        TemplateCreator(const TemplateCreator&) = delete;
        auto operator=(const TemplateCreator&) -> TemplateCreator& = delete;

        TemplateCreator(TemplateCreator&&) noexcept = delete;
        auto operator=(TemplateCreator&&) noexcept -> TemplateCreator& = delete;

        auto create() -> std::shared_ptr<Object> override
        {
            return std::make_shared<T>();
        }
    };

    class ObjectFactory
    {
    public:
        ObjectFactory() = default;

        ObjectFactory(const ObjectFactory&) = delete;
        auto operator=(const ObjectFactory&) -> ObjectFactory& = delete;

        ObjectFactory(ObjectFactory&&) noexcept = delete;
        auto operator=(ObjectFactory&&) noexcept -> ObjectFactory& = delete;

        template <ObjectType T>
        auto registerObject(std::string_view name = {}) -> void
        {
            const auto key = name.empty() ? std::string{detail::typeName<T>()} : std::string{name};
            creators_[key] = std::make_unique<TemplateCreator<T>>();
        }

        auto create(const std::string& x) const -> std::shared_ptr<Object>
        {
            auto foundIt = creators_.find(x);

            if (foundIt != std::end(creators_))
            {
                return foundIt->second->create();
            }

            return nullptr;
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<Creator>> creators_;
    };
}
