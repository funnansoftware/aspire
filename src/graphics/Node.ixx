export module aspire.graphics:node;

import std;
import aspire.core;
import :drawlist;
import :drawstate;
import :rect;
import :renderer;
import :transform;

export namespace aspire::graphics
{
    /// @brief A node in a 2D scene: positioned and scaled relative to its parent, and drawn by `Collect`.
    ///
    /// Registers the properties `position` (`[x, y]`), `scale` (`[x, y]`), `layer` (a number, or null to inherit),
    /// `visible`, `enabled`, `clip` (`[x, y, w, h]` in local coordinates, or null for none), and `bounds` (the same,
    /// for the area that receives presses).
    ///
    /// `RenderService` calls `update()` and `updateFixed()` on every started, enabled node each frame, and routes
    /// mouse and keyboard input to every started, enabled, visible node through `eventMouse()` and
    /// `eventKeyboard()`.
    class Node : public aspire::core::Object
    {
    public:
        Node()
        {
            registerProperty("position", position_);
            registerProperty("scale", scale_);
            registerProperty("layer", layer_);
            registerProperty("visible", visible_);
            registerProperty("enabled", enabled_);
            registerProperty("clip", clip_);
            registerProperty("bounds", bounds_);
        }

        /// @brief Sets where this node sits in its parent's coordinates.
        /// @param x The position of this node's origin.
        auto setPosition(aspire::core::Vec2 x) -> void
        {
            position_ = x;
        }

        /// @brief Reports where this node sits in its parent's coordinates.
        /// @return The position of this node's origin.
        [[nodiscard]] auto getPosition() const -> aspire::core::Vec2
        {
            return position_;
        }

        /// @brief Sets the scale applied to this node's contents and its children.
        /// @param x The scale. Both components must be positive.
        auto setScale(aspire::core::Vec2 x) -> void
        {
            scale_ = x;
        }

        /// @brief Reports the scale applied to this node's contents and its children.
        /// @return The scale.
        [[nodiscard]] auto getScale() const -> aspire::core::Vec2
        {
            return scale_;
        }

        /// @brief Sets the layer for this node's subtree.
        /// @param x The layer, or `std::nullopt` to inherit the parent's.
        auto setLayer(std::optional<int> x) -> void
        {
            layer_ = x;
        }

        /// @brief Reports the layer set on this node.
        /// @return The layer, or `std::nullopt` if it inherits its parent's.
        [[nodiscard]] auto getLayer() const -> std::optional<int>
        {
            return layer_;
        }

        /// @brief Shows or hides this node and its subtree.
        /// @param x `false` to hide.
        auto setVisible(bool x) -> void
        {
            visible_ = x;
        }

        /// @brief Reports whether this node and its subtree are shown.
        /// @return `false` if hidden.
        [[nodiscard]] auto isVisible() const -> bool
        {
            return visible_;
        }

        /// @brief Enables or disables this node and its subtree.
        ///
        /// A disabled node, and everything below it, gets no `update()` or `updateFixed()` calls. It still draws:
        /// hide it with `setVisible(false)` as well to make it disappear.
        ///
        /// @param x `false` to disable.
        auto setEnabled(bool x) -> void
        {
            enabled_ = x;
        }

        /// @brief Reports whether this node and its subtree are enabled.
        /// @return `false` if disabled. Nodes are enabled by default.
        [[nodiscard]] auto getEnabled() const -> bool
        {
            return enabled_;
        }

        /// @brief Clips this node and its subtree to a rectangle. A child's clip can only narrow it.
        /// @param x The rectangle in local coordinates, or `std::nullopt` for no clip of its own.
        auto setClip(std::optional<Rect> x) -> void
        {
            clip_ = x;
        }

        /// @brief Reports this node's own clip rectangle.
        /// @return The rectangle in local coordinates, or `std::nullopt` for none.
        [[nodiscard]] auto getClip() const -> std::optional<Rect>
        {
            return clip_;
        }

        /// @brief Sets the area that receives presses and scrolling. Children aren't included.
        /// @param x The area in local coordinates, or `std::nullopt` (the default) for none.
        auto setBounds(std::optional<Rect> x) -> void
        {
            bounds_ = x;
        }

        /// @brief Reports the area that receives presses and scrolling.
        /// @return The area in local coordinates, or `std::nullopt` for none.
        [[nodiscard]] auto getBounds() const -> std::optional<Rect>
        {
            return bounds_;
        }

        /// @brief Reports whether a point is on this node, for routing presses and scrolling.
        ///
        /// Override it for a hit area that isn't a rectangle. By default, a point hits if it's inside `getBounds()`,
        /// so a node without bounds is never hit.
        ///
        /// @param x The point, in local coordinates.
        /// @return `true` if the point is on this node.
        [[nodiscard]] virtual auto hitTest(aspire::core::Vec2 x) const -> bool
        {
            return bounds_.has_value() && Contains(*bounds_, x);
        }

        /// @brief Reports the transform from this node's coordinates to its parent's.
        /// @return The node's position and scale as a transform.
        [[nodiscard]] auto localTransform() const -> Transform
        {
            return {.offset = position_, .scale = scale_};
        }

        /// @brief Records this node's own drawing. Called by `Collect`, before the node's children draw.
        ///
        /// Strings passed to the renderer must stay valid until the list is submitted, so format changing text
        /// into a member during update or event handling, not here.
        ///
        /// @param x The renderer, set up with this node's transform, clip and layer.
        virtual auto draw([[maybe_unused]] Renderer& x) const -> void
        {
        }

        /// @brief Advances this node by one frame of variable length. Called by `RenderService`, parents first.
        /// @param x Seconds since the previous frame.
        virtual auto update([[maybe_unused]] float x) -> void
        {
        }

        /// @brief Advances this node by one fixed step. Called by `RenderService`, parents first.
        /// @param x Seconds in one fixed step.
        virtual auto updateFixed([[maybe_unused]] float x) -> void
        {
        }

        /// @brief Receives mouse and touch input. Called by `RenderService`, front-most node first.
        ///
        /// Presses, releases and scrolling arrive only when they hit this node. Movement arrives whether or not the
        /// pointer is over it. Set `handled` to stop the event reaching the nodes behind this one.
        ///
        /// @param x The event, with `position` and `delta` in this node's local coordinates.
        virtual auto eventMouse([[maybe_unused]] aspire::core::EventMouse& x) -> void
        {
        }

        /// @brief Receives keyboard input. Called by `RenderService`: the focused node first, then the rest in tree
        /// order. Set `handled` to stop the event reaching the nodes after this one.
        /// @param x The event.
        virtual auto eventKeyboard([[maybe_unused]] aspire::core::EventKeyboard& x) -> void
        {
        }

    private:
        aspire::core::Vec2 position_{};
        aspire::core::Vec2 scale_{.x = 1.0F, .y = 1.0F};
        std::optional<int> layer_;
        std::optional<Rect> clip_;
        std::optional<Rect> bounds_;
        bool visible_{true};
        bool enabled_{true};
    };

    /// @brief Works out where a node draws, from where its parent draws.
    ///
    /// Composes the transforms, inherits the parent's layer unless the node sets its own, and narrows the parent's
    /// clip rectangle to the node's own clip, if it has one. The clip index is left as the parent's: only `Collect`
    /// allocates clip indices.
    ///
    /// @param parent Where the parent draws. Default-constructed for a root.
    /// @param x The node.
    /// @return Where the node, and the children it passes its state to, draw.
    [[nodiscard]] auto ChildState(const DrawState& parent, const Node& x) -> DrawState
    {
        auto state = DrawState{.transform = parent.transform.then(x.localTransform()),
                               .clip = parent.clip,
                               .clipRect = parent.clipRect,
                               .layer = x.getLayer().value_or(parent.layer)};

        if (const auto clip = x.getClip(); clip.has_value())
        {
            // Children can only narrow a clip, so intersect with the parent's.
            const auto screen = state.transform.apply(*clip);
            state.clipRect = parent.clipRect.has_value() ? Intersect(screen, *parent.clipRect) : screen;
        }

        return state;
    }

    /// @brief Draws a tree of nodes into a draw list: appends each started, visible node's items, in tree order.
    ///
    /// Parents draw before their children, which draw first to last. A node's position, scale, layer and clip
    /// apply to its whole subtree. A node that isn't a `Node`, and everything below it, isn't drawn.
    ///
    /// @param root The node to start from. Nothing is drawn unless it is started.
    /// @param list The list to append to. Collect doesn't clear or sort it.
    /// @note Allocates a small stack for the walk on each call, sized to the tree's depth. The list's own storage
    /// is reused.
    auto Collect(const Node& root, DrawList& list) -> void
    {
        // Each pending node with the state its parent drew with. A pair, not a local struct: GCC's module support
        // has crashed on a local struct here before (see Object::shutdown).
        std::vector<std::pair<const Node*, DrawState>> pending;
        pending.emplace_back(&root, DrawState{});

        while (!std::empty(pending))
        {
            const auto [node, parent] = pending.back();
            pending.pop_back();

            if (!node->isStarted() || !node->isVisible())
            {
                continue;
            }

            auto state = ChildState(parent, *node);

            // A node with its own clip gets a new clip index, which its children inherit.
            if (node->getClip().has_value() && state.clipRect.has_value())
            {
                state.clip = list.addClip(*state.clipRect);
            }

            Renderer renderer{list, state};
            node->draw(renderer);

            // Reversed, so the first child pops first. Non-Node children, and everything below them, are skipped.
            for (const auto& child : node->getChildren() | std::views::reverse)
            {
                if (const auto* x = dynamic_cast<const Node*>(child.get()); x != nullptr)
                {
                    pending.emplace_back(x, state);
                }
            }
        }
    }
}
