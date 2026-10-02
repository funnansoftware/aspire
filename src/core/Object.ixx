export module aspire.core:object;

import std;
import :property;
import :event;
import :overloaded;

export namespace aspire::core
{
    class Object;

    template <typename T>
    concept ObjectType = std::is_base_of_v<Object, T>;

    class Object : public std::enable_shared_from_this<Object>
    {
    public:
        enum class State : std::uint8_t
        {
            Created,
            Started,
            Shutdown,
        };

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

        [[nodiscard]] auto getState() const -> State
        {
            return state_;
        }

        [[nodiscard]] auto isStarted() const -> bool
        {
            return state_ == State::Started;
        }

        // Returns true if added. Callers that built the child themselves may ignore the result.
        auto addChild(std::shared_ptr<Object> x) -> bool
        {
            if (x == nullptr || x->parent_.lock() != nullptr)
            {
                // One parent only: remove() it first to move it.
                return false;
            }

            x->parent_ = weak_from_this();
            auto& child = children_.emplace_back(std::move(x));

            if (state_ == State::Started)
            {
                // Late start: the child's subtree is already formed.
                child->startup();
            }

            return true;
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
        auto getChild(std::size_t x = 0) -> std::shared_ptr<T>
        {
            auto children = getChildren<T>();

            if (x >= std::size(children))
            {
                return nullptr;
            }

            return children.at(x);
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
            const auto parent = parent_.lock();

            if (parent == nullptr)
            {
                return;
            }

            // Erasing below may drop the parent's reference to this.
            const auto self = shared_from_this();
            // While still attached, so hooks can reach the parent.
            shutdown();
            parent_.reset();
            // Last: touch no members afterwards.
            std::erase(parent->children_, self);
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

        auto startup() -> void
        {
            // Depth-first, parent before children, first child first.
            std::vector<std::shared_ptr<Object>> pending{shared_from_this()};

            while (!std::empty(pending))
            {
                const auto object = std::move(pending.back());
                pending.pop_back();

                if (object->state_ == State::Started)
                {
                    continue;
                }

                // Before the hook: a re-entrant call returns, and addChild() starts new children.
                object->state_ = State::Started;
                object->onStartup();

                // Snapshot taken after the hook, which may add or remove children. Reversed so the first child pops first.
                pending.insert(std::end(pending), std::rbegin(object->children_), std::rend(object->children_));
            }
        }

        auto shutdown() -> void
        {
            // Children last to first, each before its parent's hook. A frame is pushed unexpanded, then again once its children are queued.
            struct Frame
            {
                std::shared_ptr<Object> object;
                bool expanded;
            };

            std::vector<Frame> pending{{.object = shared_from_this(), .expanded = false}};

            while (!std::empty(pending))
            {
                const auto [object, expanded] = std::move(pending.back());
                pending.pop_back();

                if (expanded)
                {
                    object->onShutdown();
                    continue;
                }

                if (object->state_ != State::Started)
                {
                    continue;
                }

                // Before the children: addChild() starts nothing during teardown.
                object->state_ = State::Shutdown;
                pending.push_back({.object = object, .expanded = true});

                for (const auto& child : object->children_)
                {
                    // Last child pops first.
                    pending.push_back({.object = child, .expanded = false});
                }
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

        virtual auto onShutdown() noexcept -> void
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
        State state_{State::Created};
    };
}
