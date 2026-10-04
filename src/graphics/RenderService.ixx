module;

#include <cstdlib>

export module aspire.graphics:renderservice;

import std;
import aspire.core;
import :color;
import :drawlist;
import :node;
import :renderbackend;

export namespace aspire::graphics
{
    /// @brief Runs a 2D scene: updates its nodes each frame, then collects, sorts and submits their drawing.
    ///
    /// The scene's roots are this service's `Node` children, drawn in child order. Like any `Service`, it must be a
    /// direct child of `Engine`. It draws through a `RenderBackend` the host provides and owns.
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

        /// @brief Does nothing yet. Routing input to nodes comes later.
        /// @param x The event.
        auto event([[maybe_unused]] aspire::core::Event& x) -> void override
        {
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
        // Snapshot of the enabled, started nodes in tree order, parents first. A disabled node hides its subtree.
        template <std::invocable<Node&> F>
        auto forEachEnabledNode(const F& f) -> void
        {
            pending_.clear();
            snapshot_.clear();

            // Reversed, so the first child pops first.
            for (const auto& child : getChildren() | std::views::reverse)
            {
                if (auto root = std::dynamic_pointer_cast<Node>(child); root != nullptr)
                {
                    pending_.emplace_back(std::move(root));
                }
            }

            while (!std::empty(pending_))
            {
                auto node = std::move(pending_.back());
                pending_.pop_back();

                if (!node->isStarted() || !node->getEnabled())
                {
                    continue;
                }

                for (const auto& child : node->getChildren() | std::views::reverse)
                {
                    if (auto x = std::dynamic_pointer_cast<Node>(child); x != nullptr)
                    {
                        pending_.emplace_back(std::move(x));
                    }
                }

                snapshot_.emplace_back(std::move(node));
            }

            for (const auto& node : snapshot_)
            {
                if (node->isStarted() && node->getEnabled())
                {
                    f(*node);
                }
            }

            // Release the nodes now, so a removed node isn't kept alive until the next phase.
            snapshot_.clear();
        }

        DrawList list_;

        // Reused by forEachEnabledNode(), so each phase keeps their storage.
        std::vector<std::shared_ptr<Node>> pending_;
        std::vector<std::shared_ptr<Node>> snapshot_;

        RenderBackend* backend_{};
        Color clearColor_{Black};
    };
}
