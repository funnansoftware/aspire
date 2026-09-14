module;
#include <algorithm>
#include <memory>
export module aspire.core.object;

import std;
import aspire.core.property;

export namespace aspire::core
{
    class Object;

    template <typename T>
    concept ObjectType = std::is_base_of_v<Object, T>;

    class Object : public std::enable_shared_from_this<Object>
    {
    public:
        Object() = default;
        virtual ~Object() = default;

        Object(const Object&) = delete;
        auto operator=(const Object&) -> Object& = delete;

        Object(Object&&) noexcept = delete;
        auto operator=(Object&&) noexcept -> Object& = delete;

        auto setName(std::string_view x) -> void
        {
            name_ = x;
        }

        auto getName() const -> std::string_view
        {
            return name_;
        }

        auto addChild(std::shared_ptr<Object> x) -> void
        {
            if (x == nullptr)
            {
                return;
            }

            x->parent_ = weak_from_this();
            children_.emplace_back(std::move(x));
        }

        auto getChildren() const -> std::span<const std::shared_ptr<Object>>
        {
            return children_;
        }

        template <ObjectType T>
        auto getChildren() const -> std::vector<std::shared_ptr<T>>
        {
            std::vector<std::shared_ptr<T>> v;

            for (auto& child : children_)
            {
                if (auto casted = std::dynamic_pointer_cast<T>(child))
                {
                    v.emplace_back(std::move(casted));
                }
            }

            return v;
        }

        auto remove() -> void
        {
            if (auto parent = parent_.lock())
            {
                auto& siblings = parent->children_;
                std::erase(siblings, shared_from_this());
            }

            parent_.reset();
        }

        auto getParent() const -> std::shared_ptr<Object>
        {
            return parent_.lock();
        }

        auto registerProperty(std::string_view name, JsonSerializable auto& x) -> void
        {
            properties_.emplace_back(std::make_unique<TemplateProperty<std::decay_t<decltype(x)>>>(name, x));
        }

        auto getProperty(std::string_view name) const -> Property*
        {
            for (const auto& property : properties_)
            {
                if (property->name() == name)
                {
                    return property.get();
                }
            }

            return nullptr;
        }

        auto getProperties() const -> std::span<const std::unique_ptr<Property>>
        {
            return properties_;
        }

    private:
        std::string name_;
        std::vector<std::unique_ptr<Property>> properties_;
        std::vector<std::shared_ptr<Object>> children_;
        std::weak_ptr<Object> parent_;
    };
}