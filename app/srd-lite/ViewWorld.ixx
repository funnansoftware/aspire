export module sl.viewworld;

import std;
import aspire.core;
import aspire.graphics;
import sl.level;

export namespace sl
{
    /// @brief Shows a level with a character on it: highlights the hovered tile, the tiles the character can reach,
    /// and the path to the hovered one, and moves the character to a reachable tile when it's clicked.
    ///
    /// Expects a `Level` child and a `Sprite` child for the character, which starts on the tile under its position.
    /// It clips to the level and takes clicks anywhere on it.
    class ViewWorld : public aspire::graphics::Node
    {
    public:
        /// @brief Moves the character to its tile and recomputes where it can go.
        auto refreshMovement() -> void
        {
            movementRange_ = {};
            reachableTiles_.clear();
            path_.clear();

            if (level_ == nullptr || character_ == nullptr)
            {
                return;
            }

            const auto tile = level_->tileBounds(characterTile_);
            character_->setPosition({.x = tile.x, .y = tile.y});
            movementRange_ = level_->movementFrom(characterTile_, Range);
            reachableTiles_ = movementRange_.reachableTiles();
            updatePath();
        }

        auto eventMouse(aspire::core::EventMouse& x) -> void override
        {
            if (level_ == nullptr)
            {
                return;
            }

            const auto tile = level_->tileAt(x.position);

            if (tile != hoveredTile_)
            {
                hoveredTile_ = tile;
                updatePath();
            }

            using aspire::core::EventMouse;

            if (x.type == EventMouse::Type::ButtonPressed && x.button == EventMouse::Button::Left && tile.has_value()
                && movementRange_.canMoveTo(*tile))
            {
                characterTile_ = *tile;
                refreshMovement();
                x.handled = true;
            }
        }

        // Drawn before the children, so the level and character draw over these. The floor tile is transparent.
        auto draw(aspire::graphics::Renderer& x) const -> void override
        {
            if (level_ == nullptr)
            {
                return;
            }

            if (hoveredTile_.has_value())
            {
                x.rect(level_->tileBounds(*hoveredTile_), HoverColor);
            }

            for (const auto tile : reachableTiles_)
            {
                x.rect(level_->tileBounds(tile), ReachableColor);
            }

            // The path's first tile is where the character stands.
            for (std::size_t i = 1; i < std::size(path_); ++i)
            {
                x.rect(level_->tileBounds(path_.at(i)), PathColor);
            }
        }

    protected:
        auto onStartup() -> void override
        {
            level_ = getChild<Level>();
            character_ = getChild<aspire::graphics::Sprite>();

            if (level_ != nullptr)
            {
                const auto size = level_->size();
                const aspire::graphics::Rect area{.x = 0.0F, .y = 0.0F, .w = size.x, .h = size.y};
                setClip(area);
                setBounds(area);
            }

            if (level_ != nullptr && character_ != nullptr)
            {
                characterTile_ = level_->tileAt(character_->getPosition()).value_or(Tile{});
            }

            refreshMovement();
        }

    private:
        static constexpr int Range{3};
        static constexpr aspire::graphics::Color HoverColor{.r = 255, .g = 255, .b = 255, .a = 125};
        static constexpr aspire::graphics::Color ReachableColor{.r = 80, .g = 160, .b = 255, .a = 65};
        static constexpr aspire::graphics::Color PathColor{.r = 255, .g = 210, .b = 70, .a = 110};

        auto updatePath() -> void
        {
            path_ = hoveredTile_.has_value() && movementRange_.canMoveTo(*hoveredTile_) ? movementRange_.pathTo(*hoveredTile_) : std::vector<Tile>{};
        }

        std::shared_ptr<Level> level_;
        std::shared_ptr<aspire::graphics::Sprite> character_;
        MovementRange movementRange_;
        Tile characterTile_;
        std::vector<Tile> reachableTiles_;
        std::vector<Tile> path_;
        std::optional<Tile> hoveredTile_;
    };
}
