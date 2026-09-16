export module aspire.core.object;

import std;
import aspire.core.property;
import aspire.core.event;
import aspire.core.overloaded;

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
            auto& child = children_.emplace_back(std::move(x));

            if (started_)
            {
                child->startup();
            }
        }

        auto getChild(std::size_t x = 0) -> std::shared_ptr<Object>
        {
            if (x >= std::size(children_))
            {
                return nullptr;
            }

            return children_.at(x);
        }

        template <ObjectType T>
        auto getOrCreateChild() -> std::shared_ptr<T>
        {
            for (const auto& child : children_)
            {
                if (auto casted = std::dynamic_pointer_cast<T>(child))
                {
                    return casted;
                }
            }

            auto newChild = std::make_shared<T>();
            addChild(newChild);
            return newChild;
        }

        auto getChildren() const -> std::span<const std::shared_ptr<Object>>
        {
            return children_;
        }

        template <ObjectType T>
        auto getChildren() const -> std::vector<std::shared_ptr<T>>
        {
            std::vector<std::shared_ptr<T>> v;

            for (const auto& child : children_)
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

        template <ObjectType T>
        auto getParent() const -> std::shared_ptr<T>
        {
            auto parent = parent_.lock();

            while (parent != nullptr)
            {
                if (auto casted = std::dynamic_pointer_cast<T>(parent))
                {
                    return casted;
                }

                parent = parent->parent_.lock();
            }

            return nullptr;
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

        // NOLINTNEXTLINE(misc-no-recursion)
        auto startup() -> void
        {
            if (!started_)
            {
                started_ = true;
                onStartup();
            }

            auto children = children_;

            for (auto& child : children)
            {
                child->startup();
            }
        }

        // NOLINTNEXTLINE(misc-no-recursion)
        auto event(aspire::core::Event& x) -> void
        {
            const auto handled = std::visit(
                aspire::core::Overloaded{
                    [](std::unique_ptr<EventUser>& e) { return e->handled; },
                    [](auto& e) { return e.handled; },
                },
                x);

            if (handled)
            {
                return;
            }

            onEvent(x);

            // Copy children to avoid modification during iteration.
            auto children = children_;

            for (auto& child : children)
            {
                child->event(x);
            }
        }

        // NOLINTNEXTLINE(misc-no-recursion)
        auto update(float x) -> void
        {
            onUpdate(x);

            auto children = children_;

            for (auto& child : children)
            {
                child->update(x);
            }
        }

        // NOLINTNEXTLINE(misc-no-recursion)
        auto updateFixed(float x) -> void
        {
            onUpdateFixed(x);

            auto children = children_;

            for (auto& child : children)
            {
                child->updateFixed(x);
            }
        }

        // NOLINTNEXTLINE(misc-no-recursion)
        auto render() const -> void
        {
            onRenderPre();
            onRender();

            auto children = children_;

            for (auto& child : children)
            {
                child->render();
            }

            onRenderPost();
        }

    protected:
        virtual auto onStartup() -> void
        {
        }

        virtual auto onEvent(aspire::core::Event& /*unused*/) -> void
        {
        }

        virtual auto onUpdate(float /*unused*/) -> void
        {
        }

        virtual auto onUpdateFixed(float /*unused*/) -> void
        {
        }

        virtual auto onRenderPre() const -> void
        {
        }

        virtual auto onRender() const -> void
        {
        }

        virtual auto onRenderPost() const -> void
        {
        }

    private:
        std::string name_;
        std::vector<std::unique_ptr<Property>> properties_;
        std::vector<std::shared_ptr<Object>> children_;
        std::weak_ptr<Object> parent_;
        bool started_{false};
    };
}
