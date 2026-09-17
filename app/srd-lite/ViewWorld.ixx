module;

#include <raylib.h>
#include <sigslot/signal.hpp>

export module sl.viewworld;

import std;
import aspire.core.object;
import aspire.core.overloaded;
import aspire.core.event;
import aspire.core.vec2;
import aspire.raylib.node;
import aspire.raylib.texture;
import aspire.raylib.tilemap;

export namespace sl
{
    class ViewWorld : public aspire::raylib::Node
    {
    public:
        // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes,misc-non-private-member-variables-in-classes)
        sigslot::signal<Vector2> onTileClicked;

        auto refreshMovement() -> void
        {
            movementRange_ = {};
            reachableTiles_.clear();
            path_.clear();

            if (tileMap_ == nullptr)
            {
                return;
            }

            const auto tile = tileMap_->tileToRectangle(characterTile_);
            character_->setPosition(tileMap_->mapFromLocal({.x = tile.x, .y = tile.y}));
            constexpr auto range = 3;
            movementRange_ = tileMap_->movementFrom(characterTile_, range);
            reachableTiles_ = movementRange_.reachableTiles();
            updatePath();
        }

    protected:
        auto onStartup() -> void override
        {
            tileMap_ = getChild<aspire::raylib::TileMap>();
            character_ = getChild<aspire::raylib::Texture>();

            characterTile_ = *tileMap_->tileAtPosition(character_->getPosition());

            refreshMovement();
        }

        auto onEvent(aspire::core::Event& x) -> void override
        {
            using aspire::core::EventMouse;

            auto* event = std::get_if<EventMouse>(&x);

            if (event == nullptr)
            {
                return;
            }

            mousePosition_ = mapFromGlobal({.x = event->position.x, .y = event->position.y});
            const auto tile = tileMap_->tileAtPosition(mousePosition_);

            if (tile != hoveredTile_)
            {
                hoveredTile_ = tile;
                updatePath();
            }

            if (event->type == EventMouse::Type::ButtonPressed && event->button == EventMouse::Button::Left && tile.has_value()
                && movementRange_.canMoveTo(*tile))
            {
                characterTile_ = *tile;
                refreshMovement();
                event->handled = true;
            }
        }

        auto onRenderPre() const -> void override
        {
            Node::onRenderPre();

            const auto x = static_cast<int>(getPosition().x);
            const auto y = static_cast<int>(getPosition().y);
            BeginScissorMode(x, y, tileMap_->width(), tileMap_->height());
        }

        auto onRender() const -> void override
        {
            if (tileMap_ == nullptr)
            {
                return;
            }

            const auto tile = tileMap_->positionToRectangle(mousePosition_);
            constexpr auto transparent = 125;
            auto white = WHITE;
            white.a = transparent;
            DrawRectangle(static_cast<int>(tile.x), static_cast<int>(tile.y), static_cast<int>(tile.width), static_cast<int>(tile.height), white);
            renderMovement();
        }

        auto onRenderPost() const -> void override
        {
            EndScissorMode();
            Node::onRenderPost();
        }

        auto updatePath() -> void
        {
            path_ = hoveredTile_.has_value() && movementRange_.canMoveTo(*hoveredTile_) ? movementRange_.pathTo(*hoveredTile_)
                                                                                        : std::vector<aspire::raylib::Tile>{};
        }

        auto renderMovement() const -> void
        {
            constexpr Color reachableColor{.r = 80, .g = 160, .b = 255, .a = 65};
            constexpr Color pathColor{.r = 255, .g = 210, .b = 70, .a = 110};
            // constexpr auto centerFraction = 0.5F;

            for (const auto tile : reachableTiles_)
            {
                DrawRectangleRec(tileMap_->tileToRectangle(tile), reachableColor);
            }

            for (const auto [i, tile] : path_ | std::views::drop(1) | std::views::enumerate)
            {
                const auto to = tileMap_->tileToRectangle(path_.at(i));
                DrawRectangleRec(to, pathColor);
            }
        }

    private:
        std::shared_ptr<aspire::raylib::TileMap> tileMap_;
        std::shared_ptr<aspire::raylib::Texture> character_;
        Vector2 mousePosition_{};
        aspire::raylib::MovementRange movementRange_;
        aspire::raylib::Tile characterTile_;
        std::vector<aspire::raylib::Tile> reachableTiles_;
        std::vector<aspire::raylib::Tile> path_;
        std::optional<aspire::raylib::Tile> hoveredTile_;
    };
}