module;

#include <cstdlib>

export module aspire.graphics:renderservice;

import std;
import aspire.core;
import :color;
import :drawlist;
import :drawstate;
import :node;
import :rect;
import :renderbackend;
import :transform;

export namespace aspire::graphics
{
    /// @brief Runs a 2D scene: updates its nodes each frame, then collects, sorts and submits their drawing.
    ///
    /// The scene's roots are this service's `Node` children, drawn in child order. Like any `Service`, it must be a
    /// direct child of `Engine`. It draws through a `RenderBackend` the host provides and owns, and routes mouse and
    /// keyboard input to its nodes.
    ///
    /// Registers the property `clearColor` (`[r, g, b]` or `[r, g, b, a]`).
    class RenderService : public aspire::core::Service
    {
    public:
        RenderService()
        {
            registerProperty("clearColor", clearColor_);
        }

        /// @brief Sets the backend frames are submitted to.
        /// @param x The backend, or null to collect frames without drawing them. It must stay valid until this
        /// service has shut down.
        auto setBackend(RenderBackend* x) -> void
        {
            backend_ = x;
        }

        /// @brief Sets the color each frame is cleared to before drawing.
        /// @param x The clear color.
        auto setClearColor(Color x) -> void
        {
            clearColor_ = x;
        }

        /// @brief Reports the color each frame is cleared to.
        /// @return The clear color. Opaque black by default.
        [[nodiscard]] auto getClearColor() const -> Color
        {
            return clearColor_;
        }

        /// @brief Reports the most recent frame's draw list, sorted, for tests and debugging.
        /// @return The list `render()` last built. Its text views are only valid while the nodes are unchanged.
        [[nodiscard]] auto drawList() const -> const DrawList&
        {
            return list_;
        }

        /// @brief Sets the node that gets keyboard input first.
        /// @param x The node, or null for none. It's held weakly, so a node that's destroyed loses focus.
        auto setFocus(const std::shared_ptr<Node>& x) -> void
        {
            focus_ = x;
        }

        /// @brief Reports the node that gets keyboard input first.
        /// @return The node, or null if there's none.
        [[nodiscard]] auto getFocus() const -> std::shared_ptr<Node>
        {
            return focus_.lock();
        }

        /// @brief Routes mouse and keyboard input to the scene's nodes.
        ///
        /// Only started, enabled and visible nodes receive input, and a disabled or hidden node hides its subtree.
        ///
        /// - Presses, releases and scrolling go front to back (the reverse of draw order) to each node whose
        ///   `hitTest()` passes inside its clip, until one marks the event handled.
        /// - Movement goes front to back to every node, until one marks it handled.
        /// - Keys go to the focused node first, then to the rest in tree order, until one marks them handled.
        ///
        /// An event that's already handled when it arrives isn't routed. Other events aren't routed to nodes.
        ///
        /// @param x The event. Its `handled` flag reflects the nodes'.
        auto event(aspire::core::Event& x) -> void override
        {
            if (auto* mouse = std::get_if<aspire::core::EventMouse>(&x); mouse != nullptr && !mouse->handled)
            {
                routeMouse(*mouse);
            }
            else if (auto* keyboard = std::get_if<aspire::core::EventKeyboard>(&x); keyboard != nullptr && !keyboard->handled)
            {
                routeKeyboard(*keyboard);
            }
        }

        /// @brief Calls `update()` on every started, enabled node, parents first.
        ///
        /// Works on a snapshot taken before the first call: a node added during the phase starts updating next
        /// frame. A node that an earlier one removes, shuts down or disables is skipped. Disabling a node's parent
        /// mid-phase takes effect for its children from the next frame.
        ///
        /// @param x Seconds since the previous frame.
        auto update(float x) -> void override
        {
            forEachEnabledNode([x](Node& node) { node.update(x); });
        }

        /// @brief Calls `updateFixed()` on every started, enabled node, parents first, with the same snapshot rules
        /// as `update()`.
        /// @param x Seconds in one fixed step.
        auto updateFixed(float x) -> void override
        {
            forEachEnabledNode([x](Node& node) { node.updateFixed(x); });
        }

        /// @brief Collects every root's drawing into one list, sorts it and submits it to the backend.
        ///
        /// Without a backend, the list is still built. If the backend fails, the parent `Engine` quits with
        /// `EXIT_FAILURE`.
        auto render() -> void override
        {
            list_.clear();

            for (const auto& child : getChildren())
            {
                if (const auto* root = dynamic_cast<const Node*>(child.get()); root != nullptr)
                {
                    Collect(*root, list_);
                }
            }

            list_.sort();

            if (backend_ != nullptr && !backend_->submit(list_, clearColor_))
            {
                if (const auto engine = getParent<aspire::core::Engine>(); engine != nullptr)
                {
                    engine->quit(EXIT_FAILURE);
                }
            }
        }

    private:
        // A node that can receive input, with where it draws: its transform to screen, its clip, its layer, and its
        // position in tree order.
        struct Target
        {
            std::shared_ptr<Node> node;
            DrawState state;
            std::uint32_t order{};
        };

        // Started and enabled nodes update. Visibility doesn't matter: a hidden node keeps updating.
        static auto receivesUpdates(const Node& x) -> bool
        {
            return x.isStarted() && x.getEnabled();
        }

        // Input also needs the node to be shown: you can't click what isn't there.
        static auto receivesInput(const Node& x) -> bool
        {
            return receivesUpdates(x) && x.isVisible();
        }

        // Visits the scene's nodes in tree order, parents first, with where each draws. A node `keep` rejects is
        // skipped with its whole subtree, as is any child that isn't a Node. Nodes are passed as shared_ptrs, so the
        // visitor can keep them alive while handlers change the tree.
        template <typename Keep, typename Visit>
        auto walk(const Keep& keep, const Visit& visit) const -> void
        {
            // A pair, not a local struct: GCC's module support has crashed on a local struct before (see
            // Object::shutdown).
            std::vector<std::pair<std::shared_ptr<Node>, DrawState>> pending;

            // Reversed, so the first child pops first.
            for (const auto& child : getChildren() | std::views::reverse)
            {
                if (auto root = std::dynamic_pointer_cast<Node>(child); root != nullptr)
                {
                    pending.emplace_back(std::move(root), DrawState{});
                }
            }

            while (!std::empty(pending))
            {
                auto [node, parent] = std::move(pending.back());
                pending.pop_back();

                if (!keep(*node))
                {
                    continue;
                }

                const auto state = ChildState(parent, *node);

                for (const auto& child : node->getChildren() | std::views::reverse)
                {
                    if (auto x = std::dynamic_pointer_cast<Node>(child); x != nullptr)
                    {
                        pending.emplace_back(std::move(x), state);
                    }
                }

                visit(std::move(node), state);
            }
        }

        // The nodes that can receive input, in tree order.
        [[nodiscard]] auto gatherTargets() const -> std::vector<Target>
        {
            std::vector<Target> targets;

            walk(receivesInput,
                 [&targets](std::shared_ptr<Node> node, const DrawState& state)
                 {
                     const auto order = static_cast<std::uint32_t>(std::size(targets));
                     targets.push_back(Target{.node = std::move(node), .state = state, .order = order});
                 });

            return targets;
        }

        auto routeMouse(aspire::core::EventMouse& x) const -> void
        {
            using Type = aspire::core::EventMouse::Type;

            const auto moved = x.type == Type::Moved;

            if (!moved && x.type != Type::ButtonPressed && x.type != Type::ButtonReleased && x.type != Type::Scrolled)
            {
                return;
            }

            auto targets = gatherTargets();

            // Front-most first: the reverse of draw order, which is by layer, then tree order.
            std::ranges::sort(targets, {}, [](const Target& target) { return std::tuple{target.state.layer, target.order}; });

            for (const auto& target : targets | std::views::reverse)
            {
                // An earlier handler may have removed, disabled or hidden this node.
                if (!receivesInput(*target.node))
                {
                    continue;
                }

                const auto toLocal = target.state.transform.inverse();
                auto local = x;
                local.position = toLocal.apply(x.position);
                local.delta = {.x = x.delta.x * toLocal.scale.x, .y = x.delta.y * toLocal.scale.y};

                // Presses and scrolling need to land on the node, inside the part of it that's shown.
                const auto clipped = target.state.clipRect.has_value() && !Contains(*target.state.clipRect, x.position);

                if (!moved && (clipped || !target.node->hitTest(local.position)))
                {
                    continue;
                }

                target.node->eventMouse(local);

                if (local.handled)
                {
                    x.handled = true;
                    break;
                }
            }
        }

        auto routeKeyboard(aspire::core::EventKeyboard& x) const -> void
        {
            const auto targets = gatherTargets();
            const auto focus = focus_.lock();
            const auto isFocus = [&focus](const Target& target) { return focus != nullptr && target.node == focus; };

            // The focused node goes first, if it can receive input; then the rest, in tree order.
            if (std::ranges::any_of(targets, isFocus))
            {
                focus->eventKeyboard(x);
            }

            for (const auto& target : targets)
            {
                if (x.handled)
                {
                    break;
                }

                if (!isFocus(target) && receivesInput(*target.node))
                {
                    target.node->eventKeyboard(x);
                }
            }
        }

        // Calls f on a snapshot of the nodes that update, skipping any an earlier call removed, shut down or disabled.
        // F is called with a Node&. It isn't constrained with std::invocable: importing :rect brings <concepts> in
        // as a header through nlohmann's JSON, and clang-cl then can't see std::invocable from `import std`.
        template <typename F>
        auto forEachEnabledNode(const F& f) const -> void
        {
            std::vector<std::shared_ptr<Node>> snapshot;
            walk(receivesUpdates, [&snapshot](auto node, [[maybe_unused]] const auto& state) { snapshot.emplace_back(std::move(node)); });

            for (const auto& node : snapshot)
            {
                if (receivesUpdates(*node))
                {
                    f(*node);
                }
            }
        }

        DrawList list_;
        RenderBackend* backend_{};
        Color clearColor_{Black};
        std::weak_ptr<Node> focus_;
    };
}
