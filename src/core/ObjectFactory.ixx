module;

#include <nameof.hpp>

export module aspire.core.objectfactory;

import std;
import aspire.core.object;

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
        ~ObjectFactory() = default;

        ObjectFactory(const ObjectFactory&) = delete;
        auto operator=(const ObjectFactory&) -> ObjectFactory& = delete;

        ObjectFactory(ObjectFactory&&) noexcept = delete;
        auto operator=(ObjectFactory&&) noexcept -> ObjectFactory& = delete;

        template <ObjectType T>
        auto registerObject(std::string_view name = {}) -> void
        {
            const auto key = name.empty() ? std::string{typeName<T>()} : std::string{name};
            creators_[key] = std::make_unique<TemplateCreator<T>>();
        }

        [[nodiscard]] auto create(const std::string& x) const -> std::shared_ptr<Object>
        {
            auto foundIt = creators_.find(x);

            if (foundIt != std::end(creators_))
            {
                return foundIt->second->create();
            }

            return nullptr;
        }

    private:
        static constexpr auto shortTypeName(std::string_view name) noexcept -> std::string_view
        {
            if (const auto bracketPos = name.find('['); bracketPos != std::string_view::npos)
            {
                name = name.substr(0, bracketPos);
            }

            if (const auto nsPos = name.rfind("::"); nsPos != std::string_view::npos)
            {
                name = name.substr(nsPos + 2);
            }

            constexpr std::string_view classPrefix = "class ";
            constexpr std::string_view structPrefix = "struct ";
            constexpr std::string_view enumPrefix = "enum ";
            if (name.starts_with(classPrefix))
            {
                name.remove_prefix(classPrefix.size());
            }
            else if (name.starts_with(structPrefix))
            {
                name.remove_prefix(structPrefix.size());
            }
            else if (name.starts_with(enumPrefix))
            {
                name.remove_prefix(enumPrefix.size());
            }

            return name;
        }

        template <typename T>
        static constexpr auto typeName() noexcept -> std::string_view
        {
            return shortTypeName(nameof::nameof_type<T>());
        }

        std::unordered_map<std::string, std::unique_ptr<Creator>> creators_;
    };
}
