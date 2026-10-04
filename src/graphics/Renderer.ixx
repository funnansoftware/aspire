export module aspire.graphics:renderer;

import std;
import aspire.core;
import :color;
import :drawitem;
import :drawlist;
import :drawrect;
import :drawsprite;
import :drawstate;
import :drawtext;
import :rect;
import :transform;

export namespace aspire::graphics
{
    /// @brief What a node draws with: takes local coordinates, records screen-space draw items.
    ///
    /// It doesn't draw immediately. Items are recorded into a `DrawList`, which is sorted and then submitted to a
    /// backend after the whole tree has been collected.
    class Renderer
    {
    public:
        /// @brief Creates a renderer that records into a list with the given state.
        /// @param list The list to record into. It must outlive the renderer.
        /// @param state The transform, clip and layer to record with.
        Renderer(DrawList& list, DrawState state) : list_{&list}, state_{std::move(state)}
        {
        }

        /// @brief Draws a region of a texture.
        /// @param source The texture's name. It must stay valid until the list is submitted.
        /// @param region The part of the texture to draw, in texture pixels.
        /// @param bounds Where to draw it, in local coordinates.
        /// @param tint A color the texture is multiplied by.
        /// @param order The order within the layer. Zero lets tree order decide.
        auto sprite(std::string_view source, Rect region, Rect bounds, Color tint = White, float order = 0.0F) -> void;

        /// @brief Fills a rectangle.
        /// @param bounds The rectangle, in local coordinates.
        /// @param color The fill color.
        /// @param order The order within the layer. Zero lets tree order decide.
        auto rect(Rect bounds, Color color, float order = 0.0F) -> void;

        /// @brief Draws a line of text, scaled with the node.
        /// @param value The text. It must stay valid until the list is submitted.
        /// @param position Its top-left corner, in local coordinates.
        /// @param color The text color.
        /// @param order The order within the layer. Zero lets tree order decide.
        auto text(std::string_view value, aspire::core::Vec2 position, Color color = White, float order = 0.0F) -> void;

        /// @brief Reports the transform from local to screen coordinates.
        /// @return The transform this renderer applies.
        [[nodiscard]] auto transform() const -> const Transform&
        {
            return state_.transform;
        }

    private:
        auto add(DrawPrimitive x, float order) -> void;

        DrawList* list_;
        DrawState state_;
    };

    auto Renderer::sprite(std::string_view source, Rect region, Rect bounds, Color tint, float order) -> void
    {
        add(DrawSprite{.source = source, .region = region, .bounds = state_.transform.apply(bounds), .tint = tint}, order);
    }

    auto Renderer::rect(Rect bounds, Color color, float order) -> void
    {
        add(DrawRect{.bounds = state_.transform.apply(bounds), .color = color}, order);
    }

    auto Renderer::text(std::string_view value, aspire::core::Vec2 position, Color color, float order) -> void
    {
        add(DrawText{.text = value, .position = state_.transform.apply(position), .scale = state_.transform.scale, .color = color}, order);
    }

    auto Renderer::add(DrawPrimitive x, float order) -> void
    {
        list_->add(DrawItem{.layer = state_.layer, .order = order, .sequence = 0, .clip = state_.clip, .primitive = std::move(x)});
    }
}
